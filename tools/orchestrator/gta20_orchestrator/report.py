"""Status snapshot and the human-readable progress report (Markdown + HTML)."""

from __future__ import annotations

import html
import os
from pathlib import Path
from typing import Any, Dict, List

from . import db as D
from .util import loads, local_time


def pid_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def daemon_pid(state_dir: Path) -> int:
    try:
        pid = int((state_dir / "orchestrator.pid").read_text().strip())
    except (OSError, ValueError):
        return 0
    return pid if pid_alive(pid) else 0


def snapshot(database: D.Database) -> Dict[str, Any]:
    m = database.active_milestone()
    current = database.get_meta("current_task")
    cur = database.task(current) if current else None
    last_dec = database.decisions(1)
    quota = {}
    for name in ("astra", "claude", "gatekeeper"):
        row = database.quota(name)
        if row:
            detail = loads(row["detail"], {}) or {}
            quota[name] = {"state": row["state"], "reset_at": local_time(row["reset_at"]) if row["reset_at"] else "-",
                           "probes": row["probes"], "rate_limits": detail.get("rate_limits")}
    next_at = database.get_meta("next_attempt_at") or ""
    counts = {s: database.count_tasks(s) for s in (D.READY, D.IN_PROGRESS, D.CHANGES_REQUESTED, D.VERIFYING,
                                                      D.IN_REVIEW, D.DONE, D.BLOCKED, D.NEEDS_HUMAN, D.CANCELLED)}
    return {
        "desired_state": database.get_meta("desired_state", "STOPPED"),
        "loop_state": database.get_meta("loop_state", "NEVER_STARTED"),
        "loop_reason": database.get_meta("loop_reason", ""),
        "milestone": "%s — %s" % (m["id"], m["title"]) if m else "-",
        "current_task": "%s [%s] %s" % (cur["id"], cur["status"], cur["title"]) if cur else "-",
        "active_agent": database.get_meta("active_agent") or "-",
        "last_decision": ("%s %s: %s" % (last_dec[0]["task_id"] or "project", last_dec[0]["decision"],
                                         (last_dec[0]["rationale"] or "")[:200])) if last_dec else "-",
        "ci_consecutive_failures": database.get_meta("ci_consecutive_failures", "unknown"),
        "counts": counts,
        "last_activity": database.get_meta("last_activity", "-"),
        "last_step": database.get_meta("last_step", "-"),
        "quota": quota,
        "next_attempt_at": local_time(float(next_at)) if next_at else "-",
        "human_question": database.get_meta("human_question") or "",
        "release_candidate": database.get_meta("release_candidate") or "",
        "last_merge": next(("%s %s" % (e["ts"], e["outcome"]) for e in database.events(200) if e["type"] == "pr_merged"), "-"),
    }


def format_status(s: Dict[str, Any], pid: int) -> str:
    c = s["counts"]
    lines = [
        "GTA2.0 orchestrator",
        "  daemon:            %s" % ("running (pid %d)" % pid if pid else "not running"),
        "  desired / loop:    %s / %s %s" % (s["desired_state"], s["loop_state"],
                                             ("(" + s["loop_reason"] + ")") if s["loop_reason"] else ""),
        "  milestone:         %s" % s["milestone"],
        "  current task:      %s" % s["current_task"],
        "  active agent:      %s" % s["active_agent"],
        "  last decision:     %s" % s["last_decision"],
        "  CI (integration):  consecutive failures = %s" % s["ci_consecutive_failures"],
        "  tasks:             done %d, ready %d, active %d, blocked %d, needs-human %d, cancelled %d" % (
            c[D.DONE], c[D.READY], c[D.IN_PROGRESS] + c[D.CHANGES_REQUESTED] + c[D.VERIFYING] + c[D.IN_REVIEW],
            c[D.BLOCKED], c[D.NEEDS_HUMAN], c[D.CANCELLED]),
        "  last activity:     %s  (%s)" % (s["last_activity"], s["last_step"]),
        "  next attempt:      %s" % s["next_attempt_at"],
        "  last merge:        %s" % s.get("last_merge", "-"),
    ]
    for name, q in s["quota"].items():
        rl = q.get("rate_limits") or {}
        windows = []
        for k, v in rl.items():
            if isinstance(v, dict):
                if "utilization" in v or "status" in v:
                    windows.append("%s:%s" % (k, v.get("status", "")))
                if "unifiedWindows" in v:
                    for wn, wv in (v.get("unifiedWindows") or {}).items():
                        windows.append("%s %.0f%%" % (wn, 100 * float(wv.get("utilization") or 0)))
                if k in ("primary", "secondary") and v.get("used_percent") is not None:
                    windows.append("%s %.0f%% (window %s min)" % (k, float(v["used_percent"]), v.get("window_minutes")))
        lines.append("  quota %-7s     %s, reset %s %s" % (name + ":", q["state"], q["reset_at"],
                                                            ("[" + ", ".join(windows) + "]") if windows else ""))
    if s["human_question"]:
        lines.append("  NEEDS YOUR DECISION: %s" % s["human_question"])
    if s["release_candidate"]:
        lines.append("  Astra marked RELEASE_CANDIDATE at %s (only you can declare completion)" % s["release_candidate"])
    return "\n".join(lines)


def markdown(database: D.Database, pid: int) -> str:
    s = snapshot(database)
    out: List[str] = ["# GTA2.0 — raport Orchestratora", "", "```", format_status(s, pid), "```", "", "## Zadania", "",
                      "| ID | Status | Prób | Tytuł | Uwagi |", "|---|---|---|---|---|"]
    for t in database.tasks():
        out.append("| %s | %s | %s | %s | %s |" % (t["id"], t["status"], t["attempts"], t["title"].replace("|", "/"),
                                                  (t["blocked_reason"] or "").replace("|", "/").replace("\n", " ")[:160]))
    out += ["", "## Ostatnie decyzje Astry", ""]
    for d in database.decisions(15):
        out.append("- %s **%s** %s — %s" % (d["created_at"], d["task_id"] or "projekt", d["decision"],
                                             (d["rationale"] or "").replace("\n", " ")[:400]))
    out += ["", "## Ostatnie zdarzenia", ""]
    for e in database.events(25):
        out.append("- %s %s %s %s %s" % (e["ts"], e["task_id"] or "", e["agent"] or "", e["type"], e["outcome"] or ""))
    return "\n".join(out) + "\n"


def html_page(md: str) -> str:
    body = html.escape(md)
    return """<!doctype html><html lang="pl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1"><title>GTA2.0 Orchestrator</title>
<style>:root{--bg:#fff;--fg:#1d1d1f}@media (prefers-color-scheme:dark){:root{--bg:#141414;--fg:#e8e8e8}}
body{background:var(--bg);color:var(--fg);font:14px/1.5 ui-monospace,Menlo,monospace;margin:16px;white-space:pre-wrap}</style>
</head><body>%s</body></html>""" % body


def write(database: D.Database, state_dir: Path) -> Path:
    md = markdown(database, daemon_pid(state_dir))
    (state_dir / "report.md").write_text(md)
    (state_dir / "report.html").write_text(html_page(md))
    return state_dir / "report.md"
