/*
   Adapted from espressif/esp-matter's examples/light/main/app_priv.h
   (Public Domain / CC0). Extended from matter-onoff-poc's trimmed-down
   version back toward the upstream example's original extended_color_light
   shape, since this project's whole point is exercising colour/CCT control
   that the on/off PoC deliberately cut. See software/matter-onoff-poc/README.md
   "What was cut from Espressif's own examples/light" for that history.
*/

#pragma once

#include <esp_err.h>
#include <esp_matter.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include "esp_openthread_types.h"
#endif

/** Default attribute values used during initialization. Matches
 *  espressif/esp-matter examples/light's own defaults (level 64/254,
 *  hue/saturation at a mid-warm default, 4000K-ish colour temperature). */
#define DEFAULT_POWER            true
#define DEFAULT_BRIGHTNESS       64
#define DEFAULT_HUE              128
#define DEFAULT_SATURATION       254
#define DEFAULT_COLOR_TEMP_MIREDS 250 /* ~4000K. Matter mireds = 1,000,000 / kelvin. */

typedef void *app_driver_handle_t;

/** Initialize the light driver: 5 LEDC PWM channels (R, G, B, cool white,
 *  warm white -- see hardware/smarthome/test-circuit.md for the GPIO
 *  assignment and why) plus the DevKitM-1's onboard WS2812 (GPIO8) as a
 *  visual stand-in for what those 5 channels represent. See app_driver.cpp. */
app_driver_handle_t app_driver_light_init();

/** Driver update, called from the common app_attribute_update_cb() */
esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val);

/** Set defaults for the light driver from the created data model */
esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()                                           \
    {                                                                                   \
        .radio_mode = RADIO_MODE_NATIVE,                                                \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()                                            \
    {                                                                                   \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,                              \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()                                            \
    {                                                                                   \
        .storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10, \
    }
#endif
