#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import glob
import os
import select
import sys
import termios
import time


def find_port() -> str:
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        raise SystemExit("No /dev/cu.usbmodem* device found")
    if len(ports) > 1:
        print(f"Using {ports[0]} (available: {', '.join(ports)})", file=sys.stderr)
    return ports[0]


def configure_serial(fd: int) -> None:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] &= ~(termios.PARENB | termios.CSTOPB | termios.CSIZE)
    attrs[2] |= termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def read_available(fd: int, timeout: float) -> bytes:
    readable, _, _ = select.select([fd], [], [], timeout)
    if not readable:
        return b""
    return os.read(fd, 4096)


def drain(fd: int, seconds: float) -> None:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if not read_available(fd, max(0.0, deadline - time.monotonic())):
            return


def run_command(port: str, command: str, timeout: float, idle_timeout: float) -> bytes:
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        configure_serial(fd)
        drain(fd, 0.2)
        os.write(fd, b"\r\n" + command.encode("ascii") + b"\r\n")

        output = bytearray()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            chunk = read_available(fd, min(idle_timeout, max(0.0, deadline - time.monotonic())))
            if not chunk:
                if output:
                    break
                continue
            output.extend(chunk)
        return bytes(output)
    finally:
        os.close(fd)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a Fruit Jam PebbleOS USB CDC shell command")
    parser.add_argument("command", help="Command to send, e.g. buttons, progress, esp, bt")
    parser.add_argument("-p", "--port", default=None, help="USB CDC device, e.g. /dev/cu.usbmodem101")
    parser.add_argument("--timeout", type=float, default=5.0, help="Total read timeout in seconds")
    parser.add_argument(
        "--idle-timeout",
        type=float,
        default=0.25,
        help="Stop after this many quiet seconds once output has started",
    )
    args = parser.parse_args()

    data = run_command(args.port or find_port(), args.command, args.timeout, args.idle_timeout)
    sys.stdout.buffer.write(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
