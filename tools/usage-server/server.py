#!/usr/bin/env python3
"""
Usage server for the Kublet claude-usage app.

Fetches Claude Code usage by running `claude /usage` and parsing the output,
then serves the percentages as JSON for the Kublet device to display.

Environment variables:
  CLAUDE_BIN  - Path to claude binary (default: auto-detect)
  PORT        - Server port (default: 8198)

Usage:
  python server.py
"""

import http.server
import json
import os
import subprocess
import time
from datetime import datetime
from pathlib import Path

PORT = int(os.environ.get("PORT", "8198"))
SCRIPT_DIR = Path(__file__).parent
FETCH_SCRIPT = SCRIPT_DIR / "fetch_usage.sh"

# Cache results for 5 minutes
_cache = {"data": None, "timestamp": 0}
CACHE_TTL = 300


def get_usage_data() -> dict:
    """Get session and weekly usage percentages by running fetch_usage.sh."""
    now = time.time()
    if _cache["data"] and (now - _cache["timestamp"]) < CACHE_TTL:
        return _cache["data"]

    try:
        result = subprocess.run(
            [str(FETCH_SCRIPT)],
            capture_output=True,
            text=True,
            timeout=30,
            env={**os.environ, "CLAUDECODE": ""},
        )
        data = json.loads(result.stdout.strip())
        print(f"[{datetime.now().strftime('%H:%M:%S')}] Fetched: session={data['session']['percent']}% weekly={data['weekly']['percent']}%")
    except (subprocess.TimeoutExpired, json.JSONDecodeError, KeyError) as e:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] Fetch error: {e}")
        data = {
            "session": {"percent": 0},
            "weekly": {"percent": 0},
        }

    _cache["data"] = data
    _cache["timestamp"] = now
    return data


class UsageHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/usage":
            data = get_usage_data()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(data).encode())
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        print(f"[{datetime.now().strftime('%H:%M:%S')}] {format % args}")


if __name__ == "__main__":
    print(f"Usage server running on http://0.0.0.0:{PORT}/usage")
    print(f"Using fetch script: {FETCH_SCRIPT}")
    server = http.server.HTTPServer(("0.0.0.0", PORT), UsageHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down")
        server.server_close()
