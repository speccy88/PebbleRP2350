# Pico 2 W USB CDC Descriptor Probe

This is a standalone Pico SDK diagnostic for the RP2350 PebbleOS port. It
links the Pebble RP2350 USB descriptor source directly, but uses the Pico SDK
runtime, clocks, reset handling, and TinyUSB integration.

If this UF2 enumerates as `Pico 2 W PebbleOS Debug`, the descriptors are good
and the remaining Pico 2 W failure is in the Pebble RP2350 runtime USB
bring-up path.

Build:

```sh
cmake -S hardware_tests/pico2_w_rp2350/usb_cdc_descriptor_probe \
  -B hardware_tests/pico2_w_rp2350/usb_cdc_descriptor_probe/build \
  -DPICO_SDK_PATH=/Users/fred/.pico-sdk/sdk/2.0.0
cmake --build hardware_tests/pico2_w_rp2350/usb_cdc_descriptor_probe/build
```

Flash the current live Pico 2 W by serial:

```sh
/Users/fred/.pico-sdk/tools/picotool/2.0.0/picotool/picotool load \
  -f --ser B2047FF29EB03C7E -v -x \
  hardware_tests/pico2_w_rp2350/usb_cdc_descriptor_probe/build/pico2_w_usb_cdc_descriptor_probe.uf2
```
