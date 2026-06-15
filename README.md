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

## Fruit Jam RP2350 preview port

This fork currently carries a bring-up port for the Adafruit Fruit Jam RP2350
running PebbleOS on the onboard RP2350 with a Waveshare/Pebble-compatible
144x168 Sharp Memory LCD.

Current verified state:

- `fruitjam_rp2350` configures and builds with the Pico/RP2350 toolchain.
- The combined UF2 at `build/src/fw/tintin_fw_with_resources.uf2` packages both
  firmware and Pebble system resources for the Fruit Jam 16 MB XIP flash layout.
- The memory LCD path, Pebble boot UI, launcher handoff, fixed-power/no-battery
  mode, USB CDC debug shell, frame capture, and software BOOTSEL entry have
  been hardware-tested on the Fruit Jam board.
- Bluetooth bring-up now reaches BLE advertising through a controller-only
  ESP32-C6 HCI UART firmware. The phone app can see the watch as a Pebble BLE
  peripheral; pairing and end-to-end phone services still need more testing.

Useful local commands:

```sh
PATH="/Users/fred/Documents/Code/PebbleRP2350/.venv/bin:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH" .venv/bin/python ./pbl configure --board fruitjam_rp2350
PATH="/Users/fred/Documents/Code/PebbleRP2350/.venv/bin:/Users/fred/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH" .venv/bin/python ./pbl build
hardware_tests/fruitjam_memory_lcd/build/_deps/picotool-build/picotool uf2 convert build/src/fw/tintin_fw.elf build/src/fw/tintin_fw.uf2 --platform rp2350 --abs-block
.venv/bin/python tools/fruitjam_pack_uf2.py
```

Detailed hardware notes, wiring, hashes, caveats, and debug commands live in
[`docs/fruitjam_rp2350_port_status.md`](docs/fruitjam_rp2350_port_status.md).

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
