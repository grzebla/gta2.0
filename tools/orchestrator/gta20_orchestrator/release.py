"""Gatekeeper: final, independent review and merge of pull requests into the target branch.

Order of authority:
1. Deterministic gates (this module, no model): eligibility, mergeable, CI green on the exact head
   commit, no game data or secrets, and - for the agents' integration PR - only commits that
   belong to Astra-approved, verified tasks or to orchestrator memory updates.
2. The Gatekeeper model (`gpt-6.1-sol` by default, independent of Astra) reviews the whole change.
3. Merge with `gh pr merge --merge --match-head-commit <reviewed sha>` (never --admin), then
   retarget PRs that were stacked on the merged branch.

A model MERGE can never override a failed gate. Each head commit is reviewed once; a new push
gets a new review.
"""

from __future__ import annotations

import datetime as _dt
import os
import re
import subprocess
import time
import uuid
from typing import Any, Dict, List, Optional, Tuple

from . import db as D
from . import gitops, memory, prompts, schemas
from .adapters.base import AgentFailed, AgentTimeout, ModelMismatch
from .adapters.claude import build_settings
from .util import dumps, find_secrets, iso, loads, now, truncate
from .verify import command_env, protected_hits, run_commands

TASK_SUBJECT = re.compile(r"^\[(T\d{4})\] ")
MERGE_SUBJECT = re.compile(r"^Merge (T\d{4}): ")
MEMORY_SUBJECT = re.compile(r"^agent-memory: ")
SYNC_SUBJECT = "Sync %s into %s (orchestrator)"
CONFLICT_MARKER = re.compile(r"^(<<<<<<< |>>>>>>> |=======$)", re.M)


def unmerged(wt: Any) -> List[str]:
    out = gitops.git(wt, "diff", "--name-only", "--diff-filter=U", check=False)
    return sorted(set(f for f in out.splitlines() if f))


class Gatekeeper:
    def __init__(self, orch: Any, agent: Any, port: Any):
        self.o = orch
        self.cfg = orch.cfg
        self.db = orch.db
        self.agent = agent
        self.port = port
        self.rel = self.cfg["release"]
        self.target = self.rel["target_branch"]
        self.owner = self.cfg["github_repo"].split("/")[0]
        self.integ = self.cfg["integration_branch"]
        self.wt = self.cfg.worktree("release")

    # ------------------------------------------------------------------ selection
    def classify(self, prs: List[Dict[str, Any]]) -> Tuple[List[Dict[str, Any]], List[Tuple[int, str]]]:
        eligible, skipped = [], []
        skip_labels = set(self.rel["skip_labels"])
        for pr in sorted(prs, key=lambda p: p["number"]):
            labels = {lbl.get("name") for lbl in pr.get("labels") or []}
            is_integ = pr.get("headRefName") == self.integ
            author = (pr.get("author") or {}).get("login")
            if labels & skip_labels:
                skipped.append((pr["number"], "label %s" % ", ".join(sorted(labels & skip_labels))))
            elif pr.get("baseRefName") != self.target:
                skipped.append((pr["number"], "stacked on %s; waits until it targets %s" % (pr.get("baseRefName"), self.target)))
            elif pr.get("isDraft") and not is_integ:
                skipped.append((pr["number"], "draft"))
            elif not is_integ and self.rel["eligible"] == "integration_only":
                skipped.append((pr["number"], "only the integration PR is managed"))
            elif not is_integ and author != self.owner:
                skipped.append((pr["number"], "author %s is not the repository owner" % author))
            else:
                eligible.append(pr)
        return eligible, skipped

    def memo(self, pr: Dict[str, Any]) -> Dict[str, Any]:
        return loads(self.db.get_meta("gatekeeper:%d" % pr["number"]), {}) or {}

    def set_memo(self, pr: Dict[str, Any], decision: str, extra: Optional[Dict[str, Any]] = None) -> None:
        value = {"sha": pr["headRefOid"], "decision": decision, "at": now()}
        value.update(extra or {})
        with self.db.tx():
            self.db.set_meta("gatekeeper:%d" % pr["number"], dumps(value))

    def merges_today(self) -> int:
        start = _dt.datetime.combine(_dt.date.today(), _dt.time()).timestamp()
        return int(self.db.conn.execute("SELECT COUNT(*) AS n FROM events WHERE type='pr_merged' AND ts>=?",
                                        (iso(start),)).fetchone()["n"])

    # ------------------------------------------------------------------ gates
    def gates(self, pr: Dict[str, Any]) -> Dict[str, Any]:
        fails: List[str] = []
        pending: List[str] = []
        lines: List[str] = []
        sha = pr["headRefOid"]
        mergeable = pr.get("mergeable")
        state = pr.get("mergeStateStatus")
        if mergeable == "CONFLICTING" or state == "DIRTY":
            fails.append("merge conflicts with %s" % self.target)
        elif mergeable != "MERGEABLE":
            pending.append("GitHub has not computed mergeability yet (%s)" % mergeable)
        if state == "BLOCKED":
            fails.append("GitHub reports the merge as BLOCKED")
        checks = self.port.checks(pr["number"])
        if checks is None:
            pending.append("could not read CI checks")
            checks = []
        buckets: Dict[str, List[str]] = {}
        for c in checks:
            buckets.setdefault(c.get("bucket") or "unknown", []).append(c.get("name") or "?")
        if len(checks) < int(self.rel["min_checks"]):
            pending.append("only %d CI checks reported (need >= %d)" % (len(checks), self.rel["min_checks"]))
        if buckets.get("pending"):
            pending.append("CI still running: " + ", ".join(buckets["pending"]))
        bad = buckets.get("fail", []) + buckets.get("cancel", []) + buckets.get("unknown", [])
        excerpts = ""
        if bad:
            fails.append("CI not green: " + ", ".join(bad))
            getter = getattr(self.port, "failure_excerpts", None)
            if getter:
                try:
                    excerpts = getter(checks) or ""
                except Exception:  # noqa: BLE001 - excerpts are a convenience, never a gate
                    excerpts = ""
        lines.append("CI checks: " + ", ".join("%s=%d" % (k, len(v)) for k, v in sorted(buckets.items())))
        # Local inspection of exactly the reviewed commit.
        try:
            gitops.git(self.o.repo, "fetch", "--quiet", self.cfg["remote"])
            base = "%s/%s" % (self.cfg["remote"], self.target)
            files = [f for f in gitops.git(self.o.repo, "diff", "--name-only", "%s...%s" % (base, sha)).splitlines() if f]
            diff = gitops.git(self.o.repo, "diff", "%s...%s" % (base, sha))
        except gitops.GitError as exc:
            pending.append("cannot inspect head commit locally: %s" % str(exc)[:200])
            files, diff = [], ""
            base = self.target
        exts = tuple(self.cfg["game_data_extensions"])
        data = [f for f in files if f.lower().endswith(exts)]
        if data:
            fails.append("game data / binaries in the diff: " + ", ".join(data[:10]))
        added = "\n".join(l[1:] for l in diff.splitlines() if l.startswith("+") and not l.startswith("+++"))
        secrets = find_secrets(added)
        if secrets:
            fails.append("possible secrets in added lines: " + ", ".join(secrets[:5]))
        tasks_text = "(not an agent PR)"
        if pr.get("headRefName") == self.integ and diff:
            problems, tasks_text = self.integration_provenance(base, sha)
            fails += problems
        if self.merges_today() >= int(self.rel["max_merges_per_day"]):
            pending.append("daily merge cap (%d) reached" % self.rel["max_merges_per_day"])
        lines += ["[FAIL] " + f for f in fails] + ["[WAIT] " + p for p in pending]
        if not fails and not pending:
            lines.append("[PASS] all deterministic gates")
        return {"fails": fails, "pending": pending, "summary": "\n".join(lines), "checks": checks, "files": files,
                "diff": diff, "base": base, "tasks": tasks_text, "ci_excerpts": excerpts}

    def integration_provenance(self, base: str, sha: str) -> Tuple[List[str], str]:
        """Every commit in the agents' PR must be Astra-approved task work or a memory update."""
        problems: List[str] = []
        out = gitops.git(self.o.repo, "log", "--format=%H%x09%s", "%s..%s" % (base, sha))
        task_ids = set()
        for line in out.splitlines():
            commit, _, subject = line.partition("\t")
            sync = self.sync_merge_problems(commit, base)
            if sync is not None:
                problems += sync
                continue
            files = [f for f in gitops.git(self.o.repo, "diff-tree", "--no-commit-id", "--name-only", "-r", "-m",
                                           commit).splitlines() if f]
            m = TASK_SUBJECT.match(subject) or MERGE_SUBJECT.match(subject)
            if m:
                task_ids.add(m.group(1))
                if TASK_SUBJECT.match(subject):
                    hits = protected_hits(files, self.cfg["protected_paths"])
                    if hits:
                        problems.append("task commit %s touches protected paths: %s" % (commit[:9], ", ".join(hits)))
            elif MEMORY_SUBJECT.match(subject):
                stray = [f for f in files if not f.startswith(memory.DIR + "/")]
                if stray:
                    problems.append("memory commit %s touches %s" % (commit[:9], ", ".join(stray)))
            else:
                problems.append("unexpected commit %s on the integration branch: %s" % (commit[:9], subject[:80]))
        lines = []
        for tid in sorted(task_ids):
            t = self.db.task(tid)
            if not t:
                problems.append("commit references unknown task %s" % tid)
                continue
            dec = self.db.decisions(1, tid)
            ver = self.db.verification(tid, t["head_commit"]) if t["head_commit"] else None
            if t["status"] != D.DONE or not dec or dec[0]["decision"] != "APPROVED":
                problems.append("%s is %s, not an Astra-approved task" % (tid, t["status"]))
            elif not (ver and ver["passed"]):
                problems.append("%s has no passed verification for its head" % tid)
            lines.append("- %s [%s] %s — Astra: %s" % (tid, t["status"], t["title"],
                                                         truncate(dec[0]["rationale"] if dec else "-", 300)))
        return problems, "\n".join(lines) or "(no task commits)"

    def sync_merge_problems(self, commit: str, base: str) -> Optional[List[str]]:
        """None when `commit` is not a merge of the target branch into the integration branch.

        Such a merge (orchestrator sync or the owner resolving a conflict) brings in reviewed
        target-branch work. Only files whose result differs from BOTH parents were written by
        hand (conflict resolution, or a combined auto-merge); those must not touch protected
        paths. The Gatekeeper model still sees the whole PR diff."""
        parents = gitops.git(self.o.repo, "rev-list", "--parents", "-n", "1", commit).split()[1:]
        if len(parents) != 2:
            return None
        anc = subprocess.run(["git", "merge-base", "--is-ancestor", parents[1], base], cwd=str(self.o.repo),
                             capture_output=True)
        if anc.returncode != 0:
            return None
        manual = self.hand_merged_files(commit, parents)
        hits = protected_hits(manual, self.cfg["protected_paths"])
        if hits:
            return ["sync merge %s rewrites protected paths by hand: %s" % (commit[:9], ", ".join(hits))]
        return []

    def hand_merged_files(self, commit: str, parents: List[str]) -> List[str]:
        sides = [set(f for f in gitops.git(self.o.repo, "diff", "--name-only", p, commit).splitlines() if f)
                 for p in parents]
        return sorted(sides[0] & sides[1])

    # ------------------------------------------------------------------ keeping the agents' branch current
    def sync_due(self) -> bool:
        last = float(self.db.get_meta("integration_sync_check", "0") or 0)
        return now() - last >= float(self.rel.get("sync_every_s", 1800))

    def sync_integration(self) -> Tuple[str, str]:
        """Merge the target branch into the integration branch so the agents' PR stays mergeable.

        Returns (state, message): state is "current", "merged" (new head pushed, wait for CI) or
        "conflict". A conflicted merge is resolved by the developer agent in its own worktree
        (owner-only paths take the target branch's version) and kept only when the full
        verification passes; the owner is asked only when that fails."""
        remote = self.cfg["remote"]
        wt = self.o.wt_int
        upstream = "%s/%s" % (remote, self.target)
        with self.db.tx():
            self.db.set_meta("integration_sync_check", str(now()))
        gitops.git(wt, "fetch", "--quiet", remote, self.target)
        if subprocess.run(["git", "merge-base", "--is-ancestor", upstream, "HEAD"], cwd=str(wt),
                          capture_output=True).returncode == 0:
            return "current", ""
        if not gitops.is_clean(wt):
            return "conflict", "integration worktree is not clean; not syncing %s" % upstream
        upstream_sha = gitops.rev(wt, upstream)
        if self.db.get_meta("integration_sync_conflict") == upstream_sha:
            return "conflict", "waiting for the owner to merge %s (%s) by hand" % (upstream, upstream_sha[:9])
        message = SYNC_SUBJECT % (upstream, self.integ)
        res = subprocess.run(["git", "merge", "--no-ff", "--no-edit", "-m", message, upstream], cwd=str(wt),
                             capture_output=True, text=True)
        resolved_by = None
        if res.returncode != 0:
            conflicts = unmerged(wt)
            gitops.git(wt, "merge", "--abort", check=False)
            head, why = self.resolve_sync(upstream, upstream_sha, message, conflicts)
            if head is None:
                msg = ("%s conflicts with %s in: %s. The developer agent could not resolve it (%s). Merge it by "
                       "hand: pause, `git merge %s` in the integration worktree (%s), resolve, commit, push, "
                       "resume." % (self.integ, upstream, ", ".join(conflicts) or "(unknown)", why, upstream, wt))
                with self.db.tx():
                    self.db.set_meta("integration_sync_conflict", upstream_sha)
                    self.db.set_meta("human_question", msg)
                    self.db.event("integration_sync_conflict", agent="orchestrator", outcome="conflict",
                                  commits=[upstream_sha], detail={"files": conflicts, "why": why})
                return "conflict", msg
            gitops.git(wt, "merge", "--ff-only", "--quiet", head)
            resolved_by = {"files": conflicts, "by": "claude"}
        head = gitops.rev(wt)
        with self.db.tx():
            if self.db.get_meta("integration_sync_conflict"):
                self.db.set_meta("integration_sync_conflict", "")
            self.db.event("integration_synced", agent="orchestrator", commits=[head, upstream_sha], outcome="merged",
                          detail={"from": upstream, "conflicts": resolved_by})
        self.o.maybe_push(force_now=True)
        return "merged", "synced %s into %s (%s%s); CI runs on the new head" % (
            upstream, self.integ, head[:9], ", conflicts resolved by the developer agent" if resolved_by else "")

    def resolve_sync(self, upstream: str, upstream_sha: str, message: str,
                     conflicts: List[str]) -> Tuple[Optional[str], str]:
        """Redo the conflicted merge in the `sync` worktree and let the developer agent resolve it.

        Returns (merge commit, "") when the result passes every check, else (None, reason)."""
        wt = self.cfg.worktree("sync")
        base = gitops.rev(self.o.wt_int)
        gitops.ensure_worktree(self.o.repo, wt, detach=base)
        gitops.git(wt, "merge", "--abort", check=False)
        gitops.git(wt, "reset", "--quiet", "--hard")
        gitops.git(wt, "clean", "-fdq")
        gitops.checkout_detached(wt, base)
        subprocess.run(["git", "merge", "--no-ff", "--no-commit", upstream_sha], cwd=str(wt), capture_output=True)
        conflicts = unmerged(wt)
        if not conflicts:
            return None, "the merge did not reproduce in the sync worktree"
        # The owner's files (protected paths) are never rewritten by an agent: main's version wins.
        owner_side = protected_hits(conflicts, self.cfg["protected_paths"])
        for f in owner_side:
            if subprocess.run(["git", "cat-file", "-e", "%s:%s" % (upstream_sha, f)], cwd=str(wt)).returncode == 0:
                gitops.git(wt, "checkout", upstream_sha, "--", f)
            else:
                gitops.git(wt, "rm", "--quiet", "--", f)
        rest = [f for f in conflicts if f not in owner_side]
        mb = gitops.git(wt, "merge-base", base, upstream_sha)
        session = str(uuid.uuid4())
        attempts = max(1, int(self.rel.get("sync_resolve_attempts", 2)))
        feedback: Optional[str] = None
        why = ""
        for attempt in range(attempts):
            if rest or feedback:
                prompt = prompts.claude_sync(
                    upstream=upstream, integration=self.integ, conflicts=rest, owner_side=owner_side,
                    ours_log=gitops.log_oneline(wt, mb, base), theirs_log=gitops.log_oneline(wt, mb, upstream_sha),
                    feedback=feedback)
                settings = build_settings(
                    worktree=wt, protected=self.cfg["protected_paths"],
                    extra_write_dirs=[os.path.expanduser("~/.cache/uv"), os.path.expanduser("~/.cache/clang")],
                    sandbox=bool(self.cfg["claude"].get("sandbox", True)),
                    domains=self.cfg["claude"]["sandbox_domains"],
                    allow_web=bool(self.cfg["claude"].get("allow_web", False)))
                try:
                    res = self.o.call(self.o.claude, kind="sync", task_id=None, prompt=prompt, cwd=wt,
                                      schema=schemas.SYNC_REPORT, session_id=session, resume=attempt > 0,
                                      extra={"settings": settings, "add_dirs": [], "env_extra": self.o.claude_env()})
                except (AgentFailed, AgentTimeout, ModelMismatch) as exc:
                    return None, "developer agent failed: %s" % truncate(str(exc), 300)
                out = res.output or {}
                if out.get("status") != "resolved":
                    return None, "developer agent: %s" % truncate(out.get("summary") or "cannot resolve", 600)
            problem = self.check_sync_result(wt, base, upstream_sha, conflicts, message)
            if problem is None:
                return gitops.rev(wt), ""
            why = problem
            feedback = problem
        return None, why

    def check_sync_result(self, wt: Any, base: str, upstream_sha: str, conflicts: List[str],
                          message: str) -> Optional[str]:
        """Commit the resolution (amending an earlier attempt) and verify it. None means it passed."""
        marked = [f for f in conflicts if (wt / f).is_file() and CONFLICT_MARKER.search(
            (wt / f).read_text(errors="replace"))]
        if marked:
            return "conflict markers remain in: %s" % ", ".join(marked)
        gitops.git(wt, "add", "-A")
        left = unmerged(wt)
        if left:
            return "files still unmerged: %s" % ", ".join(left)
        merging = subprocess.run(["git", "rev-parse", "-q", "--verify", "MERGE_HEAD"], cwd=str(wt),
                                 capture_output=True).returncode == 0
        if merging:
            gitops.git(wt, "commit", "--quiet", "--no-verify", "-m", message)
        else:
            parents = gitops.git(wt, "rev-list", "--parents", "-n", "1", "HEAD").split()[1:]
            if parents != [base, upstream_sha]:
                return "the merge commit was replaced (HEAD parents %s); agents must not commit" % parents
            if subprocess.run(["git", "diff", "--cached", "--quiet"], cwd=str(wt)).returncode != 0:
                gitops.git(wt, "commit", "--quiet", "--no-verify", "--amend", "-m", message)
        head = gitops.rev(wt)
        hits = protected_hits(self.hand_merged_files(head, [base, upstream_sha]), self.cfg["protected_paths"])
        if hits:
            return "the resolution rewrites protected paths: %s" % ", ".join(hits)
        log_dir = self.o.runs_dir / ("%s-sync-verify" % time.strftime("%Y%m%d-%H%M%S"))
        log_dir.mkdir(parents=True, exist_ok=True)
        results = run_commands(self.cfg["verify"]["commands"], cwd=wt, env=command_env(self.cfg, self.o.tools_dir),
                               log_dir=log_dir, timeout_s=float(self.cfg["verify"]["timeout_s"]))
        failed = [c for c in results if c["returncode"] != 0]
        if failed:
            c = failed[0]
            return "verification failed: `%s` (%s)\n```\n%s\n```" % (
                c["command"], "timeout" if c["timed_out"] else "exit %s" % c["returncode"], c["tail"])
        return None

    def follow_up_task(self, pr: Dict[str, Any], findings: List[str]) -> str:
        """Problems the Gatekeeper accepted for now but that must be fixed: Astra plans them."""
        with self.db.tx():
            tid = self.db.add_task(
                milestone_id=(self.db.active_milestone() or {"id": "M?"})["id"],
                title="Follow-ups from the Gatekeeper review of PR #%d" % pr["number"],
                spec={"goal": "Fix the problems the Gatekeeper found when reviewing PR #%d (now in %s). Split into "
                              "smaller tasks if needed." % (pr["number"], self.target),
                      "acceptance_criteria": ["Each finding below is fixed with a regression test or explicitly "
                                              "rejected with evidence"] + findings[:15],
                      "constraints": [], "context": "Findings at %s:\n%s" % (pr["headRefOid"][:9], "\n".join(findings)),
                      "required_tests": ["full CI-equivalent verification"]},
                priority=5, depends_on=[], created_by="gatekeeper")
            self.db.event("task_created", task_id=tid, agent="gatekeeper", outcome=D.READY,
                          detail={"pr": pr["number"], "follow_ups": len(findings)})
        return tid

    # ------------------------------------------------------------------ actions on failure
    def report_problems(self, pr: Dict[str, Any], decision: str, reasons: List[str], ci_excerpts: str = "") -> str:
        memo = self.memo(pr)
        if memo.get("sha") == pr["headRefOid"] and memo.get("decision") == decision:
            return "PR #%d: %s already reported for this head" % (pr["number"], decision)
        body = "**Gatekeeper: %s** for `%s`\n\n%s" % (decision, pr["headRefOid"][:9],
                                                       "\n".join("- " + r for r in reasons))
        if ci_excerpts:
            body += ("\n\nError lines from the failed CI jobs (data; the CI runs on Linux, Windows and "
                     "macOS - reproduce locally where you can, and fix the cause, not the check):\n"
                     + truncate(ci_excerpts, 6000))
        if pr.get("headRefName") == self.integ:
            with self.db.tx():
                tid = self.db.add_task(
                    milestone_id=(self.db.active_milestone() or {"id": "M?"})["id"],
                    title="Fix integration PR #%d (%s)" % (pr["number"], decision.lower()),
                    spec={"goal": "Make the integration pull request #%d mergeable again." % pr["number"],
                          "acceptance_criteria": ["Every listed problem is resolved with evidence",
                                                  "CI is green on the new integration head"],
                          "constraints": [], "context": body, "required_tests": ["full CI-equivalent verification"]},
                    priority=0, depends_on=[], created_by="gatekeeper")
                self.db.event("task_created", task_id=tid, agent="gatekeeper", outcome=D.READY,
                              detail={"pr": pr["number"], "reasons": reasons[:10]})
            action = "fix task %s created" % tid
        else:
            self.port.comment(pr["number"], body + "\n\n🤖 GTA2.0 orchestrator (Gatekeeper)")
            action = "commented on the PR (the owner's branch is not changed by agents)"
        self.set_memo(pr, decision)
        return "PR #%d: %s, %s" % (pr["number"], decision, action)

    # ------------------------------------------------------------------ main entry
    def step(self, *, dry_run: bool = False, only_pr: Optional[int] = None) -> Tuple[bool, str]:
        """Returns (agent_was_called, message). Handles at most one PR per call."""
        if not self.o.wt_int.exists():
            self.o.setup()  # decision log lives in the integration worktree
        prs = self.port.open_prs()
        eligible, skipped = self.classify(prs)
        if only_pr is not None:
            eligible = [p for p in eligible if p["number"] == only_pr]
        with self.db.tx():
            self.db.set_meta("gatekeeper_last_check", str(now()))
            self.db.set_meta("gatekeeper_skipped", dumps(skipped))
        sync_state, sync_msg = "current", ""
        if not dry_run and any(p.get("headRefName") == self.integ for p in eligible):
            sync_state, sync_msg = self.sync_integration()
            if sync_state == "merged":
                return False, sync_msg
        for pr in eligible:
            if pr.get("headRefName") == self.integ and sync_state == "conflict":
                continue  # the developer agent could not resolve it; the owner was asked
            memo = self.memo(pr)
            if (memo.get("sha") == pr["headRefOid"] and memo.get("decision") != "HOLD" and not dry_run
                    and only_pr is None):
                continue  # this exact head was already decided
            g = self.gates(pr)
            if g["fails"]:
                return False, self.report_problems(pr, "GATES_FAILED", g["fails"], g.get("ci_excerpts", ""))
            if g["pending"]:
                self.set_memo(pr, "HOLD", {"reason": g["pending"]})
                with self.db.tx():
                    self.db.event("gatekeeper_hold", agent="gatekeeper", commits=[pr["headRefOid"]],
                                  outcome="PR #%d" % pr["number"], detail=g["pending"])
                continue
            return True, self.review(pr, g, dry_run)
        return False, ""

    def review(self, pr: Dict[str, Any], g: Dict[str, Any], dry_run: bool) -> str:
        sha = pr["headRefOid"]
        gitops.ensure_worktree(self.o.repo, self.wt, detach=sha)
        gitops.checkout_detached(self.wt, sha)
        checks = "\n".join("- %s: %s (%s)" % (c.get("name"), c.get("bucket"), c.get("workflow") or "")
                           for c in g["checks"]) or "(none)"
        prompt = prompts.gatekeeper_review(
            pr=pr, target=self.target, gates=g["summary"], checks=checks,
            stat=gitops.git(self.o.repo, "diff", "--stat", "%s...%s" % (g["base"], sha)),
            diff=truncate(g["diff"], int(self.rel["max_diff_chars"])),
            log=gitops.git(self.o.repo, "log", "--oneline", "-n", "60", "%s..%s" % (g["base"], sha)),
            tasks=g["tasks"], memory=memory.context(self.o.wt_int, log_entries=3, max_chars=4000),
            agent_pr=pr.get("headRefName") == self.integ)
        try:
            res = self.o.call(self.agent, kind="final_review", task_id=None, prompt=prompt, cwd=self.wt,
                              schema=schemas.GATEKEEPER_REVIEW)
        except (AgentFailed, AgentTimeout, ModelMismatch) as exc:
            self.o.count_failure(str(exc))
            return "PR #%d: gatekeeper review failed: %s" % (pr["number"], exc)
        out = res.output or {}
        decision = out.get("decision", "HOLD")
        findings = ["[%s] %s: %s" % (f["severity"], f["file"], f["issue"]) for f in out.get("findings", [])]
        with self.db.tx():
            self.db.add_decision(task_id=None, run_id=None, decision="GATEKEEPER_%s" % decision,
                                 rationale="PR #%d: %s" % (pr["number"], out.get("rationale", "")),
                                 commit_sha=sha, details={"pr": pr["number"], "review": out, "dry_run": dry_run})
            self.db.event("gatekeeper_decision", agent="gatekeeper", session_id=res.session_id, commits=[sha],
                          outcome="%s PR #%d%s" % (decision, pr["number"], " (dry run)" if dry_run else ""),
                          detail=out.get("rationale", "")[:2000])
        log_body = "PR #%d `%s` -> `%s` at %s%s\n\n%s\n\n%s" % (
            pr["number"], pr.get("headRefName"), self.target, sha[:9], " (dry run)" if dry_run else "",
            out.get("rationale", ""), "\n".join("- " + f for f in findings))
        if dry_run:
            self.set_memo(pr, "DRY_RUN_" + decision)
            return "PR #%d: Gatekeeper says %s (dry run, nothing merged)" % (pr["number"], decision)
        if decision == "MERGE":
            return self.merge(pr, out, log_body)
        if decision == "HOLD":
            self.set_memo(pr, "HOLD")
            return "PR #%d: HOLD — %s" % (pr["number"], out.get("rationale", "")[:200])
        if decision == "NEEDS_HUMAN":
            with self.db.tx():
                self.db.set_meta("human_question", "PR #%d: %s" % (pr["number"], out.get("human_question") or out.get("rationale")))
            self.o.commit_memory(None, "agent-memory: Gatekeeper on PR #%d" % pr["number"],
                                 [("decisions", "Gatekeeper NEEDS_HUMAN on PR #%d" % pr["number"], log_body)])
            self.port.comment(pr["number"], "**Gatekeeper: needs the owner's decision**\n\n%s\n\n🤖 GTA2.0 orchestrator"
                              % (out.get("human_question") or out.get("rationale")))
            self.set_memo(pr, "NEEDS_HUMAN")
            return "PR #%d: NEEDS_HUMAN" % pr["number"]
        self.o.commit_memory(None, "agent-memory: Gatekeeper on PR #%d" % pr["number"],
                             [("decisions", "Gatekeeper REQUEST_CHANGES on PR #%d" % pr["number"], log_body)])
        return self.report_problems(pr, "REQUEST_CHANGES", [out.get("rationale", "")] + findings)

    def merge(self, pr: Dict[str, Any], out: Dict[str, Any], log_body: str) -> str:
        sha = pr["headRefOid"]
        # Re-read: the head and CI must be exactly what was reviewed.
        fresh = next((p for p in self.port.open_prs() if p["number"] == pr["number"]), None)
        if not fresh or fresh["headRefOid"] != sha:
            self.set_memo(pr, "HOLD")
            return "PR #%d: head changed during review; will re-review" % pr["number"]
        g = self.gates(fresh)
        if g["fails"] or g["pending"]:
            self.set_memo(pr, "HOLD")
            return "PR #%d: gates changed during review: %s" % (pr["number"], "; ".join(g["fails"] + g["pending"]))
        if fresh.get("isDraft"):
            self.port.mark_ready(pr["number"])
        subject = (out.get("merge_subject") or "Merge #%d: %s" % (pr["number"], pr["title"]))[:200]
        ok, msg = self.port.merge(pr["number"], sha, self.rel["merge_method"], subject)
        if not ok:
            self.set_memo(pr, "MERGE_FAILED", {"error": msg[:500]})
            with self.db.tx():
                self.db.event("merge_failed", agent="gatekeeper", commits=[sha], outcome="PR #%d" % pr["number"],
                              detail=msg[:1000])
            return "PR #%d: merge failed: %s" % (pr["number"], msg[:200])
        retargeted = []
        for child in self.port.open_prs():
            if child.get("baseRefName") == pr.get("headRefName"):
                if self.port.retarget(child["number"], self.target):
                    retargeted.append(child["number"])
        self.set_memo(pr, "MERGED")
        follow = ["[%s] %s: %s" % (f["severity"], f["file"], f["issue"]) for f in out.get("findings", [])
                  if f.get("severity") in ("blocker", "major")]
        if follow:
            self.follow_up_task(pr, follow)
        with self.db.tx():
            self.db.set_meta("gatekeeper_last_check", "0")  # a merge may unblock the next PR in a stack
            self.db.event("pr_merged", agent="gatekeeper", commits=[sha], outcome="PR #%d" % pr["number"],
                          detail={"into": self.target, "retargeted": retargeted, "subject": subject})
        self.o.commit_memory(None, "agent-memory: Gatekeeper merged PR #%d" % pr["number"],
                             [("decisions", "Gatekeeper MERGED PR #%d" % pr["number"],
                               log_body + ("\n\nRetargeted to %s: %s" % (self.target, retargeted) if retargeted else ""))]
                             + ([("progress", "PR #%d merged into %s" % (pr["number"], self.target), out["changelog"])]
                                if out.get("changelog") else []))
        return "PR #%d: MERGED into %s%s" % (pr["number"], self.target,
                                             "; retargeted %s" % retargeted if retargeted else "")

    def status_lines(self) -> List[str]:
        eligible, skipped = self.classify(self.port.open_prs())
        lines = []
        for pr in eligible:
            g = self.gates(pr)
            memo = self.memo(pr)
            state = "FAIL: " + "; ".join(g["fails"]) if g["fails"] else (
                "WAIT: " + "; ".join(g["pending"]) if g["pending"] else "gates PASS")
            lines.append("#%d %s -> %s [%s] last=%s" % (pr["number"], pr.get("headRefName"), pr.get("baseRefName"),
                                                     state, memo.get("decision", "-")))
        for num, why in skipped:
            lines.append("#%d skipped: %s" % (num, why))
        return lines
