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

HERE = Path(__file__).resolve().parent
PKG_ROOT = HERE.parent
sys.path.insert(0, str(PKG_ROOT))

from gta20_orchestrator import db as D  # noqa: E402
from gta20_orchestrator import memory, schemas, sim  # noqa: E402
from gta20_orchestrator.adapters.claude import build_settings, parse_stream, quota_from_stream  # noqa: E402
from gta20_orchestrator.adapters.codex import parse_jsonl, read_rollout, reset_from_rate_limits  # noqa: E402
from gta20_orchestrator.adapters.fake import FakeAstra, FakeClaude, fake_plan  # noqa: E402
from gta20_orchestrator.cli import Lock, main as cli_main  # noqa: E402
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
