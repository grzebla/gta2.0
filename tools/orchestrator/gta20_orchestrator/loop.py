"""The supervision loop: Astra plans -> Claude implements -> orchestrator verifies -> Astra reviews.

Each step performs at most one agent call and ends with a committed transaction, so a crash,
a stop or a quota stop at any point resumes from the last durable checkpoint.
"""

from __future__ import annotations

import datetime as _dt
import logging
import os
import signal
import time
import uuid
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from . import github, gitops, memory, prompts, schemas, toolchain
from . import db as D
from .adapters.base import (AgentError, AgentFailed, AgentInterrupted, AgentResult, AgentTimeout, ModelMismatch,
                            QuotaExceeded)
from .adapters.claude import build_settings
from .util import dumps, iso, loads, local_time, now, truncate
from .verify import summarize, verify_task

log = logging.getLogger("gta20.orchestrator")

# Loop states (meta.loop_state); desired states (meta.desired_state) are RUNNING/PAUSED/STOPPED.
RUNNING = "RUNNING"
PAUSED = "PAUSED"
STOPPED = "STOPPED"
WAITING_FOR_QUOTA = "WAITING_FOR_QUOTA"
WAITING = "WAITING"
HALTED = "HALTED"


class Wait(Exception):
    def __init__(self, until: float, reason: str, state: str = WAITING):
        super().__init__(reason)
        self.until = until
        self.reason = reason
        self.state = state


class Halt(Exception):
    pass


class Orchestrator:
    def __init__(self, cfg: Any, database: D.Database, astra: Any, claude: Any, *, use_github: bool = True,
                 sleep: Callable[[float], None] = time.sleep):
        self.cfg = cfg
        self.db = database
        self.astra = astra
        self.claude = claude
        self.use_github = use_github
        self.sleep_fn = sleep
        self.stop_requested = False
        self.limits = cfg["limits"]
        self.repo = cfg.repo_root
        self.wt_dev = cfg.worktree("dev")
        self.wt_review = cfg.worktree("review")
        self.wt_int = cfg.worktree("integration")
        self.runs_dir = cfg.state_dir / "runs"
        self.tools_dir: Optional[Path] = None

    # ------------------------------------------------------------------ control helpers
    def request_stop(self, *_: Any) -> None:
        self.stop_requested = True

    def agent_should_stop(self) -> bool:
        return self.stop_requested or self.db.get_meta("desired_state") == STOPPED

    def set_loop_state(self, state: str, reason: str = "", next_at: Optional[float] = None) -> None:
        with self.db.tx():
            prev = self.db.get_meta("loop_state")
            self.db.set_meta("loop_state", state)
            self.db.set_meta("loop_reason", reason)
            self.db.set_meta("next_attempt_at", str(next_at) if next_at else "")
            self.db.set_meta("last_activity", iso())
            if prev != state:
                self.db.event("loop_state", outcome=state, detail=reason)

    # ------------------------------------------------------------------ setup / recovery
    def setup(self) -> None:
        base = self.cfg["base_branch"]
        integ = self.cfg["integration_branch"]
        if not gitops.ref_exists(self.repo, "refs/heads/" + integ):
            start = base if gitops.ref_exists(self.repo, "refs/heads/" + base) else "%s/%s" % (self.cfg["remote"], base)
            gitops.ensure_branch(self.repo, integ, start)
            with self.db.tx():
                self.db.event("integration_branch_created", outcome=integ, detail={"from": start})
        gitops.ensure_worktree(self.repo, self.wt_int, branch=integ)
        gitops.ensure_worktree(self.repo, self.wt_review, detach=integ)
        gitops.ensure_worktree(self.repo, self.wt_dev, detach=integ)
        self.runs_dir.mkdir(parents=True, exist_ok=True)
        if self.cfg["verify"].get("pinned_tools", True):
            self.tools_dir = toolchain.ensure(self.cfg.state_dir)
            with self.db.tx():
                self.db.set_meta("pinned_tools", dumps(toolchain.versions(self.tools_dir)))

    def recover(self) -> List[int]:
        """Runs left 'running' by a previous process (crash, kill, reboot) are abandoned; their
        task keeps its status, so the next step resumes it from the branch state."""
        abandoned = []
        with self.db.tx():
            for run in self.db.running_runs():
                self.db.finish_run(run["id"], status="abandoned", error="orchestrator restarted during run")
                self.db.event("run_abandoned", task_id=run["task_id"], agent=run["agent"],
                              session_id=run["session_id"], outcome="abandoned", detail={"run": run["id"]})
                abandoned.append(run["id"])
        return abandoned

    # ------------------------------------------------------------------ quota
    def gate(self, agent_obj: Any) -> None:
        name = agent_obj.name
        row = self.db.quota(name)
        if not row or row["state"] != "EXHAUSTED":
            return
        if row["reset_at"] and row["reset_at"] > now():
            raise Wait(float(row["reset_at"]), "%s usage limit; next attempt %s" % (name, local_time(row["reset_at"])),
                       WAITING_FOR_QUOTA)
        run_dir = self.runs_dir / ("probe-%s-%d" % (name, int(now())))
        try:
            info = agent_obj.probe(run_dir)
        except QuotaExceeded as exc:
            self.on_quota(name, exc, task_id=None)
        except AgentError as exc:
            # Probe failed for another reason (network...): retry later with the same backoff.
            self.on_quota(name, QuotaExceeded(str(exc)), task_id=None)
        with self.db.tx():
            self.db.set_quota(name, "OK", None, 0, info)
            self.db.event("quota_restored", agent=name, outcome="OK", detail=info)

    def on_quota(self, name: str, exc: QuotaExceeded, task_id: Optional[str]) -> None:
        row = self.db.quota(name)
        probes = (int(row["probes"]) if row else 0) + 1
        backoff = self.limits["quota_backoff_s"]
        if exc.reset_at:
            reset = float(exc.reset_at) + float(self.limits["quota_reset_margin_s"])
            source = "service"
        else:
            reset = now() + float(backoff[min(probes - 1, len(backoff) - 1)])
            source = "backoff"
        with self.db.tx():
            self.db.set_quota(name, "EXHAUSTED", reset, probes,
                              {"message": str(exc)[:500], "limit_type": exc.limit_type, "source": source,
                               "detail": exc.detail})
            self.db.event("quota_exhausted", task_id=task_id, agent=name, session_id=exc.session_id,
                          outcome=exc.limit_type or "unknown",
                          detail={"reset_at": iso(reset), "source": source, "probes": probes})
        if probes > int(self.limits["quota_max_probes"]):
            raise Halt("%s usage limit did not recover after %d attempts" % (name, probes - 1))
        raise Wait(reset, "%s usage limit (%s); retry at %s" % (name, exc.limit_type or "?", local_time(reset)),
                   WAITING_FOR_QUOTA)

    def note_rate_limits(self, name: str, res: AgentResult) -> None:
        if res.rate_limits:
            with self.db.tx():
                self.db.set_quota(name, "OK", None, 0, {"rate_limits": res.rate_limits, "checked_at": iso()})

    def daily_cap(self) -> None:
        midnight = _dt.datetime.combine(_dt.date.today(), _dt.time())
        start = midnight.timestamp()
        if self.db.runs_since(iso(start)) >= int(self.limits["max_runs_per_day"]):
            until = start + 86400 + 60
            raise Wait(until, "daily agent-run cap (%d) reached" % self.limits["max_runs_per_day"])

    # ------------------------------------------------------------------ agent call wrapper
    def call(self, agent_obj: Any, *, kind: str, task_id: Optional[str], prompt: str, cwd: Path,
             schema: Optional[Dict[str, Any]], session_id: Optional[str] = None, resume: bool = False,
             extra: Optional[Dict[str, Any]] = None) -> AgentResult:
        self.gate(agent_obj)
        run_dir = self.runs_dir / ("%s-%s-%s-%s" % (time.strftime("%Y%m%d-%H%M%S"), task_id or "project",
                                                   agent_obj.name, kind))
        with self.db.tx():
            run_id = self.db.start_run(task_id=task_id, agent=agent_obj.name, kind=kind, session_id=session_id,
                                       model=getattr(agent_obj, "cfg", {}).get("model"), run_dir=str(run_dir))
            self.db.event("agent_started", task_id=task_id, agent=agent_obj.name, session_id=session_id,
                          outcome=kind, detail={"run": run_id, "resume": resume})
            self.db.set_meta("active_agent", agent_obj.name)
            self.db.set_meta("last_activity", iso())
        try:
            res = agent_obj.run(kind=kind, prompt=prompt, cwd=cwd, run_dir=run_dir, schema=schema,
                                session_id=session_id, resume=resume, should_stop=self.agent_should_stop,
                                **(extra or {}))
        except QuotaExceeded as exc:
            with self.db.tx():
                self.db.finish_run(run_id, status="quota", session_id=exc.session_id, error=str(exc))
                self.db.set_meta("active_agent", "")
            self.on_quota(agent_obj.name, exc, task_id)
            raise  # unreachable: on_quota raises Wait/Halt
        except AgentInterrupted as exc:
            with self.db.tx():
                self.db.finish_run(run_id, status="interrupted", session_id=exc.session_id, error=str(exc))
                self.db.event("agent_interrupted", task_id=task_id, agent=agent_obj.name, session_id=exc.session_id)
                self.db.set_meta("active_agent", "")
            raise
        except AgentError as exc:
            status = "timeout" if isinstance(exc, AgentTimeout) else (
                "model_mismatch" if isinstance(exc, ModelMismatch) else "failed")
            with self.db.tx():
                self.db.finish_run(run_id, status=status, session_id=exc.session_id, error=str(exc))
                self.db.event("agent_failed", task_id=task_id, agent=agent_obj.name, session_id=exc.session_id,
                              outcome=status, detail=str(exc)[:1500])
                self.db.set_meta("active_agent", "")
            raise
        with self.db.tx():
            self.db.finish_run(run_id, status="succeeded", session_id=res.session_id, model=res.model,
                               result=res.output, usage=res.usage)
            self.db.event("agent_finished", task_id=task_id, agent=agent_obj.name, session_id=res.session_id,
                          outcome="succeeded", detail={"run": run_id, "model": res.model})
            self.db.set_meta("active_agent", "")
            self.db.set_meta("consecutive_failures", "0")
        self.note_rate_limits(agent_obj.name, res)
        return res

    def count_failure(self, reason: str) -> None:
        with self.db.tx():
            n = int(self.db.get_meta("consecutive_failures", "0") or 0) + 1
            self.db.set_meta("consecutive_failures", str(n))
        if n >= int(self.limits["max_consecutive_failures"]):
            raise Halt("%d consecutive agent failures, last: %s" % (n, reason))

    # ------------------------------------------------------------------ context builders
    def board(self) -> str:
        rows = self.db.tasks()
        if not rows:
            return "(no tasks yet)"
        lines = []
        for t in rows:
            line = "- %s [%s] p%s %s (attempts %s)" % (t["id"], t["status"], t["priority"], t["title"], t["attempts"])
            deps = loads(t["depends_on"], [])
            if deps:
                line += " depends_on=%s" % ",".join(deps)
            if t["blocked_reason"]:
                line += " | reason: %s" % truncate(t["blocked_reason"], 300)
            lines.append(line)
        return "\n".join(lines[-80:])

    def decisions_text(self, limit: int = 10, task_id: Optional[str] = None) -> str:
        out = []
        for d in self.db.decisions(limit, task_id):
            out.append("- %s %s %s: %s" % (d["created_at"], d["task_id"] or "project", d["decision"],
                                           truncate(d["rationale"] or "", 600)))
        return "\n".join(out) or "(none)"

    def quota_text(self) -> str:
        out = []
        for name in ("astra", "claude"):
            row = self.db.quota(name)
            if row:
                out.append("- %s: %s reset=%s" % (name, row["state"], local_time(row["reset_at"]) if row["reset_at"] else "-"))
        return "\n".join(out) or "(no data)"

    def branch_state(self, task: Any) -> str:
        base = task["base_commit"]
        commits = gitops.log_oneline(self.wt_dev, base, "HEAD") or "(no commits yet)"
        status = gitops.status_short(self.wt_dev) or "(clean)"
        return "Commits on branch since base:\n%s\n\ngit status --short:\n%s" % (commits, status)

    # ------------------------------------------------------------------ memory commits
    def commit_memory(self, updates: Optional[Dict[str, Any]], message: str,
                      logs: Optional[List[tuple]] = None) -> Optional[str]:
        changed = memory.apply_updates(self.wt_int, updates)
        for log_name, heading, body in logs or []:
            changed.append(memory.append_log(self.wt_int, log_name, heading, body))
        if not changed:
            return None
        sha = gitops.commit_paths(self.wt_int, sorted(set(changed)), message)
        with self.db.tx():
            self.db.event("memory_updated", agent="orchestrator", commits=[sha] if sha else None,
                          outcome="committed", detail={"files": sorted(set(changed))})
        return sha

    # ------------------------------------------------------------------ step
    def step(self) -> str:
        self.daily_cap()
        task = self.db.active_task()
        if task is None:
            done = self.db.count_tasks(D.DONE)
            last = int(self.db.get_meta("arch_review_at_done", "0") or 0)
            every = int(self.limits["arch_review_every"])
            if every and done - last >= every:
                return self.plan("arch_review")
            ready = self.db.ready_tasks()
            if not ready:
                return self.plan("plan")
            task = self.start_task(ready[0])
        if task["status"] in (D.IN_PROGRESS, D.CHANGES_REQUESTED):
            return self.implement(task)
        if task["status"] == D.VERIFYING:
            return self.verify(task)
        if task["status"] == D.IN_REVIEW:
            return self.review(task)
        raise RuntimeError("unexpected task status %s" % task["status"])

    # ------------------------------------------------------------------ plan
    def plan(self, kind: str) -> str:
        integ_head = gitops.rev(self.wt_int)
        gitops.checkout_detached(self.wt_review, integ_head)
        milestone = self.db.active_milestone()
        ms = None
        if milestone:
            ms = {"id": milestone["id"], "title": milestone["title"], "goal": milestone["goal"],
                  "acceptance_criteria": loads(milestone["acceptance"], []), "status": milestone["status"]}
        gh_state = "(GitHub not queried)"
        ci_fail = 0
        if self.use_github:
            gh_state = github.summarize(self.cfg["github_repo"], self.cfg["integration_branch"])
            for r in github.ci_runs(self.cfg["github_repo"], self.cfg["integration_branch"], 10):
                if r.get("status") != "completed":
                    continue
                if r.get("conclusion") == "failure":
                    ci_fail += 1
                else:
                    break
            with self.db.tx():
                self.db.set_meta("ci_consecutive_failures", str(ci_fail))
            if ci_fail >= int(self.limits["max_ci_failures"]):
                raise Halt("integration CI failed %d times in a row" % ci_fail)
        git_state = "HEAD %s\n%s" % (integ_head, gitops.git(self.wt_int, "log", "--oneline", "-n", "15"))
        prompt = prompts.astra_plan(
            kind=kind, memory=memory.context(self.wt_int), board=self.board(), decisions=self.decisions_text(),
            git_state=git_state, github_state=gh_state, quota_state=self.quota_text(), milestone=ms,
            max_tasks=int(self.limits["max_tasks_per_plan"]))
        try:
            res = self.call(self.astra, kind=kind, task_id=None, prompt=prompt, cwd=self.wt_review,
                            schema=schemas.ASTRA_PLAN)
        except (AgentFailed, AgentTimeout, ModelMismatch) as exc:
            self.count_failure(str(exc))
            return "plan failed: %s" % exc
        out = res.output or {}
        added: List[str] = []
        with self.db.tx():
            m = out.get("milestone")
            if m:
                self.db.upsert_milestone(m["id"], m["title"], m["goal"], m["acceptance_criteria"], m["status"])
                if m["status"] == "active":
                    self.db.set_meta("current_milestone", m["id"])
            mid = (m or {}).get("id") or (milestone["id"] if milestone else "M?")
            for tid in out.get("cancel_tasks") or []:
                t = self.db.task(tid)
                if t and t["status"] == D.READY:
                    self.db.update_task(tid, status=D.CANCELLED, blocked_reason="cancelled by Astra")
                    self.db.event("task_cancelled", task_id=tid, agent="astra", outcome=D.CANCELLED)
            key_to_id: Dict[str, str] = {}
            pending = list(out.get("tasks") or [])[: int(self.limits["max_tasks_per_plan"])]
            for t in pending:
                spec = {k: t[k] for k in ("goal", "acceptance_criteria", "constraints", "context", "required_tests")}
                tid = self.db.add_task(milestone_id=mid, title=t["title"], spec=spec, priority=int(t["priority"]),
                                       depends_on=[], created_by="astra")
                key_to_id[t["key"]] = tid
                added.append(tid)
            for t in pending:
                deps = []
                for d in t.get("depends_on") or []:
                    if d in key_to_id:
                        deps.append(key_to_id[d])
                    elif self.db.task(d):
                        deps.append(d)
                self.db.update_task(key_to_id[t["key"]], depends_on=dumps(deps))
                self.db.event("task_created", task_id=key_to_id[t["key"]], agent="astra", outcome=D.READY,
                              detail={"title": t["title"], "depends_on": deps})
            self.db.add_decision(task_id=None, run_id=None, decision=kind.upper(), rationale=out.get("assessment", ""),
                                 commit_sha=integ_head, details={"added": added, "cancelled": out.get("cancel_tasks"),
                                                                 "needs_human": out.get("needs_human")})
            if kind == "arch_review":
                self.db.set_meta("arch_review_at_done", str(self.db.count_tasks(D.DONE)))
            if out.get("release_candidate"):
                self.db.set_meta("release_candidate", iso())
            if out.get("needs_human"):
                self.db.set_meta("human_question", out["needs_human"])
        body = "%s\n\nNew tasks: %s" % (out.get("assessment", ""), ", ".join(added) or "none")
        if out.get("needs_human"):
            body += "\n\nNeeds human: %s" % out["needs_human"]
        self.commit_memory(out.get("memory_updates"), "agent-memory: Astra %s" % kind,
                           [("decisions", "Astra %s" % kind, body)])
        if not self.db.ready_tasks() and not self.db.active_task():
            blocked = self.db.count_tasks(D.BLOCKED) + self.db.count_tasks(D.NEEDS_HUMAN)
            raise Halt("no runnable tasks after %s (%d blocked/needs-human)%s" % (
                kind, blocked, "; Astra asks: " + out["needs_human"] if out.get("needs_human") else ""))
        return "%s: added %s" % (kind, ", ".join(added) or "nothing")

    # ------------------------------------------------------------------ task start
    def start_task(self, task: Any) -> Any:
        integ = self.cfg["integration_branch"]
        base = gitops.rev(self.wt_int)
        branch = "agent/%s" % task["id"].lower()
        gitops.switch_task_branch(self.wt_dev, branch, base, "orchestrator: leftovers before %s" % task["id"])
        with self.db.tx():
            self.db.update_task(task["id"], status=D.IN_PROGRESS, branch=branch, base_commit=base,
                                claude_session=str(uuid.uuid4()))
            self.db.set_meta("current_task", task["id"])
            self.db.event("task_started", task_id=task["id"], agent="orchestrator", commits=[base],
                          outcome=D.IN_PROGRESS, detail={"branch": branch, "integration": integ})
        return self.db.task(task["id"])

    def claude_env(self) -> Dict[str, str]:
        env = dict(self.cfg.verify_env())
        if self.tools_dir:
            env["GTA20_TOOLS_PATH"] = str(self.tools_dir)
        return env

    # ------------------------------------------------------------------ implement
    def implement(self, task: Any) -> str:
        tid = task["id"]
        gitops.switch_task_branch(self.wt_dev, task["branch"], task["base_commit"], "orchestrator: before " + tid)
        spec = loads(task["spec"], {})
        last = self.db.last_run(tid, "claude", "implement")
        session = task["claude_session"] or str(uuid.uuid4())
        # Resume only the session that belongs to the current task session id (a reset session
        # must start fresh with --session-id).
        resume = bool(last and last["session_id"] and last["session_id"] == session)
        if resume and last["status"] in ("quota", "interrupted", "abandoned", "timeout", "failed"):
            prompt = prompts.claude_continue(task=dict(task), branch_state=self.branch_state(task),
                                             reason="%s (%s)" % (last["status"], truncate(last["error"] or "", 300)))
            if task["feedback"] and task["status"] == D.CHANGES_REQUESTED:
                prompt += "\n\nOutstanding feedback to address:\n" + task["feedback"]
        else:
            prompt = prompts.claude_task(
                task=dict(task), spec=spec, branch=task["branch"], base=task["base_commit"],
                branch_state=self.branch_state(task),
                feedback=task["feedback"] if task["status"] == D.CHANGES_REQUESTED else None,
                game_dir_hint="available via the GTA20_GAME_DIR environment variable (read-only use)")
        settings = build_settings(
            worktree=self.wt_dev, protected=self.cfg["protected_paths"],
            extra_write_dirs=[os.path.expanduser("~/.cache/uv"), os.path.expanduser("~/.cache/clang")],
            sandbox=bool(self.cfg["claude"].get("sandbox", True)), domains=self.cfg["claude"]["sandbox_domains"],
            allow_web=bool(self.cfg["claude"].get("allow_web", False)))
        try:
            res = self.call(self.claude, kind="implement", task_id=tid, prompt=prompt, cwd=self.wt_dev,
                            schema=schemas.CLAUDE_REPORT, session_id=session, resume=resume,
                            extra={"settings": settings, "add_dirs": [], "env_extra": self.claude_env()})
        except AgentInterrupted:
            return "implement %s interrupted" % tid
        except (AgentFailed, AgentTimeout, ModelMismatch) as exc:
            if resume and isinstance(exc, AgentFailed) and "conversation" in str(exc).lower():
                # Session could not be restored: start a fresh one from durable state.
                with self.db.tx():
                    self.db.update_task(tid, claude_session=str(uuid.uuid4()))
                    self.db.event("session_reset", task_id=tid, agent="claude", detail=str(exc)[:300])
            with self.db.tx():
                n = int(task["agent_failures"]) + 1
                fields: Dict[str, Any] = {"agent_failures": n}
                if n >= int(self.limits["max_agent_failures_per_task"]):
                    fields.update(status=D.BLOCKED, blocked_reason="developer agent failed %d times: %s" % (n, exc))
                    self.db.event("task_blocked", task_id=tid, agent="orchestrator", outcome=D.BLOCKED,
                                  detail=fields["blocked_reason"][:500])
                self.db.update_task(tid, **fields)
            self.count_failure(str(exc))
            return "implement %s failed: %s" % (tid, exc)
        head = gitops.rev(self.wt_dev)
        with self.db.tx():
            self.db.update_task(tid, status=D.VERIFYING, head_commit=head, report=dumps(res.output),
                                claude_session=res.session_id or session, agent_failures=0)
            self.db.event("task_reported", task_id=tid, agent="claude", session_id=res.session_id, commits=[head],
                          outcome=(res.output or {}).get("status"), detail=(res.output or {}).get("summary"))
        return "implement %s -> %s" % (tid, head[:9])

    # ------------------------------------------------------------------ verify
    def verify(self, task: Any) -> str:
        tid = task["id"]
        head = gitops.rev(self.wt_dev)
        report = loads(task["report"], {}) or {}
        blocked_report = report.get("status") == "blocked"
        log_dir = self.runs_dir / ("%s-%s-verify" % (time.strftime("%Y%m%d-%H%M%S"), tid))
        result = verify_task(self.cfg, wt=self.wt_dev, base=task["base_commit"], head=head,
                             previous_hash=task["last_diff_hash"], log_dir=log_dir,
                             run_commands_too=not blocked_report, tools_dir=self.tools_dir)
        summary = summarize(result)
        (log_dir / "summary.md").write_text(summary)
        no_progress = int(task["no_progress"]) + 1 if (result["repeated"] or not result["commits"]) else 0
        with self.db.tx():
            self.db.add_verification(tid, head, result["passed"], {"checks": result["checks"], "summary": summary})
            self.db.event("verification", task_id=tid, agent="orchestrator", commits=[head],
                          outcome="PASS" if result["passed"] else "FAIL", detail=summary[:3000])
            fields: Dict[str, Any] = {"head_commit": head, "last_diff_hash": result["diff_hash"] or task["last_diff_hash"],
                                      "no_progress": no_progress}
            if no_progress >= int(self.limits["max_no_progress"]):
                fields.update(status=D.BLOCKED, blocked_reason="no measurable progress in %d attempts" % no_progress)
                self.db.event("task_blocked", task_id=tid, agent="orchestrator", outcome=D.BLOCKED,
                              detail=fields["blocked_reason"])
            elif result["repeated"] and int(task["attempts"]) > 0 and not blocked_report:
                # Nothing changed since the last review: do not spend a review on it.
                fields["status"] = D.CHANGES_REQUESTED
                fields["feedback"] = ("The orchestrator found NO CHANGE since the last review (identical diff). "
                                      "Address the requested changes below.\n\n" + (task["feedback"] or ""))
            elif result["passed"] or blocked_report:
                fields["status"] = D.IN_REVIEW
            else:
                attempts = int(task["attempts"]) + 1
                fields["attempts"] = attempts
                fields["feedback"] = "The orchestrator's independent verification FAILED:\n" + summary
                if attempts >= int(self.limits["max_attempts_per_task"]):
                    fields.update(status=D.BLOCKED, blocked_reason="verification failed %d times" % attempts)
                    self.db.event("task_blocked", task_id=tid, agent="orchestrator", outcome=D.BLOCKED,
                                  detail=fields["blocked_reason"])
                else:
                    fields["status"] = D.CHANGES_REQUESTED
            self.db.update_task(tid, **fields)
        return "verify %s: %s" % (tid, "PASS" if result["passed"] else "FAIL")

    # ------------------------------------------------------------------ review
    def review(self, task: Any) -> str:
        tid = task["id"]
        head = task["head_commit"]
        gitops.checkout_detached(self.wt_review, head)
        ver = self.db.verification(tid, head)
        ver_summary = loads(ver["checks"], {}).get("summary", "") if ver else "(no verification recorded)"
        base = task["base_commit"]
        prompt = prompts.astra_review(
            task=dict(task), spec=loads(task["spec"], {}), report=loads(task["report"], {}),
            verification=ver_summary, stat=gitops.diff_stat(self.wt_review, base, head),
            diff=truncate(gitops.diff_text(self.wt_review, base, head), 60000),
            log=gitops.log_oneline(self.wt_review, base, head), history=self.decisions_text(5, tid),
            attempts=int(task["attempts"]), max_attempts=int(self.limits["max_attempts_per_task"]),
            memory=memory.context(self.wt_int, log_entries=3, max_chars=6000), commit=head)
        try:
            res = self.call(self.astra, kind="review", task_id=tid, prompt=prompt, cwd=self.wt_review,
                            schema=schemas.ASTRA_REVIEW)
        except (AgentFailed, AgentTimeout, ModelMismatch) as exc:
            self.count_failure(str(exc))
            return "review %s failed: %s" % (tid, exc)
        out = res.output or {}
        decision = out.get("decision")
        rationale = out.get("rationale", "")
        if decision == "APPROVED" and not (ver and ver["passed"]):
            decision = "CHANGES_REQUESTED"
            rationale = "[orchestrator] approval rejected: commit %s did not pass verification. %s" % (head[:9], rationale)
        with self.db.tx():
            run = self.db.runs(tid, 1)
            self.db.add_decision(task_id=tid, run_id=run[0]["id"] if run else None, decision=decision,
                                 rationale=rationale, commit_sha=head, details=out)
            self.db.event("decision", task_id=tid, agent="astra", session_id=res.session_id, commits=[head],
                          outcome=decision, detail=rationale[:2000])
        criteria = "\n".join("- [%s] %s — %s" % ("x" if c["met"] else " ", c["criterion"], c["evidence"])
                             for c in out.get("criteria", []))
        decision_body = "Task %s \"%s\" at %s\n\n%s\n\n%s" % (tid, task["title"], head[:9], rationale, criteria)
        if decision == "APPROVED":
            gitops.merge_into(self.wt_int, task["branch"], "Merge %s: %s" % (tid, task["title"]))
            logs = [("decisions", "%s APPROVED" % tid, decision_body)]
            if out.get("progress_entry"):
                logs.append(("progress", "%s %s" % (tid, task["title"]), out["progress_entry"]))
            mem_sha = self.commit_memory(out.get("memory_updates"), "agent-memory: %s approved" % tid, logs)
            with self.db.tx():
                self.db.update_task(tid, status=D.DONE, feedback=None)
                self.db.event("task_done", task_id=tid, agent="orchestrator",
                              commits=[c for c in (head, mem_sha) if c], outcome=D.DONE)
            self.maybe_push()
            return "review %s: APPROVED" % tid
        self.commit_memory(out.get("memory_updates"), "agent-memory: %s %s" % (tid, decision.lower()),
                           [("decisions", "%s %s" % (tid, decision), decision_body)])
        with self.db.tx():
            if decision == "CHANGES_REQUESTED":
                attempts = int(task["attempts"]) + 1
                fb = "Astra requested changes (review of %s):\n%s\n\nRequired changes:\n%s" % (
                    head[:9], rationale, "\n".join("- " + c for c in out.get("required_changes", [])))
                if attempts >= int(self.limits["max_attempts_per_task"]):
                    self.db.update_task(tid, status=D.BLOCKED, attempts=attempts, feedback=fb,
                                        blocked_reason="changes requested %d times; last: %s" % (attempts, rationale[:300]))
                    self.db.event("task_blocked", task_id=tid, agent="orchestrator", outcome=D.BLOCKED)
                else:
                    self.db.update_task(tid, status=D.CHANGES_REQUESTED, attempts=attempts, feedback=fb)
            elif decision == "BLOCKED":
                self.db.update_task(tid, status=D.BLOCKED, blocked_reason=rationale[:1000])
            else:
                self.db.update_task(tid, status=D.NEEDS_HUMAN, blocked_reason=out.get("human_question") or rationale)
                self.db.set_meta("human_question", "%s: %s" % (tid, out.get("human_question") or rationale))
        return "review %s: %s" % (tid, decision)

    # ------------------------------------------------------------------ push
    def maybe_push(self, force_now: bool = False) -> bool:
        if not self.cfg["push_integration"]:
            return False
        last = float(self.db.get_meta("last_push_at", "0") or 0)
        if not force_now and now() - last < float(self.cfg["min_push_interval_s"]):
            return False
        integ = self.cfg["integration_branch"]
        try:
            gitops.push(self.repo, self.cfg["remote"], integ)
        except gitops.GitError as exc:
            with self.db.tx():
                self.db.event("push_failed", agent="orchestrator", outcome="error", detail=str(exc)[:500])
            return False
        pr_url = None
        if self.use_github and self.cfg["open_draft_pr"]:
            pr = github.find_pr(self.cfg["github_repo"], integ)
            if pr:
                pr_url = pr.get("url")
            else:
                pr_url = github.create_draft_pr(
                    self.cfg["github_repo"], integ, self.cfg["base_branch"],
                    "Agents: integration branch (Astra-approved tasks)",
                    "Work approved by Astra and integrated by the GTA2.0 orchestrator. Draft: the human "
                    "owner reviews and merges.\n\n🤖 Generated with [Claude Code](https://claude.com/claude-code)")
        with self.db.tx():
            self.db.set_meta("last_push_at", str(now()))
            self.db.event("pushed", agent="orchestrator", commits=[gitops.rev(self.wt_int)], outcome=integ,
                          detail={"pr": pr_url})
        return True

    # ------------------------------------------------------------------ main loop
    def sleep_until(self, until: float) -> None:
        poll = float(self.limits["poll_s"])
        while not self.stop_requested and now() < until:
            if self.db.get_meta("desired_state") in (PAUSED, STOPPED):
                return
            self.sleep_fn(min(poll, max(0.0, until - now())))

    def run(self, max_steps: Optional[int] = None, on_step: Callable[[], None] = lambda: None) -> str:
        self.setup()
        self.recover()
        steps = 0
        while not self.stop_requested:
            desired = self.db.get_meta("desired_state", RUNNING)
            if desired == STOPPED:
                break
            if desired == PAUSED:
                if self.db.get_meta("loop_state") != PAUSED:
                    self.set_loop_state(PAUSED, "paused by the owner")
                    on_step()
                self.sleep_fn(float(self.limits["poll_s"]))
                continue
            if max_steps is not None and steps >= max_steps:
                break
            try:
                self.set_loop_state(RUNNING)
                outcome = self.step()
                steps += 1
                log.info("step: %s", outcome)
                with self.db.tx():
                    self.db.set_meta("last_step", outcome)
            except Wait as w:
                self.set_loop_state(w.state, w.reason, w.until)
                on_step()
                log.info("waiting until %s: %s", local_time(w.until), w.reason)
                self.sleep_until(w.until)
                continue
            except Halt as h:
                self.set_loop_state(HALTED, str(h))
                log.warning("halted: %s", h)
                on_step()
                return HALTED
            except AgentInterrupted:
                break
            except Exception as exc:  # noqa: BLE001 - keep the daemon alive, but bounded
                log.exception("step crashed")
                with self.db.tx():
                    self.db.event("step_error", agent="orchestrator", outcome=type(exc).__name__, detail=str(exc)[:2000])
                try:
                    self.count_failure("%s: %s" % (type(exc).__name__, exc))
                except Halt as h:
                    self.set_loop_state(HALTED, str(h))
                    on_step()
                    return HALTED
                self.sleep_fn(min(300.0, float(self.limits["poll_s"]) * 2))
            on_step()
        by_owner = self.stop_requested or self.db.get_meta("desired_state") == STOPPED
        self.set_loop_state(STOPPED, "stopped by the owner" if by_owner else "step limit reached")
        on_step()
        return STOPPED


def install_signal_handlers(orch: Orchestrator) -> None:
    signal.signal(signal.SIGTERM, orch.request_stop)
    signal.signal(signal.SIGINT, orch.request_stop)
    signal.signal(signal.SIGHUP, orch.request_stop)
