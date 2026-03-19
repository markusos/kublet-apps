#!/usr/bin/env python3
"""
Unified Kublet app server.

Serves JSON endpoints for Kublet ESP32 apps. Each endpoint has its own
handler function and cache TTL. Adding a new app endpoint is as simple
as writing a handler and adding it to the ROUTES dict.

Current endpoints:
  GET /api/usage        — Claude Code session/weekly usage percentages
  GET /api/chart        — Intraday stock chart data (default: VOO)
  GET /api/chart?ticker=SPY  — Custom ticker

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
from zoneinfo import ZoneInfo

import yfinance as yf

PORT = int(os.environ.get("PORT", "8198"))
SCRIPT_DIR = Path(__file__).parent
ET = ZoneInfo("America/New_York")

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
            log(f"usage: session={data['session']['percent']}% weekly={data['weekly']['percent']}%")
            return data
        except (subprocess.TimeoutExpired, json.JSONDecodeError, KeyError) as e:
            log(f"usage fetch error: {e}")
            return {"session": {"percent": 0}, "weekly": {"percent": 0}}

    return cached("usage", 300, _fetch)


def get_chart_data(*, query_params: dict | None = None, **_kwargs) -> dict:
    """Fetch intraday stock chart data via yfinance."""
    ticker = "VOO"
    if query_params and "ticker" in query_params:
        ticker = query_params["ticker"][0].upper()

    def _fetch():
        try:
            return _fetch_chart(ticker)
        except Exception as e:
            log(f"chart fetch error ({ticker}): {e}")
            return {
                "ticker": ticker,
                "open": 0,
                "last": 0,
                "pct": 0,
                "market_open": False,
                "points": [],
            }

    return cached(f"chart:{ticker}", 60, _fetch)


def _fetch_chart(ticker: str) -> dict:
    """Download intraday data and build compact chart payload."""
    now_et = datetime.now(ET)

    # Try today first; if empty (weekend/holiday), get last 5 days
    df = yf.download(ticker, period="1d", interval="5m", progress=False)
    if df.empty:
        df = yf.download(ticker, period="5d", interval="5m", progress=False)

    if df.empty:
        raise ValueError(f"No data returned for {ticker}")

    # Handle multi-level columns from yfinance (ticker in second level)
    if isinstance(df.columns, __import__("pandas").MultiIndex):
        df.columns = df.columns.get_level_values(0)

    # Filter to last trading day
    df.index = df.index.tz_convert(ET)
    last_date = df.index[-1].date()
    df = df[df.index.date == last_date]

    open_price = float(df["Open"].iloc[0])
    last_price = float(df["Close"].iloc[-1])
    pct = round((last_price - open_price) / open_price * 100, 2) if open_price else 0

    # Build points: [index, delta] where index = 5-min intervals from 9:30 ET
    market_open_time = df.index[0].replace(hour=9, minute=30, second=0, microsecond=0)
    points = []
    for ts, row in df.iterrows():
        minutes_since_open = (ts - market_open_time).total_seconds() / 60
        idx = int(round(minutes_since_open / 5))
        if 0 <= idx <= 77:
            delta = round(float(row["Close"]) - open_price, 2)
            points.append([idx, delta])

    # Determine if market is currently open
    is_today = last_date == now_et.date()
    weekday = now_et.weekday() < 5
    in_hours = 9 * 60 + 30 <= now_et.hour * 60 + now_et.minute <= 16 * 60
    market_open = is_today and weekday and in_hours

    log(f"chart: {ticker} open={open_price} last={last_price} pct={pct}% pts={len(points)} market={'open' if market_open else 'closed'}")

    return {
        "ticker": ticker,
        "open": round(open_price, 2),
        "last": round(last_price, 2),
        "pct": pct,
        "market_open": market_open,
        "points": points,
    }


# ---------------------------------------------------------------------------
# Route registry
# ---------------------------------------------------------------------------

ROUTES = {
    "/api/usage": {"handler": get_usage_data, "cache_ttl": 300},
    "/api/chart": {"handler": get_chart_data, "cache_ttl": 60},
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
