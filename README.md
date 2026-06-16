<p align="center">
  <img src="docs/_static/images/logo.svg">
</p>

<p align="center">
 PebbleOS 
</p>

<p align="center">
  <a href="https://github.com/coredevices/PebbleOS/actions/workflows/build-firmware.yml?query=branch%3Amain"><img src="https://github.com/coredevices/PebbleOS/actions/workflows/build-firmware.yml/badge.svg?branch=main"></a>
  <a href="https://pebbleos-core.readthedocs.io/en/latest"><img src="https://readthedocs.org/projects/pebbleos-core/badge/?version=latest&style=flat"></a>
  <a href="https://forum.repebble.com/"><img src="https://img.shields.io/discourse/posts?server=https%3A%2F%2Fforum.repebble.com&label=forum"></a>
</p>

## RP2350 preview ports

This fork currently carries preview bring-up ports for two RP2350 targets:

- Adafruit Fruit Jam RP2350 with a Waveshare/Pebble-compatible 144x168 Sharp
  Memory LCD, DS1307-compatible external RTC support, and ESP32-C6 HCI UART
  Bluetooth.
- Raspberry Pi Pico 2 W with the shared RP2350 display, time, storage, resource,
  and USB CDC profile scaffolding. The normal Pico 2 W PebbleOS image still uses
  stub Bluetooth while the CYW43439/CYW43 backend is brought up separately.

Current verified state:

- `fruitjam_rp2350` configures and builds with the Pico/RP2350 toolchain.
- `pico2_w_rp2350` configures and builds as a separate Pico-safe profile.
- The Fruit Jam combined UF2 packages firmware and Pebble system resources for
  the Fruit Jam 16 MB XIP flash layout.
- The Pico 2 W combined UF2 uses the Pico-safe 4 MB resource layout and must not
  be swapped with the Fruit Jam image.
- The memory LCD path, Pebble boot UI, launcher handoff, fixed-power/no-battery
  mode, USB CDC debug shell, frame capture, and software BOOTSEL entry have
  been hardware-tested on the Fruit Jam board.
- Fruit Jam Bluetooth reaches BLE advertising, iOS/Core Devices pairing,
  encrypted reconnect, GATT discovery, PPOGATT session open, and bidirectional
  Pebble Protocol traffic through the controller-only ESP32-C6 HCI UART
  firmware.
- Pico 2 W has a hardware-tested PebbleOS stub-Bluetooth image plus a standalone
  CYW43 HCI probe; the PebbleOS CYW43439 backend is the next target-specific
  Bluetooth step.

Useful local commands:

```sh
PATH="/Users/fred/Documents/Code/PebbleRP2350/.venv/bin:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH" .venv/bin/python ./pbl configure --board fruitjam_rp2350
PATH="/Users/fred/Documents/Code/PebbleRP2350/.venv/bin:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH" .venv/bin/python ./pbl build
hardware_tests/fruitjam_memory_lcd/build/_deps/picotool-build/picotool uf2 convert build/src/fw/tintin_fw.elf build/src/fw/tintin_fw.uf2 --platform rp2350 --abs-block
.venv/bin/python tools/fruitjam_pack_uf2.py
```

Detailed hardware notes, wiring, hashes, caveats, and debug commands live in
[`docs/fruitjam_rp2350_port_status.md`](docs/fruitjam_rp2350_port_status.md)
and [`docs/targets/pico2_w_rp2350/README.md`](docs/targets/pico2_w_rp2350/README.md).
Agent-oriented build, flash, debug, and release workflow notes live in
[`agent.md`](agent.md).

## Resources

Here's a quick summary of resources to help you find your way around:

### Getting Started

- 📖 [Documentation](https://pebbleos-core.readthedocs.io/en/latest)
- 🚀 [Prerequisites Guide](https://pebbleos-core.readthedocs.io/en/latest/development/getting_started.html)

### Code and Development

- ⌚ [Source Code Repository](https://github.com/coredevices/PebbleOS)
- 🐛 [Issue Tracker](https://github.com/coredevices/PebbleOS/issues)
- 🤝 [Contribution Guide](CONTRIBUTING.md)

### Community and Support

- 💬 [Discord](https://discordapp.com/invite/aRUAYFN)
- 👥 [Discussions](https://github.com/coredevices/PebbleOS/discussions)
