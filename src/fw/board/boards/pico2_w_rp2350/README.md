# Pico 2 W Board Files

Pico 2 W-specific board configuration notes.

Expected contents:

- GPIO map for the Sharp Memory LCD wiring on Pico 2 W
- `Rp2350MemoryLcdConfig` instance for the shared RP2350 LCD transport
- Three-button mapping and the four-button emulation policy
- Flash/resource layout decisions for the Pico 2 W external flash
- Board config source/header files for `--board pico2_w_rp2350`
- External SPI/QSPI flash wiring for Pebble resources and filesystem storage

The first board profile uses SPI0 on GP16-GP19 for the memory LCD, GP20 for
DISP, GP21 for EXTCOMIN, GP4/GP5 for optional DS1307/DS3231 I2C, and GP6-GP8
for the three physical buttons.

Keep Fruit Jam-only pins such as ESP32-C6 reset, ESP busy, ESP UART, PSRAM CS,
and Fruit Jam header names out of this profile.

Do not rely on the Pico 2 W onboard XIP flash as the long-term Pebble
filesystem/resource store. The early profile explicitly selects
`CONFIG_RP2350_PEBBLE_STORAGE_ONBOARD_XIP=y`, but flash erase/write operations
can pause XIP instruction fetches from the same chip. The profile should move to
`CONFIG_RP2350_PEBBLE_STORAGE_EXTERNAL_NOR=y` with an external 3.3 V flash before
app install and resource-heavy testing become part of the normal Pico 2 W
workflow.
