"""Small shared helpers: time, redaction of secrets, JSON."""

from __future__ import annotations

import datetime as _dt
import json
import re
import time
from typing import Any

SECRET_PATTERNS = [
    re.compile(r"AKIA[0-9A-Z]{16}"),
    re.compile(r"ASIA[0-9A-Z]{16}"),
    re.compile(r"(?i)aws_secret_access_key\s*[=:]\s*\S+"),
    re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----"),
    re.compile(r"\bgh[pousr]_[A-Za-z0-9]{30,}"),
    re.compile(r"\bgithub_pat_[A-Za-z0-9_]{40,}"),
    re.compile(r"\bsk-(?:ant-|proj-)?[A-Za-z0-9_-]{20,}"),
    re.compile(r"(?i)\b(authorization|x-api-key)\s*:\s*\S+"),
    re.compile(r"(?i)\b(api[_-]?key|token|secret|password)\s*[=:]\s*['\"]?[A-Za-z0-9/+_.-]{16,}"),
]


def now() -> float:
    return time.time()


def iso(ts: float | None = None) -> str:
    t = _dt.datetime.fromtimestamp(ts if ts is not None else time.time(), tz=_dt.timezone.utc)
    return t.strftime("%Y-%m-%dT%H:%M:%SZ")


def local_time(ts: float | None) -> str:
    if ts is None:
        return "-"
    return _dt.datetime.fromtimestamp(ts).strftime("%Y-%m-%d %H:%M:%S")


def redact(text: str) -> str:
    for pattern in SECRET_PATTERNS:
        text = pattern.sub("[REDACTED]", text)
    return text


def find_secrets(text: str) -> list:
    hits = []
    for pattern in SECRET_PATTERNS:
        for m in pattern.finditer(text):
            hits.append(m.group(0)[:12] + "...")
    return hits


def dumps(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True)


def loads(text: str | None, default: Any = None) -> Any:
    if text is None or text == "":
        return default
    try:
        return json.loads(text)
    except (TypeError, ValueError):
        return default


def truncate(text: str, limit: int) -> str:
    if len(text) <= limit:
        return text
    head = text[: limit // 2]
    tail = text[-limit // 2 :]
    return head + "\n\n[... %d characters omitted ...]\n\n" % (len(text) - limit) + tail
