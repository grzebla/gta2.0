"""Configuration: built-in defaults, overridden by <state_dir>/config.json (local, gitignored).

Nothing user-specific is hard-coded: paths use ~ or are relative to the repository root.
"""

from __future__ import annotations

import copy
import json
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Optional

DEFAULTS: Dict[str, Any] = {
    "github_repo": "grzebla/gta2.0",
    "state_dir": ".orchestrator",
    # Agent worktrees live OUTSIDE the main checkout so tools that walk up the directory tree
    # (CLAUDE.md discovery, git) never see two copies of the project.
    "worktrees_dir": "../gta2.0-agents",
    "base_branch": "feature/orchestrator",
    "integration_branch": "agents/integration",
    "remote": "origin",
    # Pushing the integration branch triggers native-ci through its draft PR. CI minutes on a
    # private repo are limited, so pushes are rate limited.
    "push_integration": True,
    "min_push_interval_s": 6 * 3600,
    "open_draft_pr": True,
    "astra": {
        "agent": "codex",
        "command": "codex",
        "model": "gpt-6-astra",
        "reasoning_effort": "high",
        "timeout_s": 1800,
        "codex_home": "~/.codex",
    },
    "claude": {
        "agent": "claude",
        "command": "claude",
        "model": "opus",
        "effort": "high",
        "timeout_s": 5400,
        "probe_model": "haiku",
        "sandbox": True,
        "allow_web": False,
        "sandbox_domains": [
            "github.com",
            "codeload.github.com",
            "objects.githubusercontent.com",
            "release-assets.githubusercontent.com",
            "raw.githubusercontent.com",
            "pypi.org",
            "files.pythonhosted.org",
        ],
    },
    "limits": {
        "max_attempts_per_task": 3,
        "max_agent_failures_per_task": 3,
        "max_no_progress": 2,
        "max_consecutive_failures": 4,
        "max_runs_per_day": 40,
        "max_tasks_per_plan": 5,
        "arch_review_every": 5,
        "max_ci_failures": 3,
        "quota_backoff_s": [900, 1800, 3600, 7200, 14400],
        "quota_max_probes": 12,
        "quota_reset_margin_s": 120,
        "max_crash_restarts_per_hour": 5,
        "poll_s": 30,
    },
    "verify": {
        "timeout_s": 2400,
        "commands": [
            ["cmake", "--preset", "debug"],
            ["cmake", "--build", "--preset", "debug"],
            ["ctest", "--preset", "debug", "--output-on-failure"],
        ],
        "env": {"GTA20_GAME_DIR": "~/Games/GTA2"},
        "max_file_bytes": 5 * 1024 * 1024,
    },
    # Paths the developer agent must not change (enforced by Claude permissions AND by the
    # independent diff check). Changing them needs the human.
    "protected_paths": [
        "tools/orchestrator/",
        ".orchestrator/",
        ".claude/",
        "docs/agent-memory/",
        "infra/",
        "scripts/aws/",
        ".github/workflows/gfx-",
        ".github/workflows/infra-lint.yml",
        "CLAUDE.md",
        ".gitignore",
    ],
    "game_data_extensions": [
        ".gmp", ".sty", ".scr", ".gxt", ".sdt", ".mmp", ".bik", ".seq", ".gci", ".rep", ".raw",
        ".tga", ".wav", ".m3d", ".exe", ".dll",
    ],
}


def deep_merge(base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
    out = copy.deepcopy(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(out.get(key), dict):
            out[key] = deep_merge(out[key], value)
        else:
            out[key] = value
    return out


def expand(path: str, root: Path) -> Path:
    p = Path(os.path.expanduser(path))
    return p if p.is_absolute() else (root / p).resolve()


def find_repo_root(start: Optional[Path] = None) -> Path:
    env = os.environ.get("GTA20_REPO_ROOT")
    if env:
        return Path(os.path.expanduser(env)).resolve()
    cwd = start or Path.cwd()
    out = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=cwd, capture_output=True, text=True)
    if out.returncode != 0:
        raise SystemExit("not inside a git repository; pass --repo or set GTA20_REPO_ROOT")
    top = Path(out.stdout.strip())
    # Inside an agent worktree? Resolve to the main checkout.
    common = subprocess.run(["git", "rev-parse", "--git-common-dir"], cwd=top, capture_output=True, text=True)
    if common.returncode == 0:
        common_dir = (top / common.stdout.strip()).resolve()
        if common_dir.name == ".git":
            return common_dir.parent
    return top


@dataclass
class Config:
    repo_root: Path
    data: Dict[str, Any]

    @classmethod
    def load(cls, repo_root: Path, overrides: Optional[Dict[str, Any]] = None) -> "Config":
        data = copy.deepcopy(DEFAULTS)
        local = repo_root / data["state_dir"] / "config.json"
        if local.exists():
            data = deep_merge(data, json.loads(local.read_text()))
        if overrides:
            data = deep_merge(data, overrides)
        return cls(repo_root=repo_root, data=data)

    def __getitem__(self, key: str) -> Any:
        return self.data[key]

    @property
    def state_dir(self) -> Path:
        return expand(self.data["state_dir"], self.repo_root)

    @property
    def worktrees_dir(self) -> Path:
        return expand(self.data["worktrees_dir"], self.repo_root)

    def worktree(self, name: str) -> Path:
        return self.worktrees_dir / name

    @property
    def memory_dir_name(self) -> str:
        return "docs/agent-memory"

    def verify_env(self) -> Dict[str, str]:
        return {k: os.path.expanduser(v) for k, v in self.data["verify"].get("env", {}).items()}
