/*
   Matter-over-Thread Extended Color Light for the ESP32-H2-DevKitM-1 --
   the "emulate the 5 LEDC PWM channels on the onboard LED" bench step.

   Extends matter-onoff-poc (this repo's build-tested first milestone,
   commit 89c07ac) from an on/off-only light back toward the shape of
   espressif/esp-matter's own examples/light (extended_color_light: full
   RGB + brightness + colour temperature), since matter-onoff-poc's own
   README documents that on_off_light was reached by deliberately trimming
   colour/level control OUT of that upstream example for a minimal first
   smoke test. This project un-trims exactly that, to validate the Matter
   Extended Color Light cluster and HomeKit's colour picker end to end
   before any of the Connect Smarthome retrofit hardware exists -- see
   hardware/smarthome/README.md and hardware/smarthome/test-circuit.md for
   why this device needs 5 channels (RGB + cool/warm white), not 3.

   Everything below except the endpoint-creation block in app_main() is
   unchanged from matter-onoff-poc/main/app_main.cpp -- commissioning code
   printing, the OpenThread CLI diagnostic, and the event/attribute
   callbacks are all identical, already-working infrastructure, not
   reimplemented here.

   NOT BUILD-TESTED, same as app_driver.cpp -- see that file's header for
   why, and for the specific area of real risk (level_control /
   color_control config_t field names in app_main(), below).
*/

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>

#include <common_macros.h>

#include <app_priv.h>
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

#if CONFIG_OPENTHREAD_CLI
#include <esp_openthread_cli.h>
#endif

#include <setup_payload/OnboardingCodesUtil.h>

static const char *TAG = "app_main";
uint16_t light_endpoint_id = 0;

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

/* Unchanged from matter-onoff-poc/main/app_main.cpp. */
static void format_manual_code(const char *raw, size_t raw_len, char *out, size_t out_len)
{
    size_t o = 0;
    for (size_t i = 0; i < raw_len && o + 1 < out_len; i++) {
        if ((i == 4 || i == 7) && o + 1 < out_len) {
            out[o++] = '-';
        }
        if (o + 1 < out_len) {
            out[o++] = raw[i];
        }
    }
    out[o] = '\0';
}

static void print_commissioning_codes(void)
{
    char qr_code_buf[256];
    chip::MutableCharSpan qr_code(qr_code_buf);
    char manual_code_buf[32];
    chip::MutableCharSpan manual_code(manual_code_buf);

    chip::RendezvousInformationFlags rendezvous_flags(chip::RendezvousInformationFlag::kBLE);
    CHIP_ERROR qr_err     = GetQRCode(qr_code, rendezvous_flags);
    CHIP_ERROR manual_err = GetManualPairingCode(manual_code, rendezvous_flags);

    if (qr_err != CHIP_NO_ERROR || manual_err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Failed to compute commissioning codes (qr err %" CHIP_ERROR_FORMAT ", manual err %" CHIP_ERROR_FORMAT ")",
                 qr_err.Format(), manual_err.Format());
        return;
    }

    char formatted[32];
    format_manual_code(manual_code.data(), manual_code.size(), formatted, sizeof(formatted));

    printf("\n");
    printf("================================================================\n");
    printf("==                                                            ==\n");
    printf("==   PAIRING CODE -- enter manually in the Home app:          ==\n");
    printf("==                                                            ==\n");
    printf("==       %-53s==\n", formatted);
    printf("==                                                            ==\n");
    printf("==   QR payload: %.*s\n", (int)qr_code.size(), qr_code.data());
    printf("==                                                            ==\n");
    printf("================================================================\n");
    printf("\n");
}

#if CONFIG_OPENTHREAD_CLI
static void start_openthread_cli_once(void)
{
    static bool started = false;
    if (started) {
        return;
    }
    started = true;
    esp_openthread_cli_init();
    esp_openthread_cli_create_task();
    printf("\nOpenThread CLI ready on this console. Type commands bare, no "
           "\"ot \" prefix -- e.g. state / neighbor table / parent / counters.\n\n");
}
#endif

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        print_commissioning_codes();
#if CONFIG_OPENTHREAD_CLI
        start_openthread_cli_once();
#endif
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;
    default:
        break;
    }
}

// This device has no separate identify effect (no second LED to flash), so it just logs.
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    if (type == PRE_UPDATE) {
        app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
        return app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
    }
    return ESP_OK;
}

extern "C" void app_main()
{
    nvs_flash_init();

    app_driver_handle_t light_handle = app_driver_light_init();
    ABORT_APP_ON_FAILURE(light_handle != nullptr, ESP_LOGE(TAG, "Failed to initialize the light driver"));

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    /* Verified against the real esp-matter checkout at /workspace/ai/esp-matter
     * (esp_matter_endpoint_impl.h, esp_matter_cluster_impl.h,
     * esp_matter_feature_impl.h) -- not left as a guess. on_off/on_off_lighting
     * and level_control/level_control_lighting were already correct (each is a
     * top-level sibling field on the struct, matching the working on/off PoC's
     * pattern). Two real mistakes were caught and fixed here: (1)
     * color_temperature is NOT nested inside color_control -- it's the
     * separate top-level field color_control_color_temperature, and its
     * startup field is spelled start_up_color_temperature_mireds, not
     * startup_color_temperature_mireds; (2) color_capabilities was left at its
     * default of 0, which would have advertised NO supported color mode to a
     * commissioner regardless of the clusters actually being present --
     * HomeKit would very likely have shown no colour/CT control at all. */
    extended_color_light::config_t light_config;
    light_config.on_off.on_off = DEFAULT_POWER;
    light_config.on_off_lighting.start_up_on_off = nullptr;
    light_config.level_control.current_level = DEFAULT_BRIGHTNESS;
    light_config.level_control_lighting.start_up_current_level = DEFAULT_BRIGHTNESS;
    light_config.color_control.color_mode = (uint8_t)ColorControl::ColorModeEnum::kColorTemperatureMireds;
    light_config.color_control.enhanced_color_mode = (uint8_t)ColorControl::ColorModeEnum::kColorTemperatureMireds;
    light_config.color_control.color_capabilities =
        (uint16_t)ColorControl::ColorCapabilitiesBitmap::kHueSaturation |
        (uint16_t)ColorControl::ColorCapabilitiesBitmap::kXy |
        (uint16_t)ColorControl::ColorCapabilitiesBitmap::kColorTemperature;
    light_config.color_control_color_temperature.start_up_color_temperature_mireds = nullptr;

    endpoint_t *endpoint = extended_color_light::create(node, &light_config, ENDPOINT_FLAG_NONE, light_handle);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create extended color light endpoint"));

    /* esp-matter's own extended_color_light::add() (esp_matter_endpoint.cpp) only wires up the
     * ColorTemperature and Xy color_control features -- it never calls
     * cluster::color_control::feature::hue_saturation::add(), confirmed by reading that function
     * directly. Setting color_capabilities above to claim HueSaturation support does nothing on
     * its own: HomeKit reads the cluster's actual FeatureMap, found no HueSaturation feature, and
     * showed only colour-temperature + dimming controls -- no hue/saturation picker -- even
     * though the ColorCapabilities bitmap claimed otherwise. This adds the feature esp-matter's
     * helper omits. */
    cluster_t *color_control_cluster = cluster::get(endpoint, ColorControl::Id);
    cluster::color_control::feature::hue_saturation::config_t hue_saturation_config;
    cluster::color_control::feature::hue_saturation::add(color_control_cluster, &hue_saturation_config);

    light_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Extended color light created with endpoint_id %d", light_endpoint_id);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);
#endif

    esp_err_t err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    app_driver_light_set_defaults(light_endpoint_id);

    while (true) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
