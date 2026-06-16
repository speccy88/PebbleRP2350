# Pico 2 W RP2350 SoC Glue

Placeholder for Pico 2 W-specific RP2350 integration.

The main difference from Fruit Jam is wireless:

- Fruit Jam currently uses RP2350 NimBLE host over H4 UART to ESP32-C6.
- Pico 2 W should use the CYW43439 through Raspberry Pi's CYW43 SPI path.
- The shared RP2350 NimBLE H4 transport now calls the
  `rp2350_bluetooth_hci_*` backend interface. Fruit Jam supplies an ESP32-C6
  backend, while Pico 2 W currently supplies a `pico2w-cyw43-pending` stub so
  `CONFIG_BT_FW_NIMBLE=y` builds can prove the integration seam.
- The production Pico 2 W implementation path is to replace that stub with a
  CYW43439/CYW43 backend, informed by the standalone `cyw43_hci_probe` hardware
  test and the Pico SDK `pico_btstack_cyw43` path.

Keep CYW43 transport, coexistence, and Pico 2 W boot/debug helpers here or in
clearly named shared RP2350 abstractions. Avoid leaking Fruit Jam C6 assumptions
into generic RP2350 code.
