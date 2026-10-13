"""Read-mostly GitHub access through the official `gh` CLI. Always passes -R explicitly
(the checkout also has an `upstream` remote that gh could otherwise pick)."""

from __future__ import annotations

import json
import re
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


# ---------------------------------------------------------------------------- write port
def _run(repo: str, *args: str, timeout: int = 120) -> tuple:
    try:
        out = subprocess.run(["gh", *args, "-R", repo], capture_output=True, text=True, timeout=timeout)
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, "", str(exc)
    return out.returncode, out.stdout, out.stderr


PR_FIELDS = ("number,title,body,url,headRefName,baseRefName,headRefOid,isDraft,mergeable,mergeStateStatus,"
             "labels,author,createdAt")


ERROR_LINE = re.compile(r"\*\*\*Failed|FAILED|FAIL\b|Error|error[:\[]|expected|Assertion|SC\d{4}|\bnot ok\b")


class GitHubPort:
    """Everything the Gatekeeper does on GitHub. Merges never use --admin (no bypass of checks)
    and always pin --match-head-commit, so a push after the review cannot be merged unreviewed."""

    def __init__(self, repo: str):
        self.repo = repo

    def open_prs(self) -> List[Dict[str, Any]]:
        rc, out, _ = _run(self.repo, "pr", "list", "--state", "open", "--limit", "50", "--json", PR_FIELDS)
        return json.loads(out) if rc == 0 and out.strip() else []

    def checks(self, number: int) -> Optional[List[Dict[str, Any]]]:
        # Exit code is non-zero while checks fail or are pending; the JSON is still printed.
        rc, out, err = _run(self.repo, "pr", "checks", str(number), "--json", "name,state,bucket,link,workflow")
        if out.strip():
            try:
                return json.loads(out)
            except ValueError:
                return None
        return [] if "no checks reported" in (err or "") else None

    def failure_excerpts(self, checks: List[Dict[str, Any]], max_jobs: int = 4, max_lines: int = 30) -> str:
        """Error lines from the logs of failed CI jobs, so a fix task can be done without GitHub access."""
        parts: List[str] = []
        for c in [c for c in checks if c.get("bucket") == "fail"][:max_jobs]:
            m = re.search(r"/runs/(\d+)/job/(\d+)", c.get("link") or "")
            if not m:
                continue
            rc, out, _ = _run(self.repo, "run", "view", m.group(1), "--job", m.group(2), "--log-failed", timeout=180)
            if not out:
                continue
            lines: List[str] = []
            for raw in out.splitlines():
                text = raw.split("\t")[-1]
                text = re.sub(r"^\ufeff?\d{4}-\d\d-\d\dT[\d:.]+Z ?", "", text).rstrip()
                if ERROR_LINE.search(text) and text not in lines:
                    lines.append(text[:300])
                if len(lines) >= max_lines:
                    break
            if lines:
                parts.append("--- %s ---\n%s" % (c.get("name"), "\n".join(lines)))
        return "\n".join(parts)

    def mark_ready(self, number: int) -> bool:
        return _run(self.repo, "pr", "ready", str(number))[0] == 0

    def merge(self, number: int, head_sha: str, method: str, subject: str) -> tuple:
        if method not in ("merge", "squash", "rebase"):
            raise ValueError("unsupported merge method %s" % method)
        rc, out, err = _run(self.repo, "pr", "merge", str(number), "--" + method, "--match-head-commit", head_sha,
                            "--subject", subject)
        return rc == 0, (out + err).strip()

    def retarget(self, number: int, base: str) -> bool:
        return _run(self.repo, "pr", "edit", str(number), "--base", base)[0] == 0

    def comment(self, number: int, body: str) -> bool:
        return _run(self.repo, "pr", "comment", str(number), "--body", body)[0] == 0
