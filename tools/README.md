# Kublet Dev Tools

CLI tool for building, flashing, and deploying Kublet ESP32 apps.

## Quick Start

```bash
./tools/dev <command> [args]
```

## Commands

| Command | Description |
|---|---|
| `build <app>` | Build/compile firmware for an app |
| `deploy <app>` | Build and OTA-deploy to a device |
| `init <app>` | Flash dev firmware with WiFi credentials via USB |
| `logs` | Stream serial logs from the Kublet via USB |
| `devices` | List registered devices |

## Examples

```bash
./tools/dev build music        # compile the music app
./tools/dev deploy music       # build + OTA deploy to device
./tools/dev init music         # flash via USB with WiFi setup
./tools/dev logs               # stream serial output
```

## Structure

```
tools/
├── dev                  # uv script entrypoint
├── src/kublet_dev/
│   ├── cli.py           # CLI argument parsing and command dispatch
│   ├── build.py         # Firmware compilation and OTA upload
│   ├── config.py        # Paths, constants, environment loading
│   ├── flash.py         # Serial flashing and NVS generation
│   ├── devices.py       # Device registration and IP resolution
│   └── network.py       # Network utilities (device discovery)
└── tests/
    ├── test_build.py
    ├── test_config.py
    └── test_devices.py
```

## Running Tests

```bash
uv run pytest tools/tests
```
