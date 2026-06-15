# Fruit Jam ESP32-C6 HCI Firmware

This ESP-IDF project turns the Fruit Jam ESP32-C6 into a BLE controller for the
RP2350-side PebbleOS NimBLE host.

It uses the Fruit Jam board UART wiring:

- ESP32-C6 UART1 TX / GPIO16 -> RP2350 D9 / GPIO9 / UART1 RX
- ESP32-C6 UART1 RX / GPIO17 -> RP2350 D8 / GPIO8 / UART1 TX
- ESP32-C6 IO18 -> RP2350 `ESP_BUSY` / GPIO3

The firmware uses ESP-IDF's controller-only H4 UART mode on UART1, mapped to
GPIO16/GPIO17. It disables console and bootloader logs and drives IO18 high
while busy and low once the BLE controller is ready for HCI traffic.

## Build

```sh
cd /Users/fred/Documents/Code/PebbleRP2350/hardware_tests/fruitjam_esp32c6_hci
. /Users/fred/esp/v5.5.4/esp-idf/export.sh
idf.py set-target esp32c6
idf.py build
idf.py merge-bin
```

## Flash Via Fruit Jam Passthrough

Adafruit's passthrough UF2 makes the RP2350 act as the programmer for the
ESP32-C6. Put the Fruit Jam RP2350 into BOOTSEL, load the passthrough UF2, then
flash the merged ESP32-C6 image through the new serial port:

```sh
python -m esptool --chip esp32c6 --before no_reset --after no_reset \
  -p /dev/cu.usbmodem101 -b 115200 write_flash 0 build/merged-binary.bin
```

After flashing, restore the PebbleOS Fruit Jam RP2350 UF2.

## Flash Via PebbleOS CDC Passthrough

PebbleOS builds that expose the Fruit Jam USB debug shell can put the ESP32-C6
in its ROM bootloader and bridge the same CDC port to the ESP UART:

```sh
python - <<'PY'
import serial, time
with serial.Serial('/dev/cu.usbmodemFJRP23501', 115200, timeout=0.2) as s:
    s.write(b'esppass\r\n')
    s.flush()
    time.sleep(0.5)
PY

python -m esptool --chip esp32c6 --before no_reset --after no_reset \
  -p /dev/cu.usbmodemFJRP23501 -b 115200 write_flash 0 build/merged-binary.bin
```

Reset or reflash the RP2350 after using passthrough mode to return to PebbleOS.
