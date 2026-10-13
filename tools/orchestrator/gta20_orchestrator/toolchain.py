"""Pinned helper tools the developer's sandbox cannot install itself (no package downloads
from inside the sandbox). Installed by the orchestrator into <state_dir>/tools-venv and put
first on PATH for both Claude and verification, so both use the same versions as CI."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from typing import Dict, Optional

# pip package -> version; TOOLS: executable -> the package that provides it.
PINNED = {"clang-format": "23.1.3", "clang-tidy": "22.1.8", "shellcheck-py": "0.10.0.1"}
TOOLS = {"clang-format": "clang-format", "clang-tidy": "clang-tidy", "shellcheck": "shellcheck-py"}


def tools_bin(state_dir: Path) -> Path:
    return state_dir / "tools-venv" / "bin"


def ensure(state_dir: Path) -> Optional[Path]:
    venv = state_dir / "tools-venv"
    bin_dir = tools_bin(state_dir)
    marker = venv / "pinned.txt"
    wanted = "\n".join("%s==%s" % kv for kv in sorted(PINNED.items()))
    if marker.exists() and marker.read_text() == wanted and all((bin_dir / t).exists() for t in TOOLS):
        return bin_dir
    try:
        if not (bin_dir / "python3").exists():
            subprocess.run([sys.executable, "-m", "venv", str(venv)], check=True, capture_output=True, timeout=300)
        subprocess.run([str(bin_dir / "python3"), "-m", "pip", "install", "--quiet", "--disable-pip-version-check",
                        "--only-binary=:all:"] + ["%s==%s" % kv for kv in sorted(PINNED.items())],
                       check=True, capture_output=True, timeout=600)
    except (OSError, subprocess.SubprocessError):
        return None
    marker.write_text(wanted)
    return bin_dir


def versions(bin_dir: Optional[Path]) -> Dict[str, str]:
    out: Dict[str, str] = {}
    if not bin_dir:
        return out
    for tool in TOOLS:
        try:
            r = subprocess.run([str(bin_dir / tool), "--version"], capture_output=True, text=True, timeout=30)
            out[tool] = r.stdout.strip()
        except OSError:
            out[tool] = "missing"
    return out
