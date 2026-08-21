"""Claude Code usage endpoint.

Claude Code sends the rate-limit windows to the statusline command on stdin.
`~/.claude/statusline.sh` writes them to `_USAGE_FILE`, and this module reads
that file.

The earlier source was `claude -p /usage`. Print mode no longer writes the
usage report, so the text parser always read 0%. The statusline JSON gives the
same two windows as structured data, and it costs no tokens.
"""

import json
import time
from pathlib import Path

_USAGE_FILE = Path.home() / ".claude" / "kublet-usage.json"


def _empty() -> dict:
    return {
        "session": {"percent": 0, "resets_at": 0, "resets_in": 0},
        "weekly": {"percent": 0, "resets_at": 0, "resets_in": 0},
        "updated_at": 0,
    }


def _window(entry: dict, now: int) -> dict:
    """Convert one statusline rate-limit window into the device payload.

    A window whose reset time already passed rolled over after the last
    statusline render. The percentage in the file is then stale, so this
    reports an empty window instead.
    """
    try:
        percent = int(round(float(entry.get("used_percentage") or 0)))
        resets_at = int(entry.get("resets_at") or 0)
    except (TypeError, ValueError):
        return {"percent": 0, "resets_at": 0, "resets_in": 0}

    resets_in = resets_at - now
    if resets_at <= 0 or resets_in <= 0:
        return {"percent": 0, "resets_at": resets_at, "resets_in": 0}

    return {
        "percent": max(0, min(100, percent)),
        "resets_at": resets_at,
        "resets_in": resets_in,
    }


def get_usage_data(log, cached, **_kwargs) -> dict:
    """Return the 5-hour and 7-day rate-limit windows for the device."""

    def _read():
        try:
            raw = json.loads(_USAGE_FILE.read_text())
        except (OSError, json.JSONDecodeError) as e:
            log(f"usage read error: {e}")
            return _empty()

        limits = raw.get("rate_limits") or {}
        now = int(time.time())
        data = {
            "session": _window(limits.get("five_hour") or {}, now),
            "weekly": _window(limits.get("seven_day") or {}, now),
            "updated_at": int(raw.get("updated_at") or 0),
        }

        age = now - data["updated_at"] if data["updated_at"] else -1
        log(
            f"usage: session={data['session']['percent']}%"
            f" (resets in {data['session']['resets_in']}s)"
            f" weekly={data['weekly']['percent']}%"
            f" (resets in {data['weekly']['resets_in']}s)"
            f" file age {age}s"
        )
        return data

    return cached("usage", 30, _read)
