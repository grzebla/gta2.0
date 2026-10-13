"""Agent-independent process handling and result/error types."""

from __future__ import annotations

import os
import signal
import subprocess
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

# Environment passed to agents: an allowlist, so API keys or session variables of whatever
# launched the orchestrator never leak into (or silently switch) an agent's billing/auth.
ENV_ALLOWLIST = ("HOME", "USER", "LOGNAME", "SHELL", "TMPDIR", "LANG", "LC_ALL", "LC_CTYPE", "PATH")
ENV_DENYLIST_PREFIXES = ("ANTHROPIC_", "OPENAI_", "CLAUDE_CODE_", "CLAUDECODE", "AWS_", "GH_", "GITHUB_")
DEFAULT_PATH = "/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"


class AgentError(Exception):
    def __init__(self, message: str, *, session_id: Optional[str] = None, detail: Any = None):
        super().__init__(message)
        self.session_id = session_id
        self.detail = detail


class QuotaExceeded(AgentError):
    """Subscription usage limit reached. reset_at (epoch seconds) when the service said so."""

    def __init__(self, message: str, *, reset_at: Optional[float] = None, limit_type: Optional[str] = None,
                 session_id: Optional[str] = None, detail: Any = None):
        super().__init__(message, session_id=session_id, detail=detail)
        self.reset_at = reset_at
        self.limit_type = limit_type


class AgentTimeout(AgentError):
    pass


class AgentInterrupted(AgentError):
    pass


class AgentFailed(AgentError):
    pass


class ModelMismatch(AgentError):
    pass


@dataclass
class AgentResult:
    session_id: Optional[str]
    model: Optional[str]
    output: Any  # structured output (dict) when a schema was requested
    text: str = ""
    usage: Dict[str, Any] = field(default_factory=dict)
    rate_limits: Dict[str, Any] = field(default_factory=dict)


@dataclass
class ProcessOutcome:
    returncode: Optional[int]
    timed_out: bool
    interrupted: bool
    duration_s: float


def agent_env(extra: Optional[Dict[str, str]] = None) -> Dict[str, str]:
    env = {k: os.environ[k] for k in ENV_ALLOWLIST if k in os.environ}
    home = env.get("HOME", os.path.expanduser("~"))
    path = env.get("PATH", DEFAULT_PATH)
    for p in (os.path.join(home, ".local/bin"), "/opt/homebrew/bin", "/usr/local/bin"):
        if p not in path.split(":"):
            path = p + ":" + path
    env["PATH"] = path
    env["HOME"] = home
    env.setdefault("LANG", "en_US.UTF-8")
    env["NO_COLOR"] = "1"
    env["_ZO_DOCTOR"] = "0"
    for k, v in (extra or {}).items():
        if not k.startswith(ENV_DENYLIST_PREFIXES):
            env[k] = v
    if env.get("GTA20_TOOLS_PATH"):
        env["PATH"] = env["GTA20_TOOLS_PATH"] + ":" + env["PATH"]
    return env


def _signal_group(proc: subprocess.Popen, sig: int) -> None:
    try:
        os.killpg(proc.pid, sig)
    except (ProcessLookupError, PermissionError):
        pass


def stop_gracefully(proc: subprocess.Popen, grace_int_s: float = 20, grace_term_s: float = 10) -> None:
    """SIGINT lets the agent close its turn cleanly, then SIGTERM, then SIGKILL."""
    for sig, grace in ((signal.SIGINT, grace_int_s), (signal.SIGTERM, grace_term_s)):
        if proc.poll() is not None:
            return
        _signal_group(proc, sig)
        deadline = time.time() + grace
        while time.time() < deadline and proc.poll() is None:
            time.sleep(0.2)
    if proc.poll() is None:
        _signal_group(proc, signal.SIGKILL)
        proc.wait()


def run_process(
    cmd: List[str],
    *,
    cwd: Path,
    env: Dict[str, str],
    stdin_text: Optional[str],
    stdout_path: Path,
    stderr_path: Path,
    timeout_s: float,
    should_stop: Callable[[], bool] = lambda: False,
    on_start: Callable[[int], None] = lambda pid: None,
    nice: int = 5,
) -> ProcessOutcome:
    stdout_path.parent.mkdir(parents=True, exist_ok=True)
    started = time.time()
    with open(stdout_path, "wb") as out, open(stderr_path, "wb") as err:
        proc = subprocess.Popen(
            cmd,
            cwd=str(cwd),
            env=env,
            stdin=subprocess.PIPE if stdin_text is not None else subprocess.DEVNULL,
            stdout=out,
            stderr=err,
            start_new_session=True,  # own process group: we can stop the whole tree
            preexec_fn=(lambda: os.nice(nice)) if nice else None,
        )
        on_start(proc.pid)
        if stdin_text is not None:
            try:
                assert proc.stdin is not None
                proc.stdin.write(stdin_text.encode("utf-8"))
                proc.stdin.close()
            except BrokenPipeError:
                pass
        timed_out = interrupted = False
        deadline = started + timeout_s
        while proc.poll() is None:
            if time.time() > deadline:
                timed_out = True
                stop_gracefully(proc)
                break
            if should_stop():
                interrupted = True
                stop_gracefully(proc)
                break
            time.sleep(0.5)
        proc.wait()
    return ProcessOutcome(proc.returncode, timed_out, interrupted, time.time() - started)
