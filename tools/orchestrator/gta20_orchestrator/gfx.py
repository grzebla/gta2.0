"""Runs of the ORIGINAL GTA 2 on the EC2 G4dn test host, on behalf of the agents.

Agents never get AWS access. They put `gfx_requests` in their structured output; the
orchestrator validates each request, enforces hard limits (minutes per run, runs per day,
monthly budget), executes the owner's trusted operator script from the main checkout
(scripts/aws/run-gfx-compat-local.sh, which always stops the host) and hands the results
back as data. Screenshots of the original game stay outside the repository.
"""

from __future__ import annotations

import datetime as _dt
import json
import os
import re
import shutil
import subprocess
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from . import gitops
from .util import dumps, iso, loads, now, truncate

MODES = ("probe", "compat", "capture")
ID_RE = re.compile(r"^[a-z0-9-]{1,64}$")
CASES_RE = re.compile(r"^(all|[a-z0-9-]+(,[a-z0-9-]+)*)$")
KEYS = {"ENTER", "ESC", "SPACE", "TAB", "UP", "DOWN", "LEFT", "RIGHT", "LCTRL", "LSHIFT", "LALT",
        "BACKSPACE"} | set("ABCDEFGHIJKLMNOPQRSTUVWXYZ") | set("0123456789")
SCREEN_KEYS = {"start_mode", "full_width", "full_height", "window_width", "window_height", "renderdevice",
               "videodevice", "do_play_movie", "tripple_buffer"}

SCHEMA = """
CREATE TABLE IF NOT EXISTS gfx_jobs(
    id INTEGER PRIMARY KEY AUTOINCREMENT, task_id TEXT, requested_by TEXT NOT NULL, mode TEXT NOT NULL,
    target TEXT NOT NULL, reason TEXT, status TEXT NOT NULL, requested_at TEXT, started_at TEXT,
    finished_at TEXT, minutes REAL, scenario TEXT, result_dir TEXT, verdict TEXT, summary TEXT, error TEXT);
"""


class ScenarioError(ValueError):
    pass


def validate_scenario(data: Any, matrix: Dict[str, Any], scenario_id: str, max_seconds: int) -> Dict[str, Any]:
    """Strict check of an agent-written scenario: it is data only (configuration, keys, times)."""
    if not isinstance(data, dict):
        raise ScenarioError("scenario must be a JSON object")
    allowed = {"id", "wrapper", "screen", "durationSeconds", "screenshotsAtSeconds", "inputs"}
    extra = set(data) - allowed
    if extra:
        raise ScenarioError("unknown fields: %s" % ", ".join(sorted(extra)))
    if data.get("id") != scenario_id or not ID_RE.match(scenario_id):
        raise ScenarioError("id must equal the file name and match [a-z0-9-]{1,64}")
    if data.get("wrapper") not in (matrix.get("wrappers") or {}):
        raise ScenarioError("wrapper must be one of: %s" % ", ".join(sorted(matrix.get("wrappers") or {})))
    screen = data.get("screen") or {}
    if not isinstance(screen, dict) or set(screen) - SCREEN_KEYS:
        raise ScenarioError("screen may only set: %s" % ", ".join(sorted(SCREEN_KEYS)))
    for k, v in screen.items():
        if not isinstance(v, int) or not 0 <= v <= 4096:
            raise ScenarioError("screen.%s must be an integer 0..4096" % k)
    duration = data.get("durationSeconds")
    if not isinstance(duration, int) or not 10 <= duration <= min(300, max_seconds):
        raise ScenarioError("durationSeconds must be an integer 10..%d" % min(300, max_seconds))
    shots = data.get("screenshotsAtSeconds")
    if (not isinstance(shots, list) or not 1 <= len(shots) <= 20 or len(set(shots)) != len(shots)
            or not all(isinstance(s, int) and 1 <= s <= duration for s in shots)):
        raise ScenarioError("screenshotsAtSeconds: 1..20 distinct integers within the duration")
    inputs = data.get("inputs") or []
    if not isinstance(inputs, list) or len(inputs) > 100:
        raise ScenarioError("inputs: at most 100 key presses")
    for i, ev in enumerate(inputs):
        if not isinstance(ev, dict) or set(ev) - {"atSeconds", "key", "holdMs"}:
            raise ScenarioError("inputs[%d]: only atSeconds, key, holdMs" % i)
        at, key, hold = ev.get("atSeconds"), ev.get("key"), ev.get("holdMs", 80)
        if not isinstance(at, (int, float)) or not 0 <= at <= duration:
            raise ScenarioError("inputs[%d].atSeconds must be within the duration" % i)
        if key not in KEYS:
            raise ScenarioError("inputs[%d].key %r not allowed" % (i, key))
        if not isinstance(hold, int) or not 0 <= hold <= 5000:
            raise ScenarioError("inputs[%d].holdMs must be 0..5000" % i)
    # Normalised copy: exactly the fields the host reads, defaults filled in (StrictMode on the host).
    return {"id": scenario_id, "wrapper": data["wrapper"], "screen": dict(screen), "durationSeconds": duration,
            "screenshotsAtSeconds": list(shots),
            "inputs": [{"atSeconds": ev["atSeconds"], "key": ev["key"], "holdMs": ev.get("holdMs", 80)} for ev in inputs]}


class GfxRunner:
    """Executes one job with the owner's operator script and the owner's AWS CLI session.

    The script, the host scripts it uploads and the case matrix come from a dedicated detached
    worktree at <remote>/<release target> (reviewed code), never from an agent branch.
    """

    def __init__(self, cfg: Any):
        self.cfg = cfg
        self.repo = cfg.repo_root
        self.checkout = cfg.worktree("gfx")

    def env(self) -> Dict[str, str]:
        env = dict(os.environ)
        # launchd starts the daemon with a minimal PATH.
        extra = [d for d in ("/opt/homebrew/bin", "/usr/local/bin") if os.path.isdir(d)]
        env["PATH"] = os.pathsep.join([env.get("PATH", "/usr/bin:/bin")] + extra)
        env["AWS_REGION"] = "eu-north-1"
        return env

    def session_ok(self) -> Tuple[bool, str]:
        aws = shutil.which("aws", path=self.env()["PATH"])
        if not aws:
            return False, "aws CLI not found"
        try:
            out = subprocess.run([aws, "sts", "get-caller-identity", "--output", "json"], env=self.env(),
                                 capture_output=True, text=True, timeout=60)
        except (OSError, subprocess.TimeoutExpired) as exc:
            return False, str(exc)
        if out.returncode != 0:
            return False, (out.stderr or out.stdout).strip()[-300:]
        return True, "ok"

    def prepare(self) -> Path:
        remote, target = self.cfg["remote"], self.cfg["release"]["target_branch"]
        gitops.git(self.repo, "fetch", "--quiet", remote, target)
        ref = gitops.rev(self.repo, "%s/%s" % (remote, target))
        gitops.ensure_worktree(self.repo, self.checkout, detach=ref)
        gitops.checkout_detached(self.checkout, ref)
        return self.checkout

    def run(self, mode: str, target: str, minutes: int, scenarios_dir: Optional[Path], log: Path) -> Dict[str, Any]:
        root = self.prepare()
        env = self.env()
        env.update({"CONFIRM": "uruchom-ec2",
                    "GFX_RESULTS_ROOT": str(Path(os.path.expanduser(self.cfg["gfx"]["results_root"])))})
        if scenarios_dir:
            env["GFX_SCENARIOS_DIR"] = str(scenarios_dir)
        script = root / "scripts" / "aws" / "run-gfx-compat-local.sh"
        started = time.time()
        with open(log, "w") as fh:
            try:
                proc = subprocess.run(["bash", str(script), mode, target, str(minutes)], cwd=str(root), env=env,
                                      stdin=subprocess.DEVNULL, stdout=fh, stderr=subprocess.STDOUT,
                                      timeout=(minutes + 20) * 60)
                rc: Optional[int] = proc.returncode
            except subprocess.TimeoutExpired:
                rc = None
        text = log.read_text(errors="replace")
        m = re.search(r"results: (\S+) \(exit (\d+)\)", text)
        result_dir = Path(m.group(1)) if m else None
        summary = None
        if result_dir and (result_dir / "summary.json").exists():
            summary = loads((result_dir / "summary.json").read_text(encoding="utf-8-sig"), None)
        return {"rc": rc, "minutes": round((time.time() - started) / 60, 1), "result_dir": str(result_dir or ""),
                "summary": summary, "log_tail": text[-3000:]}


class GfxJobs:
    def __init__(self, orch: Any, runner: Any):
        self.o = orch
        self.db = orch.db
        self.cfg = orch.cfg
        self.lim = self.cfg["gfx"]
        self.runner = runner
        self.db.conn.executescript(SCHEMA)

    # ------------------------------------------------------------------ helpers
    def matrix(self, root: Optional[Path] = None) -> Dict[str, Any]:
        path = (root or self.o.repo) / "tests" / "gfx" / "compat-matrix.json"
        return json.loads(path.read_text())

    def jobs(self, limit: int = 20, task_id: Optional[str] = None) -> List[Any]:
        if task_id:
            return list(self.db.conn.execute("SELECT * FROM gfx_jobs WHERE task_id=? ORDER BY id DESC LIMIT ?",
                                             (task_id, limit)))
        return list(self.db.conn.execute("SELECT * FROM gfx_jobs ORDER BY id DESC LIMIT ?", (limit,)))

    def pending(self) -> int:
        return int(self.db.conn.execute("SELECT COUNT(*) AS n FROM gfx_jobs WHERE status='queued'").fetchone()["n"])

    def month_spend_usd(self) -> float:
        start = _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-01T00:00:00Z")
        row = self.db.conn.execute("SELECT COALESCE(SUM(minutes),0) AS m FROM gfx_jobs WHERE started_at>=?",
                                   (start,)).fetchone()
        return float(row["m"]) / 60.0 * float(self.lim["usd_per_hour"])

    def runs_today(self) -> int:
        start = _dt.datetime.combine(_dt.date.today(), _dt.time()).timestamp()
        return int(self.db.conn.execute("SELECT COUNT(*) AS n FROM gfx_jobs WHERE started_at>=?",
                                        (iso(start),)).fetchone()["n"])

    # ------------------------------------------------------------------ requests
    def enqueue(self, requests: Any, task_id: Optional[str], by: str, source_root: Optional[Path] = None) -> List[str]:
        """Validate agent requests; invalid ones are recorded as rejected (agents see why)."""
        msgs: List[str] = []
        if not self.lim.get("enabled", True):
            return msgs
        for req in (requests or [])[:3]:
            mode, target = str(req.get("mode", "")), str(req.get("target", "")).strip()
            reason = str(req.get("reason", ""))[:500]
            status, error, scenario = "queued", None, None
            try:
                matrix = self.matrix()
                if mode not in MODES:
                    raise ScenarioError("mode must be one of %s" % ", ".join(MODES))
                if mode == "compat":
                    if not CASES_RE.match(target):
                        raise ScenarioError("compat target: 'all' or comma-separated case ids")
                    known = {c["id"] for c in matrix["cases"]}
                    unknown = [c for c in target.split(",") if c != "all" and c not in known]
                    if unknown:
                        raise ScenarioError("unknown case ids: %s" % ", ".join(unknown))
                elif mode == "capture":
                    if not ID_RE.match(target):
                        raise ScenarioError("capture target must be a scenario id")
                    path = self.scenario_path(target, source_root)
                    if not path:
                        raise ScenarioError("scenario tests/gfx/scenarios/%s.json not found" % target)
                    data = validate_scenario(json.loads(path.read_text()), matrix, target,
                                             int(self.lim["max_minutes_per_run"]) * 60)
                    scenario = json.dumps(data, indent=1)
                else:
                    target = "all"
                pending = self.pending()
                if pending >= int(self.lim["max_pending"]):
                    raise ScenarioError("too many queued runs (%d)" % pending)
            except (ScenarioError, ValueError, KeyError) as exc:
                status, error = "rejected", str(exc)[:500]
            with self.db.tx():
                cur = self.db.conn.execute(
                    """INSERT INTO gfx_jobs(task_id,requested_by,mode,target,reason,status,requested_at,error,scenario)
                       VALUES(?,?,?,?,?,?,?,?,?)""",
                    (task_id, by, mode, target, reason, status, iso(), error, scenario))
                self.db.event("gfx_requested", task_id=task_id, agent=by, outcome=status,
                              detail={"job": cur.lastrowid, "mode": mode, "target": target, "error": error})
            msgs.append("gfx job %s %s %s: %s" % (cur.lastrowid, mode, target, status if not error else error))
        return msgs

    def scenario_path(self, scenario_id: str, source_root: Optional[Path]) -> Optional[Path]:
        for root in [r for r in (source_root, self.o.repo) if r]:
            p = Path(root) / "tests" / "gfx" / "scenarios" / ("%s.json" % scenario_id)
            if p.exists():
                return p
        return None

    def recover(self) -> None:
        """A run interrupted by a crash or restart is charged as a full run (the host may have run)."""
        with self.db.tx():
            for r in self.db.conn.execute("SELECT id FROM gfx_jobs WHERE status='running'").fetchall():
                self.db.conn.execute("UPDATE gfx_jobs SET status='failed', finished_at=?, minutes=?, error=? WHERE id=?",
                                     (iso(), float(self.lim["max_minutes_per_run"]),
                                      "orchestrator restarted during the run; check that the host is stopped", r["id"]))
                self.db.event("gfx_abandoned", agent="orchestrator", outcome="failed", detail={"job": r["id"]})

    # ------------------------------------------------------------------ execution
    def next_job(self) -> Optional[Any]:
        blocked = float(self.db.get_meta("gfx_blocked_until", "0") or 0)
        if blocked > now():
            return None
        return self.db.conn.execute("SELECT * FROM gfx_jobs WHERE status='queued' ORDER BY id LIMIT 1").fetchone()

    def next_possible(self) -> float:
        """Earliest time the next queued job may start (login back-off, daily run limit)."""
        until = max(now() + 60, float(self.db.get_meta("gfx_blocked_until", "0") or 0))
        if self.runs_today() >= int(self.lim["max_runs_per_day"]):
            tomorrow = _dt.datetime.combine(_dt.date.today() + _dt.timedelta(days=1), _dt.time())
            until = max(until, tomorrow.timestamp() + 60)
        return until

    def cancel(self, job_id: int) -> bool:
        with self.db.tx():
            cur = self.db.conn.execute("UPDATE gfx_jobs SET status='cancelled', finished_at=?, error=? "
                                       "WHERE id=? AND status='queued'", (iso(), "cancelled by the owner", job_id))
            if cur.rowcount:
                self.db.event("gfx_cancelled", agent="owner", outcome="cancelled", detail={"job": job_id})
        return bool(cur.rowcount)

    def finish(self, job_id: int, **fields: Any) -> None:
        fields["finished_at"] = iso()
        cols = ", ".join("%s=?" % k for k in fields)
        with self.db.tx():
            self.db.conn.execute("UPDATE gfx_jobs SET %s WHERE id=?" % cols, tuple(fields.values()) + (job_id,))

    def run_next(self) -> Optional[str]:
        job = self.next_job()
        if not job:
            return None
        jid = int(job["id"])
        minutes = int(self.lim["max_minutes_per_run"])
        if self.runs_today() >= int(self.lim["max_runs_per_day"]):
            return None  # stays queued until tomorrow
        projected = self.month_spend_usd() + minutes / 60.0 * float(self.lim["usd_per_hour"])
        if projected > float(self.lim["monthly_budget_usd"]):
            self.finish(jid, status="refused_budget",
                        error="monthly EC2 budget %.2f USD would be exceeded (spent %.2f)" % (
                            float(self.lim["monthly_budget_usd"]), self.month_spend_usd()))
            with self.db.tx():
                self.db.set_meta("human_question", "EC2 test budget for this month is used up (gfx job %d refused). "
                                 "Raise gfx.monthly_budget_usd or wait for next month." % jid)
            return "gfx job %d refused: monthly budget" % jid
        ok, msg = self.runner.session_ok()
        if not ok:
            with self.db.tx():
                self.db.set_meta("gfx_blocked_until", str(now() + 3600))
                self.db.set_meta("human_question", "AWS session expired - run `aws login` so queued EC2 test runs can "
                                 "continue (%s)" % msg[:200])
                self.db.event("gfx_auth_failed", agent="orchestrator", outcome="needs_login", detail=msg[:300])
            return "gfx job %d waits for `aws login`" % jid
        scenarios_dir = None
        if job["mode"] == "capture":
            # Re-validate the exact bytes that will be uploaded; only that one file is staged.
            try:
                data = validate_scenario(loads(job["scenario"], None), self.matrix(), job["target"], minutes * 60)
            except ScenarioError as exc:
                self.finish(jid, status="rejected", error=str(exc)[:500])
                return "gfx job %d rejected: %s" % (jid, exc)
            scenarios_dir = self.o.cfg.state_dir / "gfx-staging" / str(jid)
            if scenarios_dir.exists():
                shutil.rmtree(scenarios_dir)
            scenarios_dir.mkdir(parents=True)
            (scenarios_dir / ("%s.json" % job["target"])).write_text(json.dumps(data, indent=1))
        with self.db.tx():
            self.db.conn.execute("UPDATE gfx_jobs SET status='running', started_at=? WHERE id=?", (iso(), jid))
            self.db.event("gfx_started", task_id=job["task_id"], agent="orchestrator", outcome=job["mode"],
                          detail={"job": jid, "target": job["target"]})
        log = self.o.runs_dir / ("gfx-job-%d.log" % jid)
        self.o.runs_dir.mkdir(parents=True, exist_ok=True)
        res = self.runner.run(job["mode"], job["target"], minutes, scenarios_dir, log)
        summary = res.get("summary") or {}
        verdict = summary.get("overall") or ("ERROR" if res.get("rc") else "UNKNOWN")
        cases = ["%s=%s" % (c.get("id"), c.get("verdict")) for c in summary.get("cases") or []]
        status = "done" if res.get("rc") == 0 else "failed"
        self.finish(jid, status=status, minutes=res.get("minutes"), result_dir=res.get("result_dir"),
                    verdict=verdict, summary=dumps({"cases": cases}),
                    error=None if status == "done" else truncate(res.get("log_tail", ""), 1500))
        with self.db.tx():
            self.db.event("gfx_finished", task_id=job["task_id"], agent="orchestrator", outcome=verdict,
                          detail={"job": jid, "minutes": res.get("minutes"), "cases": cases[:10]})
        return "gfx job %d %s %s: %s (%s min)" % (jid, job["mode"], job["target"], verdict, res.get("minutes"))

    # ------------------------------------------------------------------ context for agents
    def context(self, task_id: Optional[str] = None, limit: int = 5) -> str:
        rows = self.jobs(limit, task_id) if task_id else self.jobs(limit)
        lines = ["Original-game runs on the EC2 test host (request with gfx_requests; limits: %s min/run, %s runs/day, "
                 "%.0f USD/month, spent this month %.2f USD):" % (
                     self.lim["max_minutes_per_run"], self.lim["max_runs_per_day"],
                     float(self.lim["monthly_budget_usd"]), self.month_spend_usd())]
        for r in rows:
            cases = (loads(r["summary"], {}) or {}).get("cases") or []
            lines.append("- job %s [%s] %s %s by %s: %s%s%s" % (
                r["id"], r["status"], r["mode"], r["target"], r["requested_by"], r["verdict"] or "-",
                (" | " + ", ".join(cases[:8])) if cases else "",
                (" | results (local, not in git): %s" % r["result_dir"]) if r["status"] == "done" and r["result_dir"] else
                (" | " + r["error"][:200] if r["error"] else "")))
        if len(lines) == 1:
            lines.append("- none yet")
        return "\n".join(lines)
