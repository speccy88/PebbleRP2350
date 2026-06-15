/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "driver/gpio.h"
#include "esp_bt.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <stdbool.h>

#define FRUITJAM_ESP_BUSY_GPIO GPIO_NUM_18

static void prv_set_busy(bool busy) {
  const gpio_config_t config = {
      .pin_bit_mask = BIT64(FRUITJAM_ESP_BUSY_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  ESP_ERROR_CHECK(gpio_config(&config));
  ESP_ERROR_CHECK(gpio_set_level(FRUITJAM_ESP_BUSY_GPIO, busy ? 1 : 0));
}

void app_main(void) {
  prv_set_busy(true);

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  esp_bt_controller_config_t config_opts = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_bt_controller_init(&config_opts));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

  prv_set_busy(false);

  for (;;) {
    vTaskDelay(portMAX_DELAY);
  }
}
