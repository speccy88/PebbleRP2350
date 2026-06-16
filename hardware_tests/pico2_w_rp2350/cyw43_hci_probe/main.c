#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#define STATUS_PERIOD_MS 1000

static btstack_packet_callback_registration_t s_hci_event_callback;
static bool s_advertising;
static uint32_t s_last_status_ms;
static uint32_t s_status_count;

static const uint8_t s_adv_data[] = {
    0x02,
    BLUETOOTH_DATA_TYPE_FLAGS,
    0x06,
    0x0b,
    BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,
    'P',
    'i',
    'c',
    'o',
    '2',
    'W',
    '-',
    'H',
    'C',
    'I',
};

static void start_advertising(void) {
  if (s_advertising) {
    return;
  }

  bd_addr_t local_addr;
  bd_addr_t null_addr;
  memset(null_addr, 0, sizeof(null_addr));
  gap_local_bd_addr(local_addr);

  printf("BT local address: %s\n", bd_addr_to_str(local_addr));

  const uint16_t adv_int_min = 0x0030;
  const uint16_t adv_int_max = 0x0060;
  gap_advertisements_set_params(adv_int_min, adv_int_max, 0, 0, null_addr, 0x07, 0x00);
  gap_advertisements_set_data(sizeof(s_adv_data), (uint8_t *)s_adv_data);
  gap_advertisements_enable(1);

  s_advertising = true;
  puts("BLE advertising as Pico2W-HCI");
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
  (void)channel;
  (void)size;

  if (packet_type != HCI_EVENT_PACKET) {
    return;
  }

  switch (hci_event_packet_get_type(packet)) {
    case BTSTACK_EVENT_STATE: {
      const uint8_t state = btstack_event_state_get_state(packet);
      printf("BTSTACK_EVENT_STATE %u\n", state);
      if (state == HCI_STATE_WORKING) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        start_advertising();
      }
      break;
    }

    case HCI_EVENT_DISCONNECTION_COMPLETE:
      puts("HCI_EVENT_DISCONNECTION_COMPLETE");
      break;

    case HCI_EVENT_LE_META:
      printf("HCI_EVENT_LE_META subevent=0x%02x\n", hci_event_le_meta_get_subevent_code(packet));
      break;

    default:
      break;
  }
}

int main(void) {
  stdio_init_all();
  sleep_ms(2000);

  puts("\nPico 2 W CYW43 HCI probe");
  puts("Initializing CYW43...");

  const int cyw43_result = cyw43_arch_init();
  printf("cyw43_arch_init result: %d\n", cyw43_result);
  if (cyw43_result != 0) {
    while (true) {
      sleep_ms(1000);
      puts("CYW43 init failed");
    }
  }

  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

  s_hci_event_callback.callback = &packet_handler;
  hci_add_event_handler(&s_hci_event_callback);

  puts("Powering Bluetooth HCI on...");
  const int hci_result = hci_power_control(HCI_POWER_ON);
  printf("hci_power_control result: %d\n", hci_result);

  s_last_status_ms = to_ms_since_boot(get_absolute_time());

  while (true) {
    cyw43_arch_poll();

    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if ((now_ms - s_last_status_ms) >= STATUS_PERIOD_MS) {
      s_last_status_ms = now_ms;
      s_status_count++;
      printf("tick %lu hci_state=%u advertising=%u\n", (unsigned long)s_status_count,
             hci_get_state(), s_advertising ? 1u : 0u);
    }

    sleep_ms(1);
  }
}
