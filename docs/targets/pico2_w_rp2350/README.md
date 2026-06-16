# Pico 2 W RP2350 Target Notes

Bring-up notes for the Raspberry Pi Pico 2 W PebbleOS target.

The target is selectable independently from Fruit Jam. The design goal is to
share common RP2350/Pebble display work while keeping these board-specific
pieces separate:

- Pico 2 W pin mapping
- CYW43439/CYW43 Bluetooth transport
- Pico 2 W flash and resource layout
- Pico 2 W UF2 packaging/validation commands
- Optional external RTC wiring on GP4/GP5
- External NOR storage profile for Pebble resources and filesystem data

The shared RP2350 Sharp Memory LCD transport now takes its SPI and control pins
from the board-provided `Rp2350MemoryLcdConfig` stored in `DISPLAY`. The Pico 2 W
board file should provide that config instead of cloning the Fruit Jam LCD code.

The shared RP2350 RTC fallback is available through the board-provided
`BOARD_CONFIG_EXTERNAL_RTC`. The RTC type, address, and pins are selected in
Kconfig with `CONFIG_RP2350_EXTERNAL_RTC_*`. The Pico 2 W profile defaults to
`CONFIG_RP2350_EXTERNAL_RTC_NONE=y` because the current live board has no RTC
attached; future hardware can enable a DS1307-compatible device at I2C address
`0x68` on GP4 SDA and GP5 SCL without changing shared RP2350 RTC code.
If the external RTC is absent or invalid, or if no timezone is set, automatic
time mode requests phone time every 10 seconds once a system session is
available.

The profile is selectable with `./pbl configure --board pico2_w_rp2350`. It uses
stub Bluetooth for now; the missing production piece is a CYW43439 HCI transport
that can replace the stub without changing the board/display/resource profile.
The normal stub profile builds after the shared RTC Kconfig/profile refactor,
RP2350 timer tick initialization changes, shared RP2350 `clk_peri` cleanup,
visible RP2350 reconnect-advertising change, AppFetch diagnostics update, and
Pico-specific USB identity. The live Pico 2 W board is now hardware-verified
with an SDK-style RP2350 runtime clock setup: it boots PebbleOS, enumerates USB
CDC, runs the Sharp Memory LCD task, and captures a TicToc framebuffer.

Pico 2 W uses the SDK's 4 MiB RP2350 flash size. Do not flash a Fruit Jam UF2
or a Pico UF2 packed with Fruit Jam addresses: Fruit Jam places resources at
`0x10620000`, which is outside the Pico 2 W flash. The Pico profile's resource
bank starts at `0x10150000`, and its RP2350-E10 absolute block is at
`0x103fff00`.

The current Pico profile also selects
`CONFIG_RP2350_PEBBLE_STORAGE_ONBOARD_XIP=y` as an early bring-up fallback. It
is not the desired long-term storage mode: app installs and resource/filesystem
updates should move to `CONFIG_RP2350_PEBBLE_STORAGE_EXTERNAL_NOR=y` once an
RP2350 external NOR transport and wiring are in place.

The BOOTSEL volume files are generic across RP2350 boards. `INFO_UF2.TXT`
confirms that the mounted volume is an RP2350 bootloader, but it does not say
Fruit Jam or Pico 2 W. On macOS, check the USB serial/location before copying:

```sh
.venv/bin/python tools/rp2350_boot_volume.py
```

For the current live Pico 2 W board, require the known BOOTSEL serial when
flashing:

```sh
.venv/bin/python tools/rp2350_boot_volume.py \
  --expect-serial B2047FF29EB03C7E \
  --copy build/artifacts/pico2_w_rp2350_pebbleos_sdk_clocks.uf2
```

Latest normal profile build evidence, 2026-06-15:

```sh
PATH="/Users/fred/Documents/Code/PebbleRP2350/.venv/bin:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH" .venv/bin/python ./pbl configure --board pico2_w_rp2350
PATH="/Users/fred/Documents/Code/PebbleRP2350/.venv/bin:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH" .venv/bin/python ./pbl build
```

- Firmware size: 716791 / 16777216 bytes
- Resource size: 741474 / 2097152 bytes

Latest Pico-safe hardware-verified packaged UF2, 2026-06-15:

```sh
hardware_tests/fruitjam_memory_lcd/build/_deps/picotool-build/picotool uf2 convert \
  build/src/fw/tintin_fw.elf \
  build/src/fw/pico2_w_tintin_fw.uf2 \
  --platform rp2350 \
  --abs-block 0x103fff00

.venv/bin/python tools/fruitjam_pack_uf2.py \
  --firmware-uf2 build/src/fw/pico2_w_tintin_fw.uf2 \
  --resources build/system_resources.pbpack \
  --output build/artifacts/pico2_w_rp2350_pebbleos_sdk_clocks.uf2 \
  --resources-address 0x10150000 \
  --resources-bank-size 0x000c0000
```

- Path: `build/artifacts/pico2_w_rp2350_pebbleos_sdk_clocks.uf2`
- SHA-256:
  `7741f4a021dc7137138ac77873b1b9285726654a4090fb79660f19060558687b`
- UF2 address check: family `0xe48bff59` range `0x10000000-0x10205100`;
  RP2350-E10 absolute block family `0xe48bff57` range
  `0x103fff00-0x10400000`
- USB identity: product `Pico 2 W PebbleOS Debug`, serial `PICO2WRP2350`

Hardware flash and runtime evidence:

```sh
/Users/fred/.pico-sdk/picotool/2.1.1/picotool/picotool load \
  -f --ser B2047FF29EB03C7E -v -x \
  build/artifacts/pico2_w_rp2350_pebbleos_sdk_clocks.uf2
```

- BOOTSEL device: chip serial `B2047FF29EB03C7E`, flash size `4096K`.
- Picotool flash and verify completed with `OK`, then rebooted into the app.
- USB CDC returned at `/dev/cu.usbmodemPICO2WRP23501`.
- CDC `ping` returned `pong`; `reason` returned `reason code=00`.
- CDC `progress` returned `stage=7 label="07 LCD"`.
- CDC `lcd` returned `hw_spi=1 spi=0 ... hz=1000000 ... rows=168`.
- CDC `tasks` included `USBDebug`, `SharpVCOM`, `PULSE`, and
  `App <TicToc>`.
- Frame capture: `/tmp/pico2_w_after_sdk_clocks.png` showed the TicToc
  framebuffer (`January 1`, `12:01`).

Hardware note: an earlier Pico UF2 packed with Fruit Jam's 16 MiB resource
address did not come back over USB on the live Pico. Use BOOTSEL to recover and
flash only the Pico-safe UF2 above.

The normal profile intentionally keeps `CONFIG_BT_FW_STUB=y`, but the shared
RP2350 NimBLE transport also compiles for Pico 2 W with:

```sh
./pbl configure --board pico2_w_rp2350 -DCONFIG_BT_FW_NIMBLE=y -DCONFIG_BT_FW_STUB=n
./pbl build
```

That build uses the `pico2w-cyw43-pending` backend stub. The next production
step is replacing that stub with the real CYW43439/CYW43 HCI backend.

There is a standalone Pico SDK Bluetooth smoke test at
`hardware_tests/pico2_w_rp2350/cyw43_hci_probe`. It uses the SDK
`pico_btstack_cyw43` path and advertises as `Pico2W-HCI` after the controller is
awake, giving us a small radio bring-up target before integrating CYW43 into
PebbleOS itself.

Latest local probe build evidence, 2026-06-15:

```sh
cmake --build hardware_tests/pico2_w_rp2350/cyw43_hci_probe/build
```

The build was already up to date. The generated UF2 is:

- Path:
  `hardware_tests/pico2_w_rp2350/cyw43_hci_probe/build/pico2_w_cyw43_hci_probe.uf2`
- Size: 698880 bytes
- SHA-256:
  `db5e675206d9cc71d12ebd51e24a230df8e610bba5ceb4df03703926c1915efd`
- CMake cache: `PICO_BOARD=pico2_w`, `PICO_PLATFORM=rp2350-arm-s`,
  `PICO_SDK_PATH=/Users/fred/.pico-sdk/sdk/2.1.1`
- Hardware check: the corrected probe enumerated as USB serial
  `B2047FF29EB03C7E`, reported `hci_state=2 advertising=1`, and was visible to
  macOS BLE scan as `Pico2W-HCI`.

The initial resource map reuses the Fruit Jam/Flint resource IDs so the firmware
and Core Devices app keep the same Pebble 2 Duo-compatible ABI while the Pico 2 W
hardware backend is brought up separately.
