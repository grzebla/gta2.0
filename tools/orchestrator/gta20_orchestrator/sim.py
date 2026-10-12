"""Stage B: the real loop against scripted fake agents in a throw-away git repository with a
local bare 'origin'. Used by the unit tests and by `gta20-orchestrator simulate`."""

from __future__ import annotations

import json
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from . import db as D
from .adapters.fake import FakeAstra, FakeClaude, empty_memory, fake_plan
from .config import Config
from .loop import Orchestrator


def _git(cwd: Path, *args: str) -> str:
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


class SimEnv:
    def __init__(self, root: Optional[Path] = None, overrides: Optional[Dict[str, Any]] = None):
        self.tmp = root or Path(tempfile.mkdtemp(prefix="gta20-sim-"))
        self.origin = self.tmp / "origin.git"
        self.repo = self.tmp / "repo"
        _git(self.tmp, "init", "-q", "--bare", str(self.origin))
        self.repo.mkdir()
        _git(self.repo, "init", "-q", "-b", "main")
        _git(self.repo, "config", "user.email", "sim@example.invalid")
        _git(self.repo, "config", "user.name", "sim")
        (self.repo / "README.md").write_text("sim\n")
        (self.repo / ".gitignore").write_text(".orchestrator/\n")
        mem = self.repo / "docs" / "agent-memory"
        mem.mkdir(parents=True)
        for name in ("PROJECT", "ARCHITECTURE", "ROADMAP", "KNOWN_ISSUES", "TEST_STATUS", "DECISIONS", "PROGRESS"):
            (mem / (name + ".md")).write_text("# %s\n" % name.title())
        _git(self.repo, "add", "-A")
        _git(self.repo, "commit", "-q", "-m", "init")
        _git(self.repo, "branch", "feature/orchestrator")
        _git(self.repo, "remote", "add", "origin", str(self.origin))
        _git(self.repo, "push", "-q", "origin", "main", "feature/orchestrator")
        base = {
            "worktrees_dir": str(self.tmp / "wt"),
            "push_integration": True, "min_push_interval_s": 0, "open_draft_pr": False,
            "verify": {"commands": [["sh", "-c", "test -f README.md"]], "timeout_s": 60, "env": {}, "pinned_tools": False},
            "limits": {"poll_s": 0.01, "quota_reset_margin_s": 0, "quota_backoff_s": [0.05, 0.05, 0.1],
                       "quota_max_probes": 5, "arch_review_every": 0, "max_runs_per_day": 1000},
        }
        if overrides:
            from .config import deep_merge
            base = deep_merge(base, overrides)
        state = self.repo / ".orchestrator"
        state.mkdir(parents=True, exist_ok=True)
        (state / "config.json").write_text(json.dumps(base, indent=1))  # same config for CLI subprocesses
        self.cfg = Config.load(self.repo)
        self.db = D.Database(self.cfg.state_dir)

    def orchestrator(self, astra: Any, claude: Any) -> Orchestrator:
        return Orchestrator(self.cfg, self.db, astra, claude, use_github=False, sleep=lambda s: None)

    def integration_log(self) -> List[str]:
        return _git(self.cfg.worktree("integration"), "log", "--format=%s").splitlines()

    def origin_has(self, branch: str) -> bool:
        return bool(_git(self.origin, "branch", "--list", branch))

    def cleanup(self) -> None:
        shutil.rmtree(self.tmp, ignore_errors=True)


def scenario_happy(env: SimEnv) -> Dict[str, Any]:
    astra = FakeAstra(plans=[fake_plan("M1", ["walls", "slopes"], {1: [0]})])
    claude = FakeClaude()
    state = env.orchestrator(astra, claude).run(max_steps=50)
    tasks = {t["id"]: t["status"] for t in env.db.tasks()}
    return {"state": state, "tasks": tasks, "log": env.integration_log(), "pushed": env.origin_has("agents/integration"),
            "decisions_md": (env.cfg.worktree("integration") / "docs/agent-memory/DECISIONS.md").read_text(),
            "progress_md": (env.cfg.worktree("integration") / "docs/agent-memory/PROGRESS.md").read_text(),
            "claude_calls": claude.calls, "astra_calls": astra.calls}


def scenario_changes_then_approve(env: SimEnv) -> Dict[str, Any]:
    astra = FakeAstra(plans=[fake_plan("M1", ["walls"])], reviews=["CHANGES_REQUESTED", "APPROVED"])
    claude = FakeClaude()
    state = env.orchestrator(astra, claude).run(max_steps=50)
    t = env.db.task("T0001")
    return {"state": state, "status": t["status"], "attempts": t["attempts"], "claude_calls": claude.calls}


def scenario_quota_known_reset(env: SimEnv) -> Dict[str, Any]:
    astra = FakeAstra(plans=[fake_plan("M1", ["walls"])])
    claude = FakeClaude(script=[{"do": "quota", "reset_in": 0.01}, {"do": "commit"}])
    state = env.orchestrator(astra, claude).run(max_steps=50)
    events = [e["type"] for e in reversed(env.db.events(200))]
    return {"state": state, "status": env.db.task("T0001")["status"], "claude_calls": claude.calls, "events": events,
            "quota": dict(env.db.quota("claude"))}


def scenario_quota_unknown_reset(env: SimEnv) -> Dict[str, Any]:
    astra = FakeAstra(plans=[fake_plan("M1", ["walls"])])
    claude = FakeClaude(script=[{"do": "quota_unknown"}, {"do": "commit"}])
    claude.probe_script = ["quota_unknown"]
    state = env.orchestrator(astra, claude).run(max_steps=50)
    events = [(e["type"], e["detail"]) for e in reversed(env.db.events(200)) if e["type"].startswith("quota")]
    return {"state": state, "status": env.db.task("T0001")["status"], "quota_events": events,
            "claude_calls": claude.calls}


def scenario_protected_path(env: SimEnv) -> Dict[str, Any]:
    astra = FakeAstra(plans=[fake_plan("M1", ["evil", "good"])])
    claude = FakeClaude(script=[{"do": "protected"}, {"do": "commit"}, {"do": "commit"}])
    state = env.orchestrator(astra, claude).run(max_steps=50)
    return {"state": state, "tasks": {t["id"]: (t["status"], t["blocked_reason"]) for t in env.db.tasks()},
            "astra_reviews": [c for c in astra.calls if c["kind"] == "review"], "log": env.integration_log()}


def scenario_no_progress(env: SimEnv) -> Dict[str, Any]:
    astra = FakeAstra(plans=[fake_plan("M1", ["stuck"])], default_review="CHANGES_REQUESTED")
    claude = FakeClaude(script=[{"do": "commit", "content": "same"}], default="no_commit")
    env.cfg.data["limits"]["max_attempts_per_task"] = 10
    state = env.orchestrator(astra, claude).run(max_steps=50)
    t = env.db.task("T0001")
    return {"state": state, "status": t["status"], "reason": t["blocked_reason"], "claude_calls": len(claude.calls),
            "loop_reason": env.db.get_meta("loop_reason")}


def scenario_astra_quota_in_review(env: SimEnv) -> Dict[str, Any]:
    astra = FakeAstra(plans=[fake_plan("M1", ["walls"])], reviews=["quota", "APPROVED"])
    claude = FakeClaude()
    state = env.orchestrator(astra, claude).run(max_steps=50)
    return {"state": state, "status": env.db.task("T0001")["status"],
            "reviews": [c["kind"] for c in astra.calls]}


def scenario_needs_human(env: SimEnv) -> Dict[str, Any]:
    astra = FakeAstra(plans=[fake_plan("M1", ["legal question", "independent"])], reviews=["NEEDS_HUMAN", "APPROVED"])
    claude = FakeClaude()
    state = env.orchestrator(astra, claude).run(max_steps=50)
    return {"state": state, "tasks": {t["id"]: t["status"] for t in env.db.tasks()},
            "question": env.db.get_meta("human_question"), "loop_reason": env.db.get_meta("loop_reason")}


SCENARIOS: Dict[str, Callable[[SimEnv], Dict[str, Any]]] = {
    "happy": scenario_happy,
    "changes_then_approve": scenario_changes_then_approve,
    "quota_known_reset": scenario_quota_known_reset,
    "quota_unknown_reset": scenario_quota_unknown_reset,
    "protected_path": scenario_protected_path,
    "no_progress": scenario_no_progress,
    "astra_quota_in_review": scenario_astra_quota_in_review,
    "needs_human": scenario_needs_human,
}


def main(name: str = "all", keep: bool = False) -> int:
    names = list(SCENARIOS) if name == "all" else [name]
    for n in names:
        env = SimEnv()
        try:
            out = SCENARIOS[n](env)
            short = {k: v for k, v in out.items() if k not in ("decisions_md", "progress_md", "claude_calls", "astra_calls",
                                                               "events")}
            print("== %s ==\n%s\n" % (n, json.dumps(short, indent=1, default=str)[:3000]))
        finally:
            if keep:
                print("kept: %s" % env.tmp)
            else:
                env.cleanup()
    return 0
