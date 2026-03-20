"""Build firmware and upload over WiFi OTA."""

import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

from kublet_dev.config import USER_SETUP_H, find_pio


def write_user_setup(app_dir: Path) -> None:
    """Write the TFT_eSPI User_Setup.h with correct Kublet pin config."""
    setup_path = app_dir / ".pio/libdeps/esp32dev/TFT_eSPI/User_Setup.h"
    if not setup_path.parent.exists():
        pio = find_pio()
        print("  Dependencies not installed. Running pio lib install...")
        subprocess.run([pio, "lib", "install"], cwd=app_dir, check=True)

    setup_path.write_text(USER_SETUP_H)
    print("  ✓ Wrote User_Setup.h (SPI @ 40 MHz)")


def build_firmware(app_dir: Path) -> Path:
    """Compile firmware and return path to the .bin file."""
    pio = find_pio()
    app_name = app_dir.name
    print(f"Building '{app_name}'...")

    write_user_setup(app_dir)

    print("  Compiling...")
    result = subprocess.run([pio, "run"], cwd=app_dir)
    if result.returncode != 0:
        print("  ✗ Build failed")
        sys.exit(1)

    firmware = app_dir / ".pio/build/esp32dev/firmware.bin"
    size_kb = firmware.stat().st_size / 1024
    print(f"  ✓ Build succeeded ({size_kb:.0f} KB)")
    return firmware


def ota_send(ip: str, firmware: Path, app_name: str) -> None:
    """Upload firmware to device over WiFi OTA."""
    size_kb = firmware.stat().st_size / 1024
    print(f"📡 Sending '{app_name}' ({size_kb:.0f} KB) to {ip}...")

    boundary = "----KubletOTA"
    firmware_data = firmware.read_bytes()

    body = (
        (
            f"--{boundary}\r\n"
            f'Content-Disposition: form-data; name="filedata"; filename="firmware.bin"\r\n'
            f"Content-Type: application/octet-stream\r\n"
            f"\r\n"
        ).encode()
        + firmware_data
        + f"\r\n--{boundary}--\r\n".encode()
    )

    url = f"http://{ip}/update"
    req = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
        method="POST",
    )

    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            result = resp.read().decode()
            print(f"  {result}")
            print(f"  ✅ '{app_name}' deployed to {ip}")
    except (TimeoutError, ConnectionResetError, urllib.error.URLError) as e:
        # ESP32 reboots after OTA, dropping the connection — treat as success
        if isinstance(e, urllib.error.URLError) and not isinstance(
            e.reason, (TimeoutError, ConnectionResetError, OSError)
        ):
            print(f"  ✗ Upload failed: {e}")
            sys.exit(1)
        print(f"  ✅ '{app_name}' deployed to {ip} (device rebooting)")
