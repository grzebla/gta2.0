"""Claude Code (developer) through the official non-interactive CLI: `claude -p`.

- stream-json output: init (model, session), rate_limit_event (5-hour / weekly windows), result.
- The session id is chosen up front (--session-id) so the checkpoint knows it before the run.
- Least privilege: --permission-mode dontAsk + --permission-prompts none (anything not allowed
  is denied, nobody is asked), user settings/hooks/MCP servers excluded, a generated settings
  file with explicit allow/deny rules and the Bash sandbox. Never --dangerously-skip-permissions.
"""

from __future__ import annotations

import json
import re
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from .base import (AgentFailed, AgentInterrupted, AgentResult, AgentTimeout, ModelMismatch,
                   QuotaExceeded, agent_env, run_process)

LIMIT_TEXT = re.compile(
    r"(usage limit|hit your (?:session |weekly |opus |sonnet |fable )?limit|limit reached|"
    r"out of (?:usage|extra usage)|rate[_ ]limit(?:ed)?\b)",
    re.I,
)
NOT_QUOTA_TEXT = re.compile(r"temporarily limiting requests|overloaded", re.I)
EPOCH_IN_TEXT = re.compile(r"\|(\d{10})\b")


def parse_stream(lines: List[str]) -> Dict[str, Any]:
    info: Dict[str, Any] = {"init": None, "result": None, "rate_limits": {}, "errors": []}
    for line in lines:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            obj = json.loads(line)
        except ValueError:
            continue
        t = obj.get("type")
        if t == "system" and obj.get("subtype") == "init":
            info["init"] = obj
        elif t == "rate_limit_event":
            rl = obj.get("rate_limit_info") or {}
            info["rate_limits"][rl.get("rateLimitType") or "unknown"] = rl
            info["rate_limits"]["_last"] = rl
        elif t == "result":
            info["result"] = obj
        elif t == "assistant":
            err = (obj.get("error") or (obj.get("message") or {}).get("error"))
            if err:
                info["errors"].append(err)
    return info


def quota_from_stream(info: Dict[str, Any]) -> Optional[QuotaExceeded]:
    """Recognise a usage-limit stop. The format is not formally documented: parse defensively."""
    result = info.get("result") or {}
    text = str(result.get("result") or "")
    last = info["rate_limits"].get("_last") or {}
    rejected = [rl for k, rl in info["rate_limits"].items() if k != "_last" and rl.get("status") == "rejected"]
    is_error = bool(result.get("is_error"))
    status = result.get("api_error_status")
    errors = " ".join(str(e) for e in info.get("errors", []))
    textual = is_error and LIMIT_TEXT.search(text + " " + errors) and not NOT_QUOTA_TEXT.search(text)
    if not (rejected or textual or (is_error and status == 429) or "rate_limit" in errors):
        return None
    if not is_error and rejected and result.get("subtype") == "success":
        # Finished the turn although a window was marked rejected (e.g. overage); not a stop.
        return None
    chosen = rejected[0] if rejected else last
    reset_at = chosen.get("resetsAt")
    if reset_at is None:
        m = EPOCH_IN_TEXT.search(text)
        reset_at = float(m.group(1)) if m else None
    return QuotaExceeded(
        text or "Claude usage limit reached",
        reset_at=float(reset_at) if reset_at else None,
        limit_type=chosen.get("rateLimitType"),
        session_id=result.get("session_id"),
        detail={"rate_limits": info["rate_limits"], "api_error_status": status},
    )


def build_settings(*, worktree: Path, protected: List[str], extra_write_dirs: List[str], sandbox: bool,
                   domains: List[str], allow_web: bool) -> Dict[str, Any]:
    wt = str(worktree)
    deny = [
        "Bash(git push *)", "Bash(git push)", "Bash(git reset --hard *)", "Bash(git checkout *)",
        "Bash(git switch *)", "Bash(git rebase *)", "Bash(git merge *)", "Bash(git worktree *)",
        "Bash(git remote *)", "Bash(git config *)", "Bash(git filter-branch *)", "Bash(git branch -D *)",
        "Bash(gh *)", "Bash(aws *)", "Bash(sudo *)", "Bash(curl *)", "Bash(wget *)", "Bash(ssh *)",
        "Bash(scp *)", "Bash(launchctl *)", "Bash(pmset *)", "Bash(defaults write *)",
        "Read(~/.aws/**)", "Read(~/.ssh/**)", "Read(~/.config/gh/**)", "Read(~/.codex/auth.json)",
        "Read(~/.claude/.credentials.json)",
    ]
    for p in protected:
        pattern = "//" + wt.lstrip("/") + "/" + p + ("**" if p.endswith("/") else "*")
        deny += ["Edit(%s)" % pattern, "Write(%s)" % pattern]
    if not allow_web:
        deny += ["WebFetch", "WebSearch"]
    allow = [
        "Read", "Glob", "Grep", "Edit", "Write", "TodoWrite", "Task",
        "Bash(git status*)", "Bash(git diff*)", "Bash(git log*)", "Bash(git show*)",
        "Bash(git add *)", "Bash(git commit *)", "Bash(git restore *)", "Bash(git mv *)",
        "Bash(git rm *)", "Bash(git rev-parse *)", "Bash(git ls-files*)", "Bash(git grep *)",
        "Bash(git stash *)", "Bash(cmake *)", "Bash(ctest *)", "Bash(ninja *)",
        "Bash(./build/*)", "Bash(build/*)", "Bash(uvx *)", "Bash(python3 *)", "Bash(bash scripts/ci/*)",
    ]
    settings: Dict[str, Any] = {"permissions": {"allow": allow, "deny": deny}}
    if sandbox:
        settings["sandbox"] = {
            "enabled": True,
            "autoAllowBashIfSandboxed": True,
            "allowUnsandboxedCommands": False,
            "filesystem": {"allowWrite": extra_write_dirs},
            "network": {"allowedDomains": domains},
        }
    return settings


class ClaudeAgent:
    name = "claude"

    def __init__(self, cfg: Dict[str, Any]):
        self.cfg = cfg

    def _base_cmd(self) -> List[str]:
        return [
            self.cfg.get("command", "claude"), "-p",
            "--output-format", "stream-json", "--verbose",
            "--permission-mode", "dontAsk", "--permission-prompts", "none",
            "--setting-sources", "project",
            "--strict-mcp-config", "--mcp-config", '{"mcpServers":{}}',
            "--no-chrome",
        ]

    def check_model(self, model: Optional[str]) -> None:
        wanted = str(self.cfg.get("model", ""))
        if not model:
            raise ModelMismatch("Claude did not report its model")
        if wanted and wanted not in model and model != wanted:
            raise ModelMismatch("Claude ran %s, expected %s" % (model, wanted))

    def run(self, *, kind: str, prompt: str, cwd: Path, run_dir: Path, schema: Optional[Dict[str, Any]],
            session_id: str, resume: bool, settings: Dict[str, Any], add_dirs: List[str],
            should_stop: Callable[[], bool], on_start: Callable[[int], None] = lambda pid: None,
            timeout_s: Optional[float] = None, env_extra: Optional[Dict[str, str]] = None) -> AgentResult:
        run_dir.mkdir(parents=True, exist_ok=True)
        settings_path = run_dir / "claude-settings.json"
        settings_path.write_text(json.dumps(settings, indent=2))
        cmd = self._base_cmd() + ["--model", self.cfg["model"], "--settings", str(settings_path)]
        if self.cfg.get("effort"):
            cmd += ["--effort", self.cfg["effort"]]
        for d in add_dirs:
            cmd += ["--add-dir", d]
        if schema is not None:
            cmd += ["--json-schema", json.dumps(schema)]
        cmd += ["--resume", session_id] if resume else ["--session-id", session_id]
        (run_dir / "command.json").write_text(json.dumps(cmd, indent=1))
        (run_dir / "prompt.md").write_text(prompt)
        outcome = run_process(
            cmd, cwd=cwd, env=agent_env(env_extra), stdin_text=prompt,
            stdout_path=run_dir / "stdout.jsonl", stderr_path=run_dir / "stderr.txt",
            timeout_s=timeout_s or float(self.cfg.get("timeout_s", 5400)),
            should_stop=should_stop, on_start=on_start,
        )
        lines = (run_dir / "stdout.jsonl").read_text(errors="replace").splitlines()
        info = parse_stream(lines)
        init = info.get("init") or {}
        result = info.get("result") or {}
        sid = result.get("session_id") or init.get("session_id") or session_id
        quota = quota_from_stream(info)
        if quota:
            quota.session_id = sid
            raise quota
        if outcome.interrupted:
            raise AgentInterrupted("Claude run interrupted by orchestrator stop", session_id=sid)
        if outcome.timed_out:
            raise AgentTimeout("Claude run exceeded %ss" % self.cfg.get("timeout_s"), session_id=sid)
        if not result:
            stderr = (run_dir / "stderr.txt").read_text(errors="replace")[-2000:]
            raise AgentFailed("Claude exited %s without a result: %s" % (outcome.returncode, stderr),
                              session_id=sid)
        if result.get("is_error"):
            raise AgentFailed("Claude error (%s): %s" % (result.get("subtype"), str(result.get("result"))[:2000]),
                              session_id=sid)
        model = init.get("model")
        model_usage = result.get("modelUsage") or {}
        if model_usage:
            # The model that did most of the output work (subagents may use smaller models).
            model = max(model_usage.items(), key=lambda kv: kv[1].get("outputTokens", 0))[0]
        self.check_model(init.get("model"))
        output = result.get("structured_output")
        if schema is not None and output is None:
            try:
                output = json.loads(result.get("result") or "")
            except ValueError:
                raise AgentFailed("Claude returned no structured report", session_id=sid)
        return AgentResult(
            session_id=sid, model=model, output=output, text=str(result.get("result") or ""),
            usage={"usage": result.get("usage"), "modelUsage": model_usage,
                   "duration_ms": result.get("duration_ms"), "num_turns": result.get("num_turns"),
                   "permission_denials": result.get("permission_denials")},
            rate_limits={k: v for k, v in info["rate_limits"].items() if k != "_last"},
        )

    def probe(self, run_dir: Path) -> Dict[str, Any]:
        """Cheapest real call: tells whether the subscription currently accepts requests."""
        run_dir.mkdir(parents=True, exist_ok=True)
        cmd = [self.cfg.get("command", "claude"), "-p", "--output-format", "stream-json", "--verbose",
               "--model", self.cfg.get("probe_model", "haiku"), "--permission-mode", "dontAsk",
               "--setting-sources", "project", "--strict-mcp-config", "--mcp-config", '{"mcpServers":{}}',
               "--no-session-persistence", "--tools", ""]
        outcome = run_process(cmd, cwd=run_dir, env=agent_env(), stdin_text="Reply with: OK",
                              stdout_path=run_dir / "stdout.jsonl", stderr_path=run_dir / "stderr.txt",
                              timeout_s=180)
        info = parse_stream((run_dir / "stdout.jsonl").read_text(errors="replace").splitlines())
        quota = quota_from_stream(info)
        if quota:
            raise quota
        result = info.get("result") or {}
        if outcome.returncode != 0 or result.get("is_error") or not result:
            raise AgentFailed("Claude probe failed (exit %s): %s" % (
                outcome.returncode, (run_dir / "stderr.txt").read_text(errors="replace")[-500:]))
        return {"ok": True, "checked_at": time.time(),
                "rate_limits": {k: v for k, v in info["rate_limits"].items() if k != "_last"},
                "api_key_source": (info.get("init") or {}).get("apiKeySource")}
