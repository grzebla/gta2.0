"""Unit, simulation (stage B) and failure/recovery (stage D) tests. Stdlib only:

    python3 -m unittest discover -s tools/orchestrator/tests -v
"""

from __future__ import annotations

import json
import os
import signal
import sqlite3
import subprocess
import sys
import time
import unittest
from pathlib import Path

os.environ["GTA20_NO_LAUNCHD"] = "1"  # tests must never touch the real LaunchAgent
HERE = Path(__file__).resolve().parent
PKG_ROOT = HERE.parent
REPO_ROOT = PKG_ROOT.parent.parent
sys.path.insert(0, str(PKG_ROOT))

from gta20_orchestrator import db as D  # noqa: E402
from gta20_orchestrator import memory, schemas, sim  # noqa: E402
from gta20_orchestrator.adapters.claude import build_settings, parse_stream, quota_from_stream  # noqa: E402
from gta20_orchestrator.adapters.codex import parse_jsonl, read_rollout, reset_from_rate_limits  # noqa: E402
from gta20_orchestrator.adapters.fake import FakeAstra, FakeClaude, FakeGfxRunner, fake_plan  # noqa: E402
from gta20_orchestrator.cli import Lock, main as cli_main  # noqa: E402
from gta20_orchestrator.gfx import ScenarioError, validate_scenario  # noqa: E402
from gta20_orchestrator.loop import Wait  # noqa: E402
from gta20_orchestrator.gitops import GitError, git  # noqa: E402
from gta20_orchestrator.util import find_secrets, redact  # noqa: E402
from gta20_orchestrator.verify import protected_hits  # noqa: E402


class SimCase(unittest.TestCase):
    def setUp(self) -> None:
        self.env = sim.SimEnv()

    def tearDown(self) -> None:
        self.env.db.close()
        self.env.cleanup()


# ------------------------------------------------------------------------------- parsing
class ParsingTests(unittest.TestCase):
    def test_claude_rate_limit_rejected_is_quota(self) -> None:
        lines = [
            json.dumps({"type": "system", "subtype": "init", "model": "claude-opus-5-5", "session_id": "s1"}),
            json.dumps({"type": "rate_limit_event", "rate_limit_info": {
                "status": "rejected", "resetsAt": 1791843000, "rateLimitType": "five_hour",
                "unifiedWindows": {"five_hour": {"utilization": 1.0, "resetsAt": 1791843000}}}}),
            json.dumps({"type": "result", "subtype": "error_during_execution", "is_error": True,
                        "result": "You've hit your session limit · resets 3:45pm", "session_id": "s1"}),
        ]
        q = quota_from_stream(parse_stream(lines))
        self.assertIsNotNone(q)
        self.assertEqual(q.reset_at, 1791843000.0)
        self.assertEqual(q.limit_type, "five_hour")

    def test_claude_weekly_text_only(self) -> None:
        lines = [json.dumps({"type": "result", "is_error": True, "result": "You've hit your weekly limit · resets Mon",
                             "session_id": "s"})]
        q = quota_from_stream(parse_stream(lines))
        self.assertIsNotNone(q)
        self.assertIsNone(q.reset_at)

    def test_claude_allowed_is_not_quota(self) -> None:
        lines = [json.dumps({"type": "rate_limit_event", "rate_limit_info": {"status": "allowed", "resetsAt": 1,
                                                                              "rateLimitType": "five_hour"}}),
                 json.dumps({"type": "result", "subtype": "success", "is_error": False, "result": "ok"})]
        self.assertIsNone(quota_from_stream(parse_stream(lines)))

    def test_server_throttle_is_not_quota(self) -> None:
        lines = [json.dumps({"type": "result", "is_error": True,
                             "result": "Server is temporarily limiting requests (not your usage limit)"})]
        self.assertIsNone(quota_from_stream(parse_stream(lines)))

    def test_codex_jsonl_and_rollout(self) -> None:
        info = parse_jsonl([
            '{"type":"thread.started","thread_id":"abc"}',
            '{"type":"item.completed","item":{"id":"i","type":"agent_message","text":"{}"}}',
            '{"type":"turn.completed","usage":{"input_tokens":1}}',
        ])
        self.assertEqual(info["thread_id"], "abc")
        tmp = Path(self.id().replace(".", "_") + ".jsonl")
        try:
            tmp.write_text("\n".join([
                json.dumps({"type": "turn_context", "payload": {"model": "gpt-6-astra"}}),
                json.dumps({"type": "event_msg", "payload": {"type": "token_count", "rate_limits": {
                    "primary": {"used_percent": 100.0, "window_minutes": 10080, "resets_at": 1792303409}}}}),
            ]))
            ro = read_rollout(tmp)
        finally:
            tmp.unlink()
        self.assertEqual(ro["models"], ["gpt-6-astra"])
        self.assertEqual(reset_from_rate_limits(ro["rate_limits"]), 1792303409.0)

    def test_schemas_are_strict(self) -> None:
        def walk(s):
            if isinstance(s, dict):
                if s.get("type") == "object":
                    self.assertFalse(s.get("additionalProperties", True))
                    self.assertEqual(sorted(s["required"]), sorted(s["properties"]))
                for v in s.values():
                    walk(v)
            elif isinstance(s, list):
                for v in s:
                    walk(v)
        for sch in (schemas.ASTRA_PLAN, schemas.ASTRA_REVIEW, schemas.CLAUDE_REPORT):
            walk(sch)


class SafetyTests(unittest.TestCase):
    def test_settings_least_privilege(self) -> None:
        s = build_settings(worktree=Path("/w"), protected=["tools/orchestrator/", "CLAUDE.md"], extra_write_dirs=[],
                           sandbox=True, domains=["github.com"], allow_web=False)
        deny = s["permissions"]["deny"]
        self.assertIn("Edit(//w/tools/orchestrator/**)", deny)
        self.assertIn("Write(//w/CLAUDE.md*)", deny)
        for rule in ("Bash(git push *)", "Bash(aws *)", "Bash(gh *)", "Bash(sudo *)", "WebFetch"):
            self.assertIn(rule, deny)
        self.assertTrue(s["sandbox"]["enabled"])
        self.assertFalse(s["sandbox"]["allowUnsandboxedCommands"])
        self.assertNotIn("bypassPermissions", json.dumps(s))

    def test_protected_hits(self) -> None:
        prot = ["tools/orchestrator/", ".github/workflows/gfx-", "CLAUDE.md"]
        self.assertEqual(protected_hits(["engine/a.cpp", "tools/orchestrator/x.py", ".github/workflows/gfx-e2e.yml",
                                         ".github/workflows/native-ci.yml", "CLAUDE.md"], prot),
                         ["tools/orchestrator/x.py", ".github/workflows/gfx-e2e.yml", "CLAUDE.md"])

    def test_redaction(self) -> None:
        text = "key AKIAABCDEFGHIJKLMNOP and ghp_" + "a" * 36
        self.assertNotIn("AKIA", redact(text))
        self.assertEqual(len(find_secrets(text)), 2)

    def test_force_push_refused(self) -> None:
        with self.assertRaises(GitError):
            git(Path("."), "push", "--force", "origin", "x")
        with self.assertRaises(GitError):
            git(Path("."), "push", "origin", "+x:x")

    def test_memory_log_tail(self) -> None:
        text = "# D\n" + "".join("\n## e%d\nbody\n" % i for i in range(20))
        tail = memory.log_tail(text, 3)
        self.assertIn("e19", tail)
        self.assertNotIn("e10\n", tail)


class DatabaseTests(unittest.TestCase):
    def test_events_append_only_and_jsonl(self) -> None:
        env = sim.SimEnv()
        try:
            with env.db.tx():
                env.db.event("x", task_id="T1", detail={"token": "secret=abcdefghijklmnopqrstuvwxyz"})
            with self.assertRaises(sqlite3.DatabaseError):
                env.db.conn.execute("UPDATE events SET type='y'")
            with self.assertRaises(sqlite3.DatabaseError):
                env.db.conn.execute("DELETE FROM events")
            lines = env.db.events_path.read_text().splitlines()
            self.assertEqual(json.loads(lines[-1])["type"], "x")
            self.assertIn("[REDACTED]", lines[-1])
        finally:
            env.db.close()
            env.cleanup()

    def test_transaction_rolls_back(self) -> None:
        env = sim.SimEnv()
        try:
            with self.assertRaises(RuntimeError):
                with env.db.tx():
                    env.db.set_meta("k", "v")
                    raise RuntimeError("boom")
            self.assertIsNone(env.db.get_meta("k"))
        finally:
            env.db.close()
            env.cleanup()

    def test_single_instance_lock(self) -> None:
        env = sim.SimEnv()
        try:
            a, b = Lock(env.cfg.state_dir), Lock(env.cfg.state_dir)
            self.assertTrue(a.acquire())
            out = subprocess.run([sys.executable, "-c",
                                  "import sys; sys.path.insert(0, %r)\nfrom gta20_orchestrator.cli import Lock\n"
                                  "from pathlib import Path\nprint(Lock(Path(%r)).acquire())" % (str(PKG_ROOT), str(env.cfg.state_dir))],
                                 capture_output=True, text=True)
            self.assertEqual(out.stdout.strip(), "False")
            a.release()
            self.assertTrue(b.acquire())
            b.release()
        finally:
            env.db.close()
            env.cleanup()


# ------------------------------------------------------------------------------- stage B
class SimulationTests(SimCase):
    def test_happy_path(self) -> None:
        out = sim.scenario_happy(self.env)
        self.assertEqual(out["tasks"], {"T0001": "DONE", "T0002": "DONE"})
        self.assertTrue(out["pushed"])
        self.assertIn("T0001 APPROVED", out["decisions_md"])
        self.assertIn("simulated progress", out["progress_md"])
        # Astra reviewed in the read-only review worktree, Claude worked in the dev worktree.
        self.assertTrue(all(c["cwd"].endswith("/wt/review") for c in out["astra_calls"]))
        self.assertTrue(all(c["cwd"].endswith("/wt/dev") for c in out["claude_calls"]))
        # Task -> branch -> commit -> verification -> decision links are recorded.
        t = self.env.db.task("T0001")
        self.assertEqual(t["branch"], "agent/t0001")
        self.assertTrue(self.env.db.verification("T0001", t["head_commit"])["passed"])
        self.assertEqual(self.env.db.decisions(1, "T0001")[0]["commit_sha"], t["head_commit"])

    def test_changes_requested_then_approved(self) -> None:
        out = sim.scenario_changes_then_approve(self.env)
        self.assertEqual((out["status"], out["attempts"]), ("DONE", 1))
        self.assertTrue(out["claude_calls"][1]["resume"])  # same session continues with feedback

    def test_quota_with_known_reset_resumes_same_session(self) -> None:
        out = sim.scenario_quota_known_reset(self.env)
        self.assertEqual(out["status"], "DONE")
        first, second = out["claude_calls"][0], out["claude_calls"][1]
        self.assertFalse(first["resume"])
        self.assertTrue(second["resume"])
        self.assertEqual(first["session_id"], second["session_id"])
        self.assertIn("quota_exhausted", out["events"])
        self.assertIn("quota_restored", out["events"])

    def test_quota_unknown_reset_uses_backoff(self) -> None:
        out = sim.scenario_quota_unknown_reset(self.env)
        self.assertEqual(out["status"], "DONE")
        self.assertEqual([e[0] for e in out["quota_events"]], ["quota_exhausted", "quota_exhausted", "quota_restored"])
        self.assertIn('"source": "backoff"', out["quota_events"][0][1])

    def test_protected_path_never_integrated(self) -> None:
        out = sim.scenario_protected_path(self.env)
        self.assertEqual(out["tasks"]["T0001"][0], "BLOCKED")
        self.assertEqual(out["tasks"]["T0002"][0], "DONE")  # loop moved on to the independent task
        self.assertNotIn("[T0001] touch orchestrator", out["log"])
        self.assertFalse((self.env.cfg.worktree("integration") / "tools" / "orchestrator" / "evil.txt").exists())

    def test_no_progress_blocks(self) -> None:
        out = sim.scenario_no_progress(self.env)
        self.assertEqual(out["status"], "BLOCKED")
        self.assertIn("no measurable progress", out["reason"])
        self.assertLessEqual(out["claude_calls"], 4)

    def test_astra_quota_during_review(self) -> None:
        out = sim.scenario_astra_quota_in_review(self.env)
        self.assertEqual(out["status"], "DONE")
        self.assertEqual(out["reviews"].count("review"), 2)

    def test_needs_human_does_not_stop_independent_work(self) -> None:
        out = sim.scenario_needs_human(self.env)
        self.assertEqual(out["tasks"], {"T0001": "NEEDS_HUMAN", "T0002": "DONE"})
        self.assertIn("simulated question", out["question"])
        self.assertEqual(self.env.db.get_meta("loop_state"), "HALTED")

    def test_all_blocked_halts_with_report(self) -> None:
        astra = FakeAstra(plans=[fake_plan("M1", ["a"])], default_review="BLOCKED")
        state = self.env.orchestrator(astra, FakeClaude()).run(max_steps=20)
        self.assertEqual(state, "HALTED")
        self.assertEqual(self.env.db.task("T0001")["status"], "BLOCKED")

    def test_agent_crashes_block_task_and_halt(self) -> None:
        astra = FakeAstra(plans=[fake_plan("M1", ["a"])])
        claude = FakeClaude(default="fail")
        state = self.env.orchestrator(astra, claude).run(max_steps=20)
        self.assertEqual(state, "HALTED")
        self.assertEqual(self.env.db.task("T0001")["status"], "BLOCKED")

    def test_pause_and_stop_control(self) -> None:
        with self.env.db.tx():
            self.env.db.set_meta("desired_state", "PAUSED")
        orch = self.env.orchestrator(FakeAstra(plans=[fake_plan("M1", ["a"])]), FakeClaude())
        calls = {"n": 0}

        def fake_sleep(_s: float) -> None:
            calls["n"] += 1
            if calls["n"] == 3:
                with self.env.db.tx():
                    self.env.db.set_meta("desired_state", "STOPPED")
        orch.sleep_fn = fake_sleep
        self.assertEqual(orch.run(), "STOPPED")
        self.assertEqual(self.env.db.tasks(), [])  # nothing ran while paused

    def test_approval_of_unverified_commit_is_rejected(self) -> None:
        astra = FakeAstra(plans=[fake_plan("M1", ["a"])], reviews=["APPROVED", "APPROVED"])
        orch = self.env.orchestrator(astra, FakeClaude())
        orch.setup()
        orch.step()  # plan
        orch.step()  # implement
        orch.step()  # verify (passes)
        t = self.env.db.task("T0001")
        with self.env.db.tx():  # tamper: pretend verification failed for this head
            self.env.db.conn.execute("UPDATE verifications SET passed=0 WHERE task_id='T0001'")
        orch.step()  # review: Astra says APPROVED, orchestrator overrides
        self.assertEqual(self.env.db.decisions(1, "T0001")[0]["decision"], "CHANGES_REQUESTED")
        self.assertEqual(self.env.db.task("T0001")["status"], "CHANGES_REQUESTED")
        self.assertEqual(t["status"], "IN_REVIEW")

    def test_restart_refuses_invalid_config_and_handles_no_daemon(self) -> None:
        repo = str(self.env.repo)
        self.assertEqual(cli_main(["--repo", repo, "restart"]), 0)  # nothing running: just informs
        cfg_path = self.env.cfg.state_dir / "config.json"
        good = cfg_path.read_text()
        cfg_path.write_text('{"release": {"max_merges_per_day": 10,}}')  # trailing comma
        self.assertEqual(cli_main(["--repo", repo, "restart"]), 2)
        cfg_path.write_text("[1, 2]")
        self.assertEqual(cli_main(["--repo", repo, "status"]), 2)
        cfg_path.write_text(good)

    def test_restart_request_interrupts_quota_wait(self) -> None:
        orch = self.env.orchestrator(FakeAstra(plans=[fake_plan("M1", ["a"])]),
                                     FakeClaude(script=[{"do": "quota", "reset_in": 3600}]))
        calls = {"n": 0}

        def fake_sleep(_s: float) -> None:
            calls["n"] += 1
            if calls["n"] == 2:
                with self.env.db.tx():
                    self.env.db.set_meta("restart_requested", "1")
        orch.sleep_fn = fake_sleep
        self.assertEqual(orch.run(), "RESTARTING")
        self.assertEqual(self.env.db.task("T0001")["status"], "IN_PROGRESS")  # checkpoint kept

    def test_cli_status_pause_resume_stop(self) -> None:
        repo = str(self.env.repo)
        self.assertEqual(cli_main(["--repo", repo, "pause"]), 0)
        self.assertEqual(self.env.db.get_meta("desired_state"), "PAUSED")
        self.assertEqual(cli_main(["--repo", repo, "resume"]), 0)
        self.assertEqual(self.env.db.get_meta("desired_state"), "RUNNING")
        self.assertEqual(cli_main(["--repo", repo, "stop"]), 0)
        self.assertEqual(cli_main(["--repo", repo, "status", "--json"]), 0)
        self.assertEqual(cli_main(["--repo", repo, "report"]), 0)
        self.assertTrue((self.env.cfg.state_dir / "report.html").exists())


# ------------------------------------------------------------------------------- Gatekeeper
class GatekeeperTests(SimCase):
    def setUp(self) -> None:
        super().setUp()
        self.env.cfg.data["release"]["every_s"] = 10 ** 9  # only explicit steps in these tests

    def _integration_pr(self, port, **kw) -> None:
        port.add_pr(6, "agents/integration", "main", draft=True, **kw)

    def test_merges_approved_integration_pr(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.finish_one_task(self.env, orch)
        self._integration_pr(port)
        called, msg = orch.gatekeeper.step()
        self.assertTrue(called)
        self.assertIn("MERGED", msg)
        self.assertEqual(port.merged, [6])
        main_log = subprocess.run(["git", "log", "--format=%s", "origin/main"], cwd=self.env.repo,
                                  capture_output=True, text=True)
        subprocess.run(["git", "fetch", "-q", "origin"], cwd=self.env.repo)
        main_log = subprocess.run(["git", "log", "--format=%s", "origin/main"], cwd=self.env.repo,
                                  capture_output=True, text=True).stdout
        self.assertIn("[T0001] simulated change", main_log)
        self.assertEqual(self.env.db.decisions(1)[0]["decision"], "GATEKEEPER_MERGE")
        self.assertIn("Gatekeeper MERGED PR #6",
                      (self.env.cfg.worktree("integration") / "docs/agent-memory/DECISIONS.md").read_text())
        self.assertTrue(gk.calls[0]["cwd"].endswith("/wt/release"))

    def _main_commit(self, path: str, content: str) -> None:
        """The owner lands a change on main (as a merged PR would)."""
        wt = self.env.tmp / ("main-%d" % len(list(self.env.tmp.glob("main-*"))))
        subprocess.run(["git", "clone", "-q", "-b", "main", str(self.env.origin), str(wt)], check=True)
        for k, v in (("user.email", "o@example.invalid"), ("user.name", "owner")):
            subprocess.run(["git", "config", k, v], cwd=wt, check=True)
        (wt / path).parent.mkdir(parents=True, exist_ok=True)
        (wt / path).write_text(content)
        subprocess.run(["git", "add", "-A"], cwd=wt, check=True)
        subprocess.run(["git", "commit", "-q", "-m", "owner: " + path], cwd=wt, check=True)
        subprocess.run(["git", "push", "-q", "origin", "main"], cwd=wt, check=True)

    def _wt_int(self) -> Path:
        return self.env.cfg.worktree("integration")

    def _git_int(self, *args: str) -> subprocess.CompletedProcess:
        return subprocess.run(["git", *args], cwd=self._wt_int(), capture_output=True, text=True)

    def test_main_is_synced_into_integration_and_gate_accepts_the_merge(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.finish_one_task(self.env, orch)
        self._main_commit("docs/owner.md", "owner work\n")
        self._integration_pr(port)
        called, msg = orch.gatekeeper.step()
        self.assertFalse(called)
        self.assertIn("synced origin/main into agents/integration", msg)
        subprocess.run(["git", "fetch", "-q", "origin"], cwd=self.env.repo, check=True)
        self.assertEqual(subprocess.run(["git", "merge-base", "--is-ancestor", "origin/main", "origin/agents/integration"],
                                        cwd=self.env.repo).returncode, 0)
        self._integration_pr(port)  # GitHub sees the new head
        called, msg = orch.gatekeeper.step()
        self.assertIn("MERGED", msg)
        self.assertFalse([t for t in self.env.db.tasks() if t["created_by"] == "gatekeeper"])

    def test_sync_conflict_asks_the_owner_not_an_agent(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.finish_one_task(self.env, orch)
        head = self._git_int("rev-parse", "HEAD").stdout.strip()
        self._main_commit("sim/T0001.txt", "owner version\n")
        self._integration_pr(port)
        called, msg = orch.gatekeeper.step()
        self.assertFalse(called)
        self.assertEqual(self._git_int("rev-parse", "HEAD").stdout.strip(), head)
        self.assertEqual(self._git_int("status", "--porcelain").stdout.strip(), "")
        self.assertFalse([t for t in self.env.db.tasks() if t["created_by"] == "gatekeeper"])
        self.assertIn("sim/T0001.txt", self.env.db.get_meta("human_question"))
        # The owner resolves it by hand (keeps the agents' version); the gate accepts that merge.
        self._git_int("merge", "origin/main")
        self._git_int("checkout", "--ours", "sim/T0001.txt")
        self._git_int("commit", "-q", "-a", "--no-edit")
        orch.maybe_push(force_now=True)
        self._integration_pr(port)
        called, msg = orch.gatekeeper.step()
        self.assertIn("MERGED", msg)

    def test_sync_merge_may_not_rewrite_protected_paths(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.finish_one_task(self.env, orch)
        self._main_commit("tools/orchestrator/x.txt", "reviewed\n")
        self._git_int("fetch", "-q", "origin")
        self._git_int("merge", "--no-commit", "origin/main")
        (self._wt_int() / "tools/orchestrator/x.txt").write_text("rewritten during the merge\n")
        self._git_int("commit", "-q", "-a", "--no-edit")
        orch.maybe_push(force_now=True)
        self._integration_pr(port)
        g = orch.gatekeeper.gates(port.open_prs()[0])
        self.assertTrue(any("rewrites protected paths by hand: tools/orchestrator/x.txt" in f for f in g["fails"]),
                        g["fails"])

    def test_red_ci_creates_fix_task_without_model_call(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.finish_one_task(self.env, orch)
        self._integration_pr(port, checks="fail")
        called, msg = orch.gatekeeper.step()
        self.assertFalse(called)
        self.assertEqual(gk.calls, [])
        fix = [t for t in self.env.db.tasks() if t["created_by"] == "gatekeeper"]
        self.assertEqual(len(fix), 1)
        self.assertEqual((fix[0]["status"], fix[0]["priority"]), ("READY", 0))
        orch.gatekeeper.step()  # same head: no duplicate task
        self.assertEqual(len([t for t in self.env.db.tasks() if t["created_by"] == "gatekeeper"]), 1)
        self.assertEqual(port.merged, [])

    def test_pending_ci_holds(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.finish_one_task(self.env, orch)
        self._integration_pr(port, checks="pending")
        called, msg = orch.gatekeeper.step()
        self.assertFalse(called)
        self.assertEqual(port.merged, [])
        self.assertEqual(gk.calls, [])

    def test_unapproved_commit_on_integration_blocks_merge(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.finish_one_task(self.env, orch)
        wt = self.env.cfg.worktree("integration")
        (wt / "sneaky.txt").write_text("not reviewed\n")
        subprocess.run(["git", "add", "-A"], cwd=wt, check=True)
        subprocess.run(["git", "commit", "-q", "-m", "sneak in a change"], cwd=wt, check=True)
        subprocess.run(["git", "push", "-q", "origin", "agents/integration"], cwd=wt, check=True)
        self._integration_pr(port)
        called, msg = orch.gatekeeper.step()
        self.assertFalse(called)
        self.assertIn("GATES_FAILED", msg)
        self.assertEqual(port.merged, [])

    def test_model_merge_cannot_override_game_data_gate(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.human_branch(self.env, "feat-data")
        wt = self.env.tmp / "h-feat-data"
        (wt / "wil.gmp").write_bytes(b"GBMP")
        subprocess.run(["git", "add", "-A"], cwd=wt, check=True)
        subprocess.run(["git", "commit", "-q", "-m", "oops"], cwd=wt, check=True)
        subprocess.run(["git", "push", "-q", "origin", "feat-data"], cwd=wt, check=True)
        port.add_pr(9, "feat-data", "main")
        called, msg = orch.gatekeeper.step()
        self.assertFalse(called)
        self.assertEqual(port.merged, [])
        self.assertEqual(port.comments[0][0], 9)

    def test_stack_is_merged_bottom_up_and_retargeted(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.human_branch(self.env, "feat-a")
        sim.human_branch(self.env, "feat-b", start="feat-a")
        port.add_pr(2, "feat-a", "main")
        port.add_pr(3, "feat-b", "feat-a")
        self.assertIn("MERGED", orch.gatekeeper.step()[1])
        self.assertEqual(port.retargets, [(3, "main")])
        self.assertIn("MERGED", orch.gatekeeper.step()[1])
        self.assertEqual(port.merged, [2, 3])

    def test_skips_labels_drafts_and_foreign_authors(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.human_branch(self.env, "feat-l")
        sim.human_branch(self.env, "feat-d")
        sim.human_branch(self.env, "feat-f")
        port.add_pr(4, "feat-l", "main", labels=["do-not-merge"])
        port.add_pr(5, "feat-d", "main", draft=True)
        port.add_pr(7, "feat-f", "main", author="someone-else")
        called, msg = orch.gatekeeper.step()
        self.assertEqual((called, msg), (False, ""))
        self.assertEqual(port.merged, [])
        reasons = dict(json.loads(self.env.db.get_meta("gatekeeper_skipped")))
        self.assertIn("label", reasons["4"] if "4" in reasons else reasons[4])

    def test_request_changes_comments_once_per_head(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env, decisions=["REQUEST_CHANGES", "MERGE"])
        sim.human_branch(self.env, "feat-r")
        port.add_pr(8, "feat-r", "main")
        orch.gatekeeper.step()
        orch.gatekeeper.step()  # same head: not reviewed again
        self.assertEqual(len(gk.calls), 1)
        self.assertEqual(len(port.comments), 1)
        self.assertEqual(port.merged, [])

    def test_dry_run_never_merges(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.human_branch(self.env, "feat-y")
        port.add_pr(11, "feat-y", "main")
        called, msg = orch.gatekeeper.step(dry_run=True)
        self.assertTrue(called)
        self.assertIn("dry run", msg)
        self.assertEqual(port.merged, [])

    def test_daily_merge_cap(self) -> None:
        self.env.cfg.data["release"]["max_merges_per_day"] = 1
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.human_branch(self.env, "feat-1")
        sim.human_branch(self.env, "feat-2")
        port.add_pr(12, "feat-1", "main")
        port.add_pr(13, "feat-2", "main")
        orch.gatekeeper.step()
        orch.gatekeeper.step()
        self.assertEqual(port.merged, [12])

    def test_merge_with_major_findings_creates_follow_up_task(self) -> None:
        orch, gk, port = sim.gatekeeper_env(self.env, decisions=["MERGE_WITH_FINDINGS"])
        sim.human_branch(self.env, "feat-m")
        port.add_pr(15, "feat-m", "main")
        self.assertIn("MERGED", orch.gatekeeper.step()[1])
        follow = [t for t in self.env.db.tasks() if t["created_by"] == "gatekeeper"]
        self.assertEqual(len(follow), 1)
        self.assertIn("PR #15", follow[0]["title"])

    def test_never_manages_foreign_launch_agent(self) -> None:
        from gta20_orchestrator import service
        self.assertFalse(service.manages(self.env.repo))
        with self.assertRaises(SystemExit):
            service.enable()

    def test_loop_runs_gatekeeper_when_due(self) -> None:
        self.env.cfg.data["release"]["every_s"] = 0
        orch, gk, port = sim.gatekeeper_env(self.env)
        sim.human_branch(self.env, "feat-z")
        port.add_pr(14, "feat-z", "main")
        orch.setup()
        self.assertIn("gatekeeper: PR #14: MERGED", orch.step())


# ------------------------------------------------------------------------------- EC2 test host jobs
SCENARIO = {"id": "menu-walk", "wrapper": "ddrawcompat", "screen": {"start_mode": 1}, "durationSeconds": 40,
            "screenshotsAtSeconds": [10, 20, 30], "inputs": [{"atSeconds": 12, "key": "ENTER", "holdMs": 80}]}


class GfxTests(SimCase):
    def setUp(self) -> None:
        super().setUp()
        self.matrix = json.loads((REPO_ROOT / "tests/gfx/compat-matrix.json").read_text())
        dest = self.env.repo / "tests" / "gfx" / "compat-matrix.json"
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(json.dumps(self.matrix))

    def _orch(self, runner, astra=None, claude=None):
        from gta20_orchestrator.loop import Orchestrator
        return Orchestrator(self.env.cfg, self.env.db, astra or FakeAstra(plans=[fake_plan("M1", ["walls"])]),
                            claude or FakeClaude(), use_github=False, sleep=lambda s: None, gfx_runner=runner)

    def _jobs(self):
        return [dict(r) for r in self.env.db.conn.execute("SELECT * FROM gfx_jobs ORDER BY id")]

    def test_repository_scenarios_are_valid(self) -> None:
        files = sorted((REPO_ROOT / "tests/gfx/scenarios").glob("*.json"))
        self.assertTrue(files)
        for f in files:
            validate_scenario(json.loads(f.read_text()), self.matrix, f.stem, 45 * 60)

    def test_scenario_validation_is_strict(self) -> None:
        validate_scenario(dict(SCENARIO), self.matrix, "menu-walk", 2700)
        bad = [
            dict(SCENARIO, id="other"),
            dict(SCENARIO, script="Remove-Item C:\\"),
            dict(SCENARIO, wrapper="unknown"),
            dict(SCENARIO, screen={"exe": 1}),
            dict(SCENARIO, durationSeconds=301),
            dict(SCENARIO, screenshotsAtSeconds=[10, 10]),
            dict(SCENARIO, screenshotsAtSeconds=[50]),
            dict(SCENARIO, inputs=[{"atSeconds": 1, "key": "F4"}]),
            dict(SCENARIO, inputs=[{"atSeconds": 1, "key": "ENTER", "cmd": "x"}]),
            dict(SCENARIO, inputs=[{"atSeconds": 1, "key": "ENTER", "holdMs": 60000}]),
        ]
        for b in bad:
            with self.assertRaises(ScenarioError, msg=str(b)):
                validate_scenario(b, self.matrix, "menu-walk", 2700)

    def test_claude_capture_runs_and_results_reach_review(self) -> None:
        runner = FakeGfxRunner()
        claude = FakeClaude(script=[{"do": "commit", "scenario": SCENARIO,
                                     "gfx": [{"mode": "capture", "target": "menu-walk", "reason": "reference"}]}])
        astra = FakeAstra(plans=[fake_plan("M1", ["walls"])])
        orch = self._orch(runner, astra, claude)
        orch.run(max_steps=10)
        self.assertEqual(len(runner.calls), 1)
        call = runner.calls[0]
        self.assertEqual((call["mode"], call["target"], call["minutes"], call["staged"]),
                         ("capture", "menu-walk", 45, ["menu-walk.json"]))
        job = self._jobs()[0]
        self.assertEqual((job["status"], job["verdict"], job["task_id"], job["requested_by"]),
                         ("done", "PASS", "T0001", "claude"))
        review_prompts = sorted(self.env.cfg.state_dir.glob("runs/*T0001-astra-review/prompt.md"))
        self.assertTrue(review_prompts)
        text = review_prompts[-1].read_text()
        self.assertIn("EC2 TEST HOST RUNS", text)
        self.assertIn("capture-menu-walk=PASS", text)

    def test_invalid_requests_are_rejected_without_running(self) -> None:
        runner = FakeGfxRunner()
        orch = self._orch(runner)
        msgs = orch.gfx.enqueue([{"mode": "shell", "target": "x", "reason": ""},
                                 {"mode": "compat", "target": "no-such-case", "reason": ""},
                                 {"mode": "capture", "target": "missing", "reason": ""}], None, "astra", self.env.repo)
        self.assertEqual(len(msgs), 3)
        self.assertEqual([j["status"] for j in self._jobs()], ["rejected"] * 3)
        self.assertIsNone(orch.gfx.run_next())
        self.assertEqual(runner.calls, [])

    def test_daily_limit_keeps_job_queued(self) -> None:
        self.env.cfg.data["gfx"]["max_runs_per_day"] = 1
        runner = FakeGfxRunner()
        orch = self._orch(runner)
        orch.gfx.enqueue([{"mode": "probe", "target": "all", "reason": "a"},
                          {"mode": "probe", "target": "all", "reason": "b"}], None, "astra", self.env.repo)
        self.assertIn("PASS", orch.gfx.run_next())
        self.assertIsNone(orch.gfx.run_next())
        self.assertEqual(len(runner.calls), 1)
        self.assertEqual([j["status"] for j in self._jobs()], ["done", "queued"])
        self.assertGreater(orch.gfx.next_possible(), time.time() + 60)

    def test_monthly_budget_refuses_and_asks_owner(self) -> None:
        self.env.cfg.data["gfx"]["monthly_budget_usd"] = 0.5
        runner = FakeGfxRunner()
        orch = self._orch(runner)
        orch.gfx.enqueue([{"mode": "probe", "target": "all", "reason": "a"}], None, "astra", self.env.repo)
        self.assertIn("monthly budget", orch.gfx.run_next())
        self.assertEqual(runner.calls, [])
        self.assertEqual(self._jobs()[0]["status"], "refused_budget")
        self.assertIn("budget", self.env.db.get_meta("human_question"))

    def test_expired_aws_session_waits_for_login(self) -> None:
        runner = FakeGfxRunner(session=False)
        orch = self._orch(runner)
        orch.gfx.enqueue([{"mode": "probe", "target": "all", "reason": "a"}], None, "astra", self.env.repo)
        self.assertIn("aws login", orch.gfx.run_next())
        self.assertEqual(runner.calls, [])
        self.assertEqual(self._jobs()[0]["status"], "queued")
        self.assertIn("aws login", self.env.db.get_meta("human_question"))
        self.assertIsNone(orch.gfx.run_next())  # backed off, no retry storm

    def test_plan_waiting_for_run_does_not_halt_or_replan(self) -> None:
        plan = dict(fake_plan("M1", []), gfx_requests=[{"mode": "probe", "target": "all", "reason": "health"}])
        astra = FakeAstra(plans=[plan, fake_plan("M1", ["walls"])])
        runner = FakeGfxRunner(session=False)
        orch = self._orch(runner, astra)
        orch.setup()
        self.assertIn("plan", orch.step())
        self.assertIn("aws login", orch.step())
        with self.assertRaises(Wait):
            orch.step()
        self.assertEqual(len([c for c in astra.calls if c["kind"] == "plan"]), 1)
        runner.session = True
        self.env.db.set_meta("gfx_blocked_until", "0")
        self.assertIn("gfx job 1", orch.step())
        orch.step()  # re-plan with the result in the prompt
        plans = sorted(self.env.cfg.state_dir.glob("runs/*plan*/prompt.md"))
        self.assertIn("job 1 [done] probe all by astra: PASS", plans[-1].read_text())

    def test_interrupted_run_is_charged_and_failed(self) -> None:
        orch = self._orch(FakeGfxRunner())
        orch.gfx.enqueue([{"mode": "probe", "target": "all", "reason": "a"}], None, "astra", self.env.repo)
        self.env.db.conn.execute("UPDATE gfx_jobs SET status='running', started_at=?",
                                 (time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),))
        orch.recover()
        job = self._jobs()[0]
        self.assertEqual((job["status"], job["minutes"]), ("failed", 45.0))
        self.assertGreater(orch.gfx.month_spend_usd(), 0.5)

    def test_host_scripts_are_protected(self) -> None:
        prot = self.env.cfg["protected_paths"]
        self.assertEqual(protected_hits(["tests/gfx/windows/Run-Gta2Case.ps1", "tests/gfx/compat-matrix.json",
                                         "scripts/ci/ec2-gpu-session.sh", "tests/gfx/scenarios/x.json"], prot),
                         ["tests/gfx/windows/Run-Gta2Case.ps1", "tests/gfx/compat-matrix.json",
                          "scripts/ci/ec2-gpu-session.sh"])


# ------------------------------------------------------------------------------- stage D
class RecoveryTests(SimCase):
    def _spawn(self, spec: dict) -> subprocess.Popen:
        spec_path = self.env.tmp / ("fake-%d.json" % time.time_ns())
        spec_path.write_text(json.dumps(spec))
        env = dict(os.environ, GTA20_FAKE_AGENTS=str(spec_path), PYTHONPATH=str(PKG_ROOT))
        return subprocess.Popen([sys.executable, "-m", "gta20_orchestrator", "--repo", str(self.env.repo), "start",
                                 "--foreground"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                start_new_session=True)

    def _wait(self, cond, timeout: float = 60) -> None:
        deadline = time.time() + timeout
        while time.time() < deadline:
            if cond():
                return
            time.sleep(0.1)
        self.fail("condition not reached in %ss" % timeout)

    def _partial_committed(self) -> bool:
        dev = self.env.cfg.worktree("dev")
        if not dev.exists():
            return False
        out = subprocess.run(["git", "log", "--format=%s", "-n", "1"], cwd=dev, capture_output=True, text=True)
        return out.stdout.startswith("[T0001]")

    def test_hard_kill_mid_run_resumes_without_duplicating_work(self) -> None:
        plan = fake_plan("M1", ["walls"])
        p = self._spawn({"astra_plans": [plan], "claude_script": [{"do": "partial_then_hang"}]})
        self._wait(self._partial_committed)
        os.killpg(p.pid, signal.SIGKILL)  # crash: no cleanup, run row stays 'running'
        p.wait()
        db = D.Database(self.env.cfg.state_dir)
        self.assertEqual([r["status"] for r in db.running_runs()], ["running"])
        db.close()
        p = self._spawn({"claude_script": [{"do": "commit", "content": "finished"}]})
        self.assertEqual(p.wait(timeout=120), 0)
        db = D.Database(self.env.cfg.state_dir)
        runs = [(r["kind"], r["status"]) for r in reversed(db.runs("T0001"))]
        self.assertIn(("implement", "abandoned"), runs)
        self.assertEqual(db.task("T0001")["status"], "DONE")
        log = subprocess.run(["git", "log", "--format=%s"], cwd=self.env.cfg.worktree("integration"),
                             capture_output=True, text=True).stdout.splitlines()
        self.assertEqual(sum(1 for line in log if line == "[T0001] simulated change"), 2)  # partial + finish, once each
        resumed = [r for r in db.runs("T0001") if r["kind"] == "implement"][0]
        prompt = (Path(resumed["run_dir"]) / "prompt.md").read_text()
        self.assertIn("Continue task T0001", prompt)
        self.assertIn("simulated change", prompt)  # the partial commit is shown as already done
        db.close()

    def test_graceful_stop_mid_run_then_resume(self) -> None:
        plan = fake_plan("M1", ["walls"])
        p = self._spawn({"astra_plans": [plan], "claude_script": [{"do": "hang", "seconds": 60}]})
        self._wait(lambda: self.env.cfg.worktree("dev").exists() and self._run_status("running"))
        os.kill(p.pid, signal.SIGTERM)
        self.assertEqual(p.wait(timeout=60), 0)
        self.assertTrue(self._run_status("interrupted"))
        p = self._spawn({"claude_script": [{"do": "commit"}]})
        self.assertEqual(p.wait(timeout=120), 0)
        db = D.Database(self.env.cfg.state_dir)
        self.assertEqual(db.task("T0001")["status"], "DONE")
        db.close()

    def _daemon_running(self) -> bool:
        from gta20_orchestrator.report import daemon_pid
        return daemon_pid(self.env.cfg.state_dir) > 0

    def test_restart_waits_for_the_current_step(self) -> None:
        plan = fake_plan("M1", ["walls"])
        p = self._spawn({"astra_plans": [plan], "claude_script": [{"do": "hang", "seconds": 3}]})
        self._wait(lambda: self._run_status("running"))
        out = subprocess.run([sys.executable, "-m", "gta20_orchestrator", "--repo", str(self.env.repo), "restart",
                              "--timeout", "60"], env=dict(os.environ, PYTHONPATH=str(PKG_ROOT)),
                             capture_output=True, text=True, timeout=120)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
        self.assertIn("start it again", out.stdout)  # foreground daemon: no launchd respawn in tests
        self.assertEqual(p.wait(timeout=30), 0)
        db = D.Database(self.env.cfg.state_dir)
        claude_runs = [r["status"] for r in db.runs() if r["agent"] == "claude"]
        self.assertEqual(claude_runs, ["succeeded"])  # the step finished, nothing was interrupted
        self.assertEqual(db.get_meta("loop_state"), "RESTARTING")
        self.assertEqual(db.get_meta("restart_requested"), "")
        self.assertEqual(db.get_meta("desired_state"), "RUNNING")
        db.close()
        p = self._spawn({})  # the next start continues from the checkpoint
        self.assertEqual(p.wait(timeout=120), 0)
        db = D.Database(self.env.cfg.state_dir)
        self.assertEqual(db.task("T0001")["status"], "DONE")
        db.close()

    def test_restart_now_interrupts_and_checkpoints(self) -> None:
        plan = fake_plan("M1", ["walls"])
        p = self._spawn({"astra_plans": [plan], "claude_script": [{"do": "hang", "seconds": 120}]})
        self._wait(lambda: self._run_status("running"))
        out = subprocess.run([sys.executable, "-m", "gta20_orchestrator", "--repo", str(self.env.repo), "restart",
                              "--now"], env=dict(os.environ, PYTHONPATH=str(PKG_ROOT)),
                             capture_output=True, text=True, timeout=120)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
        self.assertEqual(p.wait(timeout=30), 0)
        self.assertTrue(self._run_status("interrupted"))

    def _run_status(self, status: str) -> bool:
        db = D.Database(self.env.cfg.state_dir)
        try:
            return any(r["status"] == status and r["agent"] == "claude" for r in db.runs())
        finally:
            db.close()

    def test_daemon_respects_stopped_and_crash_loop(self) -> None:
        repo = str(self.env.repo)
        with self.env.db.tx():
            self.env.db.set_meta("desired_state", "STOPPED")
        env = dict(os.environ, PYTHONPATH=str(PKG_ROOT), GTA20_FAKE_AGENTS="")
        out = subprocess.run([sys.executable, "-m", "gta20_orchestrator", "--repo", repo, "daemon"], env=env,
                             capture_output=True, text=True, timeout=60)
        self.assertEqual(out.returncode, 0)
        self.assertEqual(self.env.db.tasks(), [])
        with self.env.db.tx():
            self.env.db.set_meta("desired_state", "RUNNING")
            self.env.db.set_meta("start_times", json.dumps([time.time()] * 6))
        out = subprocess.run([sys.executable, "-m", "gta20_orchestrator", "--repo", repo, "daemon"], env=env,
                             capture_output=True, text=True, timeout=60)
        self.assertEqual(out.returncode, 0)
        self.assertEqual(self.env.db.get_meta("loop_state"), "HALTED")
        self.assertIn("crash loop", self.env.db.get_meta("loop_reason"))


if __name__ == "__main__":
    unittest.main()
