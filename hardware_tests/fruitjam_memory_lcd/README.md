# Fruit Jam Waveshare 1.3inch Memory LCD Test

Standalone Pico SDK bring-up test for the Waveshare 1.3inch Memory LCD on an
Adafruit Fruit Jam RP2350.

## Wiring

| LCD breakout | Fruit Jam |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| MISO | MISO / GPIO28 |
| MOSI | MOSI / GPIO31 |
| SCLK | SCK / GPIO30 |
| LCD_CS | D6 / GPIO6 |
| RAM_CS | A1 / GPIO41 |
| DISP | D7 / GPIO7 |
| EIN | D10 / GPIO10 |

Use 3.3V logic. This test does not use the breakout SRAM, so `RAM_CS` is held
high. `DISP` is driven high, and `EIN` is toggled as an external VCOM signal.

## Behavior

The firmware alternates between an all-white clear screen and a test pattern
with a black border, checkerboard, diagonal line, and text.

The Fruit Jam buttons are also polled so the same image can test the basic
input mapping for a future PebbleOS port:

| Fruit Jam button | GPIO | Bring-up role |
| --- | ---: | --- |
| Button #1 | 0 | Back |
| Button #2 | 4 | Up |
| Button #3 | 5 | Select |
| Button #2 + Button #3 | 4 + 5 | Down |
| Hold all three | 0 + 4 + 5 | Reboot to UF2/BOOTSEL |

The pattern screen also reports whether the onboard 8 MB PSRAM probe passed.

## Build

```sh
cd /Users/fred/Documents/Code/PebbleRP2350/hardware_tests/fruitjam_memory_lcd
export PATH="/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:/Users/fred/.pico-sdk/ninja/v1.12.1:$PATH"
/Users/fred/.pico-sdk/cmake/v3.31.5/CMake.app/Contents/bin/cmake -S . -B build -G Ninja \
  -DPICO_SDK_PATH=/Users/fred/.pico-sdk/sdk/2.1.1 \
  -DPICO_TOOLCHAIN_PATH=/Users/fred/.pico-sdk/toolchain/14_2_Rel1 \
  -DPICOTOOL_FORCE_FETCH_FROM_GIT=1
/Users/fred/.pico-sdk/cmake/v3.31.5/CMake.app/Contents/bin/cmake --build build
```

The UF2 output is:

```text
build/fruitjam_memory_lcd_test.uf2
```

## Flash

Put the Fruit Jam into BOOTSEL mode and copy the UF2 to the mounted
`RP2350`/`RPI-RP2` volume. The command used for the hardware-confirmed flash was:

```sh
/Users/fred/.pico-sdk/tools/picotool/2.0.0/picotool/picotool load -x build/fruitjam_memory_lcd_test.uf2
```

USB serial logs should appear on `/dev/cu.usbmodem101` after the program starts.
