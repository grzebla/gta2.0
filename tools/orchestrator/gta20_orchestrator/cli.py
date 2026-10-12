"""Command line interface: gta20-orchestrator <command>."""

from __future__ import annotations

import argparse
import fcntl
import json
import logging
import logging.handlers
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, List, Optional

from . import __version__, gitops, memory, prompts, report, service
from . import db as D
from .config import Config, find_repo_root
from .loop import HALTED, PAUSED, RUNNING, STOPPED, Orchestrator, install_signal_handlers
from .util import dumps, iso, loads, now


# ---------------------------------------------------------------------------- plumbing
def setup_logging(state_dir: Path, verbose: bool = False) -> None:
    log_dir = state_dir / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    root = logging.getLogger()
    root.setLevel(logging.DEBUG if verbose else logging.INFO)
    fh = logging.handlers.RotatingFileHandler(log_dir / "orchestrator.log", maxBytes=5_000_000, backupCount=5)
    fh.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(name)s: %(message)s"))
    root.addHandler(fh)
    sh = logging.StreamHandler(sys.stderr)
    sh.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(message)s", "%H:%M:%S"))
    root.addHandler(sh)


class Lock:
    """One orchestrator per repository (flock on .orchestrator/orchestrator.lock)."""

    def __init__(self, state_dir: Path):
        self.path = state_dir / "orchestrator.lock"
        self.pid_path = state_dir / "orchestrator.pid"
        self.fh: Any = None

    def acquire(self) -> bool:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.fh = open(self.path, "a+")
        try:
            fcntl.flock(self.fh.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return False
        self.pid_path.write_text(str(os.getpid()))
        return True

    def release(self) -> None:
        if self.fh:
            try:
                if self.pid_path.exists() and self.pid_path.read_text().strip() == str(os.getpid()):
                    self.pid_path.unlink()
            except OSError:
                pass
            fcntl.flock(self.fh.fileno(), fcntl.LOCK_UN)
            self.fh.close()


def make_agents(cfg: Config):
    from .adapters.claude import ClaudeAgent
    from .adapters.codex import CodexAgent
    return CodexAgent(cfg["astra"]), ClaudeAgent(cfg["claude"])


def open_db(cfg: Config) -> D.Database:
    database = D.Database(cfg.state_dir)
    gi = cfg.repo_root / ".gitignore"
    if gi.exists() and ".orchestrator" not in gi.read_text():
        print("WARNING: .orchestrator/ is not in .gitignore", file=sys.stderr)
    return database


def set_desired(database: D.Database, state: str, note: str) -> None:
    with database.tx():
        database.set_meta("desired_state", state)
        database.event("control", agent="human", outcome=state, detail=note)


# ---------------------------------------------------------------------------- commands
def cmd_status(cfg: Config, args: argparse.Namespace) -> int:
    database = open_db(cfg)
    snap = report.snapshot(database)
    pid = report.daemon_pid(cfg.state_dir)
    if args.json:
        snap["daemon_pid"] = pid
        print(json.dumps(snap, indent=1, ensure_ascii=False))
    else:
        print(report.format_status(snap, pid))
        print("  service:           %s" % ("loaded" if service.loaded() else "not loaded"))
        print("  report:            %s" % (cfg.state_dir / "report.md"))
    return 0


def run_loop(cfg: Config, *, max_steps: Optional[int], daemon: bool, fake: bool = False) -> int:
    lock = Lock(cfg.state_dir)
    if not lock.acquire():
        print("another orchestrator is already running (lock %s)" % lock.path, file=sys.stderr)
        return 0 if daemon else 1
    try:
        database = open_db(cfg)
        if daemon:
            starts = [t for t in loads(database.get_meta("start_times"), []) if now() - t < 3600] + [now()]
            with database.tx():
                database.set_meta("start_times", dumps(starts))
                database.event("daemon_start", agent="orchestrator", outcome=str(os.getpid()),
                               detail={"version": __version__, "starts_last_hour": len(starts)})
            if database.get_meta("desired_state", STOPPED) == STOPPED:
                logging.info("desired state is STOPPED - exiting")
                return 0
            if database.get_meta("loop_state") == HALTED:
                logging.info("loop is HALTED (%s) - run `resume` after resolving it", database.get_meta("loop_reason"))
                return 0
            if len(starts) > int(cfg["limits"]["max_crash_restarts_per_hour"]):
                with database.tx():
                    database.set_meta("loop_state", HALTED)
                    database.set_meta("loop_reason", "crash loop: %d starts within an hour" % len(starts))
                    database.event("crash_loop", agent="orchestrator", outcome=HALTED)
                report.write(database, cfg.state_dir)
                return 0
        fake_spec = os.environ.get("GTA20_FAKE_AGENTS")  # test hook: scripted agents (sim/tests only)
        if fake or fake_spec:
            from .adapters.fake import FakeAstra, FakeClaude
            spec = json.loads(Path(fake_spec).read_text()) if fake_spec else {}
            astra = FakeAstra(plans=spec.get("astra_plans"), reviews=spec.get("astra_reviews"))
            claude = FakeClaude(script=spec.get("claude_script"))
            logging.warning("using FAKE agents (%s)", fake_spec or "default")
        else:
            astra, claude = make_agents(cfg)
        orch = Orchestrator(cfg, database, astra, claude)
        install_signal_handlers(orch)
        result = orch.run(max_steps=max_steps, on_step=lambda: report.write(database, cfg.state_dir))
        report.write(database, cfg.state_dir)
        logging.info("loop finished: %s", result)
        return 0
    finally:
        lock.release()


def cmd_start(cfg: Config, args: argparse.Namespace) -> int:
    database = open_db(cfg)
    with database.tx():
        if database.get_meta("loop_state") == HALTED:
            database.set_meta("loop_state", RUNNING)
            database.set_meta("loop_reason", "restarted by the owner")
        database.set_meta("start_times", "[]")
    set_desired(database, RUNNING, "start")
    if args.foreground or args.max_steps is not None:
        setup_logging(cfg.state_dir, args.verbose)
        return run_loop(cfg, max_steps=args.max_steps, daemon=False)
    if not service.PLIST.exists():
        print("LaunchAgent not installed. Use `service install` first, or `start --foreground`.")
        return 1
    for msg in service.enable():
        print(msg)
    print("orchestrator started under launchd (%s)" % service.LABEL)
    return 0


def cmd_daemon(cfg: Config, args: argparse.Namespace) -> int:
    setup_logging(cfg.state_dir, args.verbose)
    return run_loop(cfg, max_steps=None, daemon=True)


def cmd_pause(cfg: Config, args: argparse.Namespace) -> int:
    set_desired(open_db(cfg), PAUSED, "pause")
    print("paused: the running step finishes, then the loop waits (`resume` to continue)")
    return 0


def cmd_resume(cfg: Config, args: argparse.Namespace) -> int:
    database = open_db(cfg)
    with database.tx():
        if database.get_meta("loop_state") == HALTED:
            database.set_meta("loop_state", RUNNING)
            database.set_meta("loop_reason", "resumed by the owner")
        database.set_meta("start_times", "[]")
    set_desired(database, RUNNING, "resume")
    if not report.daemon_pid(cfg.state_dir) and service.PLIST.exists():
        for msg in service.enable():
            print(msg)
    print("resumed")
    return 0


def cmd_stop(cfg: Config, args: argparse.Namespace) -> int:
    database = open_db(cfg)
    set_desired(database, STOPPED, "stop" + (" --now" if args.now else ""))
    pid = report.daemon_pid(cfg.state_dir)
    if pid and args.now:
        os.kill(pid, 15)
        print("SIGTERM sent to %d: the running agent is interrupted (SIGINT->SIGTERM), state is checkpointed" % pid)
    elif pid:
        print("stop requested: the daemon stops the current agent call and exits (state is checkpointed)")
    else:
        print("stopped (no daemon running)")
    if args.unload:
        print(service.disable())
    return 0


def cmd_logs(cfg: Config, args: argparse.Namespace) -> int:
    if args.events:
        database = open_db(cfg)
        for e in reversed(database.events(args.n)):
            print("%s %-6s %-7s %-12s %-20s %s %s" % (e["ts"], e["task_id"] or "", e["agent"] or "", e["type"],
                                                      e["outcome"] or "", e["session_id"] or "", (e["detail"] or "")[:160]))
        return 0
    path = cfg.state_dir / "logs" / "orchestrator.log"
    if not path.exists():
        print("no log yet: %s" % path)
        return 0
    cmd = ["tail", "-n", str(args.n)] + (["-f"] if args.follow else []) + [str(path)]
    return subprocess.call(cmd)


def memory_root(cfg: Config) -> Path:
    wt = cfg.worktree("integration")
    return wt if wt.exists() else cfg.repo_root


def cmd_memory(cfg: Config, args: argparse.Namespace) -> int:
    root = memory_root(cfg)
    if args.name:
        name = args.name.upper().replace(".MD", "") + ".md"
        print(memory.read(root, name) or "(missing %s)" % name)
    else:
        print("memory in %s/%s:" % (root, memory.DIR))
        for name in list(memory.REPLACEABLE.values()) + list(memory.LOGS.values()):
            p = memory.path(root, name)
            print("  %-16s %6d bytes" % (name, p.stat().st_size if p.exists() else 0))
    return 0


def cmd_tasks(cfg: Config, args: argparse.Namespace) -> int:
    database = open_db(cfg)
    if args.id:
        t = database.task(args.id)
        if not t:
            print("no task %s" % args.id)
            return 1
        print(json.dumps({k: (loads(t[k], t[k]) if k in ("spec", "report", "depends_on") else t[k]) for k in t.keys()},
                         indent=1, ensure_ascii=False))
        for d in database.decisions(10, args.id):
            print("decision %s %s: %s" % (d["created_at"], d["decision"], d["rationale"]))
        return 0
    for t in database.tasks():
        print("%s %-18s a%s %s" % (t["id"], t["status"], t["attempts"], t["title"]))
    return 0


def cmd_dry_run(cfg: Config, args: argparse.Namespace) -> int:
    """Show what the next step would do (agent, command, prompt) without calling any agent."""
    database = open_db(cfg)
    astra, claude = make_agents(cfg)
    task = database.active_task()
    ready = database.ready_tasks()
    root = memory_root(cfg)
    if task:
        nxt = {D.IN_PROGRESS: "claude implement", D.CHANGES_REQUESTED: "claude implement (changes)",
               D.VERIFYING: "orchestrator verification", D.IN_REVIEW: "astra review"}[task["status"]]
        print("next: %s for %s (%s)" % (nxt, task["id"], task["title"]))
    elif ready:
        print("next: start %s (%s) on branch agent/%s, then claude implement" % (ready[0]["id"], ready[0]["title"],
                                                                                  ready[0]["id"].lower()))
        task = ready[0]
    else:
        print("next: astra plan (no runnable task)")
    print("\nastra command: %s" % " ".join(astra._cmd(cfg.worktree("review"), Path("<schema.json>"),
                                                      Path("<last.json>"), cfg["astra"]["model"])))
    print("claude command: %s" % " ".join(claude._base_cmd() + ["--model", cfg["claude"]["model"], "--settings",
                                                                 "<generated settings.json>", "--json-schema", "<report>"]))
    if task is None or not ready and not database.active_task():
        p = prompts.astra_plan(kind="plan", memory=memory.context(root), board="(see `tasks`)", decisions="...",
                               git_state="...", github_state="...", quota_state="...", milestone=None,
                               max_tasks=int(cfg["limits"]["max_tasks_per_plan"]))
    else:
        spec = loads(task["spec"], {})
        p = prompts.claude_task(task=dict(task), spec=spec, branch=task["branch"] or "agent/" + task["id"].lower(),
                                base=task["base_commit"] or "<integration head>", branch_state="...",
                                feedback=task["feedback"], game_dir_hint="GTA20_GAME_DIR")
    print("\n--- prompt (first %d chars) ---\n%s" % (args.chars, p[: args.chars]))
    return 0


def cmd_report(cfg: Config, args: argparse.Namespace) -> int:
    path = report.write(open_db(cfg), cfg.state_dir)
    print(path)
    print(path.with_suffix(".html"))
    return 0


def cmd_unblock(cfg: Config, args: argparse.Namespace) -> int:
    database = open_db(cfg)
    t = database.task(args.id)
    if not t or t["status"] not in (D.BLOCKED, D.NEEDS_HUMAN):
        print("task %s is not BLOCKED/NEEDS_HUMAN" % args.id)
        return 1
    status = D.CHANGES_REQUESTED if t["branch"] else D.READY
    with database.tx():
        database.update_task(args.id, status=status, attempts=0, no_progress=0, agent_failures=0,
                             feedback=("Owner's note: " + args.note) if args.note else t["feedback"],
                             blocked_reason=None)
        database.set_meta("human_question", "")
        database.add_decision(task_id=args.id, run_id=None, decision="HUMAN_UNBLOCK", rationale=args.note or "",
                              commit_sha=None, details={})
        database.event("task_unblocked", task_id=args.id, agent="human", outcome=status, detail=args.note)
    print("%s -> %s" % (args.id, status))
    return 0


def cmd_answer(cfg: Config, args: argparse.Namespace) -> int:
    database = open_db(cfg)
    with database.tx():
        q = database.get_meta("human_question", "")
        database.add_decision(task_id=None, run_id=None, decision="HUMAN_ANSWER", rationale=args.text,
                              commit_sha=None, details={"question": q})
        database.set_meta("human_question", "")
        database.event("human_answer", agent="human", outcome="answered", detail={"question": q, "answer": args.text})
    print("recorded; Astra sees it in RECENT DECISIONS at the next planning step")
    return 0


def cmd_service(cfg: Config, args: argparse.Namespace) -> int:
    if args.action == "install":
        sha = gitops.rev(cfg.repo_root)
        info = service.install(cfg.repo_root, cfg.state_dir, Path(__file__).resolve().parent, sha)
        database = open_db(cfg)
        if database.get_meta("desired_state") is None:
            set_desired(database, STOPPED, "service installed (not started)")
        for k, v in info.items():
            print("%-10s %s" % (k, v))
        print("installed but NOT started. Start the autonomous loop with: gta20-orchestrator start")
    elif args.action == "uninstall":
        print(service.uninstall())
    else:
        print(service.describe())
    return 0


def cmd_doctor(cfg: Config, args: argparse.Namespace) -> int:
    from . import doctor
    return doctor.run(cfg, live=args.live)


def cmd_simulate(cfg: Config, args: argparse.Namespace) -> int:
    from . import sim
    return sim.main(args.scenario, keep=args.keep)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="gta20-orchestrator", description="GTA2.0 Astra/Claude orchestrator")
    p.add_argument("--repo", help="repository root (default: from cwd or GTA20_REPO_ROOT)")
    p.add_argument("-v", "--verbose", action="store_true")
    p.add_argument("--version", action="version", version=__version__)
    sub = p.add_subparsers(dest="command", required=True)
    s = sub.add_parser("doctor", help="check tools, auth, config")
    s.add_argument("--live", action="store_true", help="also make minimal real calls to Astra and Claude")
    s.set_defaults(fn=cmd_doctor)
    s = sub.add_parser("start", help="start the loop (launchd service, or --foreground)")
    s.add_argument("--foreground", action="store_true")
    s.add_argument("--max-steps", type=int)
    s.set_defaults(fn=cmd_start)
    sub.add_parser("daemon", help="launchd entry point").set_defaults(fn=cmd_daemon)
    sub.add_parser("pause", help="pause after the current step").set_defaults(fn=cmd_pause)
    sub.add_parser("resume", help="resume (also clears HALTED)").set_defaults(fn=cmd_resume)
    s = sub.add_parser("stop", help="stop the loop safely")
    s.add_argument("--now", action="store_true", help="interrupt the running agent immediately")
    s.add_argument("--unload", action="store_true", help="also unload the LaunchAgent")
    s.set_defaults(fn=cmd_stop)
    s = sub.add_parser("status")
    s.add_argument("--json", action="store_true")
    s.set_defaults(fn=cmd_status)
    s = sub.add_parser("logs")
    s.add_argument("-n", type=int, default=60)
    s.add_argument("-f", "--follow", action="store_true")
    s.add_argument("--events", action="store_true", help="append-only event log instead of the text log")
    s.set_defaults(fn=cmd_logs)
    s = sub.add_parser("memory")
    s.add_argument("name", nargs="?")
    s.set_defaults(fn=cmd_memory)
    s = sub.add_parser("tasks")
    s.add_argument("id", nargs="?")
    s.set_defaults(fn=cmd_tasks)
    s = sub.add_parser("dry-run", help="show the next step without calling agents")
    s.add_argument("--chars", type=int, default=4000)
    s.set_defaults(fn=cmd_dry_run)
    sub.add_parser("report", help="write report.md / report.html").set_defaults(fn=cmd_report)
    s = sub.add_parser("unblock", help="owner: put a BLOCKED/NEEDS_HUMAN task back in the queue")
    s.add_argument("id")
    s.add_argument("--note")
    s.set_defaults(fn=cmd_unblock)
    s = sub.add_parser("answer", help="owner: answer Astra's NEEDS_HUMAN question")
    s.add_argument("text")
    s.set_defaults(fn=cmd_answer)
    s = sub.add_parser("service", help="launchd LaunchAgent")
    s.add_argument("action", choices=["install", "uninstall", "status"])
    s.set_defaults(fn=cmd_service)
    s = sub.add_parser("simulate", help="run the loop against scripted fake agents in a temp repo")
    s.add_argument("scenario", nargs="?", default="all")
    s.add_argument("--keep", action="store_true")
    s.set_defaults(fn=cmd_simulate)
    return p


def main(argv: Optional[List[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    if args.command == "simulate":
        return args.fn(None, args)
    root = Path(os.path.expanduser(args.repo)).resolve() if args.repo else find_repo_root()
    cfg = Config.load(root)
    return int(args.fn(cfg, args) or 0)


if __name__ == "__main__":
    sys.exit(main())
