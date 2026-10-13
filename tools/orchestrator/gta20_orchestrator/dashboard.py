"""`gta20-orchestrator status`: a compact, coloured dashboard for the terminal.

Colours only on a TTY and never with NO_COLOR set; `--plain` gives the old line format."""

from __future__ import annotations

import datetime as _dt
import os
import shutil
import sys
from typing import Any, Dict, List, Optional, Sequence, Tuple

from . import db as D
from .report import snapshot
from .util import iso, loads, now

RESET = "\033[0m"
STYLES = {"bold": "1", "dim": "2", "red": "31", "green": "32", "yellow": "33", "blue": "34", "magenta": "35",
          "cyan": "36", "grey": "90"}

STATUS_STYLE = {
    D.IN_PROGRESS: ("yellow", "▶"), D.CHANGES_REQUESTED: ("yellow", "↻"), D.VERIFYING: ("cyan", "⧗"),
    D.IN_REVIEW: ("magenta", "◎"), D.READY: ("blue", "○"), D.DONE: ("green", "✓"), D.BLOCKED: ("red", "✗"),
    D.NEEDS_HUMAN: ("red", "?"), D.CANCELLED: ("grey", "–"),
}
LOOP_STYLE = {"RUNNING": "green", "PAUSED": "yellow", "WAITING": "yellow", "WAITING_FOR_QUOTA": "yellow",
              "HALTED": "red", "STOPPED": "grey", "NEVER_STARTED": "grey"}


class Paint:
    def __init__(self, enabled: bool):
        self.enabled = enabled

    def __call__(self, text: str, *styles: str) -> str:
        if not self.enabled or not styles:
            return text
        return "\033[%sm%s%s" % (";".join(STYLES[s] for s in styles), text, RESET)


def use_colour(stream: Any = None) -> bool:
    stream = stream or sys.stdout
    return bool(getattr(stream, "isatty", lambda: False)()) and not os.environ.get("NO_COLOR")


def visible_len(text: str) -> int:
    out, i = 0, 0
    while i < len(text):
        if text[i] == "\033":
            i = text.index("m", i) + 1
            continue
        out += 1
        i += 1
    return out


def clip(text: str, width: int) -> str:
    text = " ".join(text.split())
    return text if len(text) <= width else text[:max(0, width - 1)] + "…"


def pad(text: str, width: int) -> str:
    return text + " " * max(0, width - visible_len(text))


def ago(ts: Optional[float]) -> str:
    if not ts:
        return "-"
    secs = int(now() - ts)
    when = _dt.datetime.fromtimestamp(ts).strftime("%H:%M")
    if secs < 0:
        mins = -secs // 60
        return "%s (za %s)" % (when, "%d min" % mins if mins < 120 else "%d h" % (mins // 60))
    if secs < 90:
        return "%s (przed chwilą)" % when
    if secs < 7200:
        return "%s (%d min temu)" % (when, secs // 60)
    if secs < 86400 * 2:
        return "%s (%d h temu)" % (when, secs // 3600)
    return _dt.datetime.fromtimestamp(ts).strftime("%Y-%m-%d %H:%M")


def parse_iso(text: Optional[str]) -> Optional[float]:
    if not text or text == "-":
        return None
    try:
        return _dt.datetime.strptime(text, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=_dt.timezone.utc).timestamp()
    except ValueError:
        return None


def bar(pct: float, paint: Paint, width: int = 12) -> str:
    pct = max(0.0, min(100.0, pct))
    full = int(round(pct / 100 * width))
    colour = "green" if pct < 60 else ("yellow" if pct < 85 else "red")
    return paint("█" * full, colour) + paint("░" * (width - full), "grey") + " %3.0f%%" % pct


def quota_windows(rate_limits: Dict[str, Any]) -> List[Tuple[str, float]]:
    out: List[Tuple[str, float]] = []
    names = {"five_hour": "5 h", "seven_day": "7 dni", "seven_day_opus": "7 dni opus"}
    for key, v in (rate_limits or {}).items():
        if not isinstance(v, dict):
            continue
        for wn, wv in (v.get("unifiedWindows") or {}).items():
            out.append((names.get(wn, wn), 100 * float(wv.get("utilization") or 0)))
        if key in ("primary", "secondary") and v.get("used_percent") is not None:
            mins = int(v.get("window_minutes") or 0)
            label = "7 dni" if mins == 10080 else ("%d h" % (mins // 60) if mins >= 60 else "%d min" % mins)
            out.append((label, float(v["used_percent"])))
    return out


def table(headers: Sequence[str], rows: List[List[str]], widths: List[int], paint: Paint) -> List[str]:
    def line(left: str, mid: str, right: str) -> str:
        return paint(left + mid.join("─" * (w + 2) for w in widths) + right, "grey")

    sep = paint("│", "grey")
    out = [line("┌", "┬", "┐"),
           sep + sep.join(" " + pad(paint(h, "bold"), w) + " " for h, w in zip(headers, widths)) + sep,
           line("├", "┼", "┤")]
    for r in rows:
        if len(r) == 1:  # a full-width note under the previous row
            span = sum(widths) + 3 * (len(widths) - 1)
            out.append(sep + " " + pad(r[0], span) + " " + sep)
            continue
        out.append(sep + sep.join(" " + pad(c, w) + " " for c, w in zip(r, widths)) + sep)
    out.append(line("└", "┴", "┘"))
    return out


def counting_since(database: D.Database) -> float:
    midnight = _dt.datetime.combine(_dt.date.today(), _dt.time()).timestamp()
    return max(midnight, float(database.get_meta("limits_reset_at", "0") or 0))


def render(cfg: Any, database: D.Database, pid: int, *, colour: bool, show_all: bool = False,
           width: Optional[int] = None) -> str:
    p = Paint(colour)
    s = snapshot(database)
    width = max(72, min(140, width or shutil.get_terminal_size((100, 24)).columns))
    limits = cfg["limits"]
    out: List[str] = []

    # ---------------------------------------------------------------- header
    loop = s["loop_state"]
    title = p(" GTA2.0 Orchestrator ", "bold")
    clock = p(_dt.datetime.now().strftime("%Y-%m-%d %H:%M"), "grey")
    out.append(title + " " * max(1, width - visible_len(title) - visible_len(clock)) + clock)
    daemon = p("● działa", "green") + p(" (pid %d)" % pid, "grey") if pid else p("● nie działa", "red")
    loop_txt = p(loop, LOOP_STYLE.get(loop, "bold"), "bold")
    if s["desired_state"] != loop and s["desired_state"] in ("PAUSED", "STOPPED"):
        loop_txt += p(" → %s" % s["desired_state"], "yellow")
    rows = [("Demon", daemon), ("Pętla", loop_txt + (p("  " + clip(s["loop_reason"], width - 30), "grey")
                                                     if s["loop_reason"] else ""))]
    if s["next_attempt_at"] != "-":
        rows.append(("Wznowienie", s["next_attempt_at"]))
    rows.append(("Etap", s["milestone"]))
    agent = s["active_agent"]
    rows.append(("Agent", p(agent, "cyan", "bold") + " pracuje" if agent != "-" else p("nikt teraz nie pracuje", "grey")))
    rows.append(("Ostatnio", "%s  %s" % (ago(parse_iso(s["last_activity"])), p(clip(s["last_step"], width - 40), "grey"))))
    last_merge = s.get("last_merge", "-")
    if last_merge != "-":
        ts, _, what = last_merge.partition(" ")
        rows.append(("Ostatni merge", "%s  %s" % (ago(parse_iso(ts)), what)))
    out.append("")
    out += ["  %s %s" % (p(pad(k, 14), "grey"), v) for k, v in rows]

    if s["human_question"]:
        out.append("")
        out.append(p("  ⚠ POTRZEBNA TWOJA DECYZJA", "red", "bold"))
        out.append("    " + clip(s["human_question"], 3 * width).replace("\n", " "))

    # ---------------------------------------------------------------- tasks
    c = s["counts"]
    active_n = sum(c[x] for x in D.ACTIVE_STATUSES)
    summary = "  ".join([p("✓ done %d" % c[D.DONE], "green"), p("▶ w toku %d" % active_n, "yellow"),
                         p("○ ready %d" % c[D.READY], "blue"),
                         p("✗ blocked %d" % c[D.BLOCKED], "red" if c[D.BLOCKED] else "grey"),
                         p("? needs-human %d" % c[D.NEEDS_HUMAN], "red" if c[D.NEEDS_HUMAN] else "grey")])
    out += ["", "  " + p("ZADANIA", "bold") + "   " + summary]
    current = database.active_task()
    done_ids = {t["id"] for t in database.tasks((D.DONE, D.CANCELLED))}
    tasks: List[Any] = sorted(database.tasks(D.ACTIVE_STATUSES), key=lambda t: t["updated_at"])
    tasks += database.tasks((D.NEEDS_HUMAN, D.BLOCKED)) + database.tasks((D.READY,))
    done = sorted(database.tasks((D.DONE,)), key=lambda t: t["updated_at"], reverse=True)
    tasks += done if show_all else done[:3]
    if show_all:
        tasks += database.tasks((D.CANCELLED,))
    w_id, w_status, w_try = 6, 19, 6
    w_title = width - (w_id + w_status + w_try) - 13
    body: List[List[str]] = []
    for t in tasks:
        colour, icon = STATUS_STYLE.get(t["status"], ("bold", " "))
        tid = p(t["id"], "bold") if current is not None and t["id"] == current["id"] else t["id"]
        tries = "%d/%d" % (t["attempts"], limits["max_attempts_per_task"]) if t["status"] in D.OPEN_STATUSES + (
            D.BLOCKED, D.NEEDS_HUMAN) else ""
        name = clip(t["title"], w_title)
        if t["status"] == D.DONE:
            name = p(name, "grey")
        body.append([tid, p("%s %s" % (icon, t["status"]), colour), tries, name])
        if t["status"] in (D.BLOCKED, D.NEEDS_HUMAN) and t["blocked_reason"]:
            body.append([p("   └ " + clip(t["blocked_reason"], w_title + w_status + w_try + 2), "red")])
        elif t["status"] == D.READY:
            waits = [d for d in loads(t["depends_on"], []) if d not in done_ids]
            if waits:
                body[-1][3] = clip(t["title"], w_title - 14 - 6 * len(waits)) + p("  czeka na " + ", ".join(waits), "grey")
    out += ["  " + r for r in table(["ID", "Status", "Próby", "Tytuł"], body, [w_id, w_status, w_try, w_title], p)]
    hidden = len(done) - 3
    if not show_all and hidden > 0:
        out.append(p("  … i %d wcześniej zakończonych (`status --all`)" % hidden, "grey"))
    if s["last_decision"] != "-":
        out.append("  %s %s" % (p("Ostatnia decyzja:", "grey"), clip(s["last_decision"], width - 22)))

    # ---------------------------------------------------------------- limits
    out += ["", "  " + p("LIMITY", "bold")]
    since = counting_since(database)
    runs = database.runs_since(iso(since))
    cap = int(limits["max_runs_per_day"])
    out.append("  %s %s  %s" % (p(pad("Uruchomienia", 14), "grey"), bar(100.0 * runs / max(1, cap), p),
                                p("%d/%d dziś" % (runs, cap), "grey")))
    for name, q in s["quota"].items():
        wins = quota_windows(q.get("rate_limits") or {})
        state = q["state"]
        head = p(pad(name, 14), "grey")
        if state != "OK":
            out.append("  %s %s  reset %s" % (head, p(state, "red", "bold"), q["reset_at"]))
            continue
        if not wins:
            out.append("  %s %s" % (head, p("OK", "green")))
            continue
        parts = ["%s %s" % (bar(pct, p, 8), p(label, "grey")) for label, pct in wins]
        out.append("  %s %s" % (head, "   ".join(parts)))
    gfx = cfg["gfx"]
    if gfx.get("enabled", True):
        try:
            g_today = int(database.conn.execute("SELECT COUNT(*) AS n FROM gfx_jobs WHERE started_at>=?",
                                                (iso(since),)).fetchone()["n"])
            month = _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-01T00:00:00Z")
            mins = float(database.conn.execute("SELECT COALESCE(SUM(minutes),0) AS m FROM gfx_jobs WHERE started_at>=?",
                                               (month,)).fetchone()["m"])
            queued = int(database.conn.execute("SELECT COUNT(*) AS n FROM gfx_jobs WHERE status='queued'")
                         .fetchone()["n"])
        except Exception:  # noqa: BLE001 - an old database without the gfx table
            g_today, mins, queued = 0, 0.0, 0
        spend = mins / 60.0 * float(gfx["usd_per_hour"])
        budget = float(gfx["monthly_budget_usd"])
        out.append("  %s %s  %s" % (p(pad("EC2 dziś", 14), "grey"),
                                    bar(100.0 * g_today / max(1, int(gfx["max_runs_per_day"])), p),
                                    p("%d/%d przebiegów%s" % (g_today, gfx["max_runs_per_day"],
                                                            ", w kolejce %d" % queued if queued else ""), "grey")))
        out.append("  %s %s  %s" % (p(pad("EC2 miesiąc", 14), "grey"), bar(100.0 * spend / max(0.01, budget), p),
                                    p("%.2f / %.0f USD" % (spend, budget), "grey")))
    if s["release_candidate"]:
        out += ["", p("  ★ Astra oznaczyła RELEASE_CANDIDATE (%s); zakończenie ogłaszasz tylko Ty" % s["release_candidate"],
                      "magenta")]
    return "\n".join(out)
