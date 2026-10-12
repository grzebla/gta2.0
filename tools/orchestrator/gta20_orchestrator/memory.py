"""Human-readable project memory in docs/agent-memory/*.md (on the integration branch).

"Current state" files are replaced wholesale by Astra (size-capped); DECISIONS.md and
PROGRESS.md are append-only logs written by the orchestrator from recorded decisions.
Astra receives the current files plus only the tail of the logs, never the whole history.
"""

from __future__ import annotations

from pathlib import Path
from typing import Dict, List, Optional

from .util import iso, redact

DIR = "docs/agent-memory"
REPLACEABLE = {
    "project": "PROJECT.md",
    "architecture": "ARCHITECTURE.md",
    "roadmap": "ROADMAP.md",
    "known_issues": "KNOWN_ISSUES.md",
    "test_status": "TEST_STATUS.md",
}
LOGS = {"decisions": "DECISIONS.md", "progress": "PROGRESS.md"}
MAX_FILE_CHARS = 20000


class MemoryError_(ValueError):
    pass


def path(wt: Path, name: str) -> Path:
    return wt / DIR / name


def read(wt: Path, name: str) -> str:
    p = path(wt, name)
    return p.read_text() if p.exists() else ""


def log_tail(text: str, entries: int) -> str:
    """Last N '## ' sections of an append-only log."""
    parts = text.split("\n## ")
    if len(parts) <= entries + 1:
        return text
    return "(older entries omitted)\n\n## " + "\n## ".join(parts[-entries:])


def context(wt: Path, log_entries: int = 8, max_chars: int = 12000) -> str:
    out: List[str] = []
    for key, name in list(REPLACEABLE.items()):
        text = read(wt, name).strip()
        if len(text) > max_chars:
            text = text[:max_chars] + "\n[... truncated ...]"
        out.append("### %s\n%s" % (name, text or "(empty)"))
    for key, name in LOGS.items():
        out.append("### %s (latest entries)\n%s" % (name, log_tail(read(wt, name).strip(), log_entries) or "(empty)"))
    return "\n\n".join(out)


def apply_updates(wt: Path, updates: Optional[Dict[str, Optional[str]]]) -> List[str]:
    changed: List[str] = []
    for key, content in (updates or {}).items():
        if content is None or key not in REPLACEABLE:
            continue
        content = redact(content).strip() + "\n"
        if len(content) > MAX_FILE_CHARS:
            raise MemoryError_("%s update too large (%d chars > %d)" % (key, len(content), MAX_FILE_CHARS))
        if not content.startswith("# "):
            content = "# %s\n\n%s" % (REPLACEABLE[key][:-3].replace("_", " ").title(), content)
        p = path(wt, REPLACEABLE[key])
        p.parent.mkdir(parents=True, exist_ok=True)
        if not p.exists() or p.read_text() != content:
            p.write_text(content)
            changed.append(str(p.relative_to(wt)))
    return changed


def append_log(wt: Path, log: str, heading: str, body: str) -> str:
    name = LOGS[log]
    p = path(wt, name)
    p.parent.mkdir(parents=True, exist_ok=True)
    if not p.exists():
        p.write_text("# %s\n" % name[:-3].title())
    with open(p, "a", encoding="utf-8") as fh:
        fh.write("\n## %s — %s\n\n%s\n" % (iso(), heading, redact(body).strip()))
    return str(p.relative_to(wt))
