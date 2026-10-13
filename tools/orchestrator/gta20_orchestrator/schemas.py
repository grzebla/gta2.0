"""JSON Schemas for structured agent output.

Strict style (every property required, additionalProperties false, nullable via type lists) so
the same schema works for Codex --output-schema and Claude --json-schema.
"""

from __future__ import annotations

from typing import Any, Dict


def _obj(props: Dict[str, Any]) -> Dict[str, Any]:
    return {"type": "object", "additionalProperties": False, "required": list(props), "properties": props}


STR = {"type": "string"}
NSTR = {"type": ["string", "null"]}
STRS = {"type": "array", "items": STR}

# A run of the original game on the EC2 test host; the orchestrator validates and executes it.
GFX_REQUESTS = {"type": "array", "items": _obj({
    "mode": {"type": "string", "enum": ["probe", "compat", "capture"]},
    "target": STR,
    "reason": STR,
})}

MEMORY_UPDATES = _obj({
    "project": NSTR,
    "architecture": NSTR,
    "roadmap": NSTR,
    "known_issues": NSTR,
    "test_status": NSTR,
})

TASK = _obj({
    "key": STR,
    "title": STR,
    "goal": STR,
    "acceptance_criteria": STRS,
    "constraints": STRS,
    "context": STR,
    "required_tests": STRS,
    "priority": {"type": "integer"},
    "depends_on": STRS,
})

ASTRA_PLAN = _obj({
    "assessment": STR,
    "milestone": {"anyOf": [_obj({
        "id": STR,
        "title": STR,
        "goal": STR,
        "acceptance_criteria": STRS,
        "status": {"type": "string", "enum": ["active", "done"]},
    }), {"type": "null"}]},
    "tasks": {"type": "array", "items": TASK},
    "cancel_tasks": STRS,
    "memory_updates": MEMORY_UPDATES,
    "needs_human": NSTR,
    "release_candidate": {"type": "boolean"},
    "gfx_requests": GFX_REQUESTS,
})

ASTRA_REVIEW = _obj({
    "decision": {"type": "string", "enum": ["APPROVED", "CHANGES_REQUESTED", "BLOCKED", "NEEDS_HUMAN"]},
    "rationale": STR,
    "criteria": {"type": "array", "items": _obj({"criterion": STR, "met": {"type": "boolean"}, "evidence": STR})},
    "required_changes": STRS,
    "architecture_notes": NSTR,
    "memory_updates": MEMORY_UPDATES,
    "progress_entry": NSTR,
    "human_question": NSTR,
    "gfx_requests": GFX_REQUESTS,
})

CLAUDE_REPORT = _obj({
    "task_id": STR,
    "status": {"type": "string", "enum": ["completed", "partial", "blocked"]},
    "summary": STR,
    "changes": STRS,
    "files_changed": STRS,
    "commits": STRS,
    "tests": {"type": "array", "items": _obj({"command": STR, "passed": {"type": "boolean"}, "summary": STR})},
    "benchmarks": {"type": "array", "items": _obj({"name": STR, "value": STR, "notes": STR})},
    "unresolved": STRS,
    "questions_for_astra": STRS,
    "gfx_requests": GFX_REQUESTS,
})

# Resolution of a conflicted merge of the target branch into the integration branch.
SYNC_REPORT = _obj({
    "status": {"type": "string", "enum": ["resolved", "cannot_resolve"]},
    "summary": STR,
    "files": {"type": "array", "items": _obj({"file": STR, "resolution": STR})},
    "tests": STRS,
})

GATEKEEPER_REVIEW = _obj({
    "decision": {"type": "string", "enum": ["MERGE", "REQUEST_CHANGES", "HOLD", "NEEDS_HUMAN"]},
    "rationale": STR,
    "findings": {"type": "array", "items": _obj({
        "severity": {"type": "string", "enum": ["blocker", "major", "minor"]},
        "file": STR,
        "issue": STR,
    })},
    "merge_subject": STR,
    "changelog": NSTR,
    "human_question": NSTR,
})
