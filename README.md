# MIO CYD Dashboard for Pi-Star

[![Release](https://img.shields.io/badge/release-Stable%201.0-brightgreen)](https://github.com/HS8MIO/MIO-CYD-Dashboard/releases)

A serial dashboard for displaying Pi-Star MMDVM activity on an ESP32-based 2.8-inch CYD (Cheap Yellow Display).

**This project adds a separate display to an existing Pi-Star installation. It does not include a Pi-Star image.**

## Recommended hardware

- **Primary recommendation:** ESP32 CYD 2.8-inch (320×240) board.
- **Alternative to evaluate:** ESP32-S3 2.8-inch display boards. Pinout, display controller, and firmware compatibility must be checked for the exact board. The initial Stable 1.0 baseline was tested on the ESP32 CYD 2.8-inch board.

## What it displays

- Callsign and mode
- Link status
- TX activity / standby
- Last received station or slow-data text
- Pi-Star time and IP address

## Installation overview

1. Install Pi-Star and configure your MMDVM/D-Star/DMR system normally. Verify that Pi-Star is working before adding this display.
2. Connect the CYD to the Raspberry Pi using USB serial.
3. Install the Python bridge and systemd service from the `Pi-Star/` directory.
4. Build and upload the Arduino sketch from the `CYD/` directory to the display.
5. Follow the detailed guide in [docs/INSTALLATION.md](docs/INSTALLATION.md).

## Repository contents

- `Pi-Star/pistar_cyd_bridge.py` — reads MMDVM log activity and sends dashboard updates over serial.
- `Pi-Star/pistar-cyd-bridge.service` — systemd unit for the bridge.
- `CYD/CYD_28_MMDVM_Serial_Test_UI_Time_IP.ino` — ESP32 display firmware.
- `Checksums/SHA256.txt` — SHA-256 checksums for the distributed files.
- `docs/INSTALLATION.md` — installation guide.
- `BASELINE-INFO.txt` — Stable 1.0 baseline and verification notes.

## Stable 1.0 verification baseline

The initial baseline was tested on Raspberry Pi 4 Model B running Pi-Star 4.2.3, with an ESP32 CYD 2.8-inch display connected over USB serial at 115200 baud. Live activity was verified with D-Star links REF087 and XLX568 C.

The bridge reads MMDVM log files and writes to the CYD serial device. It does not open the MMDVM modem port or change Pi-Star configuration.

## Important notes

- Back up your Pi-Star system before making changes.
- Device names such as `/dev/ttyUSB0` may differ; confirm the correct serial device on your own Raspberry Pi.
- The provided systemd service uses `/dev/ttyUSB0` by default.
- The bridge expects Pi-Star MMDVM logs at `/var/log/pi-star/MMDVM-*.log`.
- This is an initial community release. Hardware variants beyond the tested CYD baseline may need firmware adaptation.

## Release

Stable 1.0 — 2026-10-31.

## Credits

Developed by HS8MIO / MIO TECH for the amateur-radio and Pi-Star community.
