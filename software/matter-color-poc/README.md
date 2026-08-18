# Matter-over-Thread Extended Color Light PoC (ESP32-H2-DevKitM-1)

The "emulate the 5 LEDC PWM channels on the onboard LED" bench step: validates the Matter
Extended Color Light cluster and HomeKit's colour picker end to end, entirely on the DevKitM-1,
before any Connect Smarthome retrofit hardware exists. Drives 5 LEDC PWM channels (R, G, B, cool
white, warm white — nothing physically wired to them yet on this step) and the DevKitM-1's own
onboard WS2812 LED (GPIO8) as a visual stand-in for what those 5 channels represent.

**This is not the TYWE2L-leg-mapped device**, and it is a different retrofit target from
`software/matter-onoff-poc/`. It has nothing to do with the carrier board, the v2 pin mapping, or
`docs/pin-mapping-v2.md`. It targets the **Connect Smarthome CSH-240RGB10W** teardown documented
in `hardware/smarthome/README.md` and `hardware/smarthome/test-circuit.md` — a separate product
line from the Mirabello/TYWE2L devices. Once this validates the cluster/colour-picker behaviour,
the same LEDC channel setup is what the real retrofit firmware builds on, wired to the actual
host board pads instead of left unconnected.

**Extends, does not replace, `software/matter-onoff-poc/`.** That project is this repo's
build-tested first milestone (commit `89c07ac`) proving Matter-over-Thread works on this chip at
all, kept as-is as a working reference. This project un-trims the colour/brightness/level control
that `matter-onoff-poc`'s own README documents as deliberately cut from Espressif's
`examples/light` for that minimal first smoke test — see "What was cut" there for that history.

## What this proves, and what it does not

**Proves, if it builds and commissions:** a Thread-only Matter **Extended Color Light** (hue/
saturation and colour temperature, not just on/off) builds and flashes on this chip; it
commissions into a Matter fabric over BLE and joins Thread; Apple Home's colour and colour-
temperature pickers drive real attribute changes that reach the firmware; those changes visibly
move the DevKitM-1's onboard LED and would drive 5 real PWM channels if something were connected.

**Does not prove:** anything about the actual Connect Smarthome retrofit circuit (see
`hardware/smarthome/test-circuit.md` for that — this project assumes no host board is attached),
colour accuracy (the WS2812 approximation is explicitly not colour-accurate, see
`main/app_driver.cpp`), real Matter certification (test identity, see below), or OTA updates.

## Status: not build-tested

**This project has not been built, flashed, or run on real hardware.** Unlike
`matter-onoff-poc` (which was checked against `espressif/esp-matter`'s actual `examples/light`
source directly), this project was written in a sandbox with **no access to the
esp-matter/connectedhomeip source tree at all** — extrapolated from `matter-onoff-poc`'s own
already-working, build-tested patterns (the `on_off` / `on_off_lighting` config split, the
`led_strip`-direct driver approach) rather than independently re-verified against upstream
source. The `level_control` / `color_control` config field names in `main/app_main.cpp` are the
least-confident part of this project — flagged inline there and in "Open questions" below, and
worth checking against `esp-matter/examples/light/main/app_main.cpp` before trusting them.
**Expect the first build to surface at least one problem**, likely in that specific area. Please
report back whatever `idf.py build` prints so it can be fixed.

## Prerequisites

1. **ESP-IDF v5.5.x or v6.0.x, installed via EIM** (ESP-IDF Installation Manager) on your own
   machine. Two different pages on Espressif's own site disagree on which to use for Matter work
   as of this writing: esp-matter's top-level README currently recommends **v6.0.2**, but the
   esp32h2-specific esp-matter programming guide currently recommends **v5.5.5**. Neither
   supersedes the other cleanly and this project has not been built against either. Start with
   whichever EIM offers as current v5.5.x or v6.0.x; if the build fails in a way that looks
   version-related, try the other. `docs/devkit-bringup.md` in this repo already flags this same
   moving target for the H2's chip revision requirements (v1.2 silicon needs v5.1.6/v5.2.5/
   v5.3.3/v5.4.1/v5.5+ depending on branch) — check `idf.py --version` against both that table
   and esp-matter's current README before assuming either number is still right.
2. **`espressif/esp-matter`, cloned separately, with its `connectedhomeip` submodule**, and its
   `export.sh` sourced so `ESP_MATTER_PATH` is set. This project's `CMakeLists.txt` requires that
   variable — it is not optional, and there is currently no way to build an esp-matter project
   without it (the component-manager-only path, `idf.py add-dependency "espressif/esp_matter"`,
   does not include the example device-HAL/cmake plumbing this project's build still depends on).
   Roughly:
   ```bash
   git clone --depth 1 https://github.com/espressif/esp-matter.git
   cd esp-matter
   git submodule update --init --depth 1
   cd connectedhomeip/connectedhomeip
   ./scripts/checkout_submodules.py --platform esp32 linux --shallow
   cd ../..
   ./install.sh
   cd ..
   ```
   Then in every new shell, in this order:
   ```bash
   cd esp-idf   && source ./export.sh && cd ..
   cd esp-matter && source ./export.sh && cd ..
   ```
   This step is real work — the connectedhomeip submodule tree is large. Not attempted in the
   environment that wrote this PoC; sourced from esp-matter's own "Getting the Repositories"
   docs page, not from a completed run.
3. **A Thread Border Router already on your network** — a HomePod mini, HomePod, or Apple TV
   4K. A phone alone cannot commission a Thread device; it needs a border router to hand the
   device a Thread network to join. If you don't already have one of these, commissioning will
   silently stall or fail in a way that looks like a firmware problem but isn't.
4. The DevKitM-1, connected over its **`UART` port (J2, the CP2102N)**, not the native `USB`
   port — see `docs/devkit-bringup.md` section 3 for why, driver notes, and how to tell the two
   USB-C ports apart if the silkscreen is worn.

## Building

From this directory (`software/matter-color-poc`), with both `export.sh` scripts already
sourced:

```bash
idf.py set-target esp32h2
idf.py build
```

`set-target` only needs to run once; it also picks up `sdkconfig.defaults.esp32h2` automatically
and merges it over `sdkconfig.defaults`.

## Flashing

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Substitute the port: `/dev/ttyUSB0` on Linux, `/dev/cu.usbserial-XXXX` (or
`/dev/cu.SLAB_USBtoUART`) on macOS, `COM5` or similar on Windows — see
`docs/devkit-bringup.md` section 3. If auto-reset into download mode misbehaves, add `-b 115200`
to slow the handshake, or use manual download mode (hold BOOT, press and release RESET, release
BOOT) per that same doc's section 5. `idf.py monitor` exits with `Ctrl+]`.

## What the console should show

Right when `Commissioning window opened` prints, the console now also prints a highlighted block
with this specific unit's own pairing code, computed live at that moment rather than hardcoded:

```
================================================================
==                                                            ==
==   PAIRING CODE -- enter manually in the Home app:          ==
==                                                            ==
==       3497-011-2332                                        ==
==                                                            ==
==   QR payload: MT:Y.K9042C00KA0648G00
==                                                            ==
================================================================
```

This is added in `main/app_main.cpp`'s `print_commissioning_codes()`, called from the
`kCommissioningWindowOpened` case in `app_event_cb`. It calls connectedhomeip's own
`GetQRCode()`/`GetManualPairingCode()` (`<setup_payload/OnboardingCodesUtil.h>`, the same
reference implementation upstream's own examples use), which read the discriminator and passcode
live from `GetCommissionableDataProvider()` — **not** compile-time constants. On this test build
that resolves to the same fixed values as before (see below), but on a unit provisioned with
`esp-matter-mfg-tool`'s own unique per-device discriminator/passcode (see
`docs/provisioning/01-firmware-build-and-flash.md`), the exact same firmware image prints *that*
unit's own code instead. **Not build-tested** — the API surface was verified against
connectedhomeip's actual current source (not memory), but the exact printed formatting/line
wrapping has not been seen on a real console.

## Diagnostic: OpenThread CLI for RSSI/counters during a stalled pairing

Currently **on** in `sdkconfig.defaults.esp32h2` (`CONFIG_OPENTHREAD_CLI=y`) — added specifically
to dig into a live commissioning failure: a fabric commits successfully, then a second (Home Hub)
fabric-add round times out with the controller going silent for its full 30-second fail-safe
window, no error ever logged on the device side. Application-layer CHIP logs alone can't tell
"the controller never sent the next command" apart from "something got silently dropped in the
Thread mesh below the layer that would log an error" — this gives a way to check.

Right when `Commissioning window opened` prints (same event as the pairing code above),
`start_openthread_cli_once()` in `main/app_main.cpp` starts an interactive console on the same
UART. Type commands **bare, with no `ot ` prefix** — this uses `esp_openthread_cli_create_task()`
(a dedicated CLI task reading its own prompt loop), not the alternate integration mode that
registers a single prefixed command into an existing `esp_console` REPL; this project doesn't run
one of those, so the dedicated-task mode is the simpler, self-contained fit. Verified against
ESP-IDF v6.0.2's actual `components/openthread/src/esp_openthread_cli.c` and
`include/esp_openthread_cli.h` source, not memory or a general web search summary (an earlier
search suggested a "prefix mechanism" that turned out to only apply to the *other* integration
mode).

Useful commands after a failed pairing attempt:

```
state              # Thread role: leader / router / child / detached
neighbor table      # every Thread neighbor, incl. LQIn / AvgRssi / LastRssi columns
parent              # same signal info, specifically for whichever device this attached through
counters            # MAC and IP RX/TX/drop counts on the Thread radio itself
```

If `neighbor table`/`parent` RSSI looks weak, that's a real link-quality lead — try moving the
board physically closer to the Border Router (HomePod mini/HomePod/Apple TV) and retry pairing.
If `counters`' RX count climbs during a stall despite nothing showing in the application log,
something is arriving and being silently dropped at a layer below Matter's own logging — a
different, real bug. If RX stays flat through the stall, that's evidence nothing reached the
device at all during that window, which points back at the controller rather than this firmware
or the Thread network.

**Not build-tested.** The two functions this calls (`esp_openthread_cli_init()`,
`esp_openthread_cli_create_task()`) and their required call order relative to
`esp_matter::start()` were verified by reading their actual current source and header docstring,
not run. The startup point (`kCommissioningWindowOpened`) is empirically safe rather than
API-guaranteed safe: a real boot log from this project, this session, showed
`OpenThread started: OK` well before that event fires, but there's no documented "OpenThread is
ready" callback exposed to application code to hook instead. Diagnostic-only — reconsider before
carrying this into a real shipping build (extra ~4KB task stack, extra flash, and a second thing
reading the same UART as normal log output, which can visually interleave with log lines).

## Commissioning codes for this build

This build never overrides the discriminator or passcode (`CONFIG_ENABLE_TEST_SETUP_PARAMS=y`,
the SDK's own test defaults), so these values are constant across every boot and every unit built
from this exact source — not per-device or per-boot generated, since nothing in `sdkconfig`
provisions a unique identity yet. Confirmed against Espressif's esp-matter docs and independently
checked with a connectedhomeip manual-code parser, and now also what the console itself prints at
boot (see above):

- **Manual pairing code: `34970112332`** (displayed hyphenated as `3497-011-2332`, standard 4-3-4 grouping)
- **QR code payload: `MT:Y.K9042C00KA0648G00`**
- Discriminator: `3840`, Passcode: `20202021`, VID `0xFFF1`, PID `0x8000`

Use the manual pairing code in Apple Home's "Enter Code Manually" flow (see below) — no QR display
needed.

## Commissioning into Apple Home

1. Open the Home app, add accessory, scan the QR code (or enter the manual pairing code) from
   the console.
2. **Expect an "Uncertified Accessory" prompt. Tap "Add Anyway."** This is documented, expected
   behaviour for a device built on the CHIP SDK's default test credentials (test VID `0xFFF1`),
   not a bug — see `docs/matter-identity-and-certification.md` section 2.3. Every open-source
   Matter project (Tasmota, matter.js, Matterbridge, this SDK's own sample apps) ships the same
   way.
3. It should commission over BLE, join the Thread network via your border router, and appear as
   a colour light. Toggling on/off, dragging the brightness slider, picking a colour, and picking
   a colour temperature should all visibly change the DevKitM-1's onboard LED — see
   `main/app_driver.cpp` for exactly how each maps (colour picks drive the LED directly in RGB;
   colour-temperature picks are approximated, since WS2812 has no separate white die).

## Matter identity: deliberately using the stock test VID/PID, not a custom one

This build does **not** override `CONFIG_DEVICE_VENDOR_ID` or `CONFIG_DEVICE_PRODUCT_ID` — it
ships on the CHIP SDK's own defaults, **VID `0xFFF1`, PID `0x8000`** (confirmed from
`connectedhomeip`'s `config/esp32/components/chip/Kconfig`, "Device Identification Options").

An earlier version of this plan intended to pick a distinct test PID in the reserved
`0x8000`–`0x801F` range to avoid clashing with other test devices on the same Thread network
(Tasmota uses `0x8000`, the SDK's own default sample apps use `0x8001`). **That turned out to be
the wrong call and was reversed.** The reason: the device's declared PID (a Basic Information
cluster attribute) and the PID baked into its Device Attestation Certificate have to match — a
commissioner checks VID/PID consistency between the CD, the DAC and the Basic Information
cluster (`docs/matter-identity-and-certification.md` section 1.4). Changing only the Kconfig PID
without also generating and flashing a matching DAC via `esp-matter-mfg-tool` would create that
mismatch and could fail commissioning outright — a real risk to introduce on an unbuilt,
unverified first attempt. `docs/matter-identity-and-certification.md` independently confirms
this is also just what every comparable open-source project does: Tasmota, matter.js,
Matterbridge, home-assistant-matter-hub and ioBroker.matter all ship on the stock `0xFFF1` VID
and a stock PID, customising only the human-readable vendor *name* string, never the numeric
IDs, for exactly this reason.

If a distinct PID is wanted later (e.g. once this boots and the next step is provisioning real
per-device identity), the settled path is `esp-matter-mfg-tool` generating a matching PAI/DAC
pair into the `fctry` partition this project's `partitions.csv` already reserves — not a
Kconfig-only change. Not attempted here; out of scope for a first smoke test.

## What's still cut from Espressif's own `examples/light`, and why

This project reuses `matter-onoff-poc`'s build boilerplate (top-level `CMakeLists.txt`,
`sdkconfig.defaults`, `partitions.csv`), not Espressif's `examples/light` directly, and restores
that project's `endpoint::extended_color_light` cluster set (this project's whole point). Still
cut, same reasoning as `matter-onoff-poc`:

- **The BOOT-button factory-reset handler** — the real example uses `iot_button` +
  `app_reset_button_register()` from esp-matter's `examples/common`, which pulls in the
  `button_gpio` component and a per-board `button_driver_get_config()`. Left out to keep the
  first build's dependency surface smaller. Between commissioning attempts, use
  `idf.py -p PORT erase-flash` instead of a button press.
- **esp-matter's own `led_driver`/`device_hal` abstraction layer** — that layer's
  `led_driver_get_config()` supplies GPIO defaults for Espressif's own `esp32h2_devkit_c`
  reference board, which is not the board this project targets (the DevKitM-1). Rather than
  assume the two boards' WS2812 wiring coincides, `main/app_driver.cpp` calls
  `espressif/led_strip` directly, hardcoded to **GPIO8**, sourced from this repo's own
  schematic-verified `docs/devkit-bringup.md` rather than from Espressif's reference board.
- **OTA requestor, encrypted OTA, the CHIP console shell, memory profiling** — none needed to
  answer "does Thread/Matter work on this chip."
- **`esp_matter_bridge` compiles even though this PoC never calls into it**, and that was the
  cause of the first real build failure: this component lives under `esp-matter/components/`, on
  this project's component search path, and ESP-IDF pulls in everything it finds there
  unconditionally — there is no `EXCLUDE_COMPONENTS` mechanism in ESP-IDF's build system (checked
  directly against the current CMake build-system docs after a first attempt to add one silently
  did nothing), and the alternative, `COMPONENTS` as an allowlist, would mean hand-enumerating this
  entire dependency graph, which is worse. `esp_matter_bridge`'s `MAX_BRIDGED_DEVICE_COUNT` macro
  (`CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT - 1 [root endpoint] -
  CONFIG_ESP_MATTER_AGGREGATOR_ENDPOINT_COUNT`) is sized at compile time regardless of whether the
  component is used, and went negative at this project's original
  `CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT=1` (upstream's own default is 16). Since
  `CONFIG_ESP_MATTER_AGGREGATOR_ENDPOINT_COUNT`'s own Kconfig range is 1–5 (can't be lowered to 0),
  the actual fix is in `sdkconfig.defaults`: `CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT=3`, the
  minimum that keeps that unused component's arithmetic non-negative (root + mandatory minimum
  aggregator + one spare). See the comment there for the full derivation.

## Open questions / assumptions worth checking before or during the first build

- **`level_control` / `level_control_lighting` / `color_control` field names in
  `main/app_main.cpp`'s `extended_color_light::config_t` setup.** The highest-risk unverified
  part of this project — see that file's inline comment. Written by extrapolating
  `matter-onoff-poc`'s proven `on_off` / `on_off_lighting` sibling-struct pattern onto
  `extended_color_light::config_t`, with no esp-matter source available to check against in the
  sandbox that wrote this. Also uncertain: whether the enum is `ColorControl::ColorModeEnum::
  kColorTemperatureMireds` (used here) or an older `ColorControl::ColorMode::...` naming without
  the `Enum` suffix — connectedhomeip has used both conventions across versions.
- **`esp_matter_attr_val_t` field names for non-bool attributes** (`val->val.u8`, `val->val.u16`)
  in `main/app_driver.cpp`. `val->val.b` for the OnOff attribute is proven (matter-onoff-poc uses
  it in a working build); the `.u8`/`.u16` extensions for Level/Hue/Saturation/
  ColorTemperatureMireds follow the same union convention but aren't independently confirmed.
- **The mireds-to-Kelvin range (153–500) in `app_driver.cpp`'s `mireds_to_cw_ww()`** is a generic
  Matter light bulb range, not sourced from this specific device's actual CW/WW LED spectral
  output, which was never measured (see `hardware/smarthome/README.md` open items).
- **ESP-IDF version conflict** (v6.0.2 vs v5.5.5) noted above under Prerequisites — genuinely
  unresolved, both sourced from Espressif's own current pages.
- **The `esp32h2_devkit_c` fallback device path in `CMakeLists.txt`.** The build system requires
  *some* `ESP_MATTER_DEVICE_PATH` to resolve for `esp32h2`; this project points it at
  Espressif's own DevKitC reference board purely so the build has a valid path to include, since
  it's the only H2 entry in the upstream table. This project's own code never calls into it — no
  `<device.h>`, no `led_driver_get_config()` — so a mismatch there shouldn't matter, but this
  hasn't been proven by an actual successful build.
- **WS2812 RMT resolution (10 MHz)** — copied from `device_hal/led_driver/ws2812/led_driver.c`
  as a known-good value for this same LED part on other H2 boards, not independently verified.
- **Console onboarding-code log format** — described generically above, not verified verbatim.
- If `idf.py build` fails on a missing component or unresolved symbol, the most likely cause is
  a version skew between the pinned `espressif/led_strip: "^3.0.0"` dependency in
  `main/idf_component.yml` and whatever ESP-IDF version ends up in use — check the component's
  registry page for the ESP-IDF versions it supports.

## Sources

- `software/matter-onoff-poc/` (this repo, commit `89c07ac`) — the build-tested project this one
  extends. Its build boilerplate is reused verbatim; its `on_off`/`on_off_lighting` config
  pattern and `val->val.b` attribute-access pattern are the proven foundation the less-certain
  parts of this project extrapolate from. **Unlike that project, this one was not checked against
  `espressif/esp-matter`'s actual source** — no esp-matter/connectedhomeip checkout was available
  in the environment that wrote it. See "Open questions" above for exactly what that affects.
- `espressif/idf-extra-components`, `led_strip` — for the `led_strip_new_rmt_device` /
  `led_strip_config_t` / `led_strip_rmt_config_t` API, unchanged from `matter-onoff-poc`'s already
  build-tested use of it.
- This repo: `hardware/smarthome/README.md` and `hardware/smarthome/test-circuit.md` (the Connect
  Smarthome teardown this project's 5-channel layout and GPIO assignment come from),
  `docs/devkit-bringup.md` (onboard WS2812 on GPIO8), and
  `docs/matter-identity-and-certification.md` (test VID/PID convention, unchanged from
  `matter-onoff-poc`).
