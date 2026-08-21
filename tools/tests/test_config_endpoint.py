"""Tests for the OTA config endpoint client and the 'dev config' command."""

import urllib.error
import urllib.parse

import pytest

from kublet_dev import network


class FakeResponse:
    """Minimal stand-in for the object urlopen returns."""

    def __init__(self, body: bytes):
        self._body = body

    def read(self) -> bytes:
        return self._body

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False


def test_read_config_parses_json(monkeypatch):
    """read_config returns the decoded JSON body."""
    monkeypatch.setattr(
        network.urllib.request,
        "urlopen",
        lambda *_a, **_k: FakeResponse(b'{"server_url":"http://10.0.0.5:8198"}'),
    )
    assert network.read_config("10.0.0.9") == {"server_url": "http://10.0.0.5:8198"}


def test_read_config_returns_none_when_unreachable(monkeypatch):
    """A device without the endpoint yields None, not an exception."""

    def refuse(*_a, **_k):
        raise urllib.error.URLError("connection refused")

    monkeypatch.setattr(network.urllib.request, "urlopen", refuse)
    assert network.read_config("10.0.0.9") is None


def test_read_config_returns_none_on_bad_json(monkeypatch):
    """Old firmware may answer 200 with a non-JSON body."""
    monkeypatch.setattr(
        network.urllib.request, "urlopen", lambda *_a, **_k: FakeResponse(b"OK")
    )
    assert network.read_config("10.0.0.9") is None


def test_send_config_posts_form_encoded_body(monkeypatch):
    """Values are sent as a urlencoded POST body to /config."""
    captured = {}

    def capture(req, *_a, **_k):
        captured["url"] = req.full_url
        captured["method"] = req.get_method()
        captured["body"] = req.data
        captured["type"] = req.get_header("Content-type")
        return FakeResponse(b"OK restarting")

    monkeypatch.setattr(network.urllib.request, "urlopen", capture)

    assert network.send_config("10.0.0.9", {"server_url": "http://10.0.0.5:8198"})

    assert captured["url"] == "http://10.0.0.9/config"
    assert captured["method"] == "POST"
    assert captured["type"] == "application/x-www-form-urlencoded"
    parsed = urllib.parse.parse_qs(captured["body"].decode())
    assert parsed["server_url"] == ["http://10.0.0.5:8198"]
    assert "restart" not in parsed


def test_send_config_no_restart_flag(monkeypatch):
    """restart=0 travels with the body when the caller opts out."""
    captured = {}

    def capture(req, *_a, **_k):
        captured["body"] = req.data
        return FakeResponse(b"OK")

    monkeypatch.setattr(network.urllib.request, "urlopen", capture)

    assert network.send_config("10.0.0.9", {"server_url": "x"}, restart=False)
    parsed = urllib.parse.parse_qs(captured["body"].decode())
    assert parsed["restart"] == ["0"]


def test_send_config_returns_false_on_error(monkeypatch):
    """An unreachable device reports failure instead of raising."""

    def refuse(*_a, **_k):
        raise urllib.error.URLError("no route to host")

    monkeypatch.setattr(network.urllib.request, "urlopen", refuse)
    assert network.send_config("10.0.0.9", {"server_url": "x"}) is False


# ---------------------------------------------------------------------------
# CLI argument handling
# ---------------------------------------------------------------------------


def _run_cli(monkeypatch, argv, sent):
    """Run 'dev config' with the network layer replaced."""
    from kublet_dev import cli

    monkeypatch.setattr(cli.sys, "argv", ["dev", *argv])
    monkeypatch.setattr(cli, "resolve_device_ip", lambda d, ip: ip or "10.0.0.9")
    monkeypatch.setattr(
        cli,
        "send_config",
        lambda ip, values, restart=True: (
            sent.update(ip=ip, values=values, restart=restart) or True
        ),
    )
    cli.main()


def test_cli_writes_key_value_pair(monkeypatch):
    """'config kitchen key=value' sends that pair."""
    sent: dict = {}
    _run_cli(monkeypatch, ["config", "kitchen", "server_url=http://x:8198"], sent)
    assert sent["values"] == {"server_url": "http://x:8198"}
    assert sent["restart"] is True


def test_cli_accepts_pair_without_device_name(monkeypatch):
    """'config key=value' uses the default device, not a device named 'key=value'."""
    sent: dict = {}
    _run_cli(monkeypatch, ["config", "server_url=http://x:8198"], sent)
    assert sent["values"] == {"server_url": "http://x:8198"}


def test_cli_resolves_auto_server_url(monkeypatch):
    """server_url=auto expands to this machine's IP on port 8198."""
    from kublet_dev import cli

    monkeypatch.setattr(cli, "get_local_ip", lambda: "10.0.0.5")
    sent: dict = {}
    _run_cli(monkeypatch, ["config", "kitchen", "server_url=auto"], sent)
    assert sent["values"] == {"server_url": "http://10.0.0.5:8198"}


def test_cli_no_restart_flag(monkeypatch):
    """--no-restart is passed through to the network layer."""
    sent: dict = {}
    _run_cli(monkeypatch, ["config", "kitchen", "a=b", "--no-restart"], sent)
    assert sent["restart"] is False


def test_cli_rejects_value_without_equals(monkeypatch):
    """A malformed pair exits with an error instead of silently passing."""
    sent: dict = {}
    with pytest.raises(SystemExit):
        _run_cli(monkeypatch, ["config", "kitchen", "server_url"], sent)
    assert sent == {}
