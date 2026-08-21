"""Claude Code usage endpoint."""

import json
import os
import subprocess
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

_DIR = Path(__file__).parent

# "Aug 21 at 12:30am" and "Aug 24 at 10am" both occur
_RESET_FORMATS = ("%b %d at %I:%M%p", "%b %d at %I%p")

_EMPTY = {
    "session": {"percent": 0, "resets_at": "", "resets_in": 0},
    "weekly": {"percent": 0, "resets_at": "", "resets_in": 0},
    "timezone": "",
}


def _parse_reset(text: str, timezone: str) -> int:
    """Return seconds until the reset time, or 0 when it cannot be read.

    The CLI prints no year, so this assumes the next occurrence: it takes the
    current year first, and adds one year when that date already passed.
    """
    if not text:
        return 0

    try:
        tz = ZoneInfo(timezone) if timezone else None
    except (ZoneInfoNotFoundError, ValueError):
        tz = None

    now = datetime.now(tz)
    for fmt in _RESET_FORMATS:
        try:
            parsed = datetime.strptime(text.strip(), fmt)
        except ValueError:
            continue

        target = parsed.replace(year=now.year, tzinfo=tz)
        if (now - target).total_seconds() > 86400:
            target = target.replace(year=now.year + 1)
        return max(0, int((target - now).total_seconds()))

    return 0


def get_usage_data(log, cached, **_kwargs) -> dict:
    """Fetch Claude Code usage by running fetch_claude_usage.sh."""

    def _fetch():
        script = _DIR / "fetch_claude_usage.sh"
        try:
            result = subprocess.run(
                [str(script)],
                capture_output=True,
                text=True,
                timeout=60,
                env={**os.environ, "CLAUDECODE": ""},
            )
            data = json.loads(result.stdout.strip())
            timezone = data.get("timezone", "")

            for window in ("session", "weekly"):
                entry = data.setdefault(window, {})
                entry.setdefault("percent", 0)
                entry.setdefault("resets_at", "")
                entry["resets_in"] = _parse_reset(entry["resets_at"], timezone)

            log(
                f"usage: session={data['session']['percent']}%"
                f" (resets in {data['session']['resets_in']}s)"
                f" weekly={data['weekly']['percent']}%"
                f" (resets in {data['weekly']['resets_in']}s)"
            )
            return data
        except (subprocess.TimeoutExpired, json.JSONDecodeError, KeyError) as e:
            log(f"usage fetch error: {e}")
            return dict(_EMPTY)

    return cached("usage", 300, _fetch)
