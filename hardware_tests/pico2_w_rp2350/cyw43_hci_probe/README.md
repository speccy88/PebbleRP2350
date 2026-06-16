# Pico 2 W CYW43 HCI Probe

Standalone Pico SDK smoke test for the Raspberry Pi Pico 2 W CYW43439 Bluetooth
path. It powers up the CYW43 controller through the SDK `pico_btstack_cyw43`
transport and advertises as `Pico2W-HCI` when BTstack reaches
`HCI_STATE_WORKING`. The build links `pico_lwip_nosys` only to satisfy the CYW43
driver headers; the test does not start Wi-Fi.

Build:

```sh
export PATH="$HOME/.pico-sdk/toolchain/14_2_Rel1/bin:$HOME/.pico-sdk/ninja/v1.12.1:$PATH"
$HOME/.pico-sdk/cmake/v3.31.5/CMake.app/Contents/bin/cmake \
  -S hardware_tests/pico2_w_rp2350/cyw43_hci_probe \
  -B hardware_tests/pico2_w_rp2350/cyw43_hci_probe/build \
  -G Ninja \
  -DPICO_SDK_PATH=$HOME/.pico-sdk/sdk/2.1.1 \
  -DPICO_TOOLCHAIN_PATH=$HOME/.pico-sdk/toolchain/14_2_Rel1 \
  -Dpicotool_DIR=$HOME/.pico-sdk/picotool/2.1.1/picotool
$HOME/.pico-sdk/cmake/v3.31.5/CMake.app/Contents/bin/cmake \
  --build hardware_tests/pico2_w_rp2350/cyw43_hci_probe/build
```

Flash:

```sh
.venv/bin/python tools/rp2350_boot_volume.py \
  --expect-serial B2047FF29EB03C7E \
  --copy hardware_tests/pico2_w_rp2350/cyw43_hci_probe/build/pico2_w_cyw43_hci_probe.uf2
```

Expected USB output:

```text
Pico 2 W CYW43 HCI probe
cyw43_arch_init result: 0
Powering Bluetooth HCI on...
BTSTACK_EVENT_STATE 2
BT local address: xx:xx:xx:xx:xx:xx
BLE advertising as Pico2W-HCI
tick 1 hci_state=2 advertising=1
```

Latest local probe build evidence, 2026-06-15:

- Path:
  `hardware_tests/pico2_w_rp2350/cyw43_hci_probe/build/pico2_w_cyw43_hci_probe.uf2`
- Size: 698880 bytes
- SHA-256:
  `db5e675206d9cc71d12ebd51e24a230df8e610bba5ceb4df03703926c1915efd`
- Hardware check: USB serial output reached `hci_state=2 advertising=1`, and a
  macOS BLE scan found local name `Pico2W-HCI`.
