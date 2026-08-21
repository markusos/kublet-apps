#!/usr/bin/env bash
# fetch_claude_usage.sh — Fetch Claude Code session and weekly usage percentages.
#
# Runs `claude -p /usage` in print mode and parses the plain-text output.
# Print mode needs no TTY and shows no folder-trust prompt, so the script
# works from any working directory, including one Claude Code has not seen.
#
# Requirements:
#   - claude CLI binary
#
# Environment variables:
#   CLAUDE_BIN  - Path to claude binary (default: auto-detect via PATH or ~/.local/bin)
#
# Output (stdout):
#   JSON: {"session":{"percent":N},"weekly":{"percent":N}}
#
# Exit codes:
#   0 - Success (JSON always printed, even on parse failure with 0% fallback)
#   1 - Claude binary not found

# Find claude binary
if [ -n "$CLAUDE_BIN" ]; then
    :
elif command -v claude &>/dev/null; then
    CLAUDE_BIN=$(command -v claude)
elif [ -x "$HOME/.local/bin/claude" ]; then
    CLAUDE_BIN="$HOME/.local/bin/claude"
else
    echo '{"session":{"percent":0},"weekly":{"percent":0}}'
    exit 1
fi

export CLAUDECODE=

# Print mode writes the usage report to stdout and exits
output=$("$CLAUDE_BIN" -p "/usage" 2>/dev/null)

# Expected lines:
#   Current session: 4% used · resets Aug 21 at 12:29am (America/New_York)
#   Current week (all models): 26% used · resets Aug 24 at 9:59am (America/New_York)
session_pct=$(echo "$output" | grep -oE 'Current session: [0-9]+%' | grep -oE '[0-9]+' | head -1)
weekly_pct=$(echo "$output" | grep -oE 'Current week \(all models\): [0-9]+%' | grep -oE '[0-9]+' | head -1)

# Fall back to positional parsing if the labels change
if [ -z "$session_pct" ] || [ -z "$weekly_pct" ]; then
    pcts=$(echo "$output" | grep -oE '[0-9]+% used' | head -2)
    session_pct=${session_pct:-$(echo "$pcts" | sed -n '1s/% used//p')}
    weekly_pct=${weekly_pct:-$(echo "$pcts" | sed -n '2s/% used//p')}
fi

# Fall back to 0 if parsing failed
session_pct=${session_pct:-0}
weekly_pct=${weekly_pct:-0}

echo "{\"session\":{\"percent\":${session_pct}},\"weekly\":{\"percent\":${weekly_pct}}}"
