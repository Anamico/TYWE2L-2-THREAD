# Matter-over-Thread on/off light PoC (ESP32-H2-DevKitM-1)

A minimal feasibility test: does Matter-over-Thread actually work on this chip, on this board,
end to end — build, flash, commission into Apple Home, toggle the light? Turning the Matter
On/Off cluster on or off lights or kills the DevKitM-1's own onboard WS2812 LED (GPIO8).

**This is not the TYWE2L-leg-mapped device.** It has nothing to do with the carrier board, the
v2 pin mapping, or `docs/pin-mapping-v2.md`. It only drives the DevKitM-1's own onboard LED, to
prove the Thread/Matter approach is viable on the ESP32-H2 before more design effort goes into
the real device. Once this works, the same pattern (an on/off or dimmable-light endpoint,
Thread-only, test VID) is what the real TYWE2L replacement firmware would build on — but wired
to the actual PWM legs, not this LED.

## What this proves, and what it does not

**Proves, if it builds and commissions:** the ESP-IDF + esp-matter toolchain works for this
chip; a Thread-only (no Wi-Fi, since the H2 has no Wi-Fi radio) Matter On/Off Light builds and
flashes; it commissions into a Matter fabric over BLE and joins Thread; Apple Home can control
it end to end.

**Does not prove:** anything about the TYWE2L leg mapping, the carrier board, real Matter
certification (this uses test identity, see below), or OTA updates (not included in this build).

## Status: not build-tested

**This project has not been built, flashed, or run on real hardware.** It was assembled in an
environment with no ESP-IDF toolchain available, from Espressif's current documented patterns
and the actual source of `espressif/esp-matter`'s `examples/light` (fetched and read directly,
not reconstructed from memory) — not guessed. Specifics that could not be verified against a
real build are called out inline as comments and in "Open questions" below. **Expect the first
build to surface at least one problem.** Please report back whatever `idf.py build` prints so it
can be fixed — that report is exactly what this PoC is for.

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

From this directory (`software/matter-onoff-poc`), with both `export.sh` scripts already
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
   a light. Toggling it on/off in the Home app should light or kill the DevKitM-1's onboard LED
   (white when on).

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

## What was cut from Espressif's own `examples/light`, and why

This project is a trimmed adaptation of `espressif/esp-matter`'s `examples/light`
(`extended_color_light`, full RGB + brightness + colour temperature), not a from-scratch
reinvention — reusing the proven build boilerplate (top-level `CMakeLists.txt`,
`sdkconfig.defaults`, `partitions.csv`) rather than hand-rolling it was a deliberate choice to
reduce the number of new things that could be wrong on a first build. Cut for this PoC:

- **Colour, brightness, level control** — this is `endpoint::on_off_light`, not
  `endpoint::extended_color_light`. Only the OnOff cluster is wired to anything.
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

- `espressif/esp-matter`, `examples/light/` (`app_main.cpp`, `app_driver.cpp`, `app_priv.h`,
  `CMakeLists.txt`, `sdkconfig.defaults`, `sdkconfig.defaults.esp32h2`, `partitions.csv`,
  `idf_component.yml`) — read directly from the repository, Apache-2.0.
- `espressif/esp-matter`, `device_hal/led_driver/ws2812/led_driver.c` and
  `components/esp_matter/data_model/legacy/esp_matter_endpoint_impl.h` (for the
  `endpoint::on_off_light` API) — read directly, Apache-2.0.
- `espressif/idf-extra-components`, `led_strip/include/led_strip_rmt.h` — for the
  `led_strip_new_rmt_device` / `led_strip_config_t` / `led_strip_rmt_config_t` API.
- `project-chip/connectedhomeip`, `config/esp32/components/chip/Kconfig` — for the
  `DEVICE_VENDOR_ID` / `DEVICE_PRODUCT_ID` / `ENABLE_TEST_SETUP_PARAMS` defaults.
- This repo: `docs/devkit-bringup.md` (onboard WS2812 on GPIO8, UART port identification, ESP-IDF
  version table) and `docs/matter-identity-and-certification.md` (test VID/PID convention,
  attestation consistency requirement, partition layout).
