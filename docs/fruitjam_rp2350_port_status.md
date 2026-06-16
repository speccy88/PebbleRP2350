# Fruit Jam RP2350 Port Status

Date: 2026-06-15

## Current Snapshot

This is a preview hardware bring-up port for the Adafruit Fruit Jam RP2350. It
is useful for flashing and continuing development on real hardware, but it is
not a finished watch port yet.

Latest packaged Fruit Jam RP2350 UF2:

- Path after configuring `fruitjam_rp2350`:
  `build/src/fw/tintin_fw_with_resources.uf2`. Rebuild/repack Fruit Jam before
  reusing this path if the last local build selected another board profile.
- Release tag: previous public preview
  `v9.9.9-fruitjam-rp2350-preview-20260614`; the current local rebuild is not
  a new release tag.
- Size: 3.0 MiB / 3,073,536 bytes
- Contents: RP2350 PebbleOS firmware plus system resources packed at
  `0x10620000`
- Storage caveat: the current Fruit Jam profile explicitly selects
  `CONFIG_RP2350_PEBBLE_STORAGE_ONBOARD_XIP=y`, so firmware, system resources,
  and the Pebble filesystem all live on the RP2350 boot/XIP flash. Erasing or
  programming filesystem/resource sectors can stall code fetch and interrupts
  while the RP2350 flash ROM operation runs. This is a likely contributor to
  unreliable phone-driven app/watchface installs; the intended next hardware
  storage profile is a separate 3.3 V NOR flash for resources and PFS, not PSRAM.
  The current local mitigation throttles PutBytes and slices full-sector erases
  into 4 KiB subsector erases while this onboard-XIP fallback is in use.
- SHA-256: published with the GitHub release asset. The firmware contains
  git-version metadata, so the final release checksum is generated from the
  tagged commit rather than hard-coded here.
- Latest local packaged and hardware-flashed SHA-256, rebuilt after the
  PutBytes throttle and XIP sliced-erase mitigation:
  `0540465d4d484195594ed4b07ad1ea704db212d72d3409170d154d6a3f2f57c6`
- Previous local packaged SHA-256, rebuilt after the RP2350 storage-profile
  marker and XIP hazard diagnostics update:
  `2ac9b1ba8d6625cbba32ea79056859f1a24579db50d854066e1b8a8ff9ed2cca`
- Previous local packaged and hardware-flashed SHA-256, rebuilt after the RTC
  profile refactor and AppFetch diagnostics update, then verified on hardware:
  `9791949e62adf84127b8377c15d7bcbd23c5154cb66f8b6adc18f48ca2cd331d`
- Previous Fruit Jam BLE auto-recovery SHA-256:
  `45a03412b9319cc8a8626204cd4aa48b23d659a922d03311d5d95af284551e46`
- Previous visible reconnect advertising / CDC `btdisc` recovery helper
  SHA-256:
  `a9dcd055b8b3b106b73fc581a7944c0d87655ddd5f0ddab154f4d740c908c072`
- Previous shared `clk_peri` cleanup and Fruit Jam/Pico 2 W profile validation
  SHA-256:
  `df3c445c2607eca02101ffe271a3b909e463061802e446e52bf4d7ec03f8530e`
- Previous local packaged SHA-256:
  `6c62b22767fb78ff0c3c43beb18ffe9956c965ee632f31d360df27586c1574f7`
- Previous hardware-flashed SHA-256:
  `60af6ef0e85c79e6b1991247d15e0f888d2cfb3134188c8bc2e27750b8e8f4ca`
- Previous app-fetch/storage diagnostics SHA-256:
  `a2a0353411fa20529fa561ea721d4b13f5ddd90bb6771b416f4b3401c2c0020a`
- Previous RP2350 timer/timezone build SHA-256:
  `4df1c838fea78adcf134586e0e9e0bb54ff0b548674ffd70b477c4e4b1de57ff`
- Previous DS1307 `rtcset` build SHA-256:
  `c6b8a871f57809ea7c61a2f03b4cc37c918427762013d448d6bbaff1e7214a26`
- Previous RTC scan build SHA-256:
  `bbbc75bbef16111bef96658df451ff3c94421a26148bfdd2c9f78e91362ddfb8`
- Previous Bluetooth GATT-discovery retry SHA-256:
  `6294b4cd44358f947de85d699e45909b1526a2edb7653d79d4069c13c886553b`
- Previous Bluetooth pairing/discovery debug SHA-256:
  `a6b205060a2d997b4bb3addaa38a013545af24a531be6f914df6349593869d09`
- Previous GPIO-scan experimental hardware-flashed SHA-256:
  `a28cf17c24ac15d6438739c2c7cfdf17f110f2b617e31002e7909abde6998f9e`
- Last stable watchface-verified SHA-256 before the GPIO scan experiment:
  `3a3a519ae8759fec0532c24962548cab2d0ba403c69f40f5706cf9c854d7ce53`

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
- The latest hardware-flashed build bounds CDC debug write stalls and services TinyUSB
  during `gpiowatch`, so a stopped host reader should not permanently trap the
  USB debug task.
- The latest hardware-flashed build makes normal RP2350 reconnect advertising
  visible, not empty, so Mac/iOS scans can see the watch outside a temporary
  `btpair` discovery window. Hardware CDC evidence showed `ad_len=25`,
  `scan_len=0`, and `name="Pebble RP2350"`; a Mac scan found one device named
  `Pebble RP2350` advertising service
  `0000fed9-0000-1000-8000-00805f9b34fb`.
- The latest hardware-flashed build also adds the CDC `btdisc` command for
  force-disconnecting the active BLE link. It was verified by holding a Mac BLE
  connection open, running `btdisc`, seeing `btdisc rc=0`, then confirming
  advertising returned and the Mac could scan the watch again.
- The latest hardware-flashed build adds a Fruit Jam-only retry for transient
  NimBLE internal GATT discovery failures and a CDC `btretry` command that
  manually restarts discovery on the active gateway connection.
- The latest hardware-flashed build also records and recovers from Fruit
  Jam-specific BLE HCI/advertising failure states that previously required a
  manual board reset. Host reset reason `BLE_HS_ETIMEOUT_HCI`, host-start sync
  timeout/error, advertising address failure, and advertising start failure now
  schedule a normal system reset. The CDC `bt` command reports this as
  `bt recovery pending=<0|1> count=<n> fail=<n> reason=<n> rc=<n>`. Clean
  post-flash hardware evidence showed `pending=0 count=0 fail=0 reason=0 rc=0`.
- The runtime display driver uses the RP2350 hardware SPI transport by default.
  On the connected board, the CDC `lcd` command reported `hw_spi=1`, `spi=1`,
  and a recent full-frame transfer time of about 15.8 ms for 168 rows. The early
  boot progress frame still uses the conservative GPIO path before
  `display_init()`.
- RP2350 `clk_peri` setup is centralized in `hardware/clocks.h` and uses the
  12 MHz crystal path for the generic RP2350 UART driver, Fruit Jam ESP32-C6
  HCI UART, and Memory LCD SPI transport. The LCD remains configured for a
  Sharp-spec 1 MHz SPI clock; the CDC `lcd` line now includes a decoded `hz=`
  field so future hardware captures can confirm the live rate instead of
  inferring it from raw clock/SPI registers.
- The CDC shell can be queried without an interactive terminal, for example:
  `tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 buttons`.
- The RP2350 time path can seed from a DS1307/DS3231-compatible external RTC at
  I2C address `0x68` and writes phone/manual time updates back to it. Fruit Jam
  uses GPIO20/GPIO21 for SDA/SCL; Pico 2 W uses GP4/GP5. The RTC type, address,
  and pins live in each board profile's `BOARD_CONFIG_EXTERNAL_RTC`, so future
  boards can choose a different RTC without changing the shared driver. If no
  valid external RTC time or no timezone is available, the clock service polls
  the phone for time every 10 seconds while automatic time is enabled. Hardware
  CDC evidence with the breadboarded DS1307 showed `i2cscan count=2 addrs=18,68`;
  `0x68` is the DS1307, while `0x18` is the Fruit Jam's onboard I2C audio DAC.
- Bare DS1307 wiring for Fruit Jam: pin 8 VCC to 5V, pin 4 GND to GND, pin 5 SDA
  to SDA/GPIO20, pin 6 SCL to SCL/GPIO21, pin 3 VBAT to a 3V coin cell positive
  terminal or GND if backup is not used, pins 1/2 to a 32.768 kHz watch crystal,
  and pin 7 SQW/OUT left unconnected. SDA/SCL pull-ups must go to Fruit Jam 3.3V,
  not 5V. Add a 0.1 uF decoupling capacitor close to VCC/GND.
- The CDC `rtc` command reports UTC time, local time, raw RP2350 timer state,
  Timer0 tick-generator state, timezone state, external RTC presence/validity,
  external RTC read/write counters, and phone time-poll counters. The CDC
  `rtcsync` command manually sends the phone time request and then prints the
  same RTC snapshot. The CDC `rtcset <unix_utc_seconds>` command sets PebbleOS
  UTC time through the normal clock service, which writes the DS1307 through the
  RP2350 RTC backend. The CDC `tzset <Olson/Region>` command is a bring-up
  helper for setting local timezone manually; `tzset America/Toronto` has been
  hardware-verified.
- The CDC `buttons` command has been hardware-verified on the button-diagnostic
  build. With no physical buttons pressed it reported raw, debounced, emitted,
  and live states all idle (`0000`) while Bluetooth advertising remained active.
- The current hardware-flashed build has verified physical button transitions:
  GPIO0 is Back, GPIO4 is Up, GPIO5 is Select, and GPIO4+GPIO5 emits synthetic
  Down. A 30 second `gpiowatch` captured active-low transitions on GPIO0/4/5,
  and a 12 second `buttonwatch` captured emitted `select`, `back`, `down`, and
  `up` Pebble button events.
- Bluetooth reaches NimBLE host start, advertising, iOS pairing, encrypted
  reconnect, GATT discovery, PPOGATT session open, and bidirectional Pebble
  Protocol traffic through the Fruit Jam ESP32-C6 controller-only HCI UART
  firmware.
- USB CDC app sideload can install, reinstall, cache, and launch a local
  Flint/Pebble 2 Duo-compatible watchface PBW. The current smoke test installed
  `/tmp/fruitjam_watchface_smoke/build/fruitjam_watchface_smoke.pbw` as
  `Fruit Jam Now` id `3`; `appcheck 3` reported `appdb=1 cache=1 storage=1
  info=0 resources=1`, `appcur` reported `appcur 3 name="Fruit Jam Now" ...
  watchface=1`, and the CDC frame capture at `/tmp/fruitjam_watchface_now.png`
  showed the watchface rendering on the LCD.
- The app sideload path now sends the PBW UUID from `appinfo.json`, reuses the
  existing install id for UUID reinstalls, and skips `app_db_insert()` on
  same-UUID reinstalls so the normal `APP_UPGRADED` callback does not delete
  the newly written app binary. Fruit Jam and Pico 2 W also use the smaller
  Asterix/Obelix app-cache free-space reserve; the generic 4 MiB reserve was
  too large for the current RP2350 PFS layout and caused immediate cache
  eviction.
- The latest hardware-flashed build keeps the CDC `appfetch` and `storage`
  diagnostics for phone-driven watchface/app installs. The `appfetch` line now
  includes the requested app UUID, too-short response count, invalid response
  count, last response length, and whether the phone sent `START`. Clean post-flash
  hardware evidence showed `storage pfs_active=1 pfs_size=5308416
  pfs_free=4031128`, `appfetch ... uuid={00000000-0000-0000-0000-000000000000}
  req=0 short=0 invalid=0 len=0 started=0 ... timeout=0`, and `putbytes ...
  storefail=0 crcfail=0 prepfail=0`.
- The CDC `storage` line reports `storage=onboard-xip xip_hazard=1` on the
  current Fruit Jam image. The same line exposes flash write/erase counts and
  RP2350 XIP blocking time so app-install tests can distinguish protocol errors
  from filesystem/resource flash stalls.
- The current local hardware-flashed build reduces onboard-XIP flash blackout
  time by disabling PutBytes pre-ACK, allowing only one queued PutBytes buffer,
  using middle BLE responsiveness during PutBytes transfers, and splitting
  logical 64 KiB erase requests into 4 KiB subsector erases while
  `CONFIG_RP2350_PEBBLE_STORAGE_XIP_ERASE_HAZARD=y`.
- Post-flash hardware evidence after USB sideload plus `rtcset` showed the
  intended erase shape: `erase=0/17/17/0`, `xip=220/203/17`, and `block_us`
  max around 48 ms, meaning the onboard-XIP profile used 17 subsector erases
  and no full 64 KiB sector erases in that run.
- The latest hardware-flashed build also adds CDC `appcheck <install_id>` for
  app-storage debugging. It reports app database presence, app type, cache
  presence, app binary header status, resource-bank validity, and matching DB
  versus file UUID/name.
- A macOS BLE probe helper now lives at `tools/macos_ble_probe.py`. It
  hardware-verified scanning by name, connecting, GATT service enumeration,
  Pebble pairing-status reads, trigger-pairing writes, and held-link recovery
  tests from the Mac Bluetooth controller.
- The CDC `bootsel` command now uses the RP2350 boot ROM path and has been
  tested to mount `/Volumes/RP2350`; 1200-baud CDC reset-to-BOOTSEL uses the
  same helper.
- The Fruit Jam firmware includes a boot-loop guard. After repeated unsafe
  resets before the launcher reaches ready state, it clears the fault state and
  enters BOOTSEL instead of staying trapped in a reset loop.

USB CDC debug commands:

```text
help ping progress lcd lcdtest rtc rtcsync rtcset tzset i2cscan tasks gpio gpiowatch buttons buttonmap buttonwatch button esp bt reason frame clearfault reset btdisc btretry appfetch storage btforget btpair bootsel esppass apps appcur appcheck applaunch watch watchdefault appsideload
```

`watch` launches the saved default watchface through the launcher task.
`watchdefault <install_id>` validates that the install id is a watchface, saves
it as the default, and launches it. On the current hardware image,
`watchdefault -69` selects the built-in `TicToc` watchface. `watchdefault 3`
selects the locally sideloaded `Fruit Jam Now` watchface, which is useful as a
PBW install smoke test but has shown UTC-based time while the firmware clock
service and built-in watchfaces are on local time.

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
- The CDC `button` command injects Pebble button events for remote UI tests,
  for example `tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501
  button down`.
- Latest clean post-flash hardware evidence: HCI startup completed with no H4
  parse errors, `bt driver enter=1 done=1 ok=1`, advertising was active as
  `Pebble RP2350`, the new recovery counters were idle, and a Mac scan found
  one device advertising service `0000fed9-0000-1000-8000-00805f9b34fb`.
  Earlier iPhone pairing evidence reached `pps ... bond=1 enc=1 gw=1`, GATT
  discovery found 11 services, PPOGATT reported `open=1`, and Pebble Protocol
  counters showed bidirectional traffic with no receive-router errors.
- Current pairing-retry note: after clearing the Fruit Jam-side pairing store,
  iOS/Core Device still retained stale `Pebble A35A` state and repeatedly
  connected then disconnected before encryption or PPS/GATT traffic. Board-side
  evidence showed `connect=5`, `disconnect=5`, `pair=0`, `encd=0`, and HCI
  disconnect reason `0x0213` (remote user terminated). Remove/forget the phone
  entry and re-add the watch before retesting the new GATT discovery retry.
- Mac BLE smoke-test workflow: `.venv/bin/python tools/macos_ble_probe.py scan
  --timeout 8 --name 'Pebble RP2350'`, then `.venv/bin/python
  tools/macos_ble_probe.py connect --name 'Pebble RP2350'
  --read-pairing-status`. The helper's `scan --service` option now defaults to
  no service filter, while `connect` still auto-selects the Pebble pairing
  service unless overridden. The current hardware-flashed build enumerated DIS,
  Pebble pairing service `0000fed9-0000-1000-8000-00805f9b34fb`, and Battery
  service. Pairing status decoded as `connected=1 bonded=0 encrypted=0
  gateway=1 pinning=1 reversed=0`.
- Mac BLE pairing-trigger probes can exercise more of the Fruit Jam pairing
  service but do not yet replace the iPhone/Core Devices app. A `no-sec`
  trigger write was accepted as `flags=02` but did not start discovery. A
  `force` trigger write was accepted as `flags=04` and started SMP state on the
  board, but macOS did not complete bonding or expose the phone-side PPoGATT
  server. `btdisc` cleanly recovered the held link and restored advertising.
- `tools/macos_ble_probe.py ppog-server --duration 30` now publishes a minimal
  CoreBluetooth phone-side PPoGATT service on macOS. It exposes service
  `10000000-328e-0fbb-c642-1aa6699bdada`, meta characteristic
  `10000002-328e-0fbb-c642-1aa6699bdada`, and data characteristic
  `10000001-328e-0fbb-c642-1aa6699bdada`, and replies to reset-request writes
  with a reset-complete notification. Local CoreBluetooth testing verified the
  service can be published, but Fruit Jam did not read/subscribe to it during
  Mac-triggered `no-sec` or `force` pairing probes because the Mac link still
  disconnects before watch-side service discovery.
- Mac BLE held-link recovery workflow: `.venv/bin/python -u
  tools/macos_ble_probe.py connect --name 'Pebble RP2350' --no-enumerate
  --hold-seconds 30 --timeout 15`, then
  `tools/fruitjam_cdc_command.py -p /dev/cu.usbmodemFJRP23501 btdisc`.
  Hardware evidence after `btdisc` showed reconnect advertising returned and
  the disconnect reason was `534` (`0x0216`, local host termination).
- Known Mac BLE edge case: a raw address-only CoreBluetooth hold attempt once
  caused a controller/host reset loop with `reset_reason=19`, `addr_rc=21`, and
  advertising start failures. A board reset recovered it. The current build now
  schedules a normal system reset for those Fruit Jam HCI/advertising failure
  states; keep watching the `bt recovery` line during future Mac BLE stress
  tests to confirm the automatic path fires when needed.
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

Last successful Fruit Jam local build, rebuilt after the PutBytes throttle and
XIP sliced-erase mitigation, then hardware-flashed:

- Firmware: `build/src/fw/tintin_fw.elf`
- Binary: `build/src/fw/tintin_fw.bin`
- Firmware-only UF2: `build/src/fw/tintin_fw.uf2`
- Firmware + system resources UF2:
  `build/src/fw/tintin_fw_with_resources.uf2`
- Resources: `build/system_resources.pbpack`
- Firmware size: 797359 / 16777216 bytes
- Resource size: 741474 / 2097152 bytes
- Firmware + resources UF2 SHA-256:
  `0540465d4d484195594ed4b07ad1ea704db212d72d3409170d154d6a3f2f57c6`

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

This combined XIP-flash layout is acceptable for early Fruit Jam bring-up, but
it is not the final storage design. The RP2350 executes firmware directly from
onboard flash, so erasing or rewriting filesystem/resource sectors in that same
flash can stall instruction fetch until the flash operation completes. The
current local build slices 64 KiB erase requests into 4 KiB erase commands to
make each stall shorter, but a release-quality Fruit Jam/Pico 2 W profile
should still move Pebble filesystem and resource storage to a separate
voltage-compatible QSPI/SPI NOR flash, matching the way Asterix keeps
resources/filesystem off the firmware XIP path.

Latest local validation:

- 2026-06-15 Fruit Jam rebuild after the RTC Kconfig/profile refactor and
  AppFetch phone-install diagnostics:
  `./pbl configure --board fruitjam_rp2350`, then `./pbl build`; firmware
  794659 / 16777216 bytes, resources 741474 / 2097152 bytes. Picotool UF2
  conversion plus `tools/fruitjam_pack_uf2.py` produced
  `build/src/fw/tintin_fw_with_resources.uf2` with SHA-256
  `9791949e62adf84127b8377c15d7bcbd23c5154cb66f8b6adc18f48ca2cd331d`.
- 2026-06-15 hardware flash of that build to Fruit Jam RP2350 through the CDC
  `bootsel` command and `/Volumes/RP2350`: after reboot, CDC `progress`
  reached `stage=17 label="17 BT OK"`, CDC `rtc` reported
  `ext_present=1 ext_valid=1 ext_err=0`, CDC `bt` reported
  `adv active=1` as `Pebble RP2350` with recovery counters idle, CDC `storage`
  reported `pfs_active=1 pfs_size=5308416 pfs_free=4031128`, and CDC
  `appfetch` reported the new fields with `short=0 invalid=0 len=0
  started=0`. `tzset America/Toronto` reported `tz=1` and local
  `2026-06-15T11:35:40`; selecting `watchdefault -69` made built-in `TicToc`
  render local time on the LCD. Frame captures:
  `/tmp/fruitjam-postflash-frame.png`, `/tmp/fruitjam-localtime-frame.png`,
  and `/tmp/fruitjam-tictoc-frame.png`.
- 2026-06-15 Pico 2 W placeholder profile rebuild after the same RTC/AppFetch
  changes:
  `./pbl configure --board pico2_w_rp2350`, then `./pbl build`; firmware
  714539 / 16777216 bytes, resources 741474 / 2097152 bytes.
- 2026-06-15 Core Devices mobile app AppFetch wire-format regression:
  `/Users/fred/Documents/Code/coredevices-mobileapp` carries a local patch
  changing `AppFetchResponseStatus.NO_DATA` from `0x01` to `0x04`, plus
  `AppFetchPacketTest`. The focused JVM regression passed freshly with
  `./gradlew :libpebble3:jvmTest --tests AppFetchPacketTest --rerun-tasks`.
- 2026-06-15 Fruit Jam rebuild after the BLE auto-recovery pass:
  `./pbl configure --board fruitjam_rp2350`, then `./pbl build`; firmware
  794499 / 16777216 bytes, resources 741474 / 2097152 bytes.
- 2026-06-15 hardware flash of the BLE auto-recovery build to Fruit Jam RP2350:
  the UF2 was copied to the BOOTSEL volume as `FW.UF2`. After reboot,
  `progress` reported `stage=17 label="17 BT OK"`, CDC `bt` reported
  `bt recovery pending=0 count=0 fail=0 reason=0 rc=0`, CDC `storage`
  reported `pfs_active=1 pfs_free=4031128`, a Mac BLE scan found
  `Pebble RP2350` with pairing service `fed9`, and a CDC frame capture showed
  the Fruit Jam watchface on the LCD.
- 2026-06-15 Mac PPoGATT server helper test:
  `.venv/bin/python tools/macos_ble_probe.py ppog-server --duration 2`
  published the phone-side PPoGATT service and added it through CoreBluetooth.
  Longer runs combined with address-directed Fruit Jam connects and
  `--trigger-pairing no-sec` / `--trigger-pairing force` produced no Mac-side
  meta reads, data subscriptions, writes, or reset-complete notifications; CDC
  `bt` stayed at `disc req=0` and `ppogatt=0`, so this is a published-service
  helper only until macOS pairing/service-discovery behavior is solved.
- 2026-06-15 Pico 2 W placeholder profile build after the BLE auto-recovery
  pass:
  `./pbl configure --board pico2_w_rp2350`, then `./pbl build`; firmware
  714347 / 16777216 bytes, resources 741474 / 2097152 bytes.
- 2026-06-15 Fruit Jam rebuild after visible RP2350 reconnect advertising and
  CDC `btdisc` recovery:
  `./pbl configure --board fruitjam_rp2350`, then `./pbl build`; firmware
  794243 / 16777216 bytes, resources 741474 / 2097152 bytes.
- 2026-06-15 Pico 2 W placeholder profile build after the visible RP2350
  reconnect-advertising change:
  `./pbl configure --board pico2_w_rp2350`, then `./pbl build`; firmware
  714219 / 16777216 bytes, resources 741474 / 2097152 bytes.
- 2026-06-15 Pico 2 W NimBLE/CYW43-stub profile build:
  `./pbl configure --board pico2_w_rp2350 -DCONFIG_BT_FW_NIMBLE=y -DCONFIG_BT_FW_STUB=n`,
  then `./pbl build`; firmware 786755 / 16777216 bytes, resources 741474 /
  2097152 bytes.
- 2026-06-15 Fruit Jam UF2 packaging after the visible reconnect/btdisc build:
  Picotool UF2 conversion plus `tools/fruitjam_pack_uf2.py`; packaged UF2
  SHA-256
  `a9dcd055b8b3b106b73fc581a7944c0d87655ddd5f0ddab154f4d740c908c072`.
- 2026-06-15 hardware flash of the visible reconnect/btdisc build to Fruit Jam
  RP2350: the UF2 was copied to the BOOTSEL volume as `FW.UF2`. After reboot,
  Mac BLE scan found `Pebble RP2350` with pairing service `fed9`; CDC `bt`
  showed the visible reconnect payload (`ad_len=25`, `scan_len=0`,
  `name="Pebble RP2350"`). A held Mac BLE connection was force-disconnected
  with CDC `btdisc`, which returned `btdisc rc=0`; advertising came back and a
  follow-up Mac scan found the watch again.
- 2026-06-15 hardware flash of the shared `clk_peri` build to Fruit Jam RP2350:
  the UF2 was copied to the BOOTSEL volume as `FW.UF2` after macOS denied the
  long filename on the FAT volume. After reboot, CDC `ping` returned `pong`,
  `progress` reported `stage=17 label="17 BT OK"`, and `lcd` reported
  `hw_spi=1 spi=1 cr0=00000207 cpsr=4 hz=1000000 peri_ctrl=10000880
  peri_div=00010000 rows=168 last_us=37489`. A post-flash frame capture was
  saved at `/tmp/fruitjam_after_clkperi_flash.png`.
- 2026-06-15 Pico 2 W SDK-style RP2350 clock build and hardware flash:
  `./pbl configure --board pico2_w_rp2350`, then `./pbl build`; firmware
  716791 / 16777216 bytes, resources 741474 / 2097152 bytes. Picotool UF2
  conversion plus `tools/fruitjam_pack_uf2.py` produced
  `build/artifacts/pico2_w_rp2350_pebbleos_sdk_clocks.uf2` with SHA-256
  `7741f4a021dc7137138ac77873b1b9285726654a4090fb79660f19060558687b`.
  UF2 address validation showed family `0xe48bff59` range
  `0x10000000-0x10205100` plus RP2350-E10 absolute block family `0xe48bff57`
  range `0x103fff00-0x10400000`.
- 2026-06-15 hardware flash of that Pico 2 W build to BOOTSEL serial
  `B2047FF29EB03C7E`: Picotool 2.1.1 verified flash (`OK`) and rebooted into
  the app. The board enumerated as `Pico 2 W PebbleOS Debug` with serial
  `PICO2WRP2350` on `/dev/cu.usbmodemPICO2WRP23501`; CDC `ping` returned
  `pong`, `reason` returned `reason code=00`, `progress` reported
  `stage=7 label="07 LCD"`, `lcd` reported `hw_spi=1 spi=0 ... hz=1000000 ...
  rows=168`, and `tasks` included `USBDebug`, `SharpVCOM`, `PULSE`, and
  `App <TicToc>`. CDC frame capture `/tmp/pico2_w_after_sdk_clocks.png`
  showed the TicToc framebuffer.
- 2026-06-15 Fruit Jam rebuild after the shared SDK-style RP2350 clock helper:
  `./pbl configure --board fruitjam_rp2350`, then `./pbl build`; firmware
  797551 / 16777216 bytes, resources 741474 / 2097152 bytes. This was a build
  regression check only; the latest Fruit Jam hardware-flashed UF2 remains the
  XIP sliced-erase build listed above until repackaged and flashed again.
- 2026-06-15 Core Devices mobile app protocol check:
  `/Users/fred/Documents/Code/coredevices-mobileapp` started from upstream
  `e4180ffc` and currently has a local AppFetch enum patch plus regression test.
  Android
  command-line tools, Android API 36, platform-tools, build-tools 36.0.0, and
  NDK 27.0.12077973 are installed under
  `/opt/homebrew/share/android-commandlinetools`. The local `libpebble3`
  metadata compile passed with `./gradlew :libpebble3:compileKotlinMetadata`,
  and the focused wire-format regression passed with
  `./gradlew :libpebble3:jvmTest --tests AppFetchPacketTest --rerun-tasks`.
- `git diff --check`
- `python -m py_compile tools/macos_ble_probe.py tools/fruitjam_cdc_command.py
  tools/fruitjam_cdc_frame.py tools/fruitjam_sideload_pbw.py`
- `./pbl configure --board fruitjam_rp2350`
- `./pbl build`
- `./pbl configure --board pico2_w_rp2350`
- `./pbl build`
- `./pbl configure --board pico2_w_rp2350 -DCONFIG_BT_FW_NIMBLE=y -DCONFIG_BT_FW_STUB=n`
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
- CDC post-flash diagnostics with `storage`, `appfetch`, `progress`, and Mac
  BLE scan/connect/pairing-status read using `tools/macos_ble_probe.py`
- CDC frame capture:
  `tools/fruitjam_cdc_frame.py -p /dev/cu.usbmodemFJRP23501 -o /tmp/fruitjam_ble_advertising.png`
- CDC frame capture after PPOGATT opened:
  `tools/fruitjam_cdc_frame.py -p /dev/cu.usbmodemFJRP23501 -o /tmp/fruitjam_ppogatt_open.png`
- Hardware flash of the bounded-CDC-write build to Fruit Jam RP2350 through
  `/Volumes/RP2350` on 2026-06-15. The flashed UF2 hash is
  `bb663d4737a1c4db8a51a08a0158059e59a1a35c34bcdd1fe70684ba258d68c4`.
  After flash, `ping` returned `pong`, `progress` reported
  `stage=17 label="17 BT OK"`, and `lcd` reported `hw_spi=1`, `spi=1`,
  `rows=168`, `last_us=15830`. Frame captures
  `/tmp/fruitjam_after_bounded_cdc_flash.png` and
  `/tmp/fruitjam_after_buttonwatch.png` showed the built-in `TicToc` watchface
  running on the Waveshare LCD. `gpiowatch 30000` captured GPIO0, GPIO4, GPIO5,
  and GPIO4+GPIO5 chord transitions; `buttonwatch 12000` captured emitted
  `select`, `back`, `down`, and `up` events and completed with
  `buttonwatch done changes=49 lines=73`.
- Hardware flash of the `btpair` discovery-advertising fix to Fruit Jam RP2350
  through `/Volumes/RP2350` on 2026-06-15. The flashed UF2 hash is
  `a6b205060a2d997b4bb3addaa38a013545af24a531be6f914df6349593869d09`.
  After flash, `progress` again reached `stage=17 label="17 BT OK"`, `lcd`
  reported `hw_spi=1`, `spi=1`, `rows=168`, `last_us=15959`, and
  `/tmp/fruitjam_after_btpair_adv_fix.png` showed the Settings app. Running
  CDC `btpair` no longer dropped the board into BOOTSEL; after a 4 second wait,
  `bt` reported `adv active=1`, `interval=20-20`, `ad_len=23`, `scan_len=23`,
  and `name="Pebble A35A"`.
- Hardware flash of the RTC-debug build to Fruit Jam RP2350 via `/Volumes/RP2350`
- CDC `rtc` evidence after the RTC-debug build:
  `ext_present=0 ext_valid=0 ext_err=-3 ext_read=0/2 ext_write=0/1
  phone_poll=1/10`, followed one poll interval later by
  `phone_tick=1 phone_no_session=1` with no phone session connected.
- CDC `rtcsync` evidence with no phone session connected:
  `phone_req=0 phone_no_session=3`.
- CDC `lcd` evidence after the same flash:
  `hw_spi=1 spi=1 rows=168 last_us=15780 max_us=135081`.
- CDC frame captures after the same flash:
  `/tmp/fruitjam_after_rtc_debug_flash.png` for the Settings list and
  `/tmp/fruitjam_after_flash_tictoc.png` for the built-in TicToc watchface.
- Hardware flash of the DS1307 scan/`rtcset` build to Fruit Jam RP2350 through
  `/Volumes/RP2350` on 2026-06-15. The flashed UF2 hash is
  `c6b8a871f57809ea7c61a2f03b4cc37c918427762013d448d6bbaff1e7214a26`.
  CDC `i2cscan` reported `count=2 addrs=18,68`, confirming the DS1307 at
  `0x68` on GPIO20/GPIO21 with another onboard Fruit Jam I2C device at `0x18`.
  `rtcset 1781521499` returned
  `utc=2026-06-15T11:04:59Z ext_write=ok err=0`. After a reboot, CDC `rtc`
  showed `utc=2026-06-15T11:07:05Z ext_present=1 ext_valid=1 ext_err=0
  ext_read=2/0 ext_write=0/0`, proving that boot-time RP2350 time was seeded
  from the DS1307 rather than falling back to the 2010 minimum timestamp.
- Hardware flash of the RP2350 timer/timezone build to Fruit Jam RP2350 through
  `/Volumes/RP2350` on 2026-06-15. The flashed UF2 hash is
  `4df1c838fea78adcf134586e0e9e0bb54ff0b548674ffd70b477c4e4b1de57ff`.
  This build initializes the RP2350 TICKS block before PebbleOS starts using
  `time_us_32()`. Before that fix, hardware samples showed the software RTC
  advancing at roughly half speed. After the flash, CDC `rtc` reported
  `tick0=00000003/12/...` and `raw_us` advanced by about 9.9 seconds during a
  10 second wall-clock sample. The DS1307 read as valid on boot and UTC was
  within a couple of seconds of the Mac clock. `tzset America/Toronto` returned
  `tzset ok id=127 region=America/Toronto`, then `rtc` reported
  `local=2026-06-15T07:45:22` with `tz=1`. CDC frame captures
  `/tmp/fruitjam_after_tzset_watch.png`,
  `/tmp/fruitjam_after_tzset_next_minute.png`, and
  `/tmp/fruitjam_watch_after_watch_cmd.png` showed the watchface rendering local
  Montreal time. A later capture at `/tmp/fruitjam_watch_next_minute_check.png`
  was a Launcher/Pebble Health screen because the physical watch buttons were
  being used during the sample, not because the RTC stopped.
- Hardware flash of the buttonwatch-heartbeat build to Fruit Jam RP2350 through
  `/Volumes/RP2350`. The flashed UF2 hash is
  `1da5817e050be692fc5f48281f2613ca1121448bb4da8e5ea9fe929c96badac9`.
- CDC `buttonmap` evidence after the buttonwatch flash:
  `back pin=0`, `up pin=4`, `select pin=5`, and synthetic Down from
  `up+select` with a 90 ms chord grace window.
- CDC `buttonwatch 1200` evidence after the same flash produced heartbeat
  lines at `t=0`, `t=500`, and `t=1000`, then completed with
  `buttonwatch done changes=1 lines=3`.
- CDC frame capture after the same flash:
  `/tmp/fruitjam_current_frame.png`, showing the built-in TicToc watchface.
- A 30 second CDC `buttonwatch` run sent after a phone notification completed
  with 61 heartbeat lines and `buttonwatch done changes=1 lines=61`; no
  physical button transitions were observed during that capture.
- A PFS cleanup wipe was performed with a temporary UF2 targeting only the
  filesystem range `0x10AA0000-0x10FBFFFF`, then the full firmware+resources
  UF2 was flashed again. The board booted past PFS to `17 BT OK` afterward.
- Built-in Flint watchface verification after the PFS wipe:
  `watchdefault -69` set `TicToc`, `appcur` reported
  `appcur -69 name="TicToc" ... watchface=1`, and CDC frame captures
  `/tmp/fruitjam_tictoc_default_working.png` and
  `/tmp/fruitjam_tictoc_after_reboot_watch.png` showed `TicToc` rendered on the
  Waveshare LCD. After reset, the normal shell starts in Launcher, but `watch`
  resolves to the persisted `TicToc` default.
- The first experimental `gpiowatch` build was flashed as
  `a28cf17c24ac15d6438739c2c7cfdf17f110f2b617e31002e7909abde6998f9e`.
  It added `gpio`/`gpiowatch` and observed GPIO10 toggling, consistent with LCD
  VCOM. The host tool timed out too early, a later overlapping command left the
  USB CDC path wedged, and the corrected bounded-CDC build is
  `bb663d4737a1c4db8a51a08a0158059e59a1a35c34bcdd1fe70684ba258d68c4`.
- Earlier local Flint watchface PBW build and sideload:
  `/tmp/fruitjam_watchface_smoke/build/fruitjam_watchface_smoke.pbw` was built
  with `build/sdk/waf`, sideloaded with `tools/fruitjam_sideload_pbw.py`, and
  installed as id `3`, name `Fruit Jam Now`. `appcur` reported
  `watchface=1`, and the frame capture `/tmp/fruitjam_now_watchface.png`
  showed the face running on the Waveshare LCD.

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
- The early smoke test and RP2350 display driver now share the same LCD helper,
  so bring-up no longer has two copies of the Sharp LCD wire protocol.
- The default runtime display path uses hardware SPI through the board-provided
  `Rp2350MemoryLcdConfig`. The GPIO transport remains available by disabling
  `CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_HARDWARE_SPI` for diagnostics.
- `CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_SPI_DIAGNOSTIC_PATTERN` can be
  enabled to show a vertical/X-pattern marker after display init for hardware
  SPI transport diagnostics.
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
- The latest local build after adding the CDC `buttons` command and
  `tools/fruitjam_cdc_command.py` was flashed with USB `picotool load -v -x` on
  2026-06-14. Picotool verified the programmed flash ranges and rebooted the
  board into PebbleOS. The debug CDC port came back as
  `/dev/cu.usbmodemFJRP23501`; `help` listed `buttons`; `progress` reported
  `17 BT OK`; `buttons` reported
  `raw=0000 debounced=0000 emitted=0000 live=0000`; `bt` still reported
  `adv active=1` and local name `Pebble A35A`. The LCD frame capture at
  `/tmp/fruitjam_buttons_build.png` showed the launcher/settings list. Its
  combined UF2 hash is
  `edfc28c4ccdb3b3b147fed49e03dd352bc3ce3fac4752fe6c38bbd4fe2c6bfa7`.
- The latest local build after PPOGATT/Pebble Protocol debug counters,
  `btforget` reset hardening, stale active-gateway cleanup, and the CDC
  `btpair` command was flashed with USB
  `picotool load -v -x build/src/fw/tintin_fw_with_resources.uf2` on
  2026-06-15. Picotool verified the programmed flash ranges and rebooted the
  board into PebbleOS. Its combined UF2 hash is
  `128ab911c8b9f49011d599a97f4cb86bd7219faed22d1ce5074aff978289848d`.
  After running the fixed `btforget`, the board came back with no saved bond
  flags and automatically advertised `Pebble A35A` with `ad_len=23` and
  `scan_len=23`. A CDC frame capture at `/tmp/fruitjam_fresh_adv.png` showed
  the Pebble Settings list. The iPhone then reached the board repeatedly, but
  the BLE security flow showed `st=4` (`BLE_SM_PROC_STATE_LTK_RESTORE`) and
  `app=5` (`BLE_HS_ENOENT`) before iOS disconnected with reason `531`. That
  indicates iOS was trying to restore an old saved bond whose LTK no longer
  exists on the Fruit Jam; the next phone-side step is to forget the stale
  `Pebble A35A` entry in iOS Bluetooth Settings and retry pairing through the
  Core Devices app.
- The latest local build after the Fruit Jam PPOGATT reset-collision fix and
  pairing-service bonded-status fallback was flashed with USB
  `picotool load -v -x build/src/fw/tintin_fw_with_resources.uf2` on
  2026-06-15. Picotool verified the programmed firmware and resource ranges and
  rebooted the board into PebbleOS. Its combined UF2 hash is
  `8fe3adb8230a6d60f6484bde7f7417928038803f5a9ebb3af97e44a48c91ac4c`.
  Hardware CDC evidence after reconnect showed `pps ... bond=1 enc=1 gw=1`,
  `disc ... ok=1 services=11`, `ppog ... st=6 ... rc=1 open=1`,
  `ppog2 rx=12 data=5 ack=6 tx=11 ok=11 fail=0`, and
  `pp rx=5 ... tx=6 ... errors=0 no_buf=0`. A frame capture at
  `/tmp/fruitjam_ppogatt_open.png` showed the Settings app with a live Music
  subtitle supplied by the phone.
- The latest local build after RP2350 RTC phone-poll debug counters and
  `rtcsync` was flashed through `/Volumes/RP2350` on 2026-06-15. Its combined
  UF2 hash is
  `df1cd9ec67caa5feb7abea223ad0ecd4c20ae9832809562bfb0b9c1d1ef0e2d7`.
  Hardware CDC evidence showed the external RTC absent as expected on the
  current wiring (`ext_present=0`, `ext_err=-3`), the 10-second phone time
  fallback armed (`phone_poll=1/10`), the first timer tick counted with no phone
  session (`phone_tick=1`, `phone_no_session=1`), and manual `rtcsync`
  incrementing the same no-session counter. The same flashed build reported
  `lcd hw_spi=1 ... rows=168 last_us=15780`, confirming the runtime display path
  stayed on hardware SPI. A final frame capture at `/tmp/fruitjam_after_flash_tictoc.png`
  showed the built-in TicToc watchface.
- The latest local flashed Fruit Jam build adds CDC `buttonmap` and
  heartbeat-enabled `buttonwatch` commands for reporting the board profile's
  button GPIO/pull/polarity map, debounce/chord timing, and live button-state
  changes while physical buttons are pressed. Its combined UF2 hash is
  `1da5817e050be692fc5f48281f2613ca1121448bb4da8e5ea9fe929c96badac9`.

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
  `reset`/`btforget`/`btpair`/`bootsel`/`clearfault`/`tasks`/`rtc`/`rtcsync`/
  `rtcset`/`i2cscan`/`buttons`/`buttonmap`/`buttonwatch`/`button`/`esp`/`bt`/
  `esppass`, and programs `PLL_USB`/`clk_usb` with bounded waits so failed USB
  clock bring-up cannot freeze PebbleOS at the early LCD screen. `btforget`
  deletes local Bluetooth pairings, invalidates stale active gateway metadata,
  and resets after a short USB flush delay. `btpair` requests a 300 second BLE
  discoverable/pairable window for pairing diagnostics.
- LCD framebuffer capture shadowing in
  `src/fw/soc/rp2350/rp2350/fruitjam_lcd.c`. It records the last rows sent by
  the firmware to the Sharp Memory LCD; it is not an electrical readback from
  the display glass.
- RP2350 XIP flash driver at `src/fw/drivers/flash/rp2350_xip.c`. It runs
  erase/program operations from `.ramfunc`, uses the RP2350 boot ROM flash
  helpers, and sends explicit 4 KiB erase commands for Pebble subsector erases.
  While `CONFIG_RP2350_PEBBLE_STORAGE_XIP_ERASE_HAZARD=y`, higher flash-region
  and filesystem erase helpers also split full-sector requests into 4 KiB erase
  commands to reduce individual XIP stalls.
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
  event so this mapping can be verified remotely. The CDC `button` command can
  inject Back/Up/Select/Down taps or down/up events into Pebble's event queue so
  UI navigation can be tested remotely while the physical buttons are still
  awaiting hands-on verification.
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
- Board-neutral NimBLE H4 transport for RP2350 at
  `third_party/nimble/transport/hci_rp2350.c`. It calls the
  `rp2350_bluetooth_hci_*` backend interface so Fruit Jam can use ESP32-C6 H4
  UART while Pico 2 W can grow a separate CYW43439/CYW43 backend.
- RP2350 NimBLE build override in `third_party/nimble/wscript_build` disables
  `BLE_HS_AUTO_START` so the host starts only after Pebble's Bluetooth service
  has registered GAP, GATT, DIS, pairing, and BAS services.
- Fruit Jam currently expects the ESP32-C6 controller-only HCI firmware for
  PebbleOS Bluetooth. Stock WiFiNINA/AirLift firmware is a high-level
  coprocessor protocol and is not the raw HCI controller interface that the
  RP2350-side NimBLE host expects.
- Fruit Jam PPOGATT client diagnostics in
  `src/fw/comm/ble/kernel_le_client/ppogatt/ppogatt.c` record meta reads,
  subscription results, reset packets, session-open state, packet types/SNs,
  and Pebble Protocol message/error counters in the CDC `bt` snapshot. The
  Fruit Jam path also replies with `ResetComplete` when the phone sends a
  simultaneous `ResetRequest` while the watch is waiting for its own reset to
  complete, which avoids the Core Devices negotiation deadlock observed on
  hardware.
- Fruit Jam pairing-service diagnostics in
  `src/bluetooth-fw/nimble/pebble_pairing_service.c` record connection-status
  reads, trigger-pairing writes, and notification results in the CDC `bt`
  snapshot. The Fruit Jam path treats an encrypted connection with a stored
  gateway bond as bonded for the pairing-service status when NimBLE's direct
  peer-security lookup misses after reboot.
- ESP32-C6 controller-only NimBLE HCI firmware scaffold at
  `hardware_tests/fruitjam_esp32c6_hci/`. Build it with ESP-IDF v5.5.4 using
  `idf.py set-target esp32c6`, `idf.py build`, and `idf.py merge-bin`, then
  flash `build/merged-binary.bin` through Adafruit's Fruit Jam serial
  passthrough UF2.
- RP2350 debug UART driver at `src/fw/drivers/uart/rp2350.c`.
- RP2350 large-segment heap support in `src/libutil/heap.c`; the live kernel
  heap remains internal SRAM only for now.
- Fruit Jam UF2 resource packer at `tools/fruitjam_pack_uf2.py`.
- RP2350 RTC support includes watchdog scratch backup registers, an optional
  DS1307/DS3231-compatible external RTC, and a phone time-request fallback. EXTI,
  fixed battery, watchdog, and remaining hardware feature stubs are still
  bring-up quality.

The Raspberry Pi Pico 2 W target now has a first buildable board profile:

- Board profile under `boards/pico2_w_rp2350/`, selectable with
  `./pbl configure --board pico2_w_rp2350`.
- Target notes under `docs/targets/pico2_w_rp2350/`.
- Pico 2 W hardware tests under `hardware_tests/pico2_w_rp2350/`, including a
  standalone `cyw43_hci_probe` Pico SDK UF2 that exercises the CYW43439
  Bluetooth HCI path and advertises as `Pico2W-HCI`.
- Board, display, and SoC notes under
  `src/fw/board/boards/pico2_w_rp2350/`,
  `src/fw/board/displays/pico2_w_rp2350/`, and
  `src/fw/soc/rp2350/pico2_w_rp2350/`.
- Concrete board/display files under
  `src/fw/board/boards/board_pico2_w_rp2350.c`,
  `src/fw/board/boards/board_pico2_w_rp2350.h`, and
  `src/fw/board/displays/display_pico2_w_rp2350.h`.
- A Pico-specific resource map under `resources/normal/pico2_w_rp2350/` that
  currently reuses the Fruit Jam/Flint resource IDs for Pebble 2 Duo
  compatibility.

The Pico 2 W profile intentionally uses stub Bluetooth until the CYW43439/CYW43
HCI transport is implemented inside PebbleOS. The shared RP2350 NimBLE transport
already has a Pico 2 W backend slot at
`src/fw/soc/rp2350/rp2350/rp2350_bluetooth_hci_cyw43_stub.c`, and an explicit
`CONFIG_BT_FW_NIMBLE=y` / `CONFIG_BT_FW_STUB=n` Pico 2 W build now passes
against that stub. The latest normal Pico 2 W profile build reported
`FW: 714347 / 16777216 bytes used`; the latest explicit NimBLE/CYW43-stub build
reported `FW: 786755 / 16777216 bytes used`. The standalone CYW43 HCI probe is
the first radio bring-up target. A local rebuild on 2026-06-15 was already up to
date and produced
`hardware_tests/pico2_w_rp2350/cyw43_hci_probe/build/pico2_w_cyw43_hci_probe.uf2`,
683 KiB, SHA-256
`ebf7d4afe131126f51cfc44db491f8726c3fc1b14eafcc0e991c5f7c0e9392ca`, with
`PICO_BOARD=pico2_w` and `PICO_PLATFORM=rp2350-arm-s` in the CMake cache. The
expected split remains shared RP2350/Pebble display work plus a separate
CYW43439 Bluetooth backend, not Fruit Jam's ESP32-C6 H4 UART transport.

## Important Caveats

This is a hardware-booting preview port, not yet a finished PebbleOS watch port
on the Fruit Jam.

Remaining work before it is useful on hardware:

- Do not treat `build/src/fw/tintin_fw_with_resources.uf2` as a finished real
  port yet; it is packaged as a valid RP2350 UF2 with system resources and
  reaches Pebble UI on hardware, but it still boots with several stubbed
  hardware drivers.
- Standalone LCD, early PebbleOS LCD output, launcher handoff, USB CDC debug,
  frame capture, BOOTSEL recovery, BLE advertising, iOS pairing, encrypted
  reconnect, GATT discovery, PPOGATT session open, and initial bidirectional
  Pebble Protocol traffic are hardware-verified. Longer UI sessions and
  higher-level phone services still need broader testing.
- The combined UF2 programming path has been USB/picotool and BOOTSEL-copy
  verified through the visible reconnect/btdisc build. Picotool verified both
  firmware and resource ranges on earlier builds, and BOOTSEL-copy flashing
  rebooted the board into the application on the latest build.
- The Fruit Jam button driver is hardware-verified for the current wiring. GPIO0
  is Back, GPIO4 is Up, GPIO5 is Select, and GPIO4+GPIO5 emits synthetic Down.
  The driver polls GPIOs, debounces in a FreeRTOS task, maps Up+Select to Pebble
  Down with a short chord grace window, and includes the all-buttons BOOTSEL
  escape. The CDC `buttonmap` command reports the board profile's GPIO, pull,
  polarity, debounce, synthetic Down, and BOOTSEL-hold timing. The CDC
  `buttonwatch [ms]` command streams bounded physical-button tests; a 12 second
  hardware run captured emitted `select`, `back`, `down`, and `up` events. The
  CDC `button` command can still inject Back/Up/Select/Down taps remotely for UI
  testing when nobody is at the board.
- Keep collecting hardware `lcd` telemetry during longer UI sessions. The
  connected board has reported `hw_spi=1` and ~15.8 ms full-frame transfers, so
  the remaining display work is polish/robustness rather than switching away
  from bit-banged refresh.
- Continue external RTC validation with longer runs and additional RTC parts.
  The bare DS1307 on GPIO20/GPIO21 is hardware-verified for I2C detection,
  `rtcset`, boot-time UTC seeding, and local watchface display after manual
  timezone selection. The driver supports the shared time register map at
  `0x68`, rejects invalid/stopped BCD time, writes phone/manual UTC time back to
  the RTC, and exposes `ext_*` plus `phone_*` counters through the CDC `rtc`
  command. The `tzset America/Toronto` CDC command is a bring-up helper; the
  manual timezone state is not yet persistent across reset on RP2350, so phone
  timezone sync remains the production path once the phone session is reliable.
- Hardware-verify debug UART output on GPIO44 TX / GPIO45 RX at 230400 baud.
- Continue RP2350-side NimBLE work against the flashed ESP32-C6 HCI image. The
  current milestone has moved past pairing, PPOGATT negotiation, and a local
  USB CDC PBW watchface sideload/launch. The next unknowns are phone-driven app
  install flows, longer reconnection runs, notification sync, timeline/database
  traffic, and recovery from phone/app restarts.
- The Core Devices mobile app source has been cloned locally at
  `/Users/fred/Documents/Code/coredevices-mobileapp` for protocol reference.
  `origin/master` at `e4180ffc` defines app fetch response `NO_DATA` as `0x01`,
  the same value as `START`, while PebbleOS expects `NO_DATA` to be `0x04`. The
  local clone has a patch changing `NO_DATA` to `0x04` plus
  `AppFetchPacketTest`, which verifies the wire bytes for a `NO_DATA` response
  are `00 02 17 71 01 04`. Both `./gradlew :libpebble3:compileKotlinMetadata`
  and `./gradlew :libpebble3:jvmTest --tests AppFetchPacketTest` pass. If a
  stock phone app watchface launch shows a loading bar and then fails, capture
  `appfetch` immediately: `last_resp=1/1:start` plus a PutBytes init timeout may
  mean the phone had no compatible PBW data but encoded that as START.
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
