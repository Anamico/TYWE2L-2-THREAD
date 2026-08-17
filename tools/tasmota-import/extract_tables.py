#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
#
# extract_tables.py: build the Tasmota decode tables used by decode_template.py.
#
# This tool reads and reproduces decode tables from Tasmota (arendst/Tasmota), which is
# GPL-3.0. See docs/tasmota-import.md section 6 for the licence discussion. The tables it
# writes are checked into this repository so that decoding is reproducible offline and so
# that the exact Tasmota commit behind every decode is recorded.
#
# What it parses, per tools/tasmota-import/README.md section 3.1:
#
#   enum UserSelectablePins        tasmota/include/tasmota_template.h
#   enum LegacyUserSelectablePins  tasmota/include/tasmota_template_legacy.h
#   const uint16_t kGpioConvert[]  tasmota/include/tasmota_template_legacy.h
#   enum SupportedModulesESP8266   tasmota/include/tasmota_template.h
#   const char kModuleNames[]      tasmota/include/tasmota_template.h
#
# THE TRAP. enum UserSelectablePins is one enum with inline "#ifdef ESP32" blocks in it, so
# 205 names have a different ordinal on ESP8266 than on ESP32 (docs/tasmota-import.md
# section 1.3). The enum must be walked with a real conditional stack. Stripping the "#"
# lines yields the ESP32 numbering, which decodes ESP8266 templates into names that look
# entirely plausible and are wrong. This file therefore aborts on any preprocessor
# directive it has not been taught, rather than guessing.

from __future__ import annotations

import argparse
import json
import re
import sys
import urllib.request
from datetime import date
from pathlib import Path

SOURCE_FILES = {
    "tasmota_template.h": "tasmota/include/tasmota_template.h",
    "tasmota_template_legacy.h": "tasmota/include/tasmota_template_legacy.h",
    "support.ino": "tasmota/tasmota_support/support.ino",
    "en_GB.h": "tasmota/language/en_GB.h",
}

# Directives we understand inside enum UserSelectablePins. Anything else is a hard abort.
KNOWN_DIRECTIVES = {"ESP32", "USE_MODBUS_RELAY"}

# tasmota/include/tasmota_globals.h: #define AGPIO(x) ((x)<<5)
AGPIO_SHIFT = 5


class ExtractError(Exception):
    pass


def strip_comments(line: str) -> str:
    line = re.sub(r"/\*.*?\*/", " ", line)
    line = re.sub(r"//.*$", "", line)
    return line


def read_sources(source_dir: Path | None, sha: str) -> dict[str, str]:
    out = {}
    for name, repo_path in SOURCE_FILES.items():
        if source_dir is not None:
            candidates = [source_dir / repo_path, source_dir / name]
            for candidate in candidates:
                if candidate.is_file():
                    out[name] = candidate.read_text(encoding="utf-8", errors="replace")
                    break
            else:
                raise ExtractError(f"cannot find {repo_path} under {source_dir}")
        else:
            url = f"https://raw.githubusercontent.com/arendst/Tasmota/{sha}/{repo_path}"
            with urllib.request.urlopen(url, timeout=60) as response:  # noqa: S310
                out[name] = response.read().decode("utf-8", errors="replace")
    return out


def parse_conditional_enum(text: str, enum_name: str, terminator: str):
    """Return [(member_name, frozenset(conditions_required))] in declaration order.

    Conditions are the macro names of enclosing "#ifdef" blocks. Negated conditions are
    recorded as "!MACRO". The terminator member is included in the result.
    """
    start = re.search(r"\benum\s+" + re.escape(enum_name) + r"\s*\{", text)
    if not start:
        raise ExtractError(f"enum {enum_name} not found")

    members: list[tuple[str, frozenset[str]]] = []
    stack: list[str] = []
    lines = text[start.end():].splitlines()

    for raw in lines:
        stripped = raw.strip()
        if stripped.startswith("#"):
            directive = stripped[1:].strip()
            head, _, rest = directive.partition(" ")
            head = head.strip()
            macro = rest.strip().split()[0] if rest.strip() else ""
            if head == "ifdef":
                if macro not in KNOWN_DIRECTIVES:
                    raise ExtractError(
                        f"unrecognised conditional '#ifdef {macro}' inside enum {enum_name}; "
                        "teach the extractor about it rather than guessing"
                    )
                stack.append(macro)
            elif head == "ifndef":
                if macro not in KNOWN_DIRECTIVES:
                    raise ExtractError(
                        f"unrecognised conditional '#ifndef {macro}' inside enum {enum_name}"
                    )
                stack.append("!" + macro)
            elif head == "endif":
                if not stack:
                    raise ExtractError(f"unbalanced #endif inside enum {enum_name}")
                stack.pop()
            elif head in ("else", "elif", "if"):
                raise ExtractError(
                    f"unsupported directive '#{directive}' inside enum {enum_name}; "
                    "the extractor only handles #ifdef/#ifndef/#endif"
                )
            else:
                raise ExtractError(f"unknown directive '#{directive}' inside enum {enum_name}")
            continue

        body = strip_comments(raw)
        for token in body.split(","):
            token = token.strip()
            if not token:
                continue
            token = token.rstrip("};").strip()
            if not token:
                continue
            if "=" in token:
                raise ExtractError(
                    f"enum {enum_name} member '{token}' assigns an explicit value; "
                    "positional ordinals are no longer safe"
                )
            if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", token):
                raise ExtractError(f"unparseable token '{token}' in enum {enum_name}")
            members.append((token, frozenset(stack)))
            if token == terminator:
                if stack:
                    raise ExtractError(f"{terminator} reached inside a conditional block")
                return members

    raise ExtractError(f"terminator {terminator} not found in enum {enum_name}")


def evaluate(members, defined: set[str]) -> dict[str, int]:
    """Assign ordinals for one build configuration."""
    table: dict[str, int] = {}
    ordinal = 0
    for name, conditions in members:
        keep = True
        for condition in conditions:
            if condition.startswith("!"):
                if condition[1:] in defined:
                    keep = False
            elif condition not in defined:
                keep = False
        if not keep:
            continue
        if name in table:
            raise ExtractError(f"duplicate enum member {name}")
        table[name] = ordinal
        ordinal += 1
    return table


def parse_gpio_convert(text: str, esp8266: dict[str, int], esp32_only: set[str]) -> list[int]:
    match = re.search(r"const\s+uint16_t\s+kGpioConvert\[\]\s*PROGMEM\s*=\s*\{(.*?)\n\};", text, re.S)
    if not match:
        raise ExtractError("kGpioConvert not found")
    body = "\n".join(strip_comments(line) for line in match.group(1).splitlines())

    values: list[int] = []
    for entry in body.split(","):
        entry = " ".join(entry.split())
        if not entry:
            continue
        if entry == "GPIO_NONE":
            values.append(0)
            continue
        packed = re.fullmatch(r"AGPIO\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)\s*(?:\+\s*(\d+))?", entry)
        if not packed:
            raise ExtractError(f"unparseable kGpioConvert entry '{entry}'")
        name, offset = packed.group(1), int(packed.group(2) or 0)
        if name in esp32_only:
            raise ExtractError(
                f"kGpioConvert references {name}, which exists only in the ESP32 arm of "
                "UserSelectablePins; the ESP8266 table is wrong"
            )
        if name not in esp8266:
            raise ExtractError(f"kGpioConvert references unknown name {name}")
        values.append((esp8266[name] << AGPIO_SHIFT) + offset)
    return values


def parse_defines(text: str) -> dict[str, str]:
    """Collect '#define D_SENSOR_X "Label"' from a Tasmota language file."""
    defines = {}
    for name, value in re.findall(r'^\s*#define\s+(D_[A-Z0-9_]+)\s+"((?:[^"\\]|\\.)*)"', text, re.M):
        defines.setdefault(name, value)
    return defines


def parse_sensor_names(text: str, defines: dict[str, str]) -> list[tuple[str, frozenset[str]]]:
    """Parse const char kSensorNames[], the labels Tasmota's own web UI shows.

    Returns [(label, conditions)] in declaration order, parallel to UserSelectablePins.
    The same "#ifdef ESP32" blocks appear here, so the same conditional walk applies.
    """
    start = re.search(r"const\s+char\s+kSensorNames\[\]\s*PROGMEM\s*=", text)
    if not start:
        raise ExtractError("kSensorNames not found")

    pieces: list[tuple[str, frozenset[str]]] = []
    stack: list[str] = []
    for raw in text[start.end():].splitlines():
        stripped = raw.strip()
        if stripped.startswith("#"):
            directive = stripped[1:].strip()
            head, _, rest = directive.partition(" ")
            macro = rest.strip().split()[0] if rest.strip() else ""
            if head == "ifdef":
                if macro not in KNOWN_DIRECTIVES:
                    raise ExtractError(f"unrecognised '#ifdef {macro}' inside kSensorNames")
                stack.append(macro)
            elif head == "endif":
                if not stack:
                    raise ExtractError("unbalanced #endif inside kSensorNames")
                stack.pop()
            else:
                raise ExtractError(f"unsupported directive '#{directive}' inside kSensorNames")
            continue

        body = strip_comments(raw)
        text_of_line = []
        for token in re.finditer(r'"((?:[^"\\]|\\.)*)"|([A-Za-z_][A-Za-z0-9_]*)', body):
            literal, macro = token.group(1), token.group(2)
            if literal is not None:
                text_of_line.append(literal)
            else:
                if macro not in defines:
                    raise ExtractError(f"kSensorNames references undefined macro {macro}")
                text_of_line.append(defines[macro])
        if text_of_line:
            pieces.append(("".join(text_of_line), frozenset(stack)))
        if ";" in body:
            break
    if stack:
        raise ExtractError("kSensorNames ended inside a conditional block")
    return pieces


def sensor_names_for(pieces, defined: set[str]) -> list[str]:
    joined = "".join(text for text, conditions in pieces if conditions <= defined)
    labels = joined.split("|")
    while labels and labels[-1] == "":  # the table ends with a trailing separator
        labels.pop()
    return labels


def parse_indexed_names(text: str) -> list[str]:
    """Names that Tasmota's UI shows with an instance number, from kGpioNiceList.

    An entry written 'AGPIO(GPIO_REL1) + AGMAX(MAX_RELAYS)' takes an index; a bare
    'AGPIO(GPIO_IRSEND)' does not. Preprocessor directives are deliberately ignored here,
    because this mapping is name-to-flag and no ordinal depends on it.
    """
    match = re.search(r"const\s+uint16_t\s+kGpioNiceList\[\]\s*PROGMEM\s*=\s*\{(.*?)\n\};", text, re.S)
    if not match:
        raise ExtractError("kGpioNiceList not found")
    body = "\n".join(strip_comments(line) for line in match.group(1).splitlines())
    return sorted(set(re.findall(r"AGPIO\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)\s*\+\s*AGMAX\(", body)))


def parse_module_names(text: str) -> list[dict]:
    enum_match = re.search(r"\benum\s+SupportedModulesESP8266\s*\{(.*?)\}", text, re.S)
    if not enum_match:
        raise ExtractError("enum SupportedModulesESP8266 not found")
    names = []
    for token in strip_comments(enum_match.group(1)).replace("\n", " ").split(","):
        token = token.strip()
        if token:
            names.append(token)
    if names[-1] != "MAXMODULE":
        raise ExtractError("SupportedModulesESP8266 does not end with MAXMODULE")
    names = names[:-1]

    # The first kModuleNames in the file is the ESP8266 one, immediately after the enum.
    tail = text[enum_match.end():]
    literal_match = re.search(r"const\s+char\s+kModuleNames\[\]\s*PROGMEM\s*=\s*(.*?);", tail, re.S)
    if not literal_match:
        raise ExtractError("kModuleNames not found")
    joined = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', literal_match.group(1)))
    display = joined.split("|")
    if len(display) != len(names):
        raise ExtractError(
            f"kModuleNames has {len(display)} names against {len(names)} enum members"
        )
    return [
        {"base": index + 1, "enum": enum_name, "name": display_name}
        for index, (enum_name, display_name) in enumerate(zip(names, display))
    ]


def self_test(tables: dict) -> list[str]:
    """The checks required by tools/tasmota-import/README.md section 3.1."""
    results = []
    esp8266 = tables["user_selectable_pins"]["esp8266"]
    esp32 = tables["user_selectable_pins"]["esp32"]
    convert = tables["gpio_convert"]

    legacy_end = tables["legacy_user_selectable_pins"]["GPI8_SENSOR_END"]
    if len(convert) != legacy_end:
        raise ExtractError(f"kGpioConvert has {len(convert)} entries against GPI8_SENSOR_END {legacy_end}")
    results.append(f"len(kGpioConvert) == GPI8_SENSOR_END == {legacy_end}")

    divergent = [n for n in esp8266 if n in esp32 and esp8266[n] != esp32[n]]
    if tables["first_divergent_ordinal"] != 154:
        raise ExtractError(
            "first divergent ordinal is "
            f"{tables['first_divergent_ordinal']}, expected 154 "
            "(docs/tasmota-import.md section 1.3); the enum has moved, re-check the analysis"
        )
    if not divergent or not tables["esp32_only_names"]:
        raise ExtractError(
            "the ESP8266 and ESP32 arms of UserSelectablePins came out identical, which means "
            "the '#ifdef ESP32' blocks were not honoured; see docs/tasmota-import.md section 1.3"
        )
    results.append(
        f"enum sizes: ESP8266 {tables['gpio_sensor_end']['esp8266']}, "
        f"ESP32 {tables['gpio_sensor_end']['esp32']}, "
        f"{len(tables['esp32_only_names'])} ESP32-only names, "
        f"{len(divergent)} names with a differing ordinal, "
        f"first divergence at ordinal {tables['first_divergent_ordinal']}"
    )

    # USE_MODBUS_RELAY must not shift anything: assert its members sit at the very end, so
    # that ordinals are not build-flag dependent and one table per platform is enough.
    modbus = tables["modbus_relay_ordinals_when_defined"]
    if modbus:
        if min(modbus.values()) < max(esp8266.values()):
            raise ExtractError("USE_MODBUS_RELAY members do not sit at the end of the enum")
        results.append("USE_MODBUS_RELAY members sit at the end and shift no ordinal")

    # Round-trip Tasmota's own Old/New example from the JsonTemplate() comment.
    old = [56, 0, 17, 0, 21, 83, 0, 0, 6, 82, 5, 22, 156]
    new = [320, 0, 32, 0, 224, 193, 0, 0, 640, 192, 608, 225, 3456, 4736]
    flag = 2
    converted = [convert[v] if v < len(convert) else (2047 << AGPIO_SHIFT) for v in old]
    # Adc0Convert() in support.ino: AGPIO(GPIO_ADC_INPUT + adc0 - 1). The addition happens
    # inside AGPIO(), so it selects a different *function*, not an instance index.
    adc0 = 0 if flag == 0 else (esp8266["GPIO_ADC_INPUT"] + flag - 1) << AGPIO_SHIFT
    converted.append(adc0)
    if converted != new:
        raise ExtractError(f"Shelly 2.5 round-trip failed:\n  got      {converted}\n  expected {new}")
    results.append("Shelly 2.5 Old/New example from JsonTemplate() round-trips exactly")

    for name, ordinal in (("GPIO_KEY1", 1), ("GPIO_REL1", 7), ("GPIO_LED1", 9), ("GPIO_PWM1", 13), ("GPIO_LEDLNK", 17)):
        if esp8266[name] != ordinal:
            raise ExtractError(f"{name} is ordinal {esp8266[name]}, expected {ordinal}")
    results.append("spot-check of GPIO_KEY1/REL1/LED1/PWM1/LEDLNK ordinals passed")
    return results


def build(sha: str, sha_date: str | None, source_dir: Path | None) -> dict:
    sources = read_sources(source_dir, sha)

    members = parse_conditional_enum(
        sources["tasmota_template.h"], "UserSelectablePins", "GPIO_SENSOR_END"
    )
    # Default build: USE_MODBUS_RELAY undefined, which is what docs/tasmota-import.md
    # section 1.3 measured (358 / 385 names). The four USE_MODBUS_RELAY codes sit
    # immediately before GPIO_SENSOR_END and shift nothing, which self_test() asserts
    # rather than assumes, so ordinals are not build-flag dependent either way.
    esp8266_full = evaluate(members, set())
    esp32_full = evaluate(members, {"ESP32"})
    esp8266_with_modbus = evaluate(members, {"USE_MODBUS_RELAY"})
    modbus_names = sorted(
        name for name, conditions in members if "USE_MODBUS_RELAY" in conditions
    )
    esp8266 = {k: v for k, v in esp8266_full.items() if k != "GPIO_SENSOR_END"}
    esp32 = {k: v for k, v in esp32_full.items() if k != "GPIO_SENSOR_END"}

    esp32_only = sorted(set(esp32) - set(esp8266))
    esp8266_only = sorted(set(esp8266) - set(esp32))
    divergent = sorted(
        (n for n in esp8266 if n in esp32 and esp8266[n] != esp32[n]),
        key=lambda n: esp8266[n],
    )
    first_divergent = esp8266[divergent[0]] if divergent else None

    defines = parse_defines(sources["en_GB.h"])
    sensor_pieces = parse_sensor_names(sources["tasmota_template.h"], defines)
    labels_esp8266 = sensor_names_for(sensor_pieces, set())
    labels_esp32 = sensor_names_for(sensor_pieces, {"ESP32"})
    # kSensorNames carries the four USE_MODBUS_RELAY labels unconditionally, even though the
    # enum members are behind "#ifdef USE_MODBUS_RELAY". They sit at the end, so the labels
    # still line up ordinal for ordinal with the default build.
    for arm, labels, table in (("esp8266", labels_esp8266, esp8266), ("esp32", labels_esp32, esp32)):
        if len(labels) != len(table) + len(modbus_names):
            raise ExtractError(
                f"kSensorNames has {len(labels)} labels against {len(table)} "
                f"UserSelectablePins members plus {len(modbus_names)} USE_MODBUS_RELAY "
                f"members on {arm}; the two tables have drifted apart"
            )

    legacy_members = parse_conditional_enum(
        sources["tasmota_template_legacy.h"], "LegacyUserSelectablePins", "GPI8_SENSOR_END"
    )
    legacy = evaluate(legacy_members, set())

    tables = {
        "generated_by": "tools/tasmota-import/extract_tables.py",
        "extracted": date.today().isoformat(),
        "tasmota_repo": "https://github.com/arendst/Tasmota",
        "tasmota_commit": sha,
        "tasmota_commit_date": sha_date,
        "tasmota_licence": "GPL-3.0",
        "source_files": SOURCE_FILES,
        "agpio_shift": AGPIO_SHIFT,
        "gpio_user_ordinal": 2047,
        "user_selectable_pins": {"esp8266": esp8266, "esp32": esp32},
        # The labels Tasmota's own "Configure Template" page shows, from kSensorNames plus
        # the D_SENSOR_* strings in tasmota/language/en_GB.h. The web UI inserts the
        # one-based instance number at the "_" if there is one and appends it otherwise,
        # which is how "Relay" becomes "Relay2" and "Led_i" becomes "Led1i".
        "sensor_names": {"esp8266": labels_esp8266, "esp32": labels_esp32},
        "indexed_names": parse_indexed_names(sources["tasmota_template.h"]),
        "gpio_sensor_end": {
            "esp8266": esp8266_full["GPIO_SENSOR_END"],
            "esp32": esp32_full["GPIO_SENSOR_END"],
        },
        "esp32_only_names": esp32_only,
        "esp8266_only_names": esp8266_only,
        "divergent_names": divergent,
        "first_divergent_ordinal": first_divergent,
        "build_flags_assumed": {"ESP32": False, "USE_MODBUS_RELAY": False},
        "modbus_relay_names": modbus_names,
        "modbus_relay_ordinals_when_defined": {
            name: esp8266_with_modbus[name] for name in modbus_names
        },
        "legacy_user_selectable_pins": legacy,
        "gpio_convert": parse_gpio_convert(
            sources["tasmota_template_legacy.h"], esp8266, set(esp32_only)
        ),
        "supported_modules_esp8266": parse_module_names(sources["tasmota_template.h"]),
        # ESP8266 slot -> physical GPIO, from TemplateGpios() in
        # tasmota/tasmota_support/support.ino ("if (6 == i) { j = 9; } if (8 == i) { j = 12; }").
        # Slot 13 is ADC0/TOUT, which is not a GPIO; it is recorded as -1.
        "esp8266_slot_to_gpio": [0, 1, 2, 3, 4, 5, 9, 10, 12, 13, 14, 15, 16, -1],
    }
    return tables


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Extract Tasmota decode tables for the TYWE2L template decoder."
    )
    parser.add_argument(
        "--sha",
        default="db56cd62aa455714e4a9a3043b8f835addf771a2",
        help="Tasmota commit to pin against (default: the commit docs/tasmota-import.md analysed)",
    )
    parser.add_argument("--sha-date", default=None, help="date of that commit, recorded in the output")
    parser.add_argument(
        "--source-dir",
        type=Path,
        default=None,
        help="local Tasmota checkout (or a directory holding the four source files); "
        "without it the files are fetched from raw.githubusercontent.com",
    )
    parser.add_argument("-o", "--output", type=Path, default=None, help="output path")
    args = parser.parse_args(argv)

    try:
        tables = build(args.sha, args.sha_date, args.source_dir)
        results = self_test(tables)
    except ExtractError as error:
        print(f"extract-tables failed: {error}", file=sys.stderr)
        return 1

    output = args.output or (
        Path(__file__).resolve().parent / "tables" / f"tasmota-{args.sha[:12]}.json"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(tables, indent=2, sort_keys=False) + "\n", encoding="utf-8")

    print(f"wrote {output}")
    for line in results:
        print(f"  ok: {line}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
