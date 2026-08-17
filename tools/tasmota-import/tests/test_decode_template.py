#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
#
# Tests for decode_template.py. Run with: python3 -m pytest tools/tasmota-import/tests
# or plain: python3 tools/tasmota-import/tests/test_decode_template.py
#
# The three "ground truth" templates are the owner's own devices, read off Tasmota 8.2.0
# "Configure Template" screenshots in hardware/tasmota/. Tasmota renders the function names
# itself in those screenshots, so they are a verified decode rather than an inference, and
# they are the strongest check we have that the tables and the label rules are right.

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

TOOL_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_DIR))

import decode_template  # noqa: E402


def build_decoder():
    tables = json.loads(decode_template.find_tables(None).read_text(encoding="utf-8"))
    board = json.loads(decode_template.DEFAULT_BOARD.read_text(encoding="utf-8"))
    return decode_template.Decoder(tables, board)


DECODER = build_decoder()


def decode(raw: str) -> dict:
    return DECODER.decode(raw)


def by_pin(result: dict) -> dict[str, str]:
    """Map "GPIO4" -> "Button3", i.e. exactly what Tasmota's UI shows."""
    return {pin["esp8285_pin"]: pin["label"] for pin in result["pins"]}


class TestLegacyGroundTruth(unittest.TestCase):
    """hardware/tasmota/EnsuiteSwitch.png, Bath1.png, Bed2Switch.png, all Tasmota 8.2.0."""

    def test_ensuite_switch_matches_the_screenshot(self):
        result = decode(
            '{"NAME":"EnsuiteSwitch","GPIO":[255,255,56,255,19,18,0,0,22,21,23,255,17],'
            '"FLAG":0,"BASE":18}'
        )
        self.assertEqual(result["encoding"], "legacy")
        self.assertEqual(result["base_name"], "Generic")
        self.assertEqual(
            by_pin(result),
            {
                "GPIO0": "User",
                "GPIO1": "User",
                "GPIO2": "Led1i",
                "GPIO3": "User",
                "GPIO4": "Button3",
                "GPIO5": "Button2",
                "GPIO9": "None",
                "GPIO10": "None",
                "GPIO12": "Relay2",
                "GPIO13": "Relay1",
                "GPIO14": "Relay3",
                "GPIO15": "User",
                "GPIO16": "Button1",
                "ADC0": "None",
            },
        )
        # Three relays, three buttons, one inverted status LED.
        roles = [pin["role"] for pin in result["pins"] if pin["assigned"]]
        self.assertEqual(roles.count("relay"), 3)
        self.assertEqual(roles.count("button"), 3)
        self.assertEqual(roles.count("led"), 1)

    def test_bath1_matches_the_screenshot(self):
        result = decode('{"NAME":"Bath1","GPIO":[0,0,0,0,37,40,0,0,38,41,39,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["encoding"], "legacy")
        self.assertEqual(
            {pin["esp8285_pin"]: pin["label"] for pin in result["pins"] if pin["assigned"]},
            {
                "GPIO4": "PWM1",
                "GPIO5": "PWM4",
                "GPIO12": "PWM2",
                "GPIO13": "PWM5",
                "GPIO14": "PWM3",
            },
        )

    def test_bed2switch_matches_the_screenshot(self):
        result = decode('{"NAME":"Bed2Switch","GPIO":[17,0,0,0,0,0,0,0,21,56,0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(
            {pin["esp8285_pin"]: pin["label"] for pin in result["pins"] if pin["assigned"]},
            {"GPIO0": "Button1", "GPIO12": "Relay1", "GPIO13": "Led1i"},
        )
        # Led1i is inverted, which is why the profile's per-channel active level matters.
        led = next(pin for pin in result["pins"] if pin["esp8285_pin"] == "GPIO13")
        self.assertEqual(led["code"], "GPIO_LED1_INV")
        self.assertIn("active low", led["description"])


class TestEncodingDetection(unittest.TestCase):
    def test_legacy_detected_by_tasmotas_own_test(self):
        result = decode('{"NAME":"x","GPIO":[0,0,0,0,0,0,0,0,21,17,0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["encoding"], "legacy")
        self.assertIn("13 entries", result["encoding_detected_by"])

    def test_modern_fourteen_entry_template(self):
        # Tasmota's own "New:" example for the Shelly 2.5, from the comment above
        # JsonTemplate() in tasmota/tasmota_support/support.ino.
        result = decode(
            '{"NAME":"Shelly 2.5","GPIO":[320,0,32,0,224,193,0,0,640,192,608,225,3456,4736],'
            '"FLAG":0,"BASE":18}'
        )
        self.assertEqual(result["encoding"], "modern")
        self.assertEqual(by_pin(result)["GPIO4"], "Relay1")
        self.assertEqual(by_pin(result)["GPIO13"], "Switch1n")

    def test_legacy_and_modern_forms_of_one_device_agree(self):
        legacy = decode(
            '{"NAME":"Shelly 2.5","GPIO":[56,0,17,0,21,83,0,0,6,82,5,22,156],"FLAG":2,"BASE":18}'
        )
        modern = decode(
            '{"NAME":"Shelly 2.5","GPIO":[320,0,32,0,224,193,0,0,640,192,608,225,3456,4736],'
            '"FLAG":0,"BASE":18}'
        )
        self.assertEqual(legacy["encoding"], "legacy")
        self.assertEqual([pin["raw"] for pin in legacy["pins"]], [pin["raw"] for pin in modern["pins"]])

    def test_thirteen_entries_with_a_large_value_is_not_legacy(self):
        # Array length alone is not the rule: Tasmota's test is 13 entries AND every value
        # below 256, so this one is read as modern.
        result = decode('{"NAME":"x","GPIO":[0,0,0,0,224,0,0,0,320,0,0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["encoding"], "modern")
        self.assertEqual(by_pin(result)["GPIO4"], "Relay1")

    def test_modern_value_one_means_user(self):
        result = decode('{"NAME":"x","GPIO":[1,1,1,1,224,1,1,1,32,1,1,1,1,1],"FLAG":0,"BASE":18}')
        self.assertEqual(by_pin(result)["GPIO0"], "User")
        self.assertFalse(next(p for p in result["pins"] if p["esp8285_pin"] == "GPIO0")["assigned"])


class TestVerdicts(unittest.TestCase):
    def test_bath1_is_compatible_and_maps_all_five_legs(self):
        result = decode('{"NAME":"Bath1","GPIO":[0,0,0,0,37,40,0,0,38,41,39,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["verdict"], decode_template.VERDICT_COMPATIBLE)
        self.assertEqual(result["blockers"], [])
        mapping = {leg["leg"]: (leg["esp8285_gpio"], leg["h2_gpio"], leg["label"]) for leg in result["leg_mapping"]}
        self.assertEqual(
            mapping,
            {
                1: (14, 13, "PWM3"),
                2: (12, 14, "PWM2"),
                3: (13, 12, "PWM5"),
                4: (5, 10, "PWM4"),
                5: (4, 11, "PWM1"),
            },
        )

    def test_unavailable_pin_is_incompatible(self):
        result = decode(
            '{"NAME":"EnsuiteSwitch","GPIO":[255,255,56,255,19,18,0,0,22,21,23,255,17],'
            '"FLAG":0,"BASE":18}'
        )
        self.assertEqual(result["verdict"], decode_template.VERDICT_INCOMPATIBLE)
        self.assertTrue(any("GPIO16" in blocker for blocker in result["blockers"]))
        self.assertEqual(result["leg_mapping"], [])
        self.assertTrue(result["leg_mapping_suppressed"])

    def test_test_pad_only_is_needs_test_pads(self):
        result = decode('{"NAME":"Bed2Switch","GPIO":[17,0,0,0,0,0,0,0,21,56,0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["verdict"], decode_template.VERDICT_TEST_PADS)
        self.assertEqual(result["blockers"], [])
        self.assertTrue(any("GPIO0" in caution for caution in result["cautions"]))
        self.assertEqual(result["unused_legs"], [1, 4, 5])

    def test_legacy_flag_puts_a_function_on_adc0(self):
        result = decode('{"NAME":"x","GPIO":[0,0,0,0,0,0,0,0,21,17,0,0,0],"FLAG":2,"BASE":18}')
        adc0 = result["pins"][13]
        self.assertEqual(adc0["code"], "GPIO_ADC_TEMP")
        self.assertEqual(result["verdict"], decode_template.VERDICT_INCOMPATIBLE)

    def test_tuya_mcu_base_is_rejected_even_with_no_pins(self):
        # The classic trap: it uses no pins at all, so it passes every pin check.
        result = decode('{"NAME":"Tuya","GPIO":[0,0,0,0,0,0,0,0,0,0,0,0,0],"FLAG":0,"BASE":54}')
        self.assertFalse(result["servable"])
        self.assertTrue(any("Tuya MCU" in blocker for blocker in result["firmware_blockers"]))


class TestDriverArchitecture(unittest.TestCase):
    """A device can fit the board perfectly and still need different firmware."""

    def test_five_channel_pwm_is_pwm_direct(self):
        result = decode('{"NAME":"Bath1","GPIO":[0,0,0,0,37,40,0,0,38,41,39,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["driver_architecture"], "pwm_direct")
        self.assertEqual(result["pwm_channels"], 5)
        self.assertEqual(result["driver_chips"], [])
        self.assertTrue(result["servable"])

    def test_legacy_sm2135_is_a_two_wire_led_driver_not_pwm(self):
        # Legacy 180 and 181 are SM2135 Clk and Dat. Both exist in Tasmota 8.2.0, which is
        # what the owner's devices run.
        result = decode('{"NAME":"x","GPIO":[0,0,0,0,181,180,0,0,0,0,0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["driver_architecture"], "two_wire_led_driver")
        self.assertEqual(result["driver_chips"], ["SM2135"])
        self.assertEqual(result["pwm_channels"], 0)
        # Pin-compatible, and still not servable, which is the whole point of the split.
        self.assertEqual(result["verdict"], decode_template.VERDICT_COMPATIBLE)
        self.assertFalse(result["servable"])
        self.assertEqual(result["blockers"], [])
        self.assertTrue(result["firmware_blockers"])

    def test_legacy_my92x1_and_sm16716_are_recognised(self):
        my92 = decode('{"NAME":"x","GPIO":[0,0,0,0,143,144,0,0,0,0,0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(my92["driver_chips"], ["MY92x1"])
        sm16716 = decode('{"NAME":"x","GPIO":[0,0,0,0,140,141,0,0,142,0,0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(sm16716["driver_chips"], ["SM16716"])

    def test_modern_only_driver_chips_are_recognised_too(self):
        # BP5758D exists only in the modern enum, so this can only arrive on Tasmota 9.1.0
        # or later. Ordinals 262 and 263 at the pinned commit.
        shift = DECODER.tables["agpio_shift"]
        clk, dat = 262 << shift, 263 << shift
        result = decode(
            f'{{"NAME":"x","GPIO":[0,0,0,0,{clk},{dat},0,0,0,0,0,0,0,0],"FLAG":0,"BASE":18}}'
        )
        self.assertEqual(result["driver_chips"], ["BP5758D"])

    def test_relay_only_device_is_gpio_direct(self):
        result = decode('{"NAME":"x","GPIO":[17,0,0,0,0,0,0,0,21,56,0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["driver_architecture"], "gpio_direct")
        self.assertEqual(result["relay_count"], 1)
        self.assertEqual(result["button_count"], 1)


class TestLegacyDriverCodeInventory(unittest.TestCase):
    """The complete set of two-wire LED driver codes reachable in the legacy encoding.

    Tasmota 8.2.0 cannot express BP5758D, SM2235, SM2335, BP1658CJ or P9813 at all: they
    were added after the 8-bit enum was frozen. If a template captured on 8.2.0 shows a
    driver at all, it can only be one of these three families.
    """

    LEGACY_DRIVER_CODES = {
        140: "GPIO_SM16716_CLK",
        141: "GPIO_SM16716_DAT",
        142: "GPIO_SM16716_SEL",
        143: "GPIO_DI",
        144: "GPIO_DCKI",
        180: "GPIO_SM2135_CLK",
        181: "GPIO_SM2135_DAT",
    }

    def test_every_legacy_driver_code_converts_to_the_expected_modern_code(self):
        for legacy_value, expected in self.LEGACY_DRIVER_CODES.items():
            packed = DECODER.gpio_convert(legacy_value)
            entry = DECODER.describe_code(packed)
            self.assertEqual(entry["code"], expected, f"legacy {legacy_value}")
            self.assertEqual(entry["role"], "led_driver", f"legacy {legacy_value}")

    def test_no_other_legacy_value_is_a_two_wire_driver(self):
        found = set()
        for legacy_value in range(256):
            packed = DECODER.gpio_convert(legacy_value)
            try:
                entry = DECODER.describe_code(packed)
            except decode_template.TemplateError:
                continue
            if entry["role"] == "led_driver":
                found.add(legacy_value)
        self.assertEqual(found, set(self.LEGACY_DRIVER_CODES))


class TestMalformedInput(unittest.TestCase):
    def test_not_json(self):
        result = decode("not json at all")
        self.assertEqual(result["verdict"], decode_template.VERDICT_ERROR)

    def test_gpio_not_a_list(self):
        result = decode('{"NAME":"x","GPIO":"nope","FLAG":0,"BASE":18}')
        self.assertEqual(result["verdict"], decode_template.VERDICT_ERROR)

    def test_wrong_array_length_is_refused(self):
        result = decode('{"NAME":"x","GPIO":[0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["verdict"], decode_template.VERDICT_ERROR)
        self.assertIn("3 entries", result["errors"][0])

    def test_esp32_length_array_is_refused_rather_than_guessed(self):
        result = decode('{"NAME":"x","GPIO":' + json.dumps([0] * 36) + ',"FLAG":0,"BASE":1}')
        self.assertEqual(result["verdict"], decode_template.VERDICT_ERROR)

    def test_unknown_function_ordinal_is_a_hard_failure(self):
        # Ordinal 1500 does not exist in either arm of the enum. It must fail, never be
        # degraded into a free pin (docs/tasmota-import.md section 1.7).
        result = decode('{"NAME":"x","GPIO":[48000,0,0,0,0,0,0,0,0,0,0,0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["verdict"], decode_template.VERDICT_ERROR)
        self.assertIn("ordinal", result["errors"][0])

    def test_declared_esp32_architecture_is_refused(self):
        result = decode('{"NAME":"x","ARCH":"ESP32","GPIO":[0,0,0,0,0,0,0,0,0,0,0,0,0],"FLAG":0,"BASE":18}')
        self.assertEqual(result["verdict"], decode_template.VERDICT_ERROR)


class TestTheEsp32Trap(unittest.TestCase):
    """docs/tasmota-import.md section 1.3: the mistake that produced confident nonsense."""

    def test_tables_carry_both_arms_and_they_differ(self):
        tables = DECODER.tables
        self.assertEqual(tables["gpio_sensor_end"]["esp8266"], 358)
        self.assertEqual(tables["gpio_sensor_end"]["esp32"], 385)
        self.assertEqual(len(tables["esp32_only_names"]), 27)
        self.assertEqual(tables["first_divergent_ordinal"], 154)
        self.assertIn("GPIO_KEY1_PD", tables["esp32_only_names"])

    def test_low_ordinals_are_identical_on_both_arms(self):
        tables = DECODER.tables
        for name in ("GPIO_KEY1", "GPIO_REL1", "GPIO_LED1", "GPIO_PWM1", "GPIO_LEDLNK"):
            self.assertEqual(
                tables["user_selectable_pins"]["esp8266"][name],
                tables["user_selectable_pins"]["esp32"][name],
            )

    def test_a_high_ordinal_decodes_differently_on_the_two_arms(self):
        # GPIO_INPUT is 222 on ESP8266 and 239 on ESP32. Decoding an ESP8285 template with
        # the ESP32 table would name a different function entirely.
        tables = DECODER.tables
        self.assertEqual(tables["user_selectable_pins"]["esp8266"]["GPIO_INPUT"], 222)
        self.assertEqual(tables["user_selectable_pins"]["esp32"]["GPIO_INPUT"], 239)
        packed = 222 << tables["agpio_shift"]
        result = decode(f'{{"NAME":"x","GPIO":[0,0,0,0,{packed},0,0,0,0,0,0,0,0,0],"FLAG":0,"BASE":18}}')
        pin = result["pins"][4]
        self.assertEqual(pin["code"], "GPIO_INPUT")
        self.assertIn("esp32_table_would_say", pin)
        self.assertNotEqual(pin["esp32_table_would_say"], "GPIO_INPUT")


class TestJsonMode(unittest.TestCase):
    def test_json_output_is_parseable_and_carries_provenance(self):
        import io
        import contextlib

        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer):
            code = decode_template.main(
                ["--json", '{"NAME":"Bath1","GPIO":[0,0,0,0,37,40,0,0,38,41,39,0,0],"FLAG":0,"BASE":18}']
            )
        self.assertEqual(code, 0)
        payload = json.loads(buffer.getvalue())
        self.assertEqual(payload[0]["verdict"], decode_template.VERDICT_COMPATIBLE)
        self.assertEqual(len(payload[0]["tasmota_commit"]), 40)
        self.assertEqual(payload[0]["pin_mapping_version"], 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
