"""Independent verification of a task head. Nothing here trusts the developer's own report."""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

from . import gitops
from .adapters.base import agent_env
from .util import find_secrets, truncate


def _check(name: str, ok: bool, detail: str = "", hard: bool = True) -> Dict[str, Any]:
    return {"name": name, "ok": bool(ok), "detail": detail, "hard": hard}


def protected_hits(files: List[str], protected: List[str]) -> List[str]:
    hits = []
    for f in files:
        for p in protected:
            if (p.endswith("/") and f.startswith(p)) or f == p or (not p.endswith("/") and f.startswith(p)):
                hits.append(f)
                break
    return hits


def normalize(entry: Any) -> Dict[str, Any]:
    if isinstance(entry, dict):
        return {"name": entry.get("name") or " ".join(entry["cmd"]), "cmd": list(entry["cmd"]),
                "env": dict(entry.get("env") or {}), "unset": list(entry.get("unset") or [])}
    return {"name": " ".join(entry), "cmd": list(entry), "env": {}, "unset": []}


def run_commands(commands: List[Any], *, cwd: Path, env: Dict[str, str], log_dir: Path,
                 timeout_s: float) -> List[Dict[str, Any]]:
    results = []
    deadline = time.time() + timeout_s
    for i, entry in enumerate(commands):
        spec = normalize(entry)
        cmd = spec["cmd"]
        run_env = {k: v for k, v in env.items() if k not in spec["unset"]}
        run_env.update(spec["env"])
        log = log_dir / ("verify-%02d.log" % i)
        remaining = max(30.0, deadline - time.time())
        started = time.time()
        try:
            with open(log, "wb") as fh:
                proc = subprocess.run(cmd, cwd=str(cwd), env=run_env, stdout=fh, stderr=subprocess.STDOUT,
                                      timeout=remaining, start_new_session=True)
            rc: Optional[int] = proc.returncode
            timed_out = False
        except subprocess.TimeoutExpired:
            rc, timed_out = None, True
        text = log.read_text(errors="replace")
        results.append({
            "command": spec["name"], "argv": " ".join(cmd), "returncode": rc, "timed_out": timed_out,
            "seconds": round(time.time() - started, 1), "log": str(log),
            "tail": truncate(text[-6000:], 4000) if (rc != 0) else text[-600:],
        })
        if rc != 0:
            break
    return results


def verify_task(cfg: Any, *, wt: Path, base: str, head: str, previous_hash: Optional[str],
                log_dir: Path, run_commands_too: bool = True, tools_dir: Optional[Path] = None) -> Dict[str, Any]:
    log_dir.mkdir(parents=True, exist_ok=True)
    checks: List[Dict[str, Any]] = []
    dirty = gitops.status_short(wt)
    checks.append(_check("worktree clean (everything committed)", dirty == "", dirty[:2000]))
    commits = gitops.commits_between(wt, base, head)
    checks.append(_check("new commits on task branch", len(commits) > 0, "%d commit(s)" % len(commits)))
    files = gitops.changed_files(wt, base, head) if commits else []
    hits = protected_hits(files, cfg["protected_paths"])
    checks.append(_check("protected paths untouched", not hits, ", ".join(hits)))
    exts = tuple(cfg["game_data_extensions"])
    data_files = [f for f in files if f.lower().endswith(exts)]
    checks.append(_check("no original game data / binaries committed", not data_files, ", ".join(data_files)))
    sizes = gitops.file_sizes(wt, head, files)
    big = [f for f, s in sizes.items() if s > cfg["verify"]["max_file_bytes"]]
    checks.append(_check("no oversized files", not big, ", ".join(big)))
    diff = gitops.diff_text(wt, base, head) if commits else ""
    added = "\n".join(l[1:] for l in diff.splitlines() if l.startswith("+") and not l.startswith("+++"))
    secrets = find_secrets(added)
    checks.append(_check("no secrets in added lines", not secrets, ", ".join(secrets)))
    dhash = gitops.diff_hash(wt, base, head) if commits else ""
    repeated = bool(previous_hash) and dhash == previous_hash
    checks.append(_check("diff differs from previous attempt", not repeated, "identical diff" if repeated else "",
                         hard=False))
    commands: List[Dict[str, Any]] = []
    structural_ok = all(c["ok"] for c in checks if c["hard"])
    if run_commands_too and structural_ok:
        env = agent_env(cfg.verify_env())
        env["GTA20_ORCHESTRATOR_VERIFY"] = "1"
        # Verification runs agent-written code outside the sandbox: it must never see the
        # owner's AWS session (`aws login` lives in ~/.aws).
        env.update({"AWS_CONFIG_FILE": "/dev/null", "AWS_SHARED_CREDENTIALS_FILE": "/dev/null",
                    "AWS_EC2_METADATA_DISABLED": "true"})
        if tools_dir:
            env["PATH"] = str(tools_dir) + ":" + env["PATH"]
        commands = run_commands(cfg["verify"]["commands"], cwd=wt, env=env, log_dir=log_dir,
                                timeout_s=float(cfg["verify"]["timeout_s"]))
        for c in commands:
            checks.append(_check("command: " + c["command"], c["returncode"] == 0,
                                 "timeout" if c["timed_out"] else ("exit %s" % c["returncode"])))
    passed = all(c["ok"] for c in checks if c["hard"])
    return {"passed": passed, "checks": checks, "commands": commands, "diff_hash": dhash,
            "repeated": repeated, "files": files, "commits": commits,
            "stat": gitops.diff_stat(wt, base, head) if commits else ""}


def summarize(result: Dict[str, Any], limit: int = 6000) -> str:
    lines = []
    for c in result["checks"]:
        lines.append("- [%s] %s%s" % ("PASS" if c["ok"] else ("FAIL" if c["hard"] else "WARN"), c["name"],
                                      (": " + c["detail"]) if c["detail"] else ""))
    for c in result.get("commands", []):
        if c["returncode"] != 0:
            lines.append("\nOutput of failing command `%s`:\n```\n%s\n```" % (c["command"], c["tail"]))
    text = "\n".join(lines)
    return truncate(text, limit)


def env_ok(path: str) -> bool:
    return os.path.isdir(os.path.expanduser(path))
