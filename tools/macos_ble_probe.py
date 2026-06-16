#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Small macOS BLE probe for Pebble/Fruit Jam bring-up."""

import argparse
import asyncio
import inspect
import sys
import time
from collections.abc import Iterable
from dataclasses import dataclass

from bleak import BleakClient, BleakError, BleakScanner


BLE_SIG_BASE_SUFFIX = "-0000-1000-8000-00805f9b34fb"
PEBBLE_BASE_SUFFIX = "-328e-0fbb-c642-1aa6699bdada"
PEBBLE_PAIRING_SERVICE_UUID = "0000fed9-0000-1000-8000-00805f9b34fb"
PEBBLE_PAIRING_STATUS_UUID = "00000001-328e-0fbb-c642-1aa6699bdada"
PEBBLE_TRIGGER_PAIRING_UUID = "00000002-328e-0fbb-c642-1aa6699bdada"
PPOGATT_SERVER_SERVICE_UUID = "10000000-328e-0fbb-c642-1aa6699bdada"
PPOGATT_SERVER_DATA_UUID = "10000001-328e-0fbb-c642-1aa6699bdada"
PPOGATT_SERVER_META_UUID = "10000002-328e-0fbb-c642-1aa6699bdada"
PPOGATT_VERSION = 1
PPOGATT_RESET_REQUEST = 0x02
PPOGATT_RESET_COMPLETE = 0x03
PPOGATT_MAX_WINDOW = 25


@dataclass(frozen=True)
class DiscoveredDevice:
    address: str
    name: str
    rssi: int | None
    service_uuids: list[str]
    manufacturer_data: dict[int, bytes]
    device: object


def normalize_uuid(uuid: str) -> str:
    value = uuid.strip().lower()
    if len(value) <= 4 and "-" not in value:
        return f"0000{int(value, 16):04x}{BLE_SIG_BASE_SUFFIX}"
    if len(value) == 8 and "-" not in value:
        return f"{int(value, 16):08x}{PEBBLE_BASE_SUFFIX}"
    return value


def pebble_uuid(value: int) -> str:
    return f"{value:08x}{PEBBLE_BASE_SUFFIX}"


def format_hex(data: bytes) -> str:
    return data.hex().upper() if data else "-"


def format_manufacturer_data(manufacturer_data: dict[int, bytes]) -> str:
    if not manufacturer_data:
        return "-"
    return ",".join(
        f"0x{company_id:04X}:{format_hex(payload)}"
        for company_id, payload in sorted(manufacturer_data.items())
    )


def ppog_packet_header(packet_type: int, sequence: int) -> int:
    return ((sequence & 0x1F) << 3) | (packet_type & 0x07)


def ppog_packet_description(data: bytes) -> str:
    if not data:
        return "empty"

    packet_type = data[0] & 0x07
    sequence = (data[0] >> 3) & 0x1F
    payload = data[1:]
    names = {
        0x00: "data",
        0x01: "ack",
        0x02: "reset-request",
        0x03: "reset-complete",
    }
    return (
        f"type={packet_type}:{names.get(packet_type, 'unknown')} "
        f"seq={sequence} payload={format_hex(payload)}"
    )


def device_name(device: object, adv: object | None = None) -> str:
    for attr_owner, attr_name in ((adv, "local_name"), (device, "name")):
        if attr_owner is None:
            continue
        value = getattr(attr_owner, attr_name, None)
        if value:
            return str(value)
    return ""


def parse_scan_result(result: object) -> list[DiscoveredDevice]:
    devices: list[DiscoveredDevice] = []
    if isinstance(result, dict):
        values: Iterable[object] = result.values()
    else:
        values = result  # type: ignore[assignment]

    for entry in values:
        if isinstance(entry, tuple):
            device, adv = entry
        else:
            device, adv = entry, None
        service_uuids = [str(uuid).lower() for uuid in getattr(adv, "service_uuids", [])]
        manufacturer_data = dict(getattr(adv, "manufacturer_data", {}) or {})
        devices.append(
            DiscoveredDevice(
                address=str(getattr(device, "address", "")),
                name=device_name(device, adv),
                rssi=getattr(adv, "rssi", getattr(device, "rssi", None)),
                service_uuids=service_uuids,
                manufacturer_data=manufacturer_data,
                device=device,
            )
        )
    return devices


def matches_filter(device: DiscoveredDevice, name: str | None, service: str | None) -> bool:
    if name and name.lower() not in device.name.lower():
        return False
    if service and normalize_uuid(service) not in device.service_uuids:
        return False
    return True


def print_scan_device(device: DiscoveredDevice) -> None:
    services = ",".join(device.service_uuids) if device.service_uuids else "-"
    rssi = "-" if device.rssi is None else str(device.rssi)
    print(
        f"{device.address} rssi={rssi} name={device.name!r} "
        f"services={services} mfg={format_manufacturer_data(device.manufacturer_data)}"
    )


async def discover(timeout: float, name: str | None, service: str | None) -> list[DiscoveredDevice]:
    result = await BleakScanner.discover(timeout=timeout, return_adv=True)
    devices = parse_scan_result(result)
    devices.sort(key=lambda item: (item.name or "", item.address))
    return [device for device in devices if matches_filter(device, name, service)]


def decode_pairing_status(data: bytes) -> str:
    if len(data) < 4:
        return f"short len={len(data)} raw={format_hex(data)}"

    flags = data[0]
    names = [
        ("connected", 0),
        ("bonded", 1),
        ("encrypted", 2),
        ("gateway", 3),
        ("pinning", 4),
        ("reversed", 5),
    ]
    decoded = " ".join(f"{name}={(flags >> bit) & 1}" for name, bit in names)
    return f"flags=0x{flags:02X} {decoded} last_pairing_result=0x{data[3]:02X}"


def trigger_flags(mode: str) -> int:
    if mode == "pin":
        return 0x01
    if mode == "no-sec":
        return 0x02
    if mode == "force":
        return 0x04
    return int(mode, 0)


def iter_services(collection: object) -> Iterable[object]:
    return collection  # type: ignore[return-value]


async def run_scan(args: argparse.Namespace) -> int:
    devices = await discover(args.timeout, args.name, args.service)
    for device in devices:
        print_scan_device(device)
    print(f"found={len(devices)}")
    return 0


async def pick_device(args: argparse.Namespace) -> object | str:
    if args.address:
        return args.address

    devices = await discover(args.scan_timeout, args.name, args.service)
    if not devices:
        raise SystemExit("No matching BLE device found")

    print("selected:")
    print_scan_device(devices[0])
    return devices[0].device


async def print_services(client: BleakClient) -> None:
    for service in iter_services(client.services):
        print(f"service {service.uuid}")
        for char in service.characteristics:
            props = ",".join(char.properties)
            print(f"  char {char.uuid} props={props} handle={char.handle}")
            for desc in char.descriptors:
                print(f"    desc {desc.uuid} handle={desc.handle}")


async def run_connect(args: argparse.Namespace) -> int:
    target = await pick_device(args)
    try:
        async with BleakClient(target, timeout=args.timeout) as client:
            print(f"connected={client.is_connected}")
            if args.enumerate:
                await print_services(client)

            if args.read_pairing_status:
                data = await client.read_gatt_char(PEBBLE_PAIRING_STATUS_UUID)
                print(f"pairing_status raw={format_hex(data)} {decode_pairing_status(data)}")

            if args.trigger_pairing is not None:
                flags = trigger_flags(args.trigger_pairing)
                await client.write_gatt_char(PEBBLE_TRIGGER_PAIRING_UUID, bytes([flags]), response=True)
                print(f"trigger_pairing flags=0x{flags:02X} written")

            if args.hold_seconds > 0:
                print(f"holding={args.hold_seconds:g}s")
                await asyncio.sleep(args.hold_seconds)
    except BleakError as exc:
        print(f"BLE error: {exc}", file=sys.stderr)
        return 2
    return 0


def run_ppog_server(args: argparse.Namespace) -> int:
    try:
        import objc
        from CoreBluetooth import (
            CBATTErrorRequestNotSupported,
            CBATTErrorSuccess,
            CBAttributePermissionsReadable,
            CBAttributePermissionsWriteable,
            CBCharacteristicPropertyNotify,
            CBCharacteristicPropertyRead,
            CBCharacteristicPropertyWriteWithoutResponse,
            CBManagerStatePoweredOn,
            CBMutableCharacteristic,
            CBMutableService,
            CBPeripheralManager,
            CBUUID,
        )
        from Foundation import NSData, NSDate, NSObject, NSRunLoop
    except ImportError as exc:
        print(f"CoreBluetooth/PyObjC is unavailable: {exc}", file=sys.stderr)
        return 2

    def cb_uuid(value: str):
        return CBUUID.UUIDWithString_(value)

    def uuid_string(value: object) -> str:
        return str(value.UUIDString()).lower()

    def nsdata(data: bytes):
        return NSData.dataWithBytes_length_(data, len(data))

    def bytes_from_nsdata(value: object | None) -> bytes:
        if value is None:
            return b""
        return bytes(value)

    class PPoGServerDelegate(NSObject):
        manager = None
        data_characteristic = None
        meta_response = b""
        notify_count = 0
        write_count = 0
        read_count = 0
        subscribed = False
        powered_on = False
        service_added = False

        def init(self):
            self = objc.super(PPoGServerDelegate, self).init()
            return self

        def peripheralManagerDidUpdateState_(self, peripheral):
            print(f"ppog-server state={peripheral.state()}", flush=True)
            if peripheral.state() != CBManagerStatePoweredOn:
                return

            self.powered_on = True
            meta = CBMutableCharacteristic.alloc().initWithType_properties_value_permissions_(
                cb_uuid(PPOGATT_SERVER_META_UUID),
                CBCharacteristicPropertyRead,
                None,
                CBAttributePermissionsReadable,
            )
            data = CBMutableCharacteristic.alloc().initWithType_properties_value_permissions_(
                cb_uuid(PPOGATT_SERVER_DATA_UUID),
                CBCharacteristicPropertyWriteWithoutResponse | CBCharacteristicPropertyNotify,
                None,
                CBAttributePermissionsWriteable,
            )
            service = CBMutableService.alloc().initWithType_primary_(
                cb_uuid(PPOGATT_SERVER_SERVICE_UUID),
                True,
            )
            service.setCharacteristics_([meta, data])
            self.data_characteristic = data
            peripheral.addService_(service)

        def peripheralManager_didAddService_error_(self, peripheral, service, error):
            if error is not None:
                print(f"ppog-server add-service error={error}", flush=True)
                return
            self.service_added = True
            print(f"ppog-server added service={uuid_string(service.UUID())}", flush=True)

        def peripheralManager_didReceiveReadRequest_(self, peripheral, request):
            characteristic_uuid = uuid_string(request.characteristic().UUID())
            self.read_count += 1
            print(f"ppog-server read uuid={characteristic_uuid}", flush=True)
            if characteristic_uuid != PPOGATT_SERVER_META_UUID:
                peripheral.respondToRequest_withResult_(request, CBATTErrorRequestNotSupported)
                return

            request.setValue_(nsdata(self.meta_response))
            peripheral.respondToRequest_withResult_(request, CBATTErrorSuccess)
            print(f"ppog-server meta={format_hex(self.meta_response)}", flush=True)

        def peripheralManager_didReceiveWriteRequests_(self, peripheral, requests):
            for request in requests:
                data = bytes_from_nsdata(request.value())
                self.write_count += 1
                print(
                    f"ppog-server write {ppog_packet_description(data)} raw={format_hex(data)}",
                    flush=True,
                )
                if data and (data[0] & 0x07) == PPOGATT_RESET_REQUEST:
                    sequence = (data[0] >> 3) & 0x1F
                    response = bytes(
                        [
                            ppog_packet_header(PPOGATT_RESET_COMPLETE, sequence),
                            args.rx_window,
                            args.tx_window,
                        ]
                    )
                    ok = peripheral.updateValue_forCharacteristic_onSubscribedCentrals_(
                        nsdata(response),
                        self.data_characteristic,
                        None,
                    )
                    self.notify_count += 1
                    print(
                        f"ppog-server notify ok={int(bool(ok))} "
                        f"{ppog_packet_description(response)} raw={format_hex(response)}",
                        flush=True,
                    )
                peripheral.respondToRequest_withResult_(request, CBATTErrorSuccess)

        def peripheralManager_central_didSubscribeToCharacteristic_(self, peripheral, central, char):
            self.subscribed = True
            print(
                f"ppog-server subscribe central={central.identifier()} "
                f"uuid={uuid_string(char.UUID())}",
                flush=True,
            )

        def peripheralManager_central_didUnsubscribeFromCharacteristic_(self, peripheral, central, char):
            self.subscribed = False
            print(
                f"ppog-server unsubscribe central={central.identifier()} "
                f"uuid={uuid_string(char.UUID())}",
                flush=True,
            )

        def peripheralManagerIsReadyToUpdateSubscribers_(self, peripheral):
            print("ppog-server ready-to-update", flush=True)

    meta_response = bytes([0x00, PPOGATT_VERSION, *([0x00] * 16), args.session_type])
    delegate = PPoGServerDelegate.alloc().init()
    delegate.meta_response = meta_response
    manager = CBPeripheralManager.alloc().initWithDelegate_queue_options_(delegate, None, None)
    delegate.manager = manager

    deadline = time.monotonic() + args.duration
    print(
        f"ppog-server publishing service={PPOGATT_SERVER_SERVICE_UUID} "
        f"meta={format_hex(meta_response)} duration={args.duration:g}s",
        flush=True,
    )
    while time.monotonic() < deadline:
        NSRunLoop.currentRunLoop().runUntilDate_(
            NSDate.dateWithTimeIntervalSinceNow_(min(0.1, deadline - time.monotonic()))
        )

    manager.removeAllServices()
    print(
        "ppog-server done "
        f"powered={int(delegate.powered_on)} added={int(delegate.service_added)} "
        f"subscribed={int(delegate.subscribed)} reads={delegate.read_count} "
        f"writes={delegate.write_count} notifies={delegate.notify_count}",
        flush=True,
    )
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    scan = subparsers.add_parser("scan", help="Scan for BLE devices")
    scan.add_argument("--timeout", type=float, default=6.0, help="Scan duration in seconds")
    scan.add_argument("--name", default=None, help="Filter by local name substring")
    scan.add_argument(
        "--service",
        default=None,
        help="Filter by service UUID, for example 0000fed9-0000-1000-8000-00805f9b34fb",
    )
    scan.set_defaults(func=run_scan)

    connect = subparsers.add_parser("connect", help="Connect and inspect a BLE device")
    connect.add_argument("--address", default=None, help="CoreBluetooth UUID/address")
    connect.add_argument("--name", default="Pebble", help="Auto-select by local name substring")
    connect.add_argument("--service", default=PEBBLE_PAIRING_SERVICE_UUID, help="Auto-select by service UUID")
    connect.add_argument("--scan-timeout", type=float, default=6.0, help="Auto-select scan duration")
    connect.add_argument("--timeout", type=float, default=10.0, help="Connect/read/write timeout")
    connect.add_argument("--no-enumerate", action="store_false", dest="enumerate",
                         help="Skip service enumeration")
    connect.add_argument("--read-pairing-status", action="store_true",
                         help="Read and decode Pebble pairing status")
    connect.add_argument(
        "--trigger-pairing",
        choices=("pin", "no-sec", "force"),
        default=None,
        help="Write the Pebble trigger-pairing characteristic",
    )
    connect.add_argument("--hold-seconds", type=float, default=0.0, help="Keep the connection open")
    connect.set_defaults(func=run_connect, enumerate=True)

    ppog_server = subparsers.add_parser(
        "ppog-server",
        help="Publish a minimal phone-side PPoGATT GATT server on macOS",
    )
    ppog_server.add_argument("--duration", type=float, default=30.0, help="Seconds to keep the server alive")
    ppog_server.add_argument(
        "--session-type",
        type=int,
        choices=(0, 1),
        default=0,
        help="PPoGATT meta session type: 0=inferred, 1=hybrid",
    )
    ppog_server.add_argument("--rx-window", type=int, default=PPOGATT_MAX_WINDOW)
    ppog_server.add_argument("--tx-window", type=int, default=PPOGATT_MAX_WINDOW)
    ppog_server.set_defaults(func=run_ppog_server)

    return parser


async def async_main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    result = args.func(args)
    if inspect.isawaitable(result):
        return await result
    return result


def main() -> int:
    return asyncio.run(async_main())


if __name__ == "__main__":
    raise SystemExit(main())
