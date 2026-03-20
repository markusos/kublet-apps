# Kublet Server

HTTP server that provides JSON endpoints for Kublet ESP32 apps.

## Quick Start

```bash
./server/run
```

The server runs on port 8198 by default (override with `PORT` env var).

## Endpoints

| Endpoint | Description |
|---|---|
| `GET /api/usage` | Claude Code session/weekly usage percentages |
| `GET /api/music` | Now-playing track info from Music.app |
| `GET /api/music/artwork` | 240x240 JPEG album artwork for current track |

## Structure

```
server/
├── run              # uv script entrypoint
└── src/
    ├── server.py    # HTTP server, routing, caching
    ├── usage.py     # /api/usage handler
    ├── music.py     # /api/music + /api/music/artwork handlers
    └── fetch_claude_usage.sh
```

## Adding a New Endpoint

1. Create a new module in `src/` with a handler function:
   ```python
   def get_my_data(log, cached, **_kwargs) -> dict | None:
       return cached("my_key", 60, _fetch_my_data)
   ```
2. Import and add it to `ROUTES` in `server.py`.

## Environment Variables

- `PORT` — Server port (default: 8198)
- `CLAUDE_BIN` — Path to claude binary (used by `fetch_claude_usage.sh`)
