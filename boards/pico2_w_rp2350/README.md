# Pico 2 W RP2350 Board Profile

Initial Raspberry Pi Pico 2 W target profile.

This board should share the generic RP2350 PebbleOS work with Fruit Jam where it
is truly common, but keep board-specific wiring and wireless choices separate.

- MCU: RP2350A
- Wireless: CYW43439 over the Pico W/RM2 gSPI interface
- Bluetooth direction: CYW43/BTstack HCI transport, not ESP32-C6 H4 UART
- Display direction: reuse the Sharp Memory LCD path when wired to the same
  Pebble display

This board is selectable as `--board pico2_w_rp2350`. Bluetooth is intentionally
stubbed in the normal profile until the CYW43439/CYW43 HCI transport is
implemented, so the first Pico UF2 target is display, buttons, flash/resources,
RTC fallback, and USB CDC diagnostics. An explicit NimBLE build can also be
enabled for compile testing; it currently links against the
`pico2w-cyw43-pending` backend stub.

The live Pico 2 W board has booted the normal stub profile into PebbleOS after
the shared RP2350 SDK-style clock setup was added. It enumerated as USB CDC
serial `PICO2WRP2350`, answered the debug shell, ran `App <TicToc>`, and
captured a valid 144x168 framebuffer. See
`docs/targets/pico2_w_rp2350/README.md` for the hardware-verified UF2 and flash
evidence.

The board has a 4 MiB flash layout. Its system resource bank starts at
`0x10150000`, and Pico UF2s must use the RP2350-E10 absolute block location
`0x103fff00`. Fruit Jam UF2s and Fruit Jam-packed resource addresses are not
safe for this target.

USB CDC should enumerate as product `Pico 2 W PebbleOS Debug` with serial
`PICO2WRP2350`, so it can coexist with Fruit Jam's `FJRP2350` debug port.

The external RTC profile is Kconfig-driven. This live bring-up profile defaults
to no external RTC because the current Pico 2 W board has no RTC attached.
Future boards can select a DS1307-compatible RTC at `0x68` on GP4 SDA / GP5 SCL,
or swap the RTC type, address, or pins with `CONFIG_RP2350_EXTERNAL_RTC_*`
without changing shared RP2350 RTC code.
