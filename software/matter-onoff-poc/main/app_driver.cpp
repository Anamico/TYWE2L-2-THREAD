/*
   WS2812 driver for the DevKitM-1's onboard addressable LED, wired directly
   to the Matter On/Off cluster. Adapted from the structure of
   espressif/esp-matter's examples/light/main/app_driver.cpp and
   device_hal/led_driver/ws2812/led_driver.c (Apache-2.0), but this file
   talks to the espressif/led_strip component directly rather than going
   through esp-matter's led_driver/device_hal abstraction: this project does
   not target one of esp-matter's own reference boards (esp32h2_devkit_c), so
   that layer's led_driver_get_config() would supply the wrong board's pin
   default. GPIO8 below is taken from this repo's own schematic-verified
   docs/devkit-bringup.md ("Addressable RGB LED | GPIO8 | WS2812B (D6) ..."),
   not assumed to match Espressif's reference board.
*/

#include <esp_err.h>
#include <esp_log.h>

#include <led_strip.h>

#include <esp_matter.h>
#include <app_priv.h>

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;

/* Confirmed on the DevKitM-1 v1.3 schematic: WS2812B (D6) on GPIO8, driven
 * through D11/R17 with R25 pulling to 5V. See docs/devkit-bringup.md
 * section 2. GPIO8 is also a strapping pin, but that only matters at reset;
 * driving it as a WS2812 data line after boot is the board's intended use
 * of the pin, not a conflict with strapping. */
#define ONBOARD_LED_GPIO   8
#define ONBOARD_LED_COUNT  1

/* Not verified against a real build: whether 10 MHz is the right RMT
 * resolution for every ESP-IDF/led_strip version combination this project
 * might be built against. Copied from device_hal/led_driver/ws2812/led_driver.c,
 * which uses the same value for the same WS2812 part on other Espressif H2
 * boards, so it should be safe, but it has not been exercised on this board. */
#define WS2812_RMT_RESOLUTION_HZ (10 * 1000 * 1000)

static esp_err_t led_set_on_off(led_strip_handle_t strip, bool on)
{
    esp_err_t err;
    if (on) {
        /* Plain white: this is an on/off test, not a colour test. */
        err = led_strip_set_pixel(strip, 0, 255, 255, 255);
    } else {
        err = led_strip_set_pixel(strip, 0, 0, 0, 0);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_set_pixel failed: %d", err);
        return err;
    }
    return led_strip_refresh(strip);
}

app_driver_handle_t app_driver_light_init()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = ONBOARD_LED_GPIO,
        .max_leds = ONBOARD_LED_COUNT,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = WS2812_RMT_RESOLUTION_HZ,
    };

    led_strip_handle_t strip = NULL;
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (err != ESP_OK || !strip) {
        ESP_LOGE(TAG, "WS2812 driver install failed: %d", err);
        return NULL;
    }
    return (app_driver_handle_t)strip;
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    if (endpoint_id != light_endpoint_id) {
        return ESP_OK;
    }
    if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
        return led_set_on_off((led_strip_handle_t)driver_handle, val->val.b);
    }
    return ESP_OK;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    led_strip_handle_t strip = (led_strip_handle_t)priv_data;

    attribute_t *attribute = attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    esp_matter_attr_val_t val;
    attribute::get_val(attribute, &val);
    return led_set_on_off(strip, val.val.b);
}
