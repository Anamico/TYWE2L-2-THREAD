/*
   Minimal Matter-over-Thread On/Off Light for the ESP32-H2-DevKitM-1.

   This is a feasibility test, not the TYWE2L-leg-mapped device: it drives
   the DevKitM-1's own onboard WS2812 LED (GPIO8), independent of the
   carrier board and pin-mapping-v2 work. See README.md in this directory.

   Adapted and trimmed from espressif/esp-matter's examples/light/main/app_main.cpp
   (Public Domain / CC0): dropped OTA, encrypted OTA, chip shell, memory
   profiling and the button/factory-reset path to keep the first build's
   moving parts to a minimum. Uses endpoint::on_off_light rather than
   extended_color_light, since this project only needs the OnOff cluster.
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

/* Diagnostic-only addition for investigating a live commissioning stall: a
 * dedicated OpenThread CLI console, giving `state`, `neighbor table`,
 * `parent` (RSSI/LQI to the Border Router) and `counters` (MAC/IP RX/TX/drop
 * counts) at the same UART used for logging. Verified against ESP-IDF
 * v6.0.2's actual source, not memory: esp_openthread_cli.h documents two
 * integration modes, and this uses the simpler one -- a dedicated task via
 * esp_openthread_cli_create_task() -- rather than registering a single
 * command into an esp_console REPL this project doesn't otherwise run.
 * Under this mode commands are typed BARE, with no "ot " prefix (that
 * prefix only applies to the *other* integration mode, which this isn't).
 * See docs/provisioning/ for how to build with this enabled and the exact
 * commands to run after a failed pairing attempt. */
#if CONFIG_OPENTHREAD_CLI
#include <esp_openthread_cli.h>
#endif

/* setup_payload/OnboardingCodesUtil.h is connectedhomeip's own reference
 * implementation for printing the QR code / manual pairing code (used by
 * upstream's own examples, e.g. examples/all-clusters-app/esp32). We call
 * GetQRCode()/GetManualPairingCode() ourselves rather than the header's
 * PrintOnboardingCodes() convenience wrapper so the output can be a
 * highlighted block instead of a single plain log line, but the values
 * themselves come from the exact same underlying calls PrintOnboardingCodes
 * uses: GetCommissionableDataProvider()->GetSetupPasscode()/
 * GetSetupDiscriminator() internally, read at runtime, not compile-time
 * constants. That means once a unit is provisioned with esp-matter-mfg-tool's
 * own unique per-device discriminator/passcode (see docs/provisioning/), this
 * same firmware image prints THAT unit's own code, not this test build's
 * shared 34970112332. Verified against connectedhomeip's actual current
 * source (src/setup_payload/OnboardingCodesUtil.h and .cpp) rather than
 * assumed from memory. */
#include <setup_payload/OnboardingCodesUtil.h>

static const char *TAG = "app_main";
uint16_t light_endpoint_id = 0;

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

/* Reformats an 11-digit manual pairing code into the standard 4-3-4
 * hyphenated grouping (e.g. "34970112332" -> "3497-011-2332"), matching how
 * the Matter spec's own examples display the code to a person. `raw_len` is
 * used rather than strlen(), because chip::MutableCharSpan is not guaranteed
 * NUL-terminated after a Get*() call -- only the first raw_len bytes are
 * valid. */
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

/* Prints the live commissioning codes in a block that stands out against
 * fast-scrolling boot log noise. Computed fresh from this device's actual
 * runtime discriminator/passcode via GetQRCode()/GetManualPairingCode()
 * (see the OnboardingCodesUtil.h include comment above) -- NOT a hardcoded
 * string, so this reflects whatever this specific unit was provisioned
 * with, test build or real mfg-tool-flashed device alike. kBLE is the
 * correct RendezvousInformationFlags value here because BLE is this
 * project's commissioning (rendezvous) transport; Thread is the separate
 * operational network joined afterward, not the rendezvous transport. */
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
        /* CHIP_ERROR_FORMAT + .Format() is connectedhomeip's own standard pairing for logging a
         * CHIP_ERROR (ChipError.h picks the matching printf conversion and .Format() return type
         * for whichever CHIP_CONFIG_ERROR_FORMAT_AS_STRING mode is compiled in) -- this specific
         * error-path line has not been seen compile, unlike the codes themselves above. */
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
/* Starts the OpenThread CLI task at most once. Hooked off
 * kCommissioningWindowOpened rather than called unconditionally in
 * app_main() because esp_openthread_cli_init() needs a live OpenThread
 * instance (it calls otCliInit(esp_openthread_get_instance(), ...)
 * internally), and esp_matter::start() launches OpenThread's own init on a
 * separate task -- there is no explicit "OpenThread is ready" callback
 * exposed to application code here. This point is empirically safe: a real
 * boot log from this project, this session, shows "OpenThread started: OK"
 * at t=843ms, well before "Commissioning window opened" at t=1503ms. Not a
 * documented API guarantee, an observed one -- if a future build somehow
 * reaches this event before OpenThread is actually up, otCliInit() would be
 * called against an invalid instance. Diagnostic-only code; not something
 * to carry into a real shipping build without re-checking this. */
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

// Identify cluster callback. This device has no separate identify effect
// (no second LED to flash), so it just logs.
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

// Called for every attribute update. Only the OnOff attribute is of
// interest here; app_driver_attribute_update() ignores everything else.
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
    ABORT_APP_ON_FAILURE(light_handle != nullptr, ESP_LOGE(TAG, "Failed to initialize the WS2812 driver"));

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    on_off_light::config_t light_config;
    light_config.on_off.on_off = DEFAULT_POWER;
    light_config.on_off_lighting.start_up_on_off = nullptr;

    endpoint_t *endpoint = on_off_light::create(node, &light_config, ENDPOINT_FLAG_NONE, light_handle);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create on/off light endpoint"));

    light_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "On/off light created with endpoint_id %d", light_endpoint_id);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    /* Set OpenThread platform config. This, plus sdkconfig.defaults.esp32h2
     * (CONFIG_OPENTHREAD_ENABLED=y, CONFIG_ENABLE_WIFI_STATION=n), is what
     * makes this a Thread-only build. The H2 has no Wi-Fi radio, so there is
     * nothing to additionally disable there; this is the only transport. */
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
