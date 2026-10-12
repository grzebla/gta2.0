"""Read-mostly GitHub access through the official `gh` CLI. Always passes -R explicitly
(the checkout also has an `upstream` remote that gh could otherwise pick)."""

from __future__ import annotations

import json
import subprocess
from typing import Any, Dict, List, Optional


def gh(repo: str, *args: str, timeout: int = 60) -> Optional[Any]:
    try:
        out = subprocess.run(["gh", *args, "-R", repo], capture_output=True, text=True, timeout=timeout)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if out.returncode != 0:
        return None
    try:
        return json.loads(out.stdout) if out.stdout.strip() else None
    except ValueError:
        return out.stdout.strip()


def open_prs(repo: str) -> List[Dict[str, Any]]:
    return gh(repo, "pr", "list", "--state", "open", "--json",
              "number,title,headRefName,baseRefName,isDraft,url") or []


def ci_runs(repo: str, branch: Optional[str] = None, limit: int = 5) -> List[Dict[str, Any]]:
    args = ["run", "list", "--limit", str(limit), "--json",
            "databaseId,workflowName,headBranch,headSha,status,conclusion,createdAt,url"]
    if branch:
        args += ["--branch", branch]
    return gh(repo, *args) or []


def failed_jobs(repo: str, run_id: int) -> List[str]:
    data = gh(repo, "run", "view", str(run_id), "--json", "jobs") or {}
    return ["%s: %s" % (j.get("name"), j.get("conclusion")) for j in data.get("jobs", [])
            if j.get("conclusion") not in ("success", "skipped", None)]


def find_pr(repo: str, head: str) -> Optional[Dict[str, Any]]:
    prs = gh(repo, "pr", "list", "--state", "open", "--head", head, "--json", "number,url,isDraft") or []
    return prs[0] if prs else None


def create_draft_pr(repo: str, head: str, base: str, title: str, body: str) -> Optional[str]:
    try:
        out = subprocess.run(["gh", "pr", "create", "-R", repo, "--draft", "--head", head, "--base", base,
                              "--title", title, "--body", body], capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.TimeoutExpired):
        return None
    return out.stdout.strip() if out.returncode == 0 else None


def summarize(repo: str, integration_branch: str) -> str:
    lines = ["Open PRs:"]
    for pr in open_prs(repo):
        lines.append("- #%s %s (%s -> %s)%s" % (pr["number"], pr["title"], pr["headRefName"],
                                                pr["baseRefName"], " [draft]" if pr.get("isDraft") else ""))
    lines.append("\nLatest CI runs on %s:" % integration_branch)
    runs = ci_runs(repo, integration_branch, 5)
    if not runs:
        lines.append("- none yet")
    for r in runs:
        lines.append("- %s %s %s/%s sha=%s %s" % (r["createdAt"], r["workflowName"], r["status"],
                                                 r.get("conclusion") or "-", r["headSha"][:9], r["url"]))
        if r.get("conclusion") == "failure":
            for j in failed_jobs(repo, r["databaseId"]):
                lines.append("    failed job: " + j)
    return "\n".join(lines)
