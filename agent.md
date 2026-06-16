# RP2350 Agent Workflow

This repository currently carries two RP2350 PebbleOS bring-up profiles:

- `fruitjam_rp2350`: Adafruit Fruit Jam RP2350 with a Waveshare 1.3 inch
  Memory LCD, optional DS1307-compatible RTC on GPIO20/GPIO21, and Bluetooth
  through the onboard ESP32-C6 running controller-only HCI UART firmware.
- `pico2_w_rp2350`: Raspberry Pi Pico 2 W profile for shared RP2350 display,
  time, storage, and USB CDC bring-up. The normal profile intentionally uses
  stub Bluetooth until the CYW43439/CYW43 backend is implemented.

The goal is to keep common RP2350 PebbleOS work shared while keeping board
wiring, Bluetooth transport, storage layout, and flash/recovery commands
separate.

## Ground Rules

- Never assume a mounted `RP2350` BOOTSEL volume is the right board. The
  `INFO_UF2.TXT` file identifies the chip family, not whether the board is the
  Fruit Jam or the Pico 2 W. Use USB serial checks before copying a UF2.
- Do not commit private contact info, phone UDIDs, notification URLs, local
  screenshots containing private data, or personal sign-off/coauthor trailers.
- Do not build a release from an unexplained dirty worktree. If a submodule is
  dirty, either publish and pin the submodule commit or document that the UF2 is
  a hardware-only local artifact.
- Treat the two UF2s as incompatible. Fruit Jam uses a 16 MB flash resource
  address; Pico 2 W uses a 4 MB-safe resource map.
- Keep PSRAM out of the default path for now. The near-term storage direction is
  external 3.3 V NOR flash for Pebble resources/filesystem data, not PSRAM.

## Environment

Use the repo virtualenv and the Pico toolchain explicitly:

```sh
export PATH="$PWD/.venv/bin:$PWD/third_party/nanopb/nanopb/generator:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH"
```

Useful installed host tools:

- `gh` for GitHub releases.
- `blueutil` and `tools/macos_ble_probe.py` for Mac BLE smoke tests.
- `pymobiledevice3` for iPhone/CoreBluetooth syslog captures.
- `hardware_tests/fruitjam_memory_lcd/build/_deps/picotool-build/picotool`
  for RP2350 UF2 conversion.
- `/Users/fred/.pico-sdk/picotool/2.1.1/picotool/picotool` for Pico SDK-style
  direct load/verify when BOOTSEL serial selection is required.

## Build and Package

Fruit Jam:

```sh
.venv/bin/python ./pbl configure --board fruitjam_rp2350
.venv/bin/python ./pbl build
hardware_tests/fruitjam_memory_lcd/build/_deps/picotool-build/picotool uf2 convert \
  build/src/fw/tintin_fw.elf \
  build/src/fw/tintin_fw.uf2 \
  --platform rp2350 \
  --abs-block
.venv/bin/python tools/fruitjam_pack_uf2.py \
  --firmware-uf2 build/src/fw/tintin_fw.uf2 \
  --resources build/system_resources.pbpack \
  --output build/artifacts/pebbleos-fruitjam-rp2350.uf2
```

Pico 2 W:

```sh
.venv/bin/python ./pbl configure --board pico2_w_rp2350
.venv/bin/python ./pbl build
hardware_tests/fruitjam_memory_lcd/build/_deps/picotool-build/picotool uf2 convert \
  build/src/fw/tintin_fw.elf \
  build/src/fw/pico2_w_tintin_fw.uf2 \
  --platform rp2350 \
  --abs-block 0x103fff00
.venv/bin/python tools/fruitjam_pack_uf2.py \
  --firmware-uf2 build/src/fw/pico2_w_tintin_fw.uf2 \
  --resources build/system_resources.pbpack \
  --output build/artifacts/pebbleos-pico2-w-rp2350.uf2 \
  --resources-address 0x10150000 \
  --resources-bank-size 0x000c0000
```

The Fruit Jam resource bank starts at `0x10620000`. The Pico 2 W resource bank
starts at `0x10150000`, and its RP2350-E10 absolute block is `0x103fff00`.

## Flashing

Fruit Jam debug CDC normally enumerates as:

```text
/dev/cu.usbmodemFJRP23501
```

The known Fruit Jam BOOTSEL serial used during bring-up is:

```text
16F06E2E896C3400
```

Flash Fruit Jam through the PebbleOS CDC `bootsel` command:

```sh
.venv/bin/python tools/rp2350_boot_volume.py \
  --cdc-bootsel-port /dev/cu.usbmodemFJRP23501 \
  --expect-serial 16F06E2E896C3400 \
  --copy build/artifacts/pebbleos-fruitjam-rp2350.uf2
```

Pico 2 W debug CDC normally enumerates as:

```text
/dev/cu.usbmodemPICO2WRP23501
```

The known live Pico 2 W BOOTSEL serial used during bring-up is:

```text
B2047FF29EB03C7E
```

Flash Pico 2 W through the PebbleOS CDC `bootsel` command:

```sh
.venv/bin/python tools/rp2350_boot_volume.py \
  --cdc-bootsel-port /dev/cu.usbmodemPICO2WRP23501 \
  --expect-serial B2047FF29EB03C7E \
  --copy build/artifacts/pebbleos-pico2-w-rp2350.uf2
```

If the app is not running, put the board in BOOTSEL manually and run the same
tool without `--cdc-bootsel-port`. Use `--read-info` when auditing the mounted
volume.

## USB CDC Debug Workflow

Use `tools/fruitjam_cdc_command.py` for noninteractive debug commands. Do not
run multiple CDC commands against the same port at the same time.

Common checks:

```sh
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 ping --timeout 3 --idle-timeout 0.2
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 progress --timeout 3 --idle-timeout 0.2
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 lcd --timeout 3 --idle-timeout 0.2
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 bt --timeout 3 --idle-timeout 0.2
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 esp --timeout 3 --idle-timeout 0.2
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 storage --timeout 3 --idle-timeout 0.2
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 appfetch --timeout 3 --idle-timeout 0.2
```

Frame capture:

```sh
.venv/bin/python tools/fruitjam_cdc_frame.py \
  -p /dev/cu.usbmodemFJRP23501 \
  -o build/fruitjam_latest.png \
  --pbm build/fruitjam_latest.pbm
```

The captured frame is the last framebuffer sent by the driver, not an optical
readback from the LCD.

Useful Fruit Jam commands include:

```text
help ping progress lcd lcdtest rtc rtcsync rtcset tzset i2cscan tasks gpio
gpiowatch buttons buttonmap buttonwatch button esp bt reason frame clearfault
reset btdisc btretry appfetch storage btforget btpair bootsel esppass apps
appcur appcheck applaunch watch watchdefault appsideload
```

Use `btforget` when the board pairing store must be cleared, and ask the user to
forget the watch in iOS Bluetooth/Core Devices before re-pairing. Use `btdisc`
to force-disconnect a stuck central and restore advertising.

## Bluetooth Workflow

Fruit Jam Bluetooth:

- The ESP32-C6 must run the controller-only HCI UART firmware from
  `hardware_tests/fruitjam_esp32c6_hci/`.
- Stock WiFiNINA/AirLift firmware is not compatible with this NimBLE path.
- The RP2350 NimBLE host talks H4 over UART1 through
  `src/fw/soc/rp2350/rp2350/rp2350_bluetooth_hci_fruitjam.c`.
- The user-facing device name is currently `Pebble RP2350`.
- The `bt` CDC command is the first truth source for connection, bonding,
  encryption, GATT discovery, PPOGATT, and Pebble Protocol packet counters.

Pico 2 W Bluetooth:

- The normal `pico2_w_rp2350` profile still uses `CONFIG_BT_FW_STUB=y`.
- The explicit compile-test profile is:

```sh
.venv/bin/python ./pbl configure --board pico2_w_rp2350 \
  -DCONFIG_BT_FW_NIMBLE=y \
  -DCONFIG_BT_FW_STUB=n
.venv/bin/python ./pbl build
```

- The future production backend should replace
  `rp2350_bluetooth_hci_cyw43_stub.c` with a CYW43439/CYW43 HCI transport.
- Use `hardware_tests/pico2_w_rp2350/cyw43_hci_probe/` as the small radio
  bring-up test before merging CYW43 into PebbleOS.

Mac/iPhone debugging:

```sh
.venv/bin/python tools/macos_ble_probe.py scan --timeout 8 --name "Pebble RP2350"
.venv/bin/python tools/fruitjam_ios_ble_debug.py --duration 35 --port /dev/cu.usbmodemFJRP23501
```

For iPhone syslog captures, pass a UDID on the command line only when needed;
do not commit it.

## ESP32-C6 Firmware Restore

The Fruit Jam Bluetooth path depends on custom ESP32-C6 firmware. Release both
pieces:

- `fruitjam-esp32c6-hci-YYYYMMDD.bin`: the ESP32-C6 merged flash image.
- `fruitjam-esp32c6-serial-passthrough-YYYYMMDD.uf2`: an RP2350 UF2 that turns
  the Fruit Jam RP2350 into a USB serial bridge to the ESP32-C6 ROM bootloader.

The passthrough UF2 is not the Bluetooth firmware. It is only the programmer
bridge used to flash the `.bin` into the ESP32-C6.

Build the ESP32-C6 firmware:

```sh
cd hardware_tests/fruitjam_esp32c6_hci
. /Users/fred/esp/v5.5.4/esp-idf/export.sh
idf.py set-target esp32c6
idf.py build
idf.py merge-bin
cd ../..
```

The merged C6 image is:

```text
hardware_tests/fruitjam_esp32c6_hci/build/merged-binary.bin
```

The RP2350 passthrough UF2 is:

```text
hardware_tests/fruitjam_esp32c6_hci/tools/SerialESPPassthrough.ino.uf2
```

Restore the ESP32-C6 firmware with the passthrough UF2:

```sh
# 1. Put Fruit Jam RP2350 into BOOTSEL.
.venv/bin/python tools/rp2350_boot_volume.py \
  --cdc-bootsel-port /dev/cu.usbmodemFJRP23501 \
  --expect-serial 16F06E2E896C3400 \
  --copy build/release/<tag>/fruitjam-esp32c6-serial-passthrough-YYYYMMDD.uf2

# 2. Find the new passthrough serial port.
ls /dev/cu.usbmodem*

# 3. Flash the C6 image through that passthrough port.
python -m esptool --chip esp32c6 --before no_reset --after no_reset \
  -p /dev/cu.usbmodem<PASSTHROUGH> -b 115200 \
  write_flash 0 build/release/<tag>/fruitjam-esp32c6-hci-YYYYMMDD.bin

# 4. Restore the normal Fruit Jam PebbleOS UF2 onto the RP2350.
.venv/bin/python tools/rp2350_boot_volume.py \
  --expect-serial 16F06E2E896C3400 \
  --copy build/release/<tag>/pebbleos-fruitjam-rp2350-preview-YYYYMMDD.uf2
```

If PebbleOS is already running and the CDC shell responds, the `esppass`
command can replace the passthrough UF2 step:

```sh
python3 - <<'PY'
import serial
import time

with serial.Serial("/dev/cu.usbmodemFJRP23501", 115200, timeout=0.2) as port:
    port.write(b"esppass\r\n")
    port.flush()
    time.sleep(0.5)
PY

python -m esptool --chip esp32c6 --before no_reset --after no_reset \
  -p /dev/cu.usbmodemFJRP23501 -b 115200 \
  write_flash 0 build/release/<tag>/fruitjam-esp32c6-hci-YYYYMMDD.bin
```

After `esppass`, reset or reflash the RP2350 to return from bridge mode to
PebbleOS.

## Time and RTC

Fruit Jam currently uses a DS1307-compatible RTC profile on GPIO20/GPIO21 at
I2C address `0x68` when present. Use:

```sh
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 i2cscan
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 rtc
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 rtcset "$(date -u +%s)"
.venv/bin/python tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 tzset America/Toronto
```

The RTC type, pins, and address are board-profile settings so a later board can
select another RTC without changing the shared driver.

## Release Checklist

1. Audit `git status --short --branch`, including submodules.
2. Run `git diff --check`.
3. Compile Python helpers:

```sh
python3 -m py_compile \
  tools/fruitjam_pack_uf2.py \
  tools/fruitjam_cdc_command.py \
  tools/fruitjam_cdc_frame.py \
  tools/fruitjam_sideload_pbw.py \
  tools/rp2350_boot_volume.py \
  tools/macos_ble_probe.py \
  tools/fruitjam_ios_ble_debug.py
```

4. Build and package both PebbleOS target UF2s with the commands above.
5. Build the ESP32-C6 HCI firmware with ESP-IDF and `idf.py merge-bin`.
6. Copy release assets into `build/release/<tag>/` with target-specific names.
7. Generate `SHA256SUMS.txt`.
8. Push source and tag.
9. Create a GitHub prerelease with both PebbleOS UF2s, the C6 merged binary, the
   C6 passthrough UF2, and `SHA256SUMS.txt`.

Recommended asset names:

```text
pebbleos-fruitjam-rp2350-preview-YYYYMMDD.uf2
pebbleos-pico2-w-rp2350-preview-YYYYMMDD.uf2
fruitjam-esp32c6-hci-YYYYMMDD.bin
fruitjam-esp32c6-serial-passthrough-YYYYMMDD.uf2
SHA256SUMS.txt
```

The release notes should say clearly that Fruit Jam Bluetooth is hardware
active, while Pico 2 W Bluetooth remains a stub in the normal PebbleOS UF2.
