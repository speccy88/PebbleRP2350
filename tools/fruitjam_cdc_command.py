#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import fcntl
import glob
import os
import select
import struct
import subprocess
import sys
import termios
import time
from typing import Optional


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
    modem_bits = struct.pack("I", termios.TIOCM_DTR | termios.TIOCM_RTS)
    fcntl.ioctl(fd, termios.TIOCMBIS, modem_bits)


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


def run_command(
    port: str,
    command: str,
    timeout: float,
    idle_timeout: float,
    stream: bool,
) -> bytes:
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
            if stream:
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
        return bytes(output)
    finally:
        os.close(fd)


def watch_duration_seconds(command: str) -> Optional[float]:
    parts = command.split()
    if not parts or parts[0] not in {"buttonwatch", "gpiowatch"}:
        return None

    duration_ms = 5000
    if len(parts) >= 2:
        try:
            duration_ms = int(parts[1], 10)
        except ValueError:
            duration_ms = 5000

    duration_ms = max(20, min(duration_ms, 30000))
    return duration_ms / 1000.0


def run_as_child(total_timeout: float) -> int:
    env = os.environ.copy()
    env["FRUITJAM_CDC_COMMAND_CHILD"] = "1"
    proc = subprocess.Popen([sys.executable, __file__, *sys.argv[1:]], env=env)
    try:
        return proc.wait(timeout=total_timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        print(f"Timed out after {total_timeout:.1f}s; serial port may be wedged", file=sys.stderr)
        return 2


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a Fruit Jam PebbleOS USB CDC shell command")
    parser.add_argument(
        "command",
        nargs="+",
        help="Command to send, e.g. buttons, progress, esp, bt, button down",
    )
    parser.add_argument("-p", "--port", default=None, help="USB CDC device, e.g. /dev/cu.usbmodem101")
    parser.add_argument("--timeout", type=float, default=None, help="Total read timeout in seconds")
    parser.add_argument(
        "--idle-timeout",
        type=float,
        default=None,
        help="Stop after this many quiet seconds once output has started",
    )
    parser.add_argument("--stream", action="store_true", help="Print output while it is received")
    parser.add_argument(
        "--open-timeout",
        type=float,
        default=3.0,
        help="Fail if opening the serial port takes longer than this many seconds",
    )
    args = parser.parse_args()

    command = " ".join(args.command)
    watch_seconds = watch_duration_seconds(command)
    if watch_seconds is None:
        timeout = args.timeout if args.timeout is not None else 5.0
        idle_timeout = args.idle_timeout if args.idle_timeout is not None else 0.25
    else:
        timeout = args.timeout if args.timeout is not None else (watch_seconds * 4.0) + 4.0
        idle_timeout = args.idle_timeout if args.idle_timeout is not None else max(5.0, watch_seconds + 1.0)

    if os.environ.get("FRUITJAM_CDC_COMMAND_CHILD") != "1":
        return run_as_child(timeout + args.open_timeout + 1.0)

    data = run_command(args.port or find_port(), command, timeout, idle_timeout, args.stream)
    if not args.stream:
        sys.stdout.buffer.write(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
