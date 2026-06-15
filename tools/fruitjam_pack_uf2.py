#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Pack Fruit Jam RP2350 firmware and system resources into one UF2."""

from __future__ import annotations

import argparse
import collections
import pathlib
import struct

UF2_BLOCK_SIZE = 512
UF2_PAYLOAD_SIZE = 256
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000

FRUITJAM_SYSTEM_RESOURCES_BANK0 = 0x10620000
FRUITJAM_SYSTEM_RESOURCES_BANK_SIZE = 0x00200000


class Uf2Error(RuntimeError):
    pass


def _iter_uf2_blocks(data: bytes):
    if len(data) % UF2_BLOCK_SIZE:
        raise Uf2Error("UF2 size is not a multiple of 512 bytes")

    for offset in range(0, len(data), UF2_BLOCK_SIZE):
        block = data[offset : offset + UF2_BLOCK_SIZE]
        header = struct.unpack_from("<IIIIIIII", block)
        magic0, magic1, flags, target_addr, payload_size, block_no, num_blocks, family = header
        magic_end = struct.unpack_from("<I", block, UF2_BLOCK_SIZE - 4)[0]

        if (
            magic0 != UF2_MAGIC_START0
            or magic1 != UF2_MAGIC_START1
            or magic_end != UF2_MAGIC_END
        ):
            raise Uf2Error(f"Invalid UF2 magic at block {offset // UF2_BLOCK_SIZE}")
        if payload_size > 476:
            raise Uf2Error(f"Invalid UF2 payload size {payload_size}")

        yield {
            "flags": flags,
            "target_addr": target_addr,
            "payload_size": payload_size,
            "block_no": block_no,
            "num_blocks": num_blocks,
            "family": family,
            "raw": block,
        }


def _main_family_id(firmware_uf2: bytes) -> int:
    families = collections.Counter()
    for block in _iter_uf2_blocks(firmware_uf2):
        if block["flags"] & UF2_FLAG_FAMILY_ID_PRESENT:
            families[block["family"]] += 1

    if not families:
        raise Uf2Error("Firmware UF2 does not contain a family ID")

    return families.most_common(1)[0][0]


def _rewrite_block_number(block: bytes, block_no: int, num_blocks: int) -> bytes:
    rewritten = bytearray(block)
    struct.pack_into("<II", rewritten, 20, block_no, num_blocks)
    return bytes(rewritten)


def _resource_blocks(
    resources: bytes,
    family_id: int,
    target_addr: int,
    first_block_no: int,
    total_blocks: int,
) -> bytes:
    num_resource_blocks = (len(resources) + UF2_PAYLOAD_SIZE - 1) // UF2_PAYLOAD_SIZE
    blocks = []

    for resource_block_no in range(num_resource_blocks):
        start = resource_block_no * UF2_PAYLOAD_SIZE
        payload = resources[start : start + UF2_PAYLOAD_SIZE]
        payload = payload.ljust(UF2_PAYLOAD_SIZE, b"\xff")

        block = bytearray(UF2_BLOCK_SIZE)
        struct.pack_into(
            "<IIIIIIII",
            block,
            0,
            UF2_MAGIC_START0,
            UF2_MAGIC_START1,
            UF2_FLAG_FAMILY_ID_PRESENT,
            target_addr + start,
            UF2_PAYLOAD_SIZE,
            first_block_no + resource_block_no,
            total_blocks,
            family_id,
        )
        block[32 : 32 + UF2_PAYLOAD_SIZE] = payload
        struct.pack_into("<I", block, UF2_BLOCK_SIZE - 4, UF2_MAGIC_END)
        blocks.append(bytes(block))

    return b"".join(blocks)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--firmware-uf2",
        type=pathlib.Path,
        default=pathlib.Path("build/src/fw/tintin_fw.uf2"),
    )
    parser.add_argument(
        "--resources",
        type=pathlib.Path,
        default=pathlib.Path("build/system_resources.pbpack"),
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path("build/src/fw/tintin_fw_with_resources.uf2"),
    )
    parser.add_argument(
        "--resources-address",
        type=lambda value: int(value, 0),
        default=FRUITJAM_SYSTEM_RESOURCES_BANK0,
    )
    args = parser.parse_args()

    firmware_uf2 = args.firmware_uf2.read_bytes()
    resources = args.resources.read_bytes()

    if len(resources) > FRUITJAM_SYSTEM_RESOURCES_BANK_SIZE:
        raise Uf2Error(
            "system_resources.pbpack is larger than the Fruit Jam resource bank "
            f"({len(resources)} > {FRUITJAM_SYSTEM_RESOURCES_BANK_SIZE})"
        )

    family_id = _main_family_id(firmware_uf2)
    firmware_blocks = [block["raw"] for block in _iter_uf2_blocks(firmware_uf2)]
    main_firmware_blocks = [
        block for block in _iter_uf2_blocks(firmware_uf2) if block["family"] == family_id
    ]
    resource_block_count = (len(resources) + UF2_PAYLOAD_SIZE - 1) // UF2_PAYLOAD_SIZE
    main_total_blocks = len(main_firmware_blocks) + resource_block_count

    rewritten_firmware = []
    next_main_block_no = 0
    for block in _iter_uf2_blocks(firmware_uf2):
        if block["family"] == family_id:
            rewritten_firmware.append(
                _rewrite_block_number(block["raw"], next_main_block_no, main_total_blocks)
            )
            next_main_block_no += 1
        else:
            rewritten_firmware.append(block["raw"])

    resource_uf2 = _resource_blocks(
        resources,
        family_id,
        args.resources_address,
        next_main_block_no,
        main_total_blocks,
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(b"".join(rewritten_firmware) + resource_uf2)

    print(f"Wrote {args.output}")
    print(f"Firmware UF2 blocks: {len(firmware_blocks)}")
    print(f"Resource UF2 blocks: {len(resource_uf2) // UF2_BLOCK_SIZE}")
    print(f"Resource address: 0x{args.resources_address:08x}")
    print(f"Family ID: 0x{family_id:08x}")


if __name__ == "__main__":
    main()
