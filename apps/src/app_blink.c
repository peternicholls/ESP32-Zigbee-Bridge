/**
 * @file app_blink.c
 * @brief Blink demo task implementation
 *
 * ESP32-C6 Zigbee Bridge OS - Demo task
 *
 * Demonstrates sleep/yield usage in a fibre.
 */

#include "app_blink.h"
#include "os.h"
#include <inttypes.h>

#ifndef OS_PLATFORM_HOST
#include "led_strip.h"
#endif

#define BLINK_MODULE "BLINK"
#define BLINK_INTERVAL_MS 500
#define BLINK_GPIO 8 /* Addressable RGB LED on ESP32-C6 dev board */

static volatile uint32_t blink_count = 0;

#ifndef OS_PLATFORM_HOST
static led_strip_handle_t led_strip = NULL;
#endif

void app_blink_task(void *arg) {
  (void)arg;

  LOG_I(BLINK_MODULE, "Blink task started");

#ifndef OS_PLATFORM_HOST
  /* Configure addressable RGB LED using RMT */
  led_strip_config_t strip_config = {
      .strip_gpio_num = BLINK_GPIO,
      .max_leds = 1,
  };
  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000, /* 10MHz */
  };
  esp_err_t err =
      led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
  if (err != ESP_OK) {
    LOG_E(BLINK_MODULE, "Failed to create LED strip: %d", err);
    return;
  }
  led_strip_clear(led_strip);
#endif

  while (1) {
    blink_count++;

#ifndef OS_PLATFORM_HOST
    /* Set LED to blue */
    led_strip_set_pixel(led_strip, 0, 0, 0, 2); /* R=0, G=0, B=2 */
    led_strip_refresh(led_strip);
#endif
    LOG_D(BLINK_MODULE, "Blink %" PRIu32 " (on)", blink_count);
    os_sleep(BLINK_INTERVAL_MS);

#ifndef OS_PLATFORM_HOST
    /* Turn off */
    led_strip_clear(led_strip);
#endif
    LOG_T(BLINK_MODULE, "Blink %" PRIu32 " (off)", blink_count);
    os_sleep(BLINK_INTERVAL_MS);
  }
}

uint32_t app_blink_count(void) { return blink_count; }
