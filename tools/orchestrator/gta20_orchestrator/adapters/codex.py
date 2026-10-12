"""Astra (supervisor) through the official non-interactive Codex CLI: `codex exec`.

- --ignore-user-config: the user's default (danger-full-access, approvals never) must not apply.
  Astra gets a read-only sandbox and never asks for approvals.
- --output-schema: the final message is JSON matching the plan/review schema.
- The model actually used is verified from the session rollout file (turn_context.model);
  the JSONL stream does not carry it. Usage limits come from the rollout's rate_limits.
"""

from __future__ import annotations

import glob
import json
import os
import re
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from .base import (AgentFailed, AgentInterrupted, AgentResult, AgentTimeout, ModelMismatch,
                   QuotaExceeded, agent_env, run_process)

LIMIT_TEXT = re.compile(r"(usage limit|rate limit|too many requests|quota|\b429\b|hit your limit)", re.I)


def parse_jsonl(lines: List[str]) -> Dict[str, Any]:
    info: Dict[str, Any] = {"thread_id": None, "messages": [], "usage": None, "errors": [], "failed": False}
    for line in lines:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            obj = json.loads(line)
        except ValueError:
            continue
        t = obj.get("type")
        if t == "thread.started":
            info["thread_id"] = obj.get("thread_id")
        elif t == "item.completed":
            item = obj.get("item") or {}
            if item.get("type") == "agent_message":
                info["messages"].append(item.get("text") or "")
            elif item.get("type") == "error":
                info["errors"].append(str(item.get("message")))
        elif t == "turn.completed":
            info["usage"] = obj.get("usage")
        elif t == "turn.failed":
            info["failed"] = True
            info["errors"].append(str((obj.get("error") or {}).get("message")))
        elif t == "error":
            info["errors"].append(str(obj.get("message")))
    return info


def find_rollout(codex_home: Path, thread_id: str) -> Optional[Path]:
    hits = glob.glob(str(codex_home / "sessions" / "*" / "*" / "*" / ("rollout-*-%s.jsonl" % thread_id)))
    return Path(sorted(hits)[-1]) if hits else None


def read_rollout(path: Path) -> Dict[str, Any]:
    models, rate_limits = [], None
    for line in path.read_text(errors="replace").splitlines():
        try:
            obj = json.loads(line)
        except ValueError:
            continue
        payload = obj.get("payload") if isinstance(obj.get("payload"), dict) else {}
        if obj.get("type") == "turn_context" and payload.get("model"):
            models.append(payload["model"])
        if payload.get("type") == "token_count" and payload.get("rate_limits"):
            rate_limits = payload["rate_limits"]
    return {"models": models, "rate_limits": rate_limits}


def reset_from_rate_limits(rl: Optional[Dict[str, Any]]) -> Optional[float]:
    if not rl:
        return None
    resets = []
    for window in ("primary", "secondary"):
        w = rl.get(window) or {}
        if w.get("resets_at") and float(w.get("used_percent") or 0) >= 99.5:
            resets.append(float(w["resets_at"]))
    return max(resets) if resets else None


class CodexAgent:
    name = "astra"

    def __init__(self, cfg: Dict[str, Any]):
        self.cfg = cfg
        self.codex_home = Path(os.path.expanduser(cfg.get("codex_home", "~/.codex")))

    def _cmd(self, cwd: Path, schema_path: Optional[Path], last_path: Path, model: str) -> List[str]:
        cmd = [
            self.cfg.get("command", "codex"), "exec",
            "--ignore-user-config",
            "--sandbox", "read-only",
            "-c", 'approval_policy="never"',
            "-c", 'model_reasoning_effort="%s"' % self.cfg.get("reasoning_effort", "high"),
            "-m", model,
            "--json", "--color", "never", "--skip-git-repo-check",
            "-C", str(cwd),
            "-o", str(last_path),
        ]
        if schema_path is not None:
            cmd += ["--output-schema", str(schema_path)]
        return cmd

    def run(self, *, kind: str, prompt: str, cwd: Path, run_dir: Path, schema: Optional[Dict[str, Any]],
            should_stop: Callable[[], bool], on_start: Callable[[int], None] = lambda pid: None,
            timeout_s: Optional[float] = None, **_: Any) -> AgentResult:
        run_dir.mkdir(parents=True, exist_ok=True)
        schema_path = None
        if schema is not None:
            schema_path = run_dir / "schema.json"
            schema_path.write_text(json.dumps(schema, indent=1))
        last_path = run_dir / "last-message.json"
        model = self.cfg["model"]
        cmd = self._cmd(cwd, schema_path, last_path, model)
        (run_dir / "command.json").write_text(json.dumps(cmd, indent=1))
        (run_dir / "prompt.md").write_text(prompt)
        outcome = run_process(
            cmd, cwd=cwd, env=agent_env({"CODEX_HOME": str(self.codex_home)}), stdin_text=prompt,
            stdout_path=run_dir / "stdout.jsonl", stderr_path=run_dir / "stderr.txt",
            timeout_s=timeout_s or float(self.cfg.get("timeout_s", 1800)),
            should_stop=should_stop, on_start=on_start,
        )
        info = parse_jsonl((run_dir / "stdout.jsonl").read_text(errors="replace").splitlines())
        tid = info["thread_id"]
        rollout = find_rollout(self.codex_home, tid) if tid else None
        ro = read_rollout(rollout) if rollout else {"models": [], "rate_limits": None}
        errors = " ".join(info["errors"])
        if (info["failed"] or outcome.returncode not in (0, None)) and LIMIT_TEXT.search(errors):
            raise QuotaExceeded("Codex usage limit: " + errors[:500], reset_at=reset_from_rate_limits(ro["rate_limits"]),
                                limit_type="codex", session_id=tid, detail={"rate_limits": ro["rate_limits"]})
        if outcome.interrupted:
            raise AgentInterrupted("%s run interrupted by orchestrator stop" % self.name, session_id=tid)
        if outcome.timed_out:
            raise AgentTimeout("%s run exceeded timeout" % self.name, session_id=tid)
        if info["failed"] or outcome.returncode != 0:
            stderr = (run_dir / "stderr.txt").read_text(errors="replace")[-1500:]
            raise AgentFailed("Codex failed (exit %s): %s %s" % (outcome.returncode, errors[:1500], stderr),
                              session_id=tid)
        if not ro["models"]:
            raise ModelMismatch("cannot verify Astra's model: no rollout for thread %s" % tid, session_id=tid)
        wrong = [m for m in ro["models"] if m != model]
        if wrong:
            raise ModelMismatch("Codex ran %s, expected %s" % (wrong, model), session_id=tid)
        text = last_path.read_text(errors="replace") if last_path.exists() else (info["messages"] or [""])[-1]
        output: Any = None
        if schema is not None:
            try:
                output = json.loads(text)
            except ValueError:
                raise AgentFailed("%s returned non-JSON output: %s" % (self.name, text[:500]), session_id=tid)
        return AgentResult(session_id=tid, model=ro["models"][-1], output=output, text=text,
                           usage=info["usage"] or {}, rate_limits=ro["rate_limits"] or {})

    def probe(self, run_dir: Path) -> Dict[str, Any]:
        run_dir.mkdir(parents=True, exist_ok=True)
        res = self.run(kind="probe", prompt="Reply with exactly: ASTRA_OK", cwd=run_dir, run_dir=run_dir,
                       schema=None, should_stop=lambda: False, timeout_s=300)
        if "ASTRA_OK" not in res.text:
            raise AgentFailed("unexpected probe answer: %r" % res.text[:200])
        return {"ok": True, "checked_at": time.time(), "model": res.model, "rate_limits": res.rate_limits}
