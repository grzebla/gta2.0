"""Git plumbing: worktrees, task branches, integration. Never force-pushes, never rewrites history."""

from __future__ import annotations

import hashlib
import os
import subprocess
from pathlib import Path
from typing import List, Optional


class GitError(RuntimeError):
    pass


def git(cwd: Path, *args: str, check: bool = True) -> str:
    forced = any(a.startswith("--force") for a in args) or (args[:1] == ("push",) and (
        "-f" in args or any(a.startswith("+") for a in args)))
    if forced:
        raise GitError("force operations are not allowed: git %s" % " ".join(args))
    env = dict(os.environ)
    env.setdefault("GIT_TERMINAL_PROMPT", "0")
    out = subprocess.run(["git", *args], cwd=str(cwd), capture_output=True, text=True, env=env)
    if check and out.returncode != 0:
        raise GitError("git %s failed in %s: %s" % (" ".join(args), cwd, (out.stderr or out.stdout).strip()))
    return out.stdout.strip()


def ref_exists(cwd: Path, ref: str) -> bool:
    return subprocess.run(["git", "rev-parse", "--verify", "--quiet", ref], cwd=str(cwd),
                          capture_output=True).returncode == 0


def rev(cwd: Path, ref: str = "HEAD") -> str:
    return git(cwd, "rev-parse", ref)


def is_clean(cwd: Path) -> bool:
    return git(cwd, "status", "--porcelain") == ""


def status_short(cwd: Path) -> str:
    return git(cwd, "status", "--short")


def worktree_list(repo: Path) -> List[str]:
    out = git(repo, "worktree", "list", "--porcelain")
    return [line.split(" ", 1)[1] for line in out.splitlines() if line.startswith("worktree ")]


def ensure_branch(repo: Path, branch: str, start: str) -> None:
    if ref_exists(repo, "refs/heads/" + branch):
        return
    git(repo, "branch", branch, start)


def ensure_worktree(repo: Path, path: Path, *, branch: Optional[str] = None, detach: Optional[str] = None) -> Path:
    path = path.resolve()
    if str(path) not in [str(Path(p).resolve()) for p in worktree_list(repo)]:
        path.parent.mkdir(parents=True, exist_ok=True)
        if branch:
            git(repo, "worktree", "add", str(path), branch)
        else:
            git(repo, "worktree", "add", "--detach", str(path), detach or "HEAD")
    return path


def checkout_detached(wt: Path, commit: str) -> None:
    git(wt, "checkout", "--quiet", "--detach", commit)


def switch_task_branch(wt: Path, branch: str, start: str, stash_label: str) -> None:
    """Put the developer worktree on the task branch, preserving any leftovers of other tasks."""
    current = git(wt, "rev-parse", "--abbrev-ref", "HEAD")
    if current == branch:
        return
    if not is_clean(wt):
        git(wt, "stash", "push", "--include-untracked", "-m", stash_label)
    if ref_exists(wt, "refs/heads/" + branch):
        git(wt, "switch", "--quiet", branch)
    else:
        git(wt, "switch", "--quiet", "-c", branch, start)


def commits_between(cwd: Path, base: str, head: str) -> List[str]:
    out = git(cwd, "rev-list", "--reverse", "%s..%s" % (base, head))
    return [c for c in out.splitlines() if c]


def log_oneline(cwd: Path, base: str, head: str, limit: int = 30) -> str:
    return git(cwd, "log", "--oneline", "-n", str(limit), "%s..%s" % (base, head))


def changed_files(cwd: Path, base: str, head: str) -> List[str]:
    out = git(cwd, "diff", "--name-only", "--no-renames", base, head)
    return [f for f in out.splitlines() if f]


def diff_stat(cwd: Path, base: str, head: str) -> str:
    return git(cwd, "diff", "--stat", base, head)


def diff_text(cwd: Path, base: str, head: str) -> str:
    return git(cwd, "diff", base, head)


def diff_hash(cwd: Path, base: str, head: str) -> str:
    return hashlib.sha256(diff_text(cwd, base, head).encode("utf-8", "replace")).hexdigest()


def file_sizes(cwd: Path, head: str, files: List[str]) -> dict:
    sizes = {}
    for f in files:
        out = subprocess.run(["git", "cat-file", "-s", "%s:%s" % (head, f)], cwd=str(cwd),
                             capture_output=True, text=True)
        if out.returncode == 0:
            sizes[f] = int(out.stdout.strip())
    return sizes


def merge_into(wt: Path, branch: str, message: str) -> str:
    """Fast-forward when possible, otherwise a merge commit. Returns the new head."""
    ff = subprocess.run(["git", "merge", "--ff-only", "--quiet", branch], cwd=str(wt), capture_output=True, text=True)
    if ff.returncode != 0:
        git(wt, "merge", "--no-ff", "--quiet", "-m", message, branch)
    return rev(wt)


def push(repo: Path, remote: str, branch: str) -> None:
    git(repo, "push", "--quiet", remote, "refs/heads/%s:refs/heads/%s" % (branch, branch))


def commit_paths(wt: Path, paths: List[str], message: str) -> Optional[str]:
    git(wt, "add", "--", *paths)
    if subprocess.run(["git", "diff", "--cached", "--quiet"], cwd=str(wt)).returncode == 0:
        return None
    git(wt, "commit", "--quiet", "-m", message)
    return rev(wt)
