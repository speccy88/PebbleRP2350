# Pico 2 W Hardware Tests

Pico 2 W bring-up tests. These build separately from the Fruit Jam hardware
tests so UF2s can be selected by board profile.

Available tests:

- `cyw43_hci_probe`: powers up the Pico 2 W CYW43439 Bluetooth controller
  through the Pico SDK `pico_btstack_cyw43` HCI path and advertises as
  `Pico2W-HCI` when BTstack is working.

Useful first tests:

- Sharp Memory LCD clear/pattern/frame-capture equivalent
- Hardware SPI smoke test for the chosen `Rp2350MemoryLcdConfig` pins
- Three-button event test
- Resource flash layout verification: Pico 2 W must keep all UF2 blocks below
  `0x10400000`; the PebbleOS resource bank starts at `0x10150000`
