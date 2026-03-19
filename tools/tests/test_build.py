"""Tests for build helpers."""

from unittest.mock import MagicMock, patch

from kublet_dev.build import ota_send, write_user_setup
from kublet_dev.config import USER_SETUP_H


def test_write_user_setup(tmp_path):
    """User_Setup.h is written with 40 MHz SPI config."""
    app_dir = tmp_path / "myapp"
    tft_dir = app_dir / ".pio/libdeps/esp32dev/TFT_eSPI"
    tft_dir.mkdir(parents=True)

    write_user_setup(app_dir)

    setup = (tft_dir / "User_Setup.h").read_text()
    assert setup == USER_SETUP_H
    assert "SPI_FREQUENCY 40000000" in setup
    assert "ST7789_DRIVER" in setup


def test_ota_send_builds_multipart(tmp_path):
    """OTA send constructs correct multipart body."""
    firmware = tmp_path / "firmware.bin"
    firmware.write_bytes(b"\x00" * 100)

    with patch("kublet_dev.build.urllib.request.urlopen") as mock_urlopen:
        mock_resp = MagicMock()
        mock_resp.read.return_value = b"OK"
        mock_resp.__enter__ = lambda s: s
        mock_resp.__exit__ = MagicMock(return_value=False)
        mock_urlopen.return_value = mock_resp

        ota_send("192.168.1.50", firmware, "test-app")

        call_args = mock_urlopen.call_args
        req = call_args[0][0]
        assert req.full_url == "http://192.168.1.50/update"
        assert req.method == "POST"
        assert b'name="filedata"' in req.data
        assert b"firmware.bin" in req.data
