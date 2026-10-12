"""Durable machine state (SQLite, WAL) and the append-only event log.

Current state (tasks, runs, quota, meta) is changed only inside transactions. Events are
append-only twice over: triggers reject UPDATE/DELETE on the events table, and every event is
also appended (and fsynced) to events.jsonl.
"""

from __future__ import annotations

import json
import os
import sqlite3
from contextlib import contextmanager
from pathlib import Path
from typing import Any, Dict, Iterator, List, Optional

from .util import dumps, iso, loads, now, redact

SCHEMA_VERSION = 1

SCHEMA = """
CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS milestones(
    id TEXT PRIMARY KEY, title TEXT NOT NULL, goal TEXT, acceptance TEXT, status TEXT NOT NULL,
    created_at TEXT, updated_at TEXT);
CREATE TABLE IF NOT EXISTS tasks(
    id TEXT PRIMARY KEY, seq INTEGER UNIQUE NOT NULL, milestone_id TEXT, title TEXT NOT NULL,
    status TEXT NOT NULL, priority INTEGER NOT NULL DEFAULT 100, spec TEXT NOT NULL,
    depends_on TEXT NOT NULL DEFAULT '[]', branch TEXT, base_commit TEXT, head_commit TEXT,
    attempts INTEGER NOT NULL DEFAULT 0, agent_failures INTEGER NOT NULL DEFAULT 0,
    no_progress INTEGER NOT NULL DEFAULT 0, last_diff_hash TEXT, feedback TEXT, claude_session TEXT,
    report TEXT,
    blocked_reason TEXT, created_by TEXT, created_at TEXT, updated_at TEXT);
CREATE TABLE IF NOT EXISTS runs(
    id INTEGER PRIMARY KEY AUTOINCREMENT, task_id TEXT, agent TEXT NOT NULL, kind TEXT NOT NULL,
    status TEXT NOT NULL, session_id TEXT, model TEXT, pid INTEGER, started_at TEXT,
    ended_at TEXT, exit_code INTEGER, run_dir TEXT, result TEXT, error TEXT, usage TEXT);
CREATE TABLE IF NOT EXISTS decisions(
    id INTEGER PRIMARY KEY AUTOINCREMENT, task_id TEXT, run_id INTEGER, decision TEXT NOT NULL,
    rationale TEXT, commit_sha TEXT, details TEXT, created_at TEXT);
CREATE TABLE IF NOT EXISTS verifications(
    id INTEGER PRIMARY KEY AUTOINCREMENT, task_id TEXT, commit_sha TEXT, passed INTEGER NOT NULL,
    checks TEXT, created_at TEXT);
CREATE TABLE IF NOT EXISTS quota(
    agent TEXT PRIMARY KEY, state TEXT NOT NULL, reset_at REAL, probes INTEGER NOT NULL DEFAULT 0,
    detail TEXT, updated_at TEXT);
CREATE TABLE IF NOT EXISTS events(
    id INTEGER PRIMARY KEY AUTOINCREMENT, ts TEXT NOT NULL, task_id TEXT, agent TEXT,
    type TEXT NOT NULL, session_id TEXT, commits TEXT, outcome TEXT, detail TEXT);
CREATE TRIGGER IF NOT EXISTS events_no_update BEFORE UPDATE ON events
    BEGIN SELECT RAISE(ABORT, 'events are append-only'); END;
CREATE TRIGGER IF NOT EXISTS events_no_delete BEFORE DELETE ON events
    BEGIN SELECT RAISE(ABORT, 'events are append-only'); END;
CREATE INDEX IF NOT EXISTS runs_task ON runs(task_id);
CREATE INDEX IF NOT EXISTS events_task ON events(task_id);
"""

# Task lifecycle.
READY = "READY"
IN_PROGRESS = "IN_PROGRESS"
VERIFYING = "VERIFYING"
IN_REVIEW = "IN_REVIEW"
CHANGES_REQUESTED = "CHANGES_REQUESTED"
DONE = "DONE"
BLOCKED = "BLOCKED"
NEEDS_HUMAN = "NEEDS_HUMAN"
CANCELLED = "CANCELLED"
ACTIVE_STATUSES = (IN_PROGRESS, CHANGES_REQUESTED, VERIFYING, IN_REVIEW)
OPEN_STATUSES = (READY,) + ACTIVE_STATUSES
TERMINAL_STATUSES = (DONE, CANCELLED)


class Database:
    def __init__(self, state_dir: Path):
        self.state_dir = state_dir
        state_dir.mkdir(parents=True, exist_ok=True)
        self.path = state_dir / "state.sqlite"
        self.events_path = state_dir / "events.jsonl"
        self.conn = sqlite3.connect(str(self.path), isolation_level=None, timeout=30)
        self.conn.row_factory = sqlite3.Row
        self.conn.execute("PRAGMA journal_mode=WAL")
        self.conn.execute("PRAGMA synchronous=FULL")
        self.conn.execute("PRAGMA foreign_keys=ON")
        self.conn.executescript(SCHEMA)
        if self.get_meta("schema_version") is None:
            self.set_meta("schema_version", str(SCHEMA_VERSION))
        try:
            os.chmod(self.path, 0o600)
        except OSError:
            pass

    def close(self) -> None:
        self.conn.close()

    @contextmanager
    def tx(self) -> Iterator[sqlite3.Connection]:
        """IMMEDIATE transaction: all-or-nothing state change."""
        if self.conn.in_transaction:
            yield self.conn
            return
        self.conn.execute("BEGIN IMMEDIATE")
        try:
            yield self.conn
        except BaseException:
            self.conn.execute("ROLLBACK")
            raise
        self.conn.execute("COMMIT")

    # ---- meta -------------------------------------------------------------------------------
    def get_meta(self, key: str, default: Optional[str] = None) -> Optional[str]:
        row = self.conn.execute("SELECT value FROM meta WHERE key=?", (key,)).fetchone()
        return row["value"] if row else default

    def set_meta(self, key: str, value: str) -> None:
        self.conn.execute(
            "INSERT INTO meta(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value",
            (key, value),
        )

    # ---- events -----------------------------------------------------------------------------
    def event(
        self,
        type_: str,
        *,
        task_id: Optional[str] = None,
        agent: Optional[str] = None,
        session_id: Optional[str] = None,
        commits: Optional[List[str]] = None,
        outcome: Optional[str] = None,
        detail: Any = None,
    ) -> None:
        ts = iso()
        detail_text = redact(detail if isinstance(detail, str) else dumps(detail)) if detail is not None else None
        commits_text = dumps(commits) if commits else None
        self.conn.execute(
            "INSERT INTO events(ts,task_id,agent,type,session_id,commits,outcome,detail) VALUES(?,?,?,?,?,?,?,?)",
            (ts, task_id, agent, type_, session_id, commits_text, outcome, detail_text),
        )
        record = {
            "ts": ts, "task_id": task_id, "agent": agent, "type": type_, "session_id": session_id,
            "commits": commits or [], "outcome": outcome, "detail": detail_text,
        }
        with open(self.events_path, "a", encoding="utf-8") as fh:
            fh.write(json.dumps(record, ensure_ascii=False) + "\n")
            fh.flush()
            os.fsync(fh.fileno())

    def events(self, limit: int = 50, task_id: Optional[str] = None) -> List[sqlite3.Row]:
        if task_id:
            q = "SELECT * FROM events WHERE task_id=? ORDER BY id DESC LIMIT ?"
            return list(self.conn.execute(q, (task_id, limit)))
        return list(self.conn.execute("SELECT * FROM events ORDER BY id DESC LIMIT ?", (limit,)))

    # ---- milestones -------------------------------------------------------------------------
    def upsert_milestone(self, mid: str, title: str, goal: str, acceptance: List[str], status: str) -> None:
        ts = iso()
        self.conn.execute(
            """INSERT INTO milestones(id,title,goal,acceptance,status,created_at,updated_at)
               VALUES(?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET title=excluded.title,
               goal=excluded.goal, acceptance=excluded.acceptance, status=excluded.status,
               updated_at=excluded.updated_at""",
            (mid, title, goal, dumps(acceptance), status, ts, ts),
        )

    def milestone(self, mid: str) -> Optional[sqlite3.Row]:
        return self.conn.execute("SELECT * FROM milestones WHERE id=?", (mid,)).fetchone()

    def active_milestone(self) -> Optional[sqlite3.Row]:
        mid = self.get_meta("current_milestone")
        if mid:
            row = self.milestone(mid)
            if row:
                return row
        return self.conn.execute(
            "SELECT * FROM milestones WHERE status='active' ORDER BY created_at LIMIT 1"
        ).fetchone()

    # ---- tasks ------------------------------------------------------------------------------
    def next_task_id(self) -> tuple:
        row = self.conn.execute("SELECT COALESCE(MAX(seq),0)+1 AS n FROM tasks").fetchone()
        n = int(row["n"])
        return n, "T%04d" % n

    def add_task(
        self, *, milestone_id: str, title: str, spec: Dict[str, Any], priority: int,
        depends_on: List[str], created_by: str,
    ) -> str:
        seq, tid = self.next_task_id()
        ts = iso()
        self.conn.execute(
            """INSERT INTO tasks(id,seq,milestone_id,title,status,priority,spec,depends_on,created_by,
               created_at,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?)""",
            (tid, seq, milestone_id, title, READY, priority, dumps(spec), dumps(depends_on), created_by, ts, ts),
        )
        return tid

    def task(self, tid: str) -> Optional[sqlite3.Row]:
        return self.conn.execute("SELECT * FROM tasks WHERE id=?", (tid,)).fetchone()

    def update_task(self, tid: str, **fields: Any) -> None:
        if not fields:
            return
        fields["updated_at"] = iso()
        cols = ", ".join("%s=?" % k for k in fields)
        self.conn.execute("UPDATE tasks SET %s WHERE id=?" % cols, tuple(fields.values()) + (tid,))

    def tasks(self, statuses: Optional[tuple] = None) -> List[sqlite3.Row]:
        if statuses:
            marks = ",".join("?" * len(statuses))
            return list(self.conn.execute(
                "SELECT * FROM tasks WHERE status IN (%s) ORDER BY priority, seq" % marks, statuses))
        return list(self.conn.execute("SELECT * FROM tasks ORDER BY seq"))

    def active_task(self) -> Optional[sqlite3.Row]:
        marks = ",".join("?" * len(ACTIVE_STATUSES))
        return self.conn.execute(
            "SELECT * FROM tasks WHERE status IN (%s) ORDER BY updated_at LIMIT 1" % marks, ACTIVE_STATUSES
        ).fetchone()

    def ready_tasks(self) -> List[sqlite3.Row]:
        done = {r["id"] for r in self.tasks((DONE, CANCELLED))}
        out = []
        for row in self.tasks((READY,)):
            deps = loads(row["depends_on"], [])
            if all(d in done for d in deps):
                out.append(row)
        return out

    def count_tasks(self, status: str) -> int:
        return int(self.conn.execute("SELECT COUNT(*) AS n FROM tasks WHERE status=?", (status,)).fetchone()["n"])

    # ---- runs -------------------------------------------------------------------------------
    def start_run(self, *, task_id: Optional[str], agent: str, kind: str, session_id: Optional[str],
                  model: Optional[str], run_dir: str) -> int:
        cur = self.conn.execute(
            """INSERT INTO runs(task_id,agent,kind,status,session_id,model,pid,started_at,run_dir)
               VALUES(?,?,?,?,?,?,?,?,?)""",
            (task_id, agent, kind, "running", session_id, model, os.getpid(), iso(), run_dir),
        )
        return int(cur.lastrowid)

    def finish_run(self, run_id: int, *, status: str, exit_code: Optional[int] = None,
                   session_id: Optional[str] = None, model: Optional[str] = None,
                   result: Any = None, error: Optional[str] = None, usage: Any = None) -> None:
        self.conn.execute(
            """UPDATE runs SET status=?, ended_at=?, exit_code=?, session_id=COALESCE(?,session_id),
               model=COALESCE(?,model), result=?, error=?, usage=? WHERE id=?""",
            (status, iso(), exit_code, session_id, model,
             dumps(result) if result is not None else None,
             redact(error) if error else None,
             dumps(usage) if usage is not None else None, run_id),
        )

    def run(self, run_id: int) -> Optional[sqlite3.Row]:
        return self.conn.execute("SELECT * FROM runs WHERE id=?", (run_id,)).fetchone()

    def runs(self, task_id: Optional[str] = None, limit: int = 50) -> List[sqlite3.Row]:
        if task_id:
            return list(self.conn.execute("SELECT * FROM runs WHERE task_id=? ORDER BY id DESC LIMIT ?",
                                          (task_id, limit)))
        return list(self.conn.execute("SELECT * FROM runs ORDER BY id DESC LIMIT ?", (limit,)))

    def last_run(self, task_id: str, agent: str, kind: str) -> Optional[sqlite3.Row]:
        return self.conn.execute(
            "SELECT * FROM runs WHERE task_id=? AND agent=? AND kind=? ORDER BY id DESC LIMIT 1",
            (task_id, agent, kind),
        ).fetchone()

    def running_runs(self) -> List[sqlite3.Row]:
        return list(self.conn.execute("SELECT * FROM runs WHERE status='running'"))

    def runs_since(self, iso_ts: str) -> int:
        return int(self.conn.execute(
            "SELECT COUNT(*) AS n FROM runs WHERE started_at>=? AND kind!='probe'", (iso_ts,)
        ).fetchone()["n"])

    # ---- decisions / verifications ----------------------------------------------------------
    def add_decision(self, *, task_id: Optional[str], run_id: Optional[int], decision: str,
                     rationale: str, commit_sha: Optional[str], details: Any) -> int:
        cur = self.conn.execute(
            """INSERT INTO decisions(task_id,run_id,decision,rationale,commit_sha,details,created_at)
               VALUES(?,?,?,?,?,?,?)""",
            (task_id, run_id, decision, rationale, commit_sha, dumps(details), iso()),
        )
        return int(cur.lastrowid)

    def decisions(self, limit: int = 20, task_id: Optional[str] = None) -> List[sqlite3.Row]:
        if task_id:
            return list(self.conn.execute(
                "SELECT * FROM decisions WHERE task_id=? ORDER BY id DESC LIMIT ?", (task_id, limit)))
        return list(self.conn.execute("SELECT * FROM decisions ORDER BY id DESC LIMIT ?", (limit,)))

    def add_verification(self, task_id: str, commit_sha: str, passed: bool, checks: Any) -> None:
        self.conn.execute(
            "INSERT INTO verifications(task_id,commit_sha,passed,checks,created_at) VALUES(?,?,?,?,?)",
            (task_id, commit_sha, 1 if passed else 0, dumps(checks), iso()),
        )

    def verification(self, task_id: str, commit_sha: str) -> Optional[sqlite3.Row]:
        return self.conn.execute(
            "SELECT * FROM verifications WHERE task_id=? AND commit_sha=? ORDER BY id DESC LIMIT 1",
            (task_id, commit_sha),
        ).fetchone()

    # ---- quota ------------------------------------------------------------------------------
    def quota(self, agent: str) -> Optional[sqlite3.Row]:
        return self.conn.execute("SELECT * FROM quota WHERE agent=?", (agent,)).fetchone()

    def set_quota(self, agent: str, state: str, reset_at: Optional[float], probes: int, detail: Any) -> None:
        self.conn.execute(
            """INSERT INTO quota(agent,state,reset_at,probes,detail,updated_at) VALUES(?,?,?,?,?,?)
               ON CONFLICT(agent) DO UPDATE SET state=excluded.state, reset_at=excluded.reset_at,
               probes=excluded.probes, detail=excluded.detail, updated_at=excluded.updated_at""",
            (agent, state, reset_at, probes, dumps(detail), iso()),
        )

    def quota_blocked_until(self, agent: str) -> Optional[float]:
        row = self.quota(agent)
        if row and row["state"] == "EXHAUSTED" and row["reset_at"] and row["reset_at"] > now():
            return float(row["reset_at"])
        return None
