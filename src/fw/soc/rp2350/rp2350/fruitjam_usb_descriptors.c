/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "tusb.h"

#include <stdint.h>
#include <string.h>

#define USB_VID 0xcafe
#define USB_PID 0x4020
#define USB_BCD 0x0200

enum {
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_TOTAL,
};

enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_CDC,
};

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static tusb_desc_device_t const s_device_descriptor = {
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = USB_BCD,
  .bDeviceClass = TUSB_CLASS_MISC,
  .bDeviceSubClass = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor = USB_VID,
  .idProduct = USB_PID,
  .bcdDevice = 0x0100,
  .iManufacturer = STRID_MANUFACTURER,
  .iProduct = STRID_PRODUCT,
  .iSerialNumber = STRID_SERIAL,
  .bNumConfigurations = 1,
};

static uint8_t const s_configuration_descriptor[] = {
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 8,
                     EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

static char const *const s_string_descriptors[] = {
  [STRID_MANUFACTURER] = "Core Devices",
  [STRID_PRODUCT] = "Fruit Jam PebbleOS Debug",
  [STRID_SERIAL] = "FJRP2350",
  [STRID_CDC] = "PebbleOS CDC",
};

static uint16_t s_string_descriptor[32];

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&s_device_descriptor;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return s_configuration_descriptor;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;

  uint8_t count;
  if (index == STRID_LANGID) {
    s_string_descriptor[1] = 0x0409;
    count = 1;
  } else {
    if (index >= sizeof(s_string_descriptors) / sizeof(s_string_descriptors[0]) ||
        s_string_descriptors[index] == NULL) {
      return NULL;
    }

    const char *str = s_string_descriptors[index];
    count = (uint8_t)strlen(str);
    if (count > 31U) {
      count = 31U;
    }

    for (uint8_t i = 0; i < count; ++i) {
      s_string_descriptor[1U + i] = (uint8_t)str[i];
    }
  }

  s_string_descriptor[0] = (uint16_t)((TUSB_DESC_STRING << 8U) | (2U * count + 2U));
  return s_string_descriptor;
}
