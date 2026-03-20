#!/usr/bin/env python3
"""
Unified Kublet app server.

Serves JSON endpoints for Kublet ESP32 apps. Each endpoint has its own
handler function and cache TTL. Adding a new app endpoint is as simple
as writing a handler and adding it to the ROUTES dict.

Current endpoints:
  GET /api/usage        — Claude Code session/weekly usage percentages

Environment variables:
  PORT       - Server port (default: 8198)
  CLAUDE_BIN - Path to claude binary (used by fetch_claude_usage.sh)

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
from urllib.parse import urlparse, parse_qs

PORT = int(os.environ.get("PORT", "8198"))
SCRIPT_DIR = Path(__file__).parent

# ---------------------------------------------------------------------------
# Cache helper
# ---------------------------------------------------------------------------

_caches: dict[str, dict] = {}


def cached(key: str, ttl: int, fetch_fn):
    """Return cached data or call fetch_fn to refresh."""
    now = time.time()
    entry = _caches.get(key)
    if entry and (now - entry["ts"]) < ttl:
        return entry["data"]
    data = fetch_fn()
    _caches[key] = {"data": data, "ts": now}
    return data


# ---------------------------------------------------------------------------
# Handlers
# ---------------------------------------------------------------------------


def get_usage_data(**_kwargs) -> dict:
    """Fetch Claude Code usage by running fetch_claude_usage.sh."""

    def _fetch():
        script = SCRIPT_DIR / "fetch_claude_usage.sh"
        try:
            result = subprocess.run(
                [str(script)],
                capture_output=True,
                text=True,
                timeout=30,
                env={**os.environ, "CLAUDECODE": ""},
            )
            data = json.loads(result.stdout.strip())
            log(
                f"usage: session={data['session']['percent']}% weekly={data['weekly']['percent']}%"
            )
            return data
        except (subprocess.TimeoutExpired, json.JSONDecodeError, KeyError) as e:
            log(f"usage fetch error: {e}")
            return {"session": {"percent": 0}, "weekly": {"percent": 0}}

    return cached("usage", 300, _fetch)


# ---------------------------------------------------------------------------
# Route registry
# ---------------------------------------------------------------------------

ROUTES = {
    "/api/usage": {"handler": get_usage_data, "cache_ttl": 300},
}


# ---------------------------------------------------------------------------
# HTTP server
# ---------------------------------------------------------------------------


def log(msg: str):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {msg}")


class AppHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        route = ROUTES.get(parsed.path)

        if route is None:
            self.send_response(404)
            self.end_headers()
            return

        query_params = parse_qs(parsed.query)
        data = route["handler"](query_params=query_params)

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def log_message(self, format, *args):
        log(format % args)


if __name__ == "__main__":
    log(f"Kublet server running on http://0.0.0.0:{PORT}")
    for path in ROUTES:
        log(f"  {path}")
    server = http.server.HTTPServer(("0.0.0.0", PORT), AppHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down")
        server.server_close()
