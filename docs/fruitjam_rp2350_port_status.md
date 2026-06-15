# Fruit Jam RP2350 Port Status

Date: 2026-06-14

## Current Snapshot

This is a preview hardware bring-up port for the Adafruit Fruit Jam RP2350. It
is useful for flashing and continuing development on real hardware, but it is
not a finished watch port yet.

Latest packaged RP2350 UF2:

- Path: `build/src/fw/tintin_fw_with_resources.uf2`
- Release tag: `v9.9.9-fruitjam-rp2350-preview-20260614`
- Size: 2.9 MiB
- Contents: RP2350 PebbleOS firmware plus system resources packed at
  `0x10620000`
- SHA-256: published with the GitHub release asset. The firmware contains
  git-version metadata, so the final release checksum is generated from the
  tagged commit rather than hard-coded here.

Current hardware-verified behavior:

- The Fruit Jam boots the RP2350 image far enough to display Pebble UI on the
  144x168 Sharp Memory LCD and hand off to the launcher.
- The board is configured as fixed-power/no-battery hardware. PebbleOS reports
  100% battery, suppresses charging and low-battery flows, and does not enter
  battery standby paths on a bench power-only setup.
- The USB CDC debug port enumerates as `/dev/cu.usbmodemFJRP23501`.
- The CDC `frame` command captures the last 144x168 1bpp LCD framebuffer sent by
  the display driver; it can be converted with
  `tools/fruitjam_cdc_frame.py -p /dev/cu.usbmodemFJRP23501 -o /tmp/fruitjam_latest.png`.
- The CDC shell can be queried without an interactive terminal, for example:
  `tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 buttons`.
- Bluetooth reaches NimBLE host start and BLE advertising through the Fruit Jam
  ESP32-C6 controller-only HCI UART firmware. A hardware CDC snapshot showed
  `bt driver ... ok=1`, `adv active=1`, and local name `Pebble A35A`.
- The CDC `bootsel` command now uses the RP2350 boot ROM path and has been
  tested to mount `/Volumes/RP2350`; 1200-baud CDC reset-to-BOOTSEL uses the
  same helper.
- The Fruit Jam firmware includes a boot-loop guard. After repeated unsafe
  resets before the launcher reaches ready state, it clears the fault state and
  enters BOOTSEL instead of staying trapped in a reset loop.

USB CDC debug commands:

```text
help ping progress tasks buttons esp bt reason frame clearfault reset bootsel esppass
```

Bluetooth state:

- The ESP32-C6 has been flashed with the controller-only HCI UART firmware from
  `hardware_tests/fruitjam_esp32c6_hci/`.
- The RP2350 NimBLE host uses the Fruit Jam UART1 H4 transport on GPIO8/GPIO9.
- RP2350 NimBLE auto-start is disabled for this target so PebbleOS can register
  services before the host starts. Leaving NimBLE's default auto-start enabled
  made the host synchronize before Pebble's Bluetooth service was ready.
- The ESP HCI UART RX path is interrupt-backed, which prevents the controller's
  larger startup responses from overflowing the RP2350 UART FIFO.
- The CDC `esp` command exposes HCI command/event history and UART RX ring
  counters. The CDC `bt` command exposes Bluetooth control state, driver start
  stage, advertising counters, GAP events, and advertising payload prefixes.
- Latest hardware evidence: HCI startup completed with no H4 parse errors,
  `bt driver enter=1 done=1 ok=1`, `adv active=1`, advertising interval
  `20-20` ms, and local name `Pebble A35A`.
- Stock WiFiNINA/AirLift firmware cannot be kept for this NimBLE path because
  it is a high-level SPI coprocessor firmware, not a raw BLE HCI controller.

Build and packaging commands:

```sh
PATH="/Users/fred/Documents/Code/PebbleRP2350/.venv/bin:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH" .venv/bin/python ./pbl configure --board fruitjam_rp2350
PATH="/Users/fred/Documents/Code/PebbleRP2350/.venv/bin:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH" .venv/bin/python ./pbl build
hardware_tests/fruitjam_memory_lcd/build/_deps/picotool-build/picotool uf2 convert build/src/fw/tintin_fw.elf build/src/fw/tintin_fw.uf2 --platform rp2350 --abs-block
.venv/bin/python tools/fruitjam_pack_uf2.py
```

## Hardware Bring-Up

The standalone Pico SDK memory LCD test lives in:

- `hardware_tests/fruitjam_memory_lcd/`
- UF2: `hardware_tests/fruitjam_memory_lcd/build/fruitjam_memory_lcd_test.uf2`

The UF2 was flashed successfully to the Adafruit Fruit Jam RP2350 while the board
was in BOOTSEL. The attached Waveshare 1.3 inch Memory LCD displayed the
alternating all-white and test-pattern screens correctly.

Standalone test behavior:

- Uses Fruit Jam default SPI pins: SCK GPIO30, MOSI GPIO31, MISO GPIO28.
- Uses LCD_CS GPIO6, RAM_CS GPIO41, DISP GPIO7, EIN GPIO10.
- Keeps RAM_CS high for LCD-only testing.
- Drives DISP high.
- Toggles EXTCOMIN/EIN periodically.
- Draws border, checkerboard, diagonal, and text.
- Uses Button 1 GPIO0, Button 2 GPIO4, Button 3 GPIO5.
- Holding all three buttons for about 2 seconds enters BOOTSEL via software.

## PebbleOS Build State

The `fruitjam_rp2350` board target now configures and builds a linked firmware
ELF/bin:

```sh
PATH="/Users/fred/Documents/Code/PebbleRP2350/.venv/bin:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH" .venv/bin/python ./pbl configure --board fruitjam_rp2350
PATH="/Users/fred/Documents/Code/PebbleRP2350/.venv/bin:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH" .venv/bin/python ./pbl build
```

Last successful build:

- Firmware: `build/src/fw/tintin_fw.elf`
- Binary: `build/src/fw/tintin_fw.bin`
- Firmware-only UF2: `build/src/fw/tintin_fw.uf2`
- Firmware + system resources UF2:
  `build/src/fw/tintin_fw_with_resources.uf2`
- Resources: `build/system_resources.pbpack`
- Firmware size: 763731 / 16777216 bytes
- Resource size: 741474 / 2097152 bytes
- UF2 SHA-256: generated for the tagged GitHub release asset. The firmware
  includes git-version metadata, so local rebuilds from different commits or
  tags intentionally produce different checksums.

The PebbleOS UF2 was generated with Picotool's RP2350-E10 absolute block:

```sh
hardware_tests/fruitjam_memory_lcd/build/_deps/picotool-build/picotool uf2 convert build/src/fw/tintin_fw.elf build/src/fw/tintin_fw.uf2 --platform rp2350 --abs-block
.venv/bin/python tools/fruitjam_pack_uf2.py
```

Picotool recognizes the ELF/UF2 as an RP2350 ARM Secure image. The image
definition block is at `0x100000f8`, immediately after the vector table.
`tools/fruitjam_pack_uf2.py` appends `build/system_resources.pbpack` to the
same UF2 at `FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN` (`0x10620000`) so
PebbleOS has the system resource bank it expects, like Asterix does on its QSPI
NOR flash.
Binary verification of `build/src/fw/tintin_fw_with_resources.uf2` confirms that
the recovered UF2 payload at `0x10620000` starts with
`build/system_resources.pbpack` byte-for-byte and pads the final UF2 block with
`0xff`.

Latest local validation:

- `git diff --check`
- `./pbl build`
- Picotool UF2 conversion plus `tools/fruitjam_pack_uf2.py`
- `shasum -a 256 build/src/fw/tintin_fw_with_resources.uf2`
- `picotool info -a build/src/fw/tintin_fw_with_resources.uf2`
- Python binary resource-payload check
- `python -m py_compile tools/fruitjam_pack_uf2.py`
- `ruff check tools/fruitjam_pack_uf2.py`
- ESP-IDF build and merge-bin for
  `hardware_tests/fruitjam_esp32c6_hci/`
- `./pbl test -M test_flash_region --no_images`
- Hardware flash to Fruit Jam RP2350 via `/Volumes/RP2350`
- CDC debug state capture with `progress`, `esp`, `bt`, `tasks`, and `reason`
- CDC frame capture:
  `tools/fruitjam_cdc_frame.py -p /dev/cu.usbmodemFJRP23501 -o /tmp/fruitjam_ble_advertising.png`

Hardware test notes:

- The first PebbleOS UF2 flashed successfully but did not produce visible LCD
  output.
- A later UF2 that drew a black/white checkerboard, border, and diagonal from
  `soc_early_init()` using SIO bit-banged SPI was hardware-verified on
  2026-06-14. This proves the RP2350 image reaches `main()` and can drive the
  LCD before FreeRTOS starts.
- The early smoke frame and display-driver diagnostic now share a Fruit Jam boot
  progress helper that draws the Pebble splash logo with a moving progress bar
  instead of the checkerboard-style lab pattern.
- The latest local build advances the progress marker through early LCD, PFS,
  driver init, resources, display init, display-driver ready, compositor,
  Bluetooth task init, services, Bluetooth controller start, Bluetooth
  unavailable, Bluetooth ready, and launcher handoff. It now also renders an
  explicit stage label on the LCD, such as `15 BT START`, `16 BT NO HCI`, or
  `18 LAUNCH`, so hardware observations do not depend on serial output. If
  system resources are missing or corrupt, it shows a solid progress bar and
  `ERR RES` before the existing reset path.
- A diagnostic UF2 with
  `CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_BRINGUP_LOOP=y` was built for
  display-driver bring-up. Its expected visual sequence is the early Pebble
  splash/progress frame, then a FreeRTOS display-init loop alternating all-white
  and splash/progress frames every ~1.8 seconds.
- The current default build has the bring-up loop disabled:
  `# CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_BRINGUP_LOOP is not set`. The UF2
  to flash for normal bring-up is now
  `build/src/fw/tintin_fw_with_resources.uf2`. It should briefly show the early
  Pebble splash/progress frame, then continue into Pebble's boot splash and
  compositor path with a valid system resource bank in flash.
- The early smoke test and RP2350 display driver now share the same SIO
  bit-banged LCD helper, so bring-up no longer has two copies of the Sharp LCD
  wire protocol.
- The default display path now stays on the hardware-verified SIO bit-banged
  transport, so the next UF2 is biased toward showing the Pebble boot splash and
  compositor output instead of depending on unverified SPI1.
- The RP2350 SPI1 display transport still exists behind
  `CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_SPI1_EXPERIMENT`. Enabling that
  option makes `display_init()` switch from the horizontal bitbang marker to a
  vertical/X-pattern SPI marker for transport diagnostics.
- `boot_splash` is enabled for `CONFIG_SOC_RP2350` and writes through a packed
  1bpp pixel helper rather than treating the buffer as one byte per pixel.
- `display_update_boot_frame()` now uses Pebble's padded raw framebuffer stride
  instead of the LCD wire stride.
- RP2350 display row formatting now mirrors the existing nRF Sharp driver:
  non-rotated rows reverse bits for MSB-first GPIO output, and 180-degree
  rotation reverses both byte order and bit order.
- The all-three-buttons BOOTSEL recovery path now looks up the RP2350 ROM
  `reboot` function (`'R','B'`) instead of the RP2040-style `reset_usb_boot`
  function.
- The latest rebuilt UF2 was copied to `/Volumes/RP2350` on 2026-06-14 at about
  17:00 EDT. The BOOTSEL volume disappeared after the copy, which is consistent
  with the board accepting the UF2 and rebooting.
- RP2350 now links NimBLE against a Fruit Jam ESP32-C6 H4 UART transport instead
  of `third_party/nimble/transport/hci_stub.c`. The helper resets the ESP into
  Bluetooth mode with ESP_CS low, ESP GPIO0/RTS high for normal boot then low
  for ready-to-receive, ESP_BUSY as CTS, and UART1 on GPIO8 TX / GPIO9 RX at
  115200 baud. The transport now sees clean startup HCI traffic and the Pebble
  Bluetooth service reaches advertising on hardware.
- RP2350 now links a real PL011 debug UART driver instead of the UART stub.
  `DBG_UART` is UART0 on Fruit Jam GPIO44 TX / GPIO45 RX at Pebble's default
  230400 baud after `dbgserial_init()`. This avoids the ESP32-C6 HCI UART1 pins
  on GPIO8/GPIO9. The driver is build-verified only and still needs serial
  hardware verification.
- A 17:06 EDT copy attempt of the UART-enabled UF2 to `/Volumes/RP2350` was not
  confirmed. macOS left the RP2350 mass-storage mount unresponsive during the
  copy, with `cp` blocked in device I/O.
- The combined firmware+resources UF2 built before the RP2350 flash-driver
  update was then flashed successfully with USB
  `picotool load -v -x build/src/fw/tintin_fw_with_resources.uf2`. Picotool
  verified the programmed flash ranges and rebooted the board into the
  application.
- The local build after the RP2350 flash-driver, boot-progress, and
  all-buttons BOOTSEL suppression updates was flashed successfully with USB
  `picotool load -v -x build/src/fw/tintin_fw_with_resources.uf2` on
  2026-06-14. Picotool verified the programmed flash ranges and rebooted the
  board into the application. Its combined UF2 hash is
  `69238cdefa3c02186dda8b96fa86c02cd24869f818a1ed49996fee46d1c9aa36`.
- The latest local build with extended post-display progress markers was
  flashed successfully with USB
  `picotool load -v -x build/src/fw/tintin_fw_with_resources.uf2` on
  2026-06-14. Picotool verified the programmed flash ranges and rebooted the
  board into the application. A follow-up `picotool info -a` 2 seconds later did
  not find a device in BOOTSEL mode. Its combined UF2 hash is
  `2fb787c11b2ab48348a48a0c61d24598b5f46def84a4d2fdbc40b59872ba263c`.
- The latest local build after the Fruit Jam Bluetooth start-failure guardrails
  was flashed successfully with USB
  `picotool load -v -x build/src/fw/tintin_fw_with_resources.uf2` on
  2026-06-14. Picotool verified the programmed flash ranges and rebooted the
  board into the application. Its combined UF2 hash is
  `cc60527f5204241cdf0c3ebe6bba01a93ee6903a85532f7cca13092418f2e085`.
- The latest local build after adding readable boot-progress stage labels,
  the 90 ms Up+Select Down-chord grace window, and the ESP_BUSY pull-up/ready
  wait was copied to `/Volumes/RP2350` on 2026-06-14 at about 18:27 EDT. The
  BOOTSEL volume disappeared after the copy, which is consistent with the board
  accepting the UF2 and rebooting. Its combined UF2 hash is
  `9615f50a703a56dffc1e2ee79234e3c7b195ebb227db0cb2f82b2484c86a2eb8`.
- After hardware observation showed a fast repeating boot-progress sequence,
  the latest diagnostic build added a short hold after each progress label and
  Fruit Jam-only software-failure labels (`ERR RESET`, `ERR ASSERT`,
  `ERR HARD`, `ERR STACK`, `ERR OOM`, `ERR WATCH`, and `ERR CORE`). It was
  copied to `/Volumes/RP2350` on 2026-06-14 at about 18:32 EDT. The BOOTSEL
  volume disappeared after the copy, which is consistent with the board
  accepting the UF2 and rebooting. Its combined UF2 hash is
  `d5b139d5f72acfbb941a38fc2dd149276c38382443c67a1859daca899164be28`.
- The latest local build maps the RP2350 backup-register IDs used for bootbits,
  reboot reason, last launched app, flash erase progress, and current time onto
  watchdog scratch registers. This should keep reset diagnostics alive across
  `NVIC_SystemReset()` instead of losing them when `.bss` is cleared. The build
  is packaged and locally validated but not yet flashed. Its combined UF2 hash
  is `cc8e9469e35eec1c7f84e8c8a6b113f73ed6de18f9cdc1956f42895877395e59`.
- The latest local build adds a Fruit Jam USB CDC debug shell and LCD frame
  capture. It was copied to `/Volumes/RP2350` on 2026-06-14. The BOOTSEL
  volume disappeared after the copy, and macOS enumerated the debug port as
  `/dev/cu.usbmodemFJRP23501`. Working CDC commands are `help`, `ping`,
  `reason`, `frame`, and `reset`. In this older build, `bootsel` responded but
  did not reliably mount the RP2350 BOOTSEL drive during testing. `frame`
  streams a 144x168 PBM P4 snapshot of the last framebuffer rows sent by the LCD
  driver.
  This command captured a valid boot screen after a CDC-triggered reset:
  `tools/fruitjam_cdc_frame.py -p /dev/cu.usbmodemFJRP23501 -o /tmp/fruitjam_frame.png`.
  Its combined UF2 hash is
  `25fc6ee23030e11d9a47db740ac22b2d8fd4f9d1bc009cfeab636532e6c837a3`.
- The latest local build fixes the Fruit Jam CDC `bootsel` command to use the
  central RP2350 ARM boot ROM helper, adds standard 1200-baud CDC
  reset-to-BOOTSEL handling, adds an `esppass` CDC command, and adds a
  three-strike unsafe-reset guard that enters BOOTSEL instead of staying in a
  boot loop. `esppass` resets the ESP32-C6 into its ROM serial bootloader and
  bridges the Fruit Jam debug CDC port to the ESP UART, so the C6 can be flashed
  with esptool without first loading Adafruit's passthrough UF2. The `bootsel`
  CDC command was tested after this change and mounted `/Volumes/RP2350`. Its
  combined UF2 hash is
  `68bdfa439adf5f171e72855c844985b17ef317b6ae1d3735682108f2485a0c7e`.
- A Fruit Jam ESP32-C6 controller-only NimBLE HCI firmware has been scaffolded
  and build-verified at `hardware_tests/fruitjam_esp32c6_hci/`. It uses
  ESP-IDF controller-only H4 UART mode on ESP32-C6 UART1, with ESP GPIO16 TX to
  RP2350 GPIO9 RX and ESP GPIO17 RX from RP2350 GPIO8 TX. IO18 drives
  `ESP_BUSY` high while the controller is booting and low once HCI traffic is
  allowed. The merged ESP32-C6 image is
  `hardware_tests/fruitjam_esp32c6_hci/build/merged-binary.bin`, SHA-256
  `b121f6cc717a3ed13fc776a3f7ded447d09650cb737379caaced20d775d0dd1d`.
  This firmware replaces the stock WiFiNINA/AirLift SPI firmware while testing
  RP2350-side NimBLE; restoring WiFiNINA later means reflashing Adafruit's
  Fruit Jam NINA firmware.
- The ESP32-C6 HCI image above was flashed through Adafruit's
  `SerialESPPassthrough.ino.uf2` on 2026-06-14. Esptool detected an
  ESP32-C6FH4 revision v0.2 with embedded 4 MB flash, wrote 587792 bytes at
  offset 0, and verified the flash hash. The RP2350 was then put back into
  BOOTSEL and restored to the PebbleOS Fruit Jam UF2 listed above.
- The current Fruit Jam UF2 listed above was flashed on 2026-06-14 and captured
  a Pebble splash frame at `/tmp/fruitjam_fixed_power_final.png`. The reboot
  reason was clear. The board is configured as fixed-power, so PebbleOS reports
  100% battery, not charging, not plugged, suppresses charging UI states, and
  never enters low-power or low-battery standby.
- The current packaged release build adds deeper HCI diagnostics: outgoing HCI
  command opcode/length counts, incoming command-complete/status event counts,
  last event status/opcode, and a short raw event prefix in the CDC `esp`
  output. It also keeps UART overrun-tagged bytes instead of dropping the byte
  outright. The release UF2 checksum is published in the GitHub release notes.
- The latest local build fixes Fruit Jam RP2350 NimBLE startup by disabling
  NimBLE host auto-start for the RP2350 target, adding an interrupt-backed ESP
  HCI UART RX ring, and exposing Bluetooth control/driver-stage diagnostics via
  CDC. It was flashed with USB `picotool load -v -x` on 2026-06-14, verified by
  Picotool, rebooted into PebbleOS, and advertised as `Pebble A35A`.

## Current Scaffold

The Fruit Jam target includes:

- Board config under `boards/fruitjam_rp2350/`.
- RP2350 SoC config under `src/fw/soc/rp2350/`.
- Cortex-M33 CMSIS shim under `third_party/cmsis_core/pebble/rp2350.h`.
- RP2350 IRQ table at `src/fw/irq_rp2350.def`.
- RP2350 Picobin image-definition block under `src/fw/soc/rp2350/rp2350/`.
- Fruit Jam board pins and display dimensions under `src/fw/board/`.
- Fruit Jam fixed-power mode via `BOARD_CONFIG_POWER.fixed_power`. This is
  enabled for the no-battery bench setup and disables battery/charging UI,
  low-power transitions, critical battery lockout, USB charge-state reporting,
  and the periodic voltage battery update loop.
- 16 MB XIP flash layout under `src/fw/flash_region/flash_region_rp2350_xip.h`.
- Polling RP2350 Sharp Memory LCD driver at
  `src/fw/drivers/display/sharp_ls013b7dh01/sharp_ls013b7dh01_rp2350.c`.
- Shared Fruit Jam LCD GPIO transport, plus an opt-in experimental RP2350 SPI1
  transport, at
  `src/fw/soc/rp2350/rp2350/fruitjam_lcd.c`.
- Dormant Fruit Jam PSRAM probe source at
  `src/fw/soc/rp2350/rp2350/fruitjam_psram.c`. It is not compiled into the
  default image while resource/filesystem flash bring-up is the priority.
- Early Fruit Jam LCD smoke test in `src/fw/soc/rp2350/rp2350/init.c`, now
  showing the Pebble splash logo/progress frame.
- Fruit Jam boot-progress helper at
  `src/fw/soc/rp2350/rp2350/fruitjam_boot_progress.c`, used by early LCD smoke,
  PFS bring-up, system resource validation, display-driver diagnostics,
  compositor bring-up, Bluetooth init, services init, launcher handoff, and
  Fruit Jam-only software-failure display before reset.
- Fruit Jam USB CDC debug helper at
  `src/fw/soc/rp2350/rp2350/fruitjam_usb_debug.c`. It uses TinyUSB CDC, mirrors
  `dbgserial` output after USB init, exposes `help`/`ping`/`reason`/`frame`/
  `reset`/`bootsel`/`clearfault`/`tasks`/`buttons`/`esp`/`bt`/`esppass`, and
  programs `PLL_USB`/`clk_usb` with bounded waits so failed USB clock bring-up
  cannot freeze PebbleOS at the early LCD screen.
- LCD framebuffer capture shadowing in
  `src/fw/soc/rp2350/rp2350/fruitjam_lcd.c`. It records the last rows sent by
  the firmware to the Sharp Memory LCD; it is not an electrical readback from
  the display glass.
- RP2350 XIP flash driver at `src/fw/drivers/flash/rp2350_xip.c`. It runs
  erase/program operations from `.ramfunc`, uses the RP2350 boot ROM flash
  helpers, and now sends explicit 4 KiB erase commands for Pebble subsector
  erases while keeping 64 KiB block erases for full-sector requests.
- Real RP2350 GPIO driver at `src/fw/drivers/gpio/rp2350.c`.
- Polling RP2350 GPIO button driver at `src/fw/drivers/rp2350/button.c`.
  Button 1/GPIO0 is Back, Button 2/GPIO4 is Up, Button 3/GPIO5 is Select, and
  Button 2 + Button 3 is mapped to Pebble Down. Up or Select is delayed for a
  90 ms grace window so a slightly staggered Down chord does not leak an
  accidental individual click. Holding all three buttons for about 2 seconds
  enters BOOTSEL through the RP2350 boot ROM. While all three physical buttons
  are held, ordinary Pebble button events are suppressed so the recovery chord
  is not also delivered as Back+Down to the UI.
  The CDC `buttons` command reports raw physical GPIO state, debounced physical
  state, emitted Pebble button state, the live `button_get_state_bits()` result,
  pending chord-delay state, BOOTSEL hold samples, and the last emitted button
  event so this mapping can be verified remotely.
- Fruit Jam ESP32-C6 HCI helper at `src/fw/soc/rp2350/rp2350/fruitjam_esp.c`.
  It uses UART1 on GPIO8/GPIO9, ESP_CS GPIO46, ESP_BUSY GPIO3,
  ESP_RESET/PERIPH_RESET GPIO22, and ESP_IRQ/GPIO0/RTS GPIO23. ESP_BUSY is
  configured with a pull-up so a missing or resetting C6 fails closed, and HCI
  startup waits up to 3 seconds for the C6 firmware to actively drop BUSY before
  draining RX. HCI writes fail instead of transmitting if ESP_BUSY/CTS never
  becomes ready.
- The Fruit Jam ESP helper also exposes a raw serial passthrough mode for the
  USB debug shell's `esppass` command. This mode holds the ESP boot pin low,
  resets the C6 into its ROM serial bootloader, bridges USB CDC traffic to
  UART1, and mirrors CDC baud-rate changes onto UART1 for esptool.
- NimBLE H4 transport for RP2350 at
  `third_party/nimble/transport/hci_fruitjam_rp2350.c`.
- RP2350 NimBLE build override in `third_party/nimble/wscript_build` disables
  `BLE_HS_AUTO_START` so the host starts only after Pebble's Bluetooth service
  has registered GAP, GATT, DIS, pairing, and BAS services.
- Fruit Jam currently expects the ESP32-C6 controller-only HCI firmware for
  PebbleOS Bluetooth. Stock WiFiNINA/AirLift firmware is a high-level
  coprocessor protocol and is not the raw HCI controller interface that the
  RP2350-side NimBLE host expects.
- ESP32-C6 controller-only NimBLE HCI firmware scaffold at
  `hardware_tests/fruitjam_esp32c6_hci/`. Build it with ESP-IDF v5.5.4 using
  `idf.py set-target esp32c6`, `idf.py build`, and `idf.py merge-bin`, then
  flash `build/merged-binary.bin` through Adafruit's Fruit Jam serial
  passthrough UF2.
- RP2350 debug UART driver at `src/fw/drivers/uart/rp2350.c`.
- RP2350 large-segment heap support in `src/libutil/heap.c`; the live kernel
  heap remains internal SRAM only for now.
- Fruit Jam UF2 resource packer at `tools/fruitjam_pack_uf2.py`.
- Temporary RP2350 RTC, EXTI, battery, watchdog, and hardware feature
  stubs.

## Important Caveats

This is a hardware-booting preview port, not yet a finished PebbleOS watch port
on the Fruit Jam.

Remaining work before it is useful on hardware:

- Do not treat `build/src/fw/tintin_fw_with_resources.uf2` as a finished real
  port yet; it is packaged as a valid RP2350 UF2 with system resources and
  reaches Pebble UI on hardware, but it still boots with several stubbed
  hardware drivers.
- Standalone LCD, early PebbleOS LCD output, launcher handoff, USB CDC debug,
  frame capture, BOOTSEL recovery, and BLE advertising are hardware-verified.
  Longer UI sessions, pairing, and end-to-end phone services still need broader
  testing.
- The combined UF2 programming path has been USB/picotool-verified through the
  Bluetooth advertising build. Picotool verified both firmware and resource
  ranges and rebooted the board into the application.
- The button driver is present, but has not been tested under PebbleOS on
  hardware yet. It polls GPIOs, debounces in a FreeRTOS task, maps Up+Select to
  Pebble Down with a short chord grace window, and includes the all-buttons
  BOOTSEL escape. The CDC `buttons` command now exposes enough raw/debounced/
  emitted state to verify the mapping without relying only on visible UI
  movement. Use
  `tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 buttons` while
  pressing Back, Up, Select, and Up+Select to verify the physical and synthetic
  states.
- Hardware-verify the experimental SPI1 LCD path if it is needed for refresh
  speed. The stable Fruit Jam build leaves SPI1 disabled for now.
- Hardware-verify debug UART output on GPIO44 TX / GPIO45 RX at 230400 baud.
- Continue RP2350-side NimBLE work against the flashed ESP32-C6 HCI image. The
  current milestone has moved past startup into advertising; the next unknowns
  are iPhone pairing, bonding persistence, GAP/GATT service behavior, and
  Pebble protocol traffic after connection.
- The stock WiFiNINA/AirLift firmware cannot be kept for the current
  RP2350-side NimBLE host path because it speaks the high-level AirLift protocol
  over SPI, not raw Bluetooth HCI over UART. Restoring WiFiNINA later requires
  reflashing Adafruit's Fruit Jam NINA firmware.
- A passive 8 second read from `/dev/cu.debug-console` at 230400 baud after the
  restored RP2350 boot produced no output, so serial log verification is still
  unavailable unless the debug UART wiring/adapter is confirmed.
- Hardware-verify RP2350 XIP flash erase/write paths, especially the first-boot
  PFS filesystem format on blank flash.
- Bring PSRAM back later only after the flash/resource/filesystem path is
  stable. App and worker executable regions should stay in internal SRAM unless
  RP2350 execute-from-PSRAM behavior is deliberately validated.
