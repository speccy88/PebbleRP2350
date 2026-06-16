#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Inspect and safely flash mounted RP2350 BOOTSEL volumes on macOS."""

from __future__ import annotations

import argparse
import glob
import os
import pathlib
import plistlib
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from typing import Any


UF2_INFO_NAME = "INFO_UF2.TXT"
RP2350_BOOT_PRODUCT = "RP2350 Boot"
RP2350_BOOT_VENDOR_ID = 0x2E8A
RP2350_BOOT_PRODUCT_ID = 0x000F
SUBPROCESS_TIMEOUT_S = 3.0


@dataclass
class UsbDevice:
    name: str
    address: str
    properties: dict[str, Any]

    @property
    def product(self) -> str:
        return str(
            self.properties.get("USB Product Name")
            or self.properties.get("kUSBProductString")
            or self.name
        )

    @property
    def serial(self) -> str:
        return str(
            self.properties.get("kUSBSerialNumberString")
            or self.properties.get("USB Serial Number")
            or ""
        )

    @property
    def vendor(self) -> str:
        return str(self.properties.get("USB Vendor Name") or self.properties.get("kUSBVendorString") or "")

    @property
    def location_id(self) -> int | None:
        value = self.properties.get("locationID")
        return int(value) if isinstance(value, int) else None

    @property
    def id_vendor(self) -> int | None:
        value = self.properties.get("idVendor")
        return int(value) if isinstance(value, int) else None

    @property
    def id_product(self) -> int | None:
        value = self.properties.get("idProduct")
        return int(value) if isinstance(value, int) else None

    @property
    def is_rp2350_boot(self) -> bool:
        return (
            self.product == RP2350_BOOT_PRODUCT
            and self.id_vendor == RP2350_BOOT_VENDOR_ID
            and self.id_product == RP2350_BOOT_PRODUCT_ID
        )


@dataclass
class BootVolume:
    path: pathlib.Path
    info: dict[str, str]
    diskutil: dict[str, Any] | None


def status(message: str) -> None:
    print(message, flush=True)


def parse_ioreg_value(raw: str) -> Any:
    value = raw.strip()
    if value.startswith('"') and value.endswith('"'):
        return value[1:-1]
    if re.fullmatch(r"-?\d+", value):
        return int(value)
    return value


def macos_usb_devices() -> list[UsbDevice]:
    try:
        output = subprocess.check_output(
            ["ioreg", "-p", "IOUSB", "-l", "-w", "0"],
            text=True,
            timeout=SUBPROCESS_TIMEOUT_S,
        )
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        raise SystemExit(f"Could not read IOUSB registry: {exc}") from exc

    devices: list[UsbDevice] = []
    current: UsbDevice | None = None
    prop_re = re.compile(r'"([^"]+)" = (.+)$')

    for line in output.splitlines():
        if "<class IOUSBHostDevice" in line and "+-o " in line:
            before_class = line.split("<class", 1)[0]
            name_addr = before_class.split("+-o ", 1)[1].strip()
            if "@" not in name_addr:
                current = None
                continue
            name, address = name_addr.rsplit("@", 1)
            current = UsbDevice(name=name.strip(), address=address.strip(), properties={})
            devices.append(current)
            continue

        if current is None:
            continue

        match = prop_re.search(line.strip())
        if match:
            current.properties[match.group(1)] = parse_ioreg_value(match.group(2))

    return devices


def parse_info_uf2(path: pathlib.Path) -> dict[str, str]:
    values: dict[str, str] = {}
    try:
        data = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return values

    for line in data.splitlines():
        if line.startswith("UF2 Bootloader "):
            values["UF2 Bootloader"] = line.removeprefix("UF2 Bootloader ").strip()
            continue
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        values[key.strip()] = value.strip()
    return values


def diskutil_info(volume: pathlib.Path) -> dict[str, Any] | None:
    try:
        output = subprocess.check_output(
            ["diskutil", "info", "-plist", str(volume)],
            timeout=SUBPROCESS_TIMEOUT_S,
        )
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
        return None
    try:
        return plistlib.loads(output)
    except plistlib.InvalidFileException:
        return None


def mounted_rp2350_paths() -> list[pathlib.Path]:
    paths: list[pathlib.Path] = []
    try:
        output = subprocess.check_output(["mount"], text=True, timeout=SUBPROCESS_TIMEOUT_S)
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
        return [pathlib.Path(path) for path in glob.glob("/Volumes/RP2350*")]

    for line in output.splitlines():
        match = re.search(r" on (/Volumes/RP2350[^ ]*) ", line)
        if match:
            paths.append(pathlib.Path(match.group(1)))
    return paths


def find_boot_volumes(
    explicit_volume: pathlib.Path | None,
    include_diskutil: bool,
    read_info: bool,
) -> list[BootVolume]:
    paths = [explicit_volume] if explicit_volume else mounted_rp2350_paths()
    volumes: list[BootVolume] = []

    for path in sorted(paths):
        if not path:
            continue
        info = parse_info_uf2(path / UF2_INFO_NAME) if read_info else {}
        if read_info and not info and not explicit_volume:
            continue
        if read_info and info.get("Board-ID") != "RP2350" and not explicit_volume:
            continue
        volumes.append(
            BootVolume(
                path=path,
                info=info,
                diskutil=diskutil_info(path) if include_diskutil else None,
            )
        )

    return volumes


def format_hex(value: int | None) -> str:
    return "-" if value is None else f"0x{value:08x}"


def print_status(volumes: list[BootVolume], devices: list[UsbDevice]) -> None:
    status("RP2350 BOOTSEL volumes:")
    if not volumes:
        status("  none mounted")
    for volume in volumes:
        status(f"  {volume.path}")
        if volume.info:
            status(
                "    "
                f"bootloader={volume.info.get('UF2 Bootloader', '-')} "
                f"model={volume.info.get('Model', '-')} "
                f"board_id={volume.info.get('Board-ID', '-')}"
            )
        else:
            status("    INFO_UF2.TXT not read; use --read-info to inspect it")
        if volume.diskutil:
            status(
                "    "
                f"disk={volume.diskutil.get('DeviceIdentifier', '-')} "
                f"tree={volume.diskutil.get('DeviceTreePath', '-')}"
            )

    status("USB devices:")
    interesting = [
        device
        for device in devices
        if device.is_rp2350_boot or device.serial or "Pico" in device.product or "Fruit Jam" in device.product
    ]
    if not interesting:
        status("  no RP2350-like USB devices found")
    for device in interesting:
        status(
            "  "
            f"product={device.product!r} serial={device.serial or '-'} "
            f"location={format_hex(device.location_id)} "
            f"vid={device.id_vendor if device.id_vendor is not None else '-'} "
            f"pid={device.id_product if device.id_product is not None else '-'}"
        )


def validate_expected_serial(expected_serial: str | None, boot_devices: list[UsbDevice]) -> None:
    if expected_serial is None:
        return

    matching = [device for device in boot_devices if device.serial == expected_serial]
    if matching:
        return

    available = ", ".join(device.serial or "-" for device in boot_devices) or "none"
    raise SystemExit(
        f"Expected RP2350 BOOTSEL serial {expected_serial!r}, but mounted USB BOOTSEL serials are: "
        f"{available}"
    )


def copy_uf2(
    uf2_path: pathlib.Path,
    volumes: list[BootVolume],
    boot_devices: list[UsbDevice],
    expected_serial: str | None,
    allow_unverified: bool,
    dry_run: bool,
    sync_after_copy: bool,
) -> None:
    if not uf2_path.is_file():
        raise SystemExit(f"UF2 does not exist: {uf2_path}")
    if not volumes:
        raise SystemExit("No mounted RP2350 BOOTSEL volume found")
    if len(volumes) != 1:
        raise SystemExit("More than one RP2350 BOOTSEL volume is mounted; refusing to guess")
    if not boot_devices:
        raise SystemExit("No RP2350 BOOTSEL USB device found")
    if expected_serial is None and not allow_unverified:
        raise SystemExit("Refusing to copy without --expect-serial or --allow-unverified")
    validate_expected_serial(expected_serial, boot_devices)
    if expected_serial is None and len(boot_devices) != 1:
        raise SystemExit("More than one RP2350 BOOTSEL USB device is present; use --expect-serial")

    destination = volumes[0].path / uf2_path.name
    status(f"copy {uf2_path} -> {destination}")
    if dry_run:
        status("dry run; not copying")
        return

    shutil.copyfile(uf2_path, destination)
    status("copyfile complete")
    if sync_after_copy:
        status("syncing filesystem")
        os.sync()
    status("copy complete")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--volume", type=pathlib.Path, help="Explicit mounted BOOTSEL volume")
    parser.add_argument("--expect-serial", help="Required RP2350 BOOTSEL USB serial")
    parser.add_argument("--copy", type=pathlib.Path, help="UF2 file to copy after validation")
    parser.add_argument(
        "--include-diskutil",
        action="store_true",
        help="Also ask diskutil for volume metadata. Disabled by default because diskutil can stall on BOOTSEL.",
    )
    parser.add_argument(
        "--read-info",
        action="store_true",
        help="Read INFO_UF2.TXT from the mounted volume. Disabled by default because BOOTSEL reads can stall.",
    )
    parser.add_argument(
        "--sync",
        action="store_true",
        help="Call os.sync() after copying. Disabled by default because macOS can stall on wedged BOOTSEL volumes.",
    )
    parser.add_argument(
        "--allow-unverified",
        action="store_true",
        help="Allow --copy without --expect-serial when exactly one RP2350 BOOTSEL device is present",
    )
    parser.add_argument("--dry-run", action="store_true", help="Validate and print the copy action only")
    args = parser.parse_args()

    devices = macos_usb_devices()
    boot_devices = [device for device in devices if device.is_rp2350_boot]
    volumes = find_boot_volumes(args.volume, args.include_diskutil, args.read_info)

    print_status(volumes, devices)
    validate_expected_serial(args.expect_serial, boot_devices)

    if args.copy:
        copy_uf2(
            args.copy,
            volumes,
            boot_devices,
            args.expect_serial,
            args.allow_unverified,
            args.dry_run,
            args.sync,
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
