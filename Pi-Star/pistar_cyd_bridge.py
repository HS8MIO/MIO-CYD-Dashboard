#!/usr/bin/env python3
"""Foreground, read-only Pi-Star MMDVM log to CYD serial bridge.

Reads /var/log/pi-star/MMDVM-*.log and writes newline-delimited dashboard
updates to the confirmed CYD CH341 port. It never opens the MMDVM modem port
or changes Pi-Star configuration. Stop with Ctrl+C.
"""

import glob
import os
import re
import stat
import sys
import termios
import time

LOG_GLOB = "/var/log/pi-star/MMDVM-*.log"
SERIAL_PATH = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
TAIL_BYTES = 65536

DEFAULT_STATE = {
    "CALL": "N0CALL",
    "MODE": "IDLE",
    "STATUS": "WAITING",
    "TX": "IDLE",
    "RX": "--",
}
state = DEFAULT_STATE.copy()


def reset_state():
    state.clear()
    state.update(DEFAULT_STATE)


def latest_log_path():
    paths = glob.glob(LOG_GLOB)
    if not paths:
        raise FileNotFoundError("No MMDVM log found at " + LOG_GLOB)
    return max(paths, key=os.path.getmtime)


def wait_for_serial_device():
    while True:
        try:
            device_stat = os.stat(SERIAL_PATH)
        except FileNotFoundError:
            print("Waiting for serial device {}".format(SERIAL_PATH), flush=True)
            time.sleep(2)
            continue
        if not stat.S_ISCHR(device_stat.st_mode):
            raise RuntimeError(SERIAL_PATH + " is not a character device")
        return


def wait_for_log_file():
    while True:
        try:
            return latest_log_path()
        except FileNotFoundError:
            print("Waiting for an MMDVM log", flush=True)
            time.sleep(2)


def clean_value(value, limit):
    value = re.sub(r"[^A-Za-z0-9 _./>+:-]", "", value)
    return value.strip()[:limit]


def parse_log_line(line):
    changed = False

    # Examples: Linked to XLX568 C; Linking to XLX568 C.
    link = re.search(r'D-Star link status set to "([^"]+)"', line)
    if link:
        link_text = link.group(1).strip().lower()
        new_status = "CONNECTED" if link_text.startswith("linked to") else "WAITING"
        if state["STATUS"] != new_status:
            state["STATUS"] = new_status
            changed = True

    if "Logged into the master successfully" in line:
        if state["STATUS"] != "CONNECTED":
            state["STATUS"] = "CONNECTED"
            changed = True
    elif "logged out" in line.lower() or "disconnected from the master" in line.lower():
        if state["STATUS"] != "OFFLINE":
            state["STATUS"] = "OFFLINE"
            changed = True

    # Examples:
    # D-Star, received network header from HS8MIO /INFO to CQCQCQ via XLX568 C
    # DMR Slot 2, received RF voice header from M1ABC to 5000
    header = re.search(
        r"\b(D-Star|DMR Slot [12]), received (network|RF) "
        r"(?:voice )?header from\s+(\S+)"
        r"(?:\s+/\s*(\S*))?\s+to\s+(.+?)"
        r"(?:\s+via\s+.+)?$",
        line,
    )
    if header:
        mode, source, callsign, suffix, target = header.groups()
        new_mode = "D-STAR" if mode == "D-Star" else mode.replace("Slot ", "S")
        suffix = "/" + suffix if suffix else ""
        last_heard = callsign + suffix + ">" + target.strip()
        updates = {
            "CALL": clean_value(callsign, 11),
            "MODE": clean_value(new_mode, 11),
            "TX": "TRANSMITTING" if source == "network" else "RECEIVING",
            "RX": clean_value(last_heard, 24),
        }
        for key, value in updates.items():
            if state[key] != value:
                state[key] = value
                changed = True

    if "received network end of transmission" in line or \
       "received RF end of transmission" in line or \
       "received network end of voice transmission" in line or \
       "received RF end of voice transmission" in line or \
       "network end of transmission" in line:
        if state["TX"] != "IDLE":
            state["TX"] = "IDLE"
            changed = True

    # D-Star slow data can carry a short text message such as the sample log's
    # "It is 02:00 PM". Keep it in the LAST RX field when available.
    slow_text = re.search(r'D-Star, slow data text = "([^"]*)"', line)
    if slow_text:
        text = clean_value(slow_text.group(1), 24)
        if text and state["RX"] != text:
            state["RX"] = text
            changed = True

    if "Mode set to Idle" in line and state["TX"] != "IDLE":
        state["TX"] = "IDLE"
        changed = True
    elif "Mode set to D-Star" in line and state["MODE"] != "D-STAR":
        state["MODE"] = "D-STAR"
        changed = True

    return changed


def configure_serial(fd):
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)


def get_pi_ip():
    try:
        import subprocess
        result = subprocess.check_output(
            ["ip", "-4", "route", "get", "1.1.1.1"],
            text=True, stderr=subprocess.DEVNULL
        )
        match = re.search(r"\bsrc\s+(\d+\.\d+\.\d+\.\d+)", result)
        return match.group(1) if match else "0.0.0.0"
    except Exception:
        return "0.0.0.0"


def send_state(fd, log=True):
    fields = ";".join("{}={}".format(key, state[key]) for key in
                      ("CALL", "MODE", "STATUS", "TX", "RX"))
    fields += ";TIME={}".format(time.strftime("%H:%M"))
    fields += ";IP={}".format(get_pi_ip())
    payload = (fields + "\n").encode("ascii", "replace")
    os.write(fd, payload)
    if log:
        print("CYD <- " + fields, flush=True)

def load_tail_and_follow(path):
    log_file = open(path, "r", encoding="utf-8", errors="replace")
    size = os.path.getsize(path)
    start = max(0, size - TAIL_BYTES)
    if start:
        log_file.seek(start)
        log_file.readline()  # Discard a possibly partial first line.
    for line in log_file:
        parse_log_line(line)
    log_file.seek(0, os.SEEK_END)
    return log_file


def main():
    path = wait_for_log_file()
    wait_for_serial_device()

    fd = os.open(SERIAL_PATH, os.O_WRONLY | os.O_NOCTTY)
    log_file = None
    try:
        configure_serial(fd)
        print("Serial open; waiting 2 seconds for CYD startup", flush=True)
        time.sleep(2)

        log_file = load_tail_and_follow(path)
        send_state(fd)
        print("Reading {} (Ctrl+C to stop)".format(path), flush=True)

        last_clock_update = time.monotonic()

        while True:
            line = log_file.readline()

            if line:
                if parse_log_line(line):
                    send_state(fd)
                    last_clock_update = time.monotonic()
            else:
                time.sleep(0.15)
                newest_path = latest_log_path()
                truncated = os.path.getsize(path) < log_file.tell()

                if newest_path != path or truncated:
                    log_file.close()
                    path = newest_path
                    reset_state()
                    log_file = load_tail_and_follow(path)
                    print("Following rotated log {}".format(path), flush=True)
                    send_state(fd)
                    last_clock_update = time.monotonic()

            now = time.monotonic()
            if now - last_clock_update >= 1:
                send_state(fd, log=False)
                last_clock_update = now
    except KeyboardInterrupt:
        print("\nStopped; no Pi-Star configuration was changed.", flush=True)
    finally:
        if log_file is not None:
            log_file.close()
        os.close(fd)


if __name__ == "__main__":
    main()
