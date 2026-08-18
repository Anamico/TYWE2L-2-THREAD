/*
   5-channel LEDC PWM driver (R, G, B, cool white, warm white) plus the
   DevKitM-1's onboard WS2812 (GPIO8) as a visual stand-in for what those 5
   channels represent, both wired to the Matter Extended Color Light cluster.

   This is the "emulate the 5 LEDC PWM channels on the onboard LED" bench
   step: none of the 5 GPIOs below have anything physically wired to them
   yet (no Connect Smarthome host board is present for this step) -- the
   point is proving the Matter Extended Color Light cluster and HomeKit's
   colour picker drive the right values end to end, using only what's
   already on the DevKitM-1. See hardware/smarthome/test-circuit.md, which
   this project reuses these exact 5 GPIO assignments from, and
   hardware/smarthome/README.md for why this device is RGB + cool/warm
   white (5 channels), not a simpler 3-channel RGB light.

   WS2812B is RGB-only -- it has no separate white die -- so cool/warm white
   is approximated by biasing the RGB output toward blue-white or amber
   rather than shown accurately. That's intentional: this step is for
   sanity-checking that a colour/temperature command arrived and moved the
   output in a sane direction, not for judging colour accuracy by eye.

   Adapted from the structure of espressif/esp-matter's
   examples/light/main/app_driver.cpp (Apache-2.0), and from this project's
   own matter-onoff-poc/main/app_driver.cpp for the WS2812/led_strip half.
   Talks to espressif/led_strip directly rather than esp-matter's
   led_driver/device_hal abstraction, for the same reason matter-onoff-poc
   does: that layer's led_driver_get_config() supplies Espressif's own
   esp32h2_devkit_c reference board's pin defaults, not this board's.

   NOT BUILD-TESTED. Written in a sandbox with no ESP-IDF/esp-matter
   toolchain available (confirmed earlier in this project's history), so it
   could not be compiled or run here. The esp_matter_attr_val_t field access
   pattern (val->val.b / .u8 / .u16) follows matter-onoff-poc's own working,
   build-tested use of val->val.b for the OnOff attribute; the .u8/.u16
   extensions for Level/Hue/Saturation/ColorTemperatureMireds follow the same
   union convention but are not independently confirmed against source here.
   Expect the first build to surface at least one problem, same as
   matter-onoff-poc's own README already sets the expectation for a first
   build of a new project in this family.
*/

#include <esp_err.h>
#include <esp_log.h>

#include <driver/ledc.h>
#include <led_strip.h>

#include <esp_matter.h>
#include <app_priv.h>

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;

/* Confirmed on the DevKitM-1 v1.3 schematic: WS2812B (D6) on GPIO8. See
 * docs/devkit-bringup.md section 2 and matter-onoff-poc/main/app_driver.cpp,
 * which this is copied from unchanged. */
#define ONBOARD_LED_GPIO   8
#define ONBOARD_LED_COUNT  1
#define WS2812_RMT_RESOLUTION_HZ (10 * 1000 * 1000)

/* The 5 channel GPIOs, from hardware/smarthome/test-circuit.md's "H2 GPIO
 * assignment" table -- the DevKitM-1's already-vetted prototyping pin set
 * (docs/devkit-bringup.md section 1.2), reused here for the same reason
 * that table reuses it: no strapping pins, not on the crystal, not
 * UART0/USB/WS2812, and no legacy leg-order constraint applies since this
 * isn't going onto a compatible module footprint. Arbitrary assignment
 * among the five. On this bench step nothing is physically wired to them. */
#define GPIO_CHAN_R   GPIO_NUM_1
#define GPIO_CHAN_G   GPIO_NUM_12
#define GPIO_CHAN_B   GPIO_NUM_10
#define GPIO_CHAN_CW  GPIO_NUM_11  /* cool white */
#define GPIO_CHAN_WW  GPIO_NUM_22  /* warm white */

#define LEDC_CHAN_R   LEDC_CHANNEL_0
#define LEDC_CHAN_G   LEDC_CHANNEL_1
#define LEDC_CHAN_B   LEDC_CHANNEL_2
#define LEDC_CHAN_CW  LEDC_CHANNEL_3
#define LEDC_CHAN_WW  LEDC_CHANNEL_4

#define LEDC_FREQ_HZ        5000            /* above audible range, well clear of flicker */
#define LEDC_DUTY_RES       LEDC_TIMER_8_BIT /* 0-255, matches Matter's 0-254 attribute scale closely enough for this PoC */
#define LEDC_DUTY_MAX       255

/* Matter's ColorControl reports which of hue/saturation or colour
 * temperature last drove the output (ColorMode / EnhancedColorMode
 * attributes). Tracked locally rather than re-read from the attribute
 * store on every change, mirroring matter-onoff-poc's pattern of small
 * local state rather than a full data-model round trip per update. */
enum class color_mode_t {
    HUE_SATURATION,
    COLOR_TEMPERATURE,
};

/* All state needed to recompute every output channel from scratch. Matches
 * espressif/esp-matter examples/light's own pattern of keeping small local
 * "current value" state per attribute rather than re-reading the data model
 * on every recompute. */
typedef struct {
    bool          on;
    uint8_t       level;             /* 0-254, CurrentLevel */
    uint8_t       hue;                /* 0-254, CurrentHue */
    uint8_t       saturation;         /* 0-254, CurrentSaturation */
    uint16_t      color_temp_mireds;  /* ColorTemperatureMireds */
    color_mode_t  color_mode;
} light_state_t;

static light_state_t s_state = {
    .on = DEFAULT_POWER,
    .level = DEFAULT_BRIGHTNESS,
    .hue = DEFAULT_HUE,
    .saturation = DEFAULT_SATURATION,
    .color_temp_mireds = DEFAULT_COLOR_TEMP_MIREDS,
    .color_mode = color_mode_t::COLOR_TEMPERATURE,
};

/* Matter hue is 0-254 (not 0-360), saturation/value 0-254. Standard HSV->RGB,
 * scaled to that range throughout rather than converting to degrees/percent
 * and back. */
static void hsv_to_rgb(uint8_t hue, uint8_t saturation, uint8_t value, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (saturation == 0) {
        *r = *g = *b = value;
        return;
    }

    uint32_t region    = (uint32_t)hue * 6;       /* 0..~1530 */
    uint8_t  sextant    = region / 254;             /* 0..5 (6 at the hue=254 boundary, handled by `default` below) */
    uint32_t remainder  = (region - (uint32_t)sextant * 254) * 6; /* fractional position within the sextant */

    /* uint32_t throughout: saturation * remainder alone can reach ~254 *
     * 1524 =~ 387000, well past uint16_t's 65535 ceiling. */
    uint8_t p = (uint8_t)(((uint32_t)value * (255 - saturation)) / 255);
    uint8_t q = (uint8_t)(((uint32_t)value * (255 - ((uint32_t)saturation * remainder) / 1524)) / 255);
    uint8_t t = (uint8_t)(((uint32_t)value * (255 - ((uint32_t)saturation * (1524 - remainder)) / 1524)) / 255);

    switch (sextant) {
    case 0: *r = value; *g = t;     *b = p;     break;
    case 1: *r = q;     *g = value; *b = p;     break;
    case 2: *r = p;     *g = value; *b = t;     break;
    case 3: *r = p;     *g = q;     *b = value; break;
    case 4: *r = t;     *g = p;     *b = value; break;
    default: *r = value; *g = p;    *b = q;     break;
    }
}

/* Colour temperature -> a cool/warm-white blend, scaled by brightness.
 * Range chosen to match typical Matter light bulb limits (153 mireds =
 * ~6500K cool, 500 mireds = ~2000K warm) -- not sourced from this specific
 * device's own capability, since the real host board's CW/WW LED spectral
 * output was never measured (see hardware/smarthome/README.md open items). */
#define MIREDS_COOL 153
#define MIREDS_WARM 500

static void mireds_to_cw_ww(uint16_t mireds, uint8_t brightness, uint8_t *cw, uint8_t *ww)
{
    if (mireds < MIREDS_COOL) mireds = MIREDS_COOL;
    if (mireds > MIREDS_WARM) mireds = MIREDS_WARM;

    /* 0.0 at the cool end, 1.0 at the warm end. */
    uint32_t warmth_pct = ((uint32_t)(mireds - MIREDS_COOL) * 100) / (MIREDS_WARM - MIREDS_COOL);

    *ww = (uint8_t)(((uint32_t)brightness * warmth_pct) / 100);
    *cw = (uint8_t)(((uint32_t)brightness * (100 - warmth_pct)) / 100);
}

static void ledc_apply(ledc_channel_t chan, uint8_t duty)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, chan, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, chan);
}

/* Recomputes every output (5 LEDC channels + the WS2812 approximation) from
 * s_state and applies them. Called after any attribute change rather than
 * patching a single channel, since colour-mode changes require every
 * channel to be re-derived together (e.g. switching from CT to HS mode
 * needs the CW/WW channels to drop to 0, not just the RGB channels to
 * rise). */
static esp_err_t apply_state(led_strip_handle_t strip)
{
    uint8_t r = 0, g = 0, b = 0, cw = 0, ww = 0;

    if (s_state.on) {
        if (s_state.color_mode == color_mode_t::HUE_SATURATION) {
            hsv_to_rgb(s_state.hue, s_state.saturation, s_state.level, &r, &g, &b);
        } else {
            mireds_to_cw_ww(s_state.color_temp_mireds, s_state.level, &cw, &ww);
        }
    }

    ledc_apply(LEDC_CHAN_R,  r);
    ledc_apply(LEDC_CHAN_G,  g);
    ledc_apply(LEDC_CHAN_B,  b);
    ledc_apply(LEDC_CHAN_CW, cw);
    ledc_apply(LEDC_CHAN_WW, ww);

    /* WS2812 approximation. In HS mode this is an exact match (WS2812 is
     * RGB, same as the R/G/B channels). In CT mode there's no white die to
     * light, so cw/ww are folded into an RGB approximation: cool biases
     * toward blue-white, warm toward amber, scaled by how much of each is
     * present. Not colour-accurate by design -- see the file header. */
    uint8_t ws_r = r, ws_g = g, ws_b = b;
    if (s_state.color_mode == color_mode_t::COLOR_TEMPERATURE) {
        /* uint32_t intermediates: cw/ww are each up to 255, so e.g.
         * 255*200 + 255*255 ~= 116000 would overflow a uint16_t. */
        ws_r = (uint8_t)(((uint32_t)cw * 200 + (uint32_t)ww * 255) / 255);
        ws_g = (uint8_t)(((uint32_t)cw * 220 + (uint32_t)ww * 170) / 255);
        ws_b = (uint8_t)(((uint32_t)cw * 255 + (uint32_t)ww * 80)  / 255);
    }

    esp_err_t err = led_strip_set_pixel(strip, 0, ws_r, ws_g, ws_b);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_set_pixel failed: %d", err);
        return err;
    }
    return led_strip_refresh(strip);
}

static esp_err_t ledc_channels_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE, /* the H2, like other post-ESP32 chips, only has low-speed LEDC */
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %d", err);
        return err;
    }

    struct { ledc_channel_t chan; gpio_num_t gpio; } channels[] = {
        { LEDC_CHAN_R,  GPIO_CHAN_R },
        { LEDC_CHAN_G,  GPIO_CHAN_G },
        { LEDC_CHAN_B,  GPIO_CHAN_B },
        { LEDC_CHAN_CW, GPIO_CHAN_CW },
        { LEDC_CHAN_WW, GPIO_CHAN_WW },
    };

    for (auto &c : channels) {
        ledc_channel_config_t chan_cfg = {
            .gpio_num   = c.gpio,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = c.chan,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = 0,
            .hpoint     = 0,
        };
        err = ledc_channel_config(&chan_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_channel_config failed for GPIO%d: %d", (int)c.gpio, err);
            return err;
        }
    }
    return ESP_OK;
}

app_driver_handle_t app_driver_light_init()
{
    esp_err_t err = ledc_channels_init();
    if (err != ESP_OK) {
        return NULL;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = ONBOARD_LED_GPIO,
        .max_leds = ONBOARD_LED_COUNT,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = WS2812_RMT_RESOLUTION_HZ,
    };

    led_strip_handle_t strip = NULL;
    err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
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
        s_state.on = val->val.b;
    } else if (cluster_id == LevelControl::Id && attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
        s_state.level = val->val.u8;
    } else if (cluster_id == ColorControl::Id && attribute_id == ColorControl::Attributes::CurrentHue::Id) {
        s_state.hue = val->val.u8;
        s_state.color_mode = color_mode_t::HUE_SATURATION;
    } else if (cluster_id == ColorControl::Id && attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
        s_state.saturation = val->val.u8;
        s_state.color_mode = color_mode_t::HUE_SATURATION;
    } else if (cluster_id == ColorControl::Id && attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
        s_state.color_temp_mireds = val->val.u16;
        s_state.color_mode = color_mode_t::COLOR_TEMPERATURE;
    } else {
        return ESP_OK;
    }

    return apply_state((led_strip_handle_t)driver_handle);
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    led_strip_handle_t strip = (led_strip_handle_t)priv_data;

    attribute_t *on_off_attr = attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    esp_matter_attr_val_t val;
    attribute::get_val(on_off_attr, &val);
    s_state.on = val.val.b;

    attribute_t *level_attr = attribute::get(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    if (level_attr) {
        attribute::get_val(level_attr, &val);
        s_state.level = val.val.u8;
    }

    attribute_t *mireds_attr = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id);
    if (mireds_attr) {
        attribute::get_val(mireds_attr, &val);
        s_state.color_temp_mireds = val.val.u16;
    }

    return apply_state(strip);
}
