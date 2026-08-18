/*
   This file is adapted from espressif/esp-matter's examples/light/main/app_priv.h
   (Public Domain / CC0), trimmed to on/off only.
*/

#pragma once

#include <esp_err.h>
#include <esp_matter.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include "esp_openthread_types.h"
#endif

/** Default attribute values used during initialization */
#define DEFAULT_POWER true

typedef void *app_driver_handle_t;

/** Initialize the on/off light driver (WS2812 on GPIO8, see app_driver.cpp) */
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
