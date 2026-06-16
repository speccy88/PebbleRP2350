#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import glob
import json
import os
import re
import select
import sys
import termios
import time
import zipfile

LAUNCHER_INSTALL_ID = -54


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


def read_some(fd: int, timeout: float) -> bytes:
    readable, _, _ = select.select([fd], [], [], timeout)
    if not readable:
        return b""
    return os.read(fd, 4096)


class CdcSession:
    def __init__(self, port: str, timeout: float) -> None:
        self.port = port
        self.timeout = timeout
        self.fd: int | None = None
        self.buf = bytearray()

    def __enter__(self) -> "CdcSession":
        self.fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        configure_serial(self.fd)
        self.drain(0.25)
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self.fd is not None:
            os.close(self.fd)

    def drain(self, seconds: float) -> None:
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if not read_some(self.fd, max(0.0, deadline - time.monotonic())):
                return

    def write_line(self, line: str) -> None:
        os.write(self.fd, line.encode("ascii") + b"\r\n")

    def read_line(self, deadline: float) -> str | None:
        while time.monotonic() < deadline:
            newline = self.buf.find(b"\n")
            if newline >= 0:
                raw = bytes(self.buf[:newline + 1])
                del self.buf[:newline + 1]
                return raw.decode("ascii", errors="replace").strip()

            chunk = read_some(self.fd, min(0.25, max(0.0, deadline - time.monotonic())))
            if chunk:
                self.buf.extend(chunk)
        return None

    def command(self, line: str, expected_prefix: str) -> str:
        self.write_line(line)
        deadline = time.monotonic() + self.timeout
        recent: list[str] = []
        error_terms = (
            " failed",
            " mismatch",
            " bad ",
            " incomplete",
            " not active",
            " exceeds",
            "usage:",
        )
        while time.monotonic() < deadline:
            response = self.read_line(deadline)
            if response is None:
                break
            if not response:
                continue
            recent.append(response)
            if len(recent) > 8:
                recent.pop(0)
            ack_at = response.find(expected_prefix)
            sideload_at = response.find("appsideload ")
            if sideload_at >= 0 and any(term in response for term in error_terms):
                raise RuntimeError(response[response.find("appsideload "):])
            if ack_at >= 0:
                return response[ack_at:]
        raise TimeoutError(f"Timed out waiting for {expected_prefix!r}; recent={recent!r}")


def normalize_uuid(uuid: str | None) -> str | None:
    if not uuid:
        return None
    return uuid.strip().strip("{}").lower()


def read_pbw_payload(path: str, platform: str) -> tuple[bytes, bytes, str | None, bool]:
    with zipfile.ZipFile(path) as zf:
        app_name = f"{platform}/pebble-app.bin"
        res_name = f"{platform}/app_resources.pbpack"
        names = set(zf.namelist())
        if app_name not in names or res_name not in names:
            available = sorted(name.split("/", 1)[0] for name in names if "/" in name)
            raise SystemExit(
                f"{path} does not contain {platform!r} app/resources; "
                f"available platforms: {', '.join(dict.fromkeys(available))}"
            )
        uuid: str | None = None
        if "appinfo.json" in names:
            with zf.open("appinfo.json") as appinfo_file:
                appinfo = json.load(appinfo_file)
            raw_uuid = appinfo.get("uuid")
            if isinstance(raw_uuid, str):
                uuid = raw_uuid
            watchapp = appinfo.get("watchapp")
            watchface = bool(isinstance(watchapp, dict) and watchapp.get("watchface"))
        else:
            watchface = False
        return zf.read(app_name), zf.read(res_name), uuid, watchface


def stream_part(session: CdcSession, name: str, data: bytes, chunk_size: int) -> None:
    for offset in range(0, len(data), chunk_size):
        chunk = data[offset:offset + chunk_size]
        response = session.command(f"appsideload {name} {chunk.hex()}",
                                   f"appsideload {name}=")
        if f"/{len(data)}" not in response:
            print(response)


def maybe_leave_running_app(session: CdcSession, pbw_uuid: str | None) -> None:
    normalized_pbw_uuid = normalize_uuid(pbw_uuid)
    if not normalized_pbw_uuid:
        return

    response = session.command("appcur", "appcur ")
    match = re.search(r"uuid=(\{?[0-9a-fA-F-]{36}\}?)", response)
    if not match or normalize_uuid(match.group(1)) != normalized_pbw_uuid:
        return

    print(f"current app matches PBW UUID; launching system launcher {LAUNCHER_INSTALL_ID} first")
    print(session.command(f"applaunch {LAUNCHER_INSTALL_ID}", "applaunch "))
    time.sleep(1.0)


def main() -> int:
    parser = argparse.ArgumentParser(description="Sideload a PBW to Fruit Jam PebbleOS over USB CDC")
    parser.add_argument("pbw", help="PBW file to sideload")
    parser.add_argument("-p", "--port", default=None, help="USB CDC device")
    parser.add_argument("--platform", default="flint", help="PBW platform directory to use")
    parser.add_argument("--chunk-size", type=int, default=64, help="Payload bytes per CDC line")
    parser.add_argument("--timeout", type=float, default=8.0, help="Per-command timeout in seconds")
    parser.add_argument("--launch", action="store_true", help="Launch the installed app afterwards")
    args = parser.parse_args()

    if args.chunk_size <= 0 or args.chunk_size > 80:
        raise SystemExit("--chunk-size must be between 1 and 80")

    app, resources, uuid, watchface = read_pbw_payload(args.pbw, args.platform)
    with CdcSession(args.port or find_port(), args.timeout) as session:
        maybe_leave_running_app(session, uuid)

        begin = f"appsideload begin {len(app)} {len(resources)}"
        if uuid:
            begin += f" {uuid}"
        print(session.command(begin, "appsideload begin "))
        stream_part(session, "app", app, args.chunk_size)
        stream_part(session, "res", resources, args.chunk_size)
        done = session.command("appsideload end", "appsideload done ")
        print(done)

        match = re.search(r"id=(-?\d+)", done)
        if args.launch and match:
            install_id = match.group(1)
            if watchface:
                print(session.command(f"watchdefault {install_id}", "watchdefault "))
            print(session.command(f"applaunch {install_id}", "applaunch "))
            time.sleep(0.5)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
