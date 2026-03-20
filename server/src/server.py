#!/usr/bin/env python3
"""
Unified Kublet app server.

Serves JSON endpoints for Kublet ESP32 apps. Each endpoint module registers
its own routes. Adding a new app is as simple as creating a module with
handler functions and adding them to the ROUTES dict below.

Current endpoints:
  GET /api/usage          — Claude Code session/weekly usage percentages
  GET /api/music          — Now-playing track info from Music.app
  GET /api/music/artwork  — 240x240 JPEG album artwork for current track

Environment variables:
  PORT       - Server port (default: 8198)
  CLAUDE_BIN - Path to claude binary (used by fetch_claude_usage.sh)

Usage:
  python server.py
"""

import http.server
import json
import os
import time
from datetime import datetime
from pathlib import Path
from urllib.parse import urlparse, parse_qs

from music import get_music_artwork, get_music_data
from music import register as music_register
from usage import get_usage_data

PORT = int(os.environ.get("PORT", "8198"))

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------


def log(msg: str):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {msg}")


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
# Route registry
# ---------------------------------------------------------------------------

# Inject shared dependencies into endpoint modules
music_register(log)

ROUTES = {
    "/api/usage": get_usage_data,
    "/api/music": get_music_data,
    "/api/music/artwork": get_music_artwork,
}


# ---------------------------------------------------------------------------
# HTTP server
# ---------------------------------------------------------------------------


class AppHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        handler = ROUTES.get(parsed.path)

        if handler is None:
            self.send_response(404)
            self.end_headers()
            return

        query_params = parse_qs(parsed.query)
        data = handler(log=log, cached=cached, query_params=query_params)

        if data is None:
            self.send_response(503)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"error": "not available"}).encode())
            return

        # Binary response (e.g. artwork image)
        if isinstance(data, dict) and "_binary" in data:
            path = data["_binary"]
            content_type = data.get("_content_type", "application/octet-stream")
            try:
                body = Path(path).read_bytes()
            except OSError:
                self.send_response(500)
                self.end_headers()
                return
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def log_message(self, format, *args):
        log(format % args)


def main():
    log(f"Kublet server running on http://0.0.0.0:{PORT}")
    for path in ROUTES:
        log(f"  {path}")
    server = http.server.HTTPServer(("0.0.0.0", PORT), AppHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down")
        server.server_close()


if __name__ == "__main__":
    main()
