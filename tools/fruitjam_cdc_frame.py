#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import binascii
import glob
import os
import select
import struct
import subprocess
import sys
import termios
import time
import zlib


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


def read_some(fd: int, deadline: float) -> bytes:
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        return b""
    readable, _, _ = select.select([fd], [], [], remaining)
    if not readable:
        return b""
    return os.read(fd, 4096)


class Reader:
    def __init__(self, fd: int, timeout: float):
        self.fd = fd
        self.timeout = timeout
        self.buf = bytearray()

    def deadline(self) -> float:
        return time.monotonic() + self.timeout

    def read_line(self) -> bytes:
        deadline = self.deadline()
        while True:
            newline = self.buf.find(b"\n")
            if newline >= 0:
                line = bytes(self.buf[:newline + 1])
                del self.buf[:newline + 1]
                return line
            chunk = read_some(self.fd, deadline)
            if not chunk:
                raise TimeoutError("Timed out waiting for a line")
            self.buf.extend(chunk)

    def read_exact(self, length: int) -> bytes:
        deadline = self.deadline()
        while len(self.buf) < length:
            chunk = read_some(self.fd, deadline)
            if not chunk:
                raise TimeoutError(f"Timed out waiting for {length} frame bytes")
            self.buf.extend(chunk)
        data = bytes(self.buf[:length])
        del self.buf[:length]
        return data


def parse_header(header: bytes) -> tuple[int, int, int, str]:
    parts = header.decode("ascii", errors="replace").strip().split()
    if len(parts) < 5 or parts[0] != "FJFRAME":
        raise ValueError(f"Unexpected frame header: {header!r}")
    width = int(parts[1])
    height = int(parts[2])
    length = int(parts[3])
    sequence = parts[4].removeprefix("seq=")
    return width, height, length, sequence


def png_chunk(tag: bytes, data: bytes) -> bytes:
    crc = binascii.crc32(tag + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)


def write_png(path: str, width: int, height: int, pbm_data: bytes) -> None:
    stride = (width + 7) // 8
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        row = pbm_data[y * stride:(y + 1) * stride]
        for x in range(width):
            bit = (row[x // 8] >> (7 - (x % 8))) & 1
            raw.append(0 if bit else 255)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    with open(path, "wb") as fout:
        fout.write(b"\x89PNG\r\n\x1a\n")
        fout.write(png_chunk(b"IHDR", ihdr))
        fout.write(png_chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        fout.write(png_chunk(b"IEND", b""))


def write_pbm(path: str, width: int, height: int, pbm_data: bytes) -> None:
    with open(path, "wb") as fout:
        fout.write(f"P4\n{width} {height}\n".encode("ascii"))
        fout.write(pbm_data)


def capture_frame(port: str, output: str, pbm_output: str | None, timeout: float) -> None:
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        configure_serial(fd)
        reader = Reader(fd, timeout)

        drain_deadline = time.monotonic() + 0.2
        while read_some(fd, drain_deadline):
            pass

        os.write(fd, b"\r\nframe\n")

        while True:
            header = reader.read_line()
            if header.startswith(b"FJFRAME "):
                break

        width, height, length, sequence = parse_header(header)
        frame = reader.read_exact(length)

        if output.endswith(".pbm"):
            write_pbm(output, width, height, frame)
        else:
            write_png(output, width, height, frame)

        if pbm_output:
            write_pbm(pbm_output, width, height, frame)

        print(f"Captured {width}x{height} frame seq={sequence} to {output}")
    finally:
        os.close(fd)


def run_as_child(total_timeout: float) -> int:
    env = os.environ.copy()
    env["FRUITJAM_CDC_FRAME_CHILD"] = "1"
    proc = subprocess.Popen([sys.executable, __file__, *sys.argv[1:]], env=env)
    try:
        return proc.wait(timeout=total_timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        print(f"Timed out after {total_timeout:.1f}s; serial port may be wedged", file=sys.stderr)
        return 2


def main() -> int:
    parser = argparse.ArgumentParser(description="Capture a Fruit Jam PebbleOS LCD frame over USB CDC")
    parser.add_argument("-p", "--port", default=None, help="USB CDC device, e.g. /dev/cu.usbmodem101")
    parser.add_argument("-o", "--output", default="/tmp/fruitjam_frame.png", help="Output .png or .pbm path")
    parser.add_argument("--pbm", default=None, help="Optional extra raw PBM output path")
    parser.add_argument("--timeout", type=float, default=5.0, help="Read timeout in seconds")
    parser.add_argument(
        "--open-timeout",
        type=float,
        default=3.0,
        help="Fail if opening the serial port takes longer than this many seconds",
    )
    args = parser.parse_args()

    if os.environ.get("FRUITJAM_CDC_FRAME_CHILD") != "1":
        return run_as_child(args.timeout + args.open_timeout + 1.0)

    capture_frame(args.port or find_port(), args.output, args.pbm, args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
