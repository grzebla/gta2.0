"""macOS launchd integration (per-user LaunchAgent) and a pinned installed copy of the code.

The daemon runs from ~/.local/share/gta20-orchestrator/<version>, not from the repository, so
an agent working in the repo can never change the code that supervises it.
"""

from __future__ import annotations

import os
import plistlib
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional

from . import __version__

LABEL = "com.gta20.orchestrator"
PLIST = Path.home() / "Library" / "LaunchAgents" / (LABEL + ".plist")
INSTALL_ROOT = Path.home() / ".local" / "share" / "gta20-orchestrator"
LAUNCHER = Path.home() / ".local" / "bin" / "gta20-orchestrator"


def uid_domain() -> str:
    return "gui/%d" % os.getuid()


def launchctl(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["launchctl", *args], capture_output=True, text=True)


def loaded() -> bool:
    return launchctl("print", "%s/%s" % (uid_domain(), LABEL)).returncode == 0


def plist_dict(python: str, entry: Path, repo: Path, log_dir: Path) -> Dict:
    path = ":".join([str(Path.home() / ".local/bin"), "/opt/homebrew/bin", "/usr/local/bin", "/usr/bin", "/bin",
                     "/usr/sbin", "/sbin"])
    return {
        "Label": LABEL,
        "ProgramArguments": [python, "-I", str(entry), "--repo", str(repo), "daemon"],
        "WorkingDirectory": str(repo),
        "RunAtLoad": True,
        # Restart only after a crash / non-zero exit. A clean stop (exit 0) stays stopped.
        "KeepAlive": {"SuccessfulExit": False},
        "ThrottleInterval": 300,
        "ExitTimeOut": 120,
        "ProcessType": "Standard",
        "StandardOutPath": str(log_dir / "launchd.out.log"),
        "StandardErrorPath": str(log_dir / "launchd.err.log"),
        "EnvironmentVariables": {"PATH": path, "LANG": "en_US.UTF-8", "PYTHONUNBUFFERED": "1"},
    }


def install(repo: Path, state_dir: Path, source_pkg: Path, git_sha: str) -> Dict[str, str]:
    target = INSTALL_ROOT / ("%s-%s" % (__version__, git_sha[:9]))
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source_pkg, target / "gta20_orchestrator", ignore=shutil.ignore_patterns("__pycache__"))
    entry = target / "main.py"
    entry.write_text("import sys\nfrom pathlib import Path\nsys.path.insert(0, str(Path(__file__).parent))\n"
                     "from gta20_orchestrator.cli import main\nsys.exit(main())\n")
    current = INSTALL_ROOT / "current"
    if current.is_symlink() or current.exists():
        current.unlink()
    current.symlink_to(target)
    LAUNCHER.parent.mkdir(parents=True, exist_ok=True)
    LAUNCHER.write_text('#!/bin/sh\nexec %s -I "%s" --repo "%s" "$@"\n' % (sys.executable, INSTALL_ROOT / "current" / "main.py", repo))
    LAUNCHER.chmod(0o755)
    log_dir = state_dir / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    PLIST.parent.mkdir(parents=True, exist_ok=True)
    with open(PLIST, "wb") as fh:
        plistlib.dump(plist_dict(sys.executable, INSTALL_ROOT / "current" / "main.py", repo, log_dir), fh)
    return {"installed": str(target), "launcher": str(LAUNCHER), "plist": str(PLIST)}


def installed_repo() -> Optional[Path]:
    """Repository the installed LaunchAgent supervises (from its --repo argument)."""
    try:
        args = plistlib.loads(PLIST.read_bytes()).get("ProgramArguments", [])
        return Path(args[args.index("--repo") + 1]).resolve()
    except (OSError, ValueError, IndexError, plistlib.InvalidFileException):
        return None


def manages(repo: Path) -> bool:
    """True only when the installed service belongs to this repository and launchd use is allowed
    (tests set GTA20_NO_LAUNCHD=1 so they can never touch the real service)."""
    if os.environ.get("GTA20_NO_LAUNCHD") == "1" or not PLIST.exists():
        return False
    return installed_repo() == Path(repo).resolve()


def enable() -> List[str]:
    """Load the LaunchAgent (starts it now and at every login)."""
    msgs = []
    if os.environ.get("GTA20_NO_LAUNCHD") == "1":
        raise SystemExit("launchd use disabled (GTA20_NO_LAUNCHD=1)")
    if not PLIST.exists():
        raise SystemExit("service not installed: run `gta20-orchestrator service install` first")
    if loaded():
        r = launchctl("kickstart", "%s/%s" % (uid_domain(), LABEL))
        msgs.append("kickstart: %s" % (r.stderr.strip() or "ok"))
    else:
        r = launchctl("bootstrap", uid_domain(), str(PLIST))
        msgs.append("bootstrap: %s" % (r.stderr.strip() or "ok"))
    return msgs


def disable() -> str:
    if not loaded():
        return "not loaded"
    r = launchctl("bootout", "%s/%s" % (uid_domain(), LABEL))
    return r.stderr.strip() or "unloaded"


def uninstall() -> str:
    msg = disable()
    if PLIST.exists():
        PLIST.unlink()
    return msg + "; plist removed (installed code and state kept)"


def describe() -> str:
    lines = ["plist: %s (%s)" % (PLIST, "present" if PLIST.exists() else "absent"),
             "loaded in launchd: %s" % loaded(),
             "installed code: %s" % ((INSTALL_ROOT / "current").resolve() if (INSTALL_ROOT / "current").exists() else "-")]
    if loaded():
        out = launchctl("print", "%s/%s" % (uid_domain(), LABEL)).stdout
        for key in ("state =", "pid =", "last exit code ="):
            for line in out.splitlines():
                if line.strip().startswith(key):
                    lines.append("  " + line.strip())
                    break
    return "\n".join(lines)
