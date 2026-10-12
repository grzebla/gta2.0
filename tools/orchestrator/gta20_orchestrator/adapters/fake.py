"""Scripted stand-ins for Astra and Claude used by the simulation and the tests.

Behaviours are consumed in order; when the script runs out the default behaviour repeats.
They do real git work in the worktree they are given, so verification, integration and
resume logic run unchanged against them.
"""

from __future__ import annotations

import json
import re
import subprocess
import time
import uuid
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from .base import AgentFailed, AgentInterrupted, AgentResult, AgentTimeout, QuotaExceeded


def _git(cwd: Path, *args: str) -> str:
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


class FakeClaude:
    name = "claude"

    def __init__(self, script: Optional[List[Dict[str, Any]]] = None, default: str = "commit"):
        self.script = list(script or [])
        self.default = default
        self.calls: List[Dict[str, Any]] = []
        self.probe_script: List[str] = []

    def _next(self) -> Dict[str, Any]:
        return self.script.pop(0) if self.script else {"do": self.default}

    def run(self, *, kind: str, prompt: str, cwd: Path, run_dir: Path, schema: Any, session_id: str,
            resume: bool, should_stop: Callable[[], bool], on_start: Callable[[int], None] = lambda pid: None,
            **_: Any) -> AgentResult:
        step = self._next()
        self.calls.append({"kind": kind, "resume": resume, "session_id": session_id, "do": step["do"],
                           "cwd": str(cwd)})
        run_dir.mkdir(parents=True, exist_ok=True)
        (run_dir / "prompt.md").write_text(prompt)
        m = re.search(r"\b(T\d{4})\b", prompt)
        task_id = m.group(1) if m else "T????"
        do = step["do"]
        if do == "quota":
            raise QuotaExceeded("You've hit your session limit", reset_at=time.time() + float(step.get("reset_in", 2)),
                                limit_type=step.get("limit_type", "five_hour"), session_id=session_id)
        if do == "quota_unknown":
            raise QuotaExceeded("usage limit reached", reset_at=None, session_id=session_id)
        if do == "fail":
            raise AgentFailed("simulated crash", session_id=session_id)
        if do == "timeout":
            raise AgentTimeout("simulated timeout", session_id=session_id)
        if do == "hang":
            deadline = time.time() + float(step.get("seconds", 30))
            while time.time() < deadline:
                if should_stop():
                    raise AgentInterrupted("stopped", session_id=session_id)
                time.sleep(0.05)
        if do == "partial_then_hang":
            self._commit(cwd, task_id, "partial")
            while True:
                if should_stop():
                    raise AgentInterrupted("stopped", session_id=session_id)
                time.sleep(0.05)
        commits: List[str] = []
        if do in ("commit", "hang"):
            commits.append(self._commit(cwd, task_id, step.get("content", "work %s" % uuid.uuid4().hex[:8])))
        elif do == "same_change":
            commits.append(self._commit(cwd, task_id, "identical content"))
        elif do == "protected":
            p = cwd / "tools" / "orchestrator" / "evil.txt"
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("agent touched the orchestrator\n")
            _git(cwd, "add", "-A")
            _git(cwd, "commit", "-q", "-m", "[%s] touch orchestrator" % task_id)
            commits.append(_git(cwd, "rev-parse", "HEAD"))
        elif do == "dirty":
            (cwd / "sim" / ("%s-dirty.txt" % task_id)).parent.mkdir(parents=True, exist_ok=True)
            (cwd / "sim" / ("%s-dirty.txt" % task_id)).write_text("uncommitted\n")
        elif do == "no_commit":
            pass
        report = {
            "task_id": task_id, "status": "completed", "summary": "simulated %s" % do,
            "changes": ["simulated"], "files_changed": ["sim/%s.txt" % task_id], "commits": commits,
            "tests": [{"command": "true", "passed": True, "summary": "simulated"}], "benchmarks": [],
            "unresolved": [], "questions_for_astra": [],
        }
        return AgentResult(session_id=session_id, model="fake-claude", output=report, text=json.dumps(report))

    def _commit(self, cwd: Path, task_id: str, content: str) -> str:
        p = cwd / "sim" / ("%s.txt" % task_id)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content + "\n")
        _git(cwd, "add", "-A")
        _git(cwd, "commit", "-q", "-m", "[%s] simulated change" % task_id)
        return _git(cwd, "rev-parse", "HEAD")

    def probe(self, run_dir: Path) -> Dict[str, Any]:
        if self.probe_script:
            do = self.probe_script.pop(0)
            if do == "quota":
                raise QuotaExceeded("still limited", reset_at=time.time() + 1)
            if do == "quota_unknown":
                raise QuotaExceeded("still limited", reset_at=None)
        return {"ok": True, "checked_at": time.time()}


class FakeAstra:
    name = "astra"

    def __init__(self, plans: Optional[List[Dict[str, Any]]] = None, reviews: Optional[List[Any]] = None,
                 default_review: str = "APPROVED"):
        self.plans = list(plans or [])
        self.reviews = list(reviews or [])
        self.default_review = default_review
        self.calls: List[Dict[str, Any]] = []

    def run(self, *, kind: str, prompt: str, cwd: Path, run_dir: Path, schema: Any,
            should_stop: Callable[[], bool], **_: Any) -> AgentResult:
        self.calls.append({"kind": kind, "cwd": str(cwd)})
        run_dir.mkdir(parents=True, exist_ok=True)
        (run_dir / "prompt.md").write_text(prompt)
        sid = "fake-astra-%s" % uuid.uuid4().hex[:8]
        if kind in ("plan", "arch_review"):
            plan = self.plans.pop(0) if self.plans else {
                "assessment": "nothing left", "milestone": None, "tasks": [], "cancel_tasks": [],
                "memory_updates": empty_memory(), "needs_human": None, "release_candidate": False}
            if plan == "quota":
                raise QuotaExceeded("codex usage limit", reset_at=time.time() + 1, session_id=sid)
            return AgentResult(session_id=sid, model="fake-astra", output=plan)
        decision = self.reviews.pop(0) if self.reviews else self.default_review
        if decision == "quota":
            raise QuotaExceeded("codex usage limit", reset_at=time.time() + 1, session_id=sid)
        if decision == "fail":
            raise AgentFailed("simulated codex failure", session_id=sid)
        review = {
            "decision": decision, "rationale": "simulated %s" % decision,
            "criteria": [{"criterion": "simulated", "met": decision == "APPROVED", "evidence": "fake"}],
            "required_changes": [] if decision == "APPROVED" else ["simulated change request"],
            "architecture_notes": None, "memory_updates": empty_memory(),
            "progress_entry": "simulated progress" if decision == "APPROVED" else None,
            "human_question": "simulated question" if decision == "NEEDS_HUMAN" else None,
        }
        return AgentResult(session_id=sid, model="fake-astra", output=review)

    def probe(self, run_dir: Path) -> Dict[str, Any]:
        return {"ok": True, "checked_at": time.time()}


def empty_memory() -> Dict[str, Any]:
    return {"project": None, "architecture": None, "roadmap": None, "known_issues": None, "test_status": None}


def fake_plan(milestone_id: str, titles: List[str], depends: Optional[Dict[int, List[int]]] = None) -> Dict[str, Any]:
    depends = depends or {}
    tasks = []
    for i, title in enumerate(titles):
        tasks.append({
            "key": "k%d" % i, "title": title, "goal": "simulate " + title,
            "acceptance_criteria": ["file sim/<task>.txt exists"], "constraints": [], "context": "",
            "required_tests": ["true"], "priority": 10 + i,
            "depends_on": ["k%d" % d for d in depends.get(i, [])],
        })
    return {
        "assessment": "simulated plan",
        "milestone": {"id": milestone_id, "title": "Simulated milestone", "goal": "simulate",
                      "acceptance_criteria": ["all simulated tasks done"], "status": "active"},
        "tasks": tasks, "cancel_tasks": [], "memory_updates": empty_memory(), "needs_human": None,
        "release_candidate": False,
    }
