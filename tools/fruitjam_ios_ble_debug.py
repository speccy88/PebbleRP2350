#!/usr/bin/env python3
"""Capture iPhone Pebble/CoreBluetooth logs with Fruit Jam BLE state snapshots."""

from __future__ import annotations

import argparse
import datetime as dt
import os
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path


DEFAULT_PORT = "/dev/cu.usbmodemFJRP23501"
DEFAULT_REGEX = "Pebble|coredevices|CoreBluetooth|Bluetooth|BLE|Kable|scan|CBCentral|CBPeripheral"


def run_to_file(cmd: list[str], out_path: Path, cwd: Path) -> int:
    with out_path.open("w", encoding="utf-8", errors="replace") as out:
        out.write("$ " + " ".join(cmd) + "\n\n")
        out.flush()
        proc = subprocess.run(cmd, cwd=cwd, stdout=out, stderr=subprocess.STDOUT, text=True)
        return proc.returncode


def start_syslog(args: argparse.Namespace, out_path: Path) -> tuple[subprocess.Popen[bytes], Path]:
    cmd = [
        "pymobiledevice3",
        "syslog",
        "live",
        "--insensitive-regex",
        args.regex,
        "--out",
        str(out_path),
    ]
    if args.udid:
        cmd.extend(["--udid", args.udid])
    if args.mobdev2:
        cmd.append("--mobdev2")

    err_path = out_path.with_suffix(".stderr.log")
    with out_path.open("w", encoding="utf-8", errors="replace") as out:
        out.write("$ " + " ".join(cmd) + "\n\n")
    err = err_path.open("w", encoding="utf-8", errors="replace")
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.DEVNULL,
        stderr=err,
        start_new_session=True,
    )
    proc._fruitjam_stderr = err  # type: ignore[attr-defined]
    return proc, err_path


def stop_process(proc: subprocess.Popen[bytes]) -> None:
    if proc.poll() is not None:
        err = getattr(proc, "_fruitjam_stderr", None)
        if err:
            err.close()
        return
    try:
        os.killpg(proc.pid, signal.SIGINT)
        proc.wait(timeout=5)
    except Exception:
        try:
            os.killpg(proc.pid, signal.SIGTERM)
            proc.wait(timeout=5)
        except Exception:
            os.killpg(proc.pid, signal.SIGKILL)
    err = getattr(proc, "_fruitjam_stderr", None)
    if err:
        err.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration", type=float, default=35.0, help="capture duration in seconds")
    parser.add_argument("--port", default=DEFAULT_PORT, help="Fruit Jam debug CDC port")
    parser.add_argument("--udid", default="", help="iPhone UDID for pymobiledevice3")
    parser.add_argument("--mobdev2", action="store_true", help="use pymobiledevice3 Bonjour/mobdev2 discovery")
    parser.add_argument("--regex", default=DEFAULT_REGEX, help="case-insensitive syslog regex")
    parser.add_argument("--out-dir", default="", help="capture directory; defaults under build/")
    parser.add_argument("--mac-scan", action="store_true", help="also run the Mac BLE probe after capture")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[1]
    repo_python = repo / ".venv" / "bin" / "python"
    tool_python = str(repo_python if repo_python.exists() else Path(sys.executable))
    timestamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = Path(args.out_dir) if args.out_dir else repo / "build" / "fruitjam_ios_ble_debug" / timestamp
    out_dir.mkdir(parents=True, exist_ok=True)

    missing = [tool for tool in ("pymobiledevice3",) if shutil.which(tool) is None]
    if missing:
        print(f"missing required tool(s): {', '.join(missing)}", file=sys.stderr)
        return 2

    fruitjam_cmd = [tool_python, "tools/fruitjam_cdc_command.py", "-p", args.port]
    run_to_file(fruitjam_cmd + ["progress", "--timeout", "3", "--idle-timeout", "0.2"],
                out_dir / "fruitjam_progress_before.txt", repo)
    run_to_file(fruitjam_cmd + ["bt", "--timeout", "3", "--idle-timeout", "0.2"],
                out_dir / "fruitjam_bt_before.txt", repo)

    syslog_path = out_dir / "iphone_syslog.log"
    syslog_proc, syslog_err_path = start_syslog(args, syslog_path)
    try:
        time.sleep(args.duration)
    finally:
        stop_process(syslog_proc)
    if syslog_proc.returncode not in (None, 0, -signal.SIGINT):
        with syslog_path.open("a", encoding="utf-8", errors="replace") as out:
            out.write(f"\n[pymobiledevice3 syslog exited with {syslog_proc.returncode}; see {syslog_err_path}]\n")

    run_to_file(fruitjam_cmd + ["progress", "--timeout", "3", "--idle-timeout", "0.2"],
                out_dir / "fruitjam_progress_after.txt", repo)
    run_to_file(fruitjam_cmd + ["bt", "--timeout", "3", "--idle-timeout", "0.2"],
                out_dir / "fruitjam_bt_after.txt", repo)

    if args.mac_scan:
        run_to_file([tool_python, "tools/macos_ble_probe.py", "scan", "--name", "Peb", "--timeout", "8"],
                    out_dir / "macos_ble_probe.txt", repo)

    print(out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
