# Backlog

Single work queue for the project. Newest decisions at the top of each section.
Items move out of here into `docs/` or code when they are actually started.

## The one thing blocking fabrication

**Leg diameter and exact leg span on the TYWE2L.** Not given in the datasheet.
The footprint assumption must be caliper-verified against a physical part before
anything goes to fabrication. Getting this wrong means a board that will not
seat. Nothing else on this page stops a board being cut.

## Documents

| Document | What it settles |
| --- | --- |
| `hardware/DESIGN.md` | Carrier geometry, support BOM, schematic rationale |
| `docs/pin-mapping-v2.md` | **The host pin mapping. Authority.** Screening of all ten candidates, the GPIO4 veto, the analogue argument, errata, leg order |
| `docs/h2-strapping-and-reset-states.md` | ESP32-H2 strapping and per-pin reset states. Authority, not re-derived elsewhere |
| `docs/ledc-erratum.md` | LEDC-253 run to ground: v0.0 and v0.1 only, fixed in v1.2, no pin dimension |
| `docs/devkit-bringup.md` | Bench prototyping on an ESP32-H2-DevKitM-1, including the prototyping pin set and the leg prober firmware |
| `docs/flasher-and-registry.md` | Flashing transport, web installer architecture, profile schema, registry design |
| `docs/tasmota-import.md` | Importing Tasmota templates as device profiles, and what does not survive the trip |
| `docs/matter-identity-and-certification.md` | VID/PID, attestation, the certification wall |
| `software/matter-onoff-poc/README.md` | Matter-over-Thread feasibility PoC on the DevKitM-1's own LED, not the TYWE2L device |
| `docs/provisioning/README.md` | Index for the build-to-labelled-unit provisioning flow, stage by stage |
| `docs/provisioning/01-firmware-build-and-flash.md` | Toolchain setup, build, `esp-matter-mfg-tool` batch identity generation, flash, and every gotcha hit doing it for real |
| `tools/tasmota-import/README.md` | Converter specification and output schema |

## First target device

**Connect SmartHome downlight, CSH-240RGB10W family. Owner has 100 or more,
bought as a single batch, confirmed all TYWE2L.** This is the anchor device and
the first firmware variant should serve it.

- **Only legs 1 and 2 are connected**, plus leg 6 (GND) and leg 7 (3V3). Legs 3,
  4 and 5 are not wired in the host at all. Two PWM channels, not five.
- Under the v2 mapping that is ESP8285 GPIO14 → H2 **GPIO13** (leg 1) and
  ESP8285 GPIO12 → H2 **GPIO14** (leg 2). Note these are precisely the two legs
  caught by the naming crossover, which makes
  [`docs/pin-mapping-v2.md`](docs/pin-mapping-v2.md) section 5.4's alternative
  leg order more attractive than when it was first assessed.
- Firmware must drive the three unconnected legs as outputs held low so they do
  not float. This is a profile matter, not a board change.
- **The blakadder template for CSH-240RGB10W does NOT match these units.** It
  assigns PWM1 to PWM5 across all five GPIOs for a full RGBCCT light. The units
  in hand wire two channels. Treat that page as a different variant and do not
  import it for this device.
- **OPEN: what the two channels drive.** Most likely cold white and warm white,
  making it a Matter Color Temperature Light (0x010C). Settle it by metering
  legs 1 and 2 through the range on the existing app, or by reading `Template`
  from any unit already running Tasmota, or with the leg prober in
  [`docs/devkit-bringup.md`](docs/devkit-bringup.md). The Matter device type and
  the profile schema both depend on the answer.

## Decided and in progress

- **Carrier board schematic** (KiCad 9). Option A geometry: 15.0 mm wide carrier,
  MINI-1 antenna overhangs the top edge into free air, 0.6 mm PCB for a 3.0 mm
  stack. See `hardware/DESIGN.md`.
- **DevKitM-1 bring-up guide** so the prototype can be wired before any PCB
  exists. See `docs/devkit-bringup.md`. Note that a stock DevKitM-1 cannot carry
  the v2 mapping in full, because GPIO13 and GPIO14 are taken by the on-board
  32.768 kHz crystal, so the guide specifies a separate bench pin set. That is
  deliberate and costs the discovery work nothing.
- **Tasmota template import** feasibility and converter specification. See
  `docs/tasmota-import.md`.
- **Matter-over-Thread on/off light PoC**, not build-tested, targeting the DevKitM-1's own onboard
  LED rather than the TYWE2L device, to test toolchain and hardware feasibility before more design
  effort goes into the real firmware. See `software/matter-onoff-poc/README.md`.

## Settled decisions (do not re-litigate)

- **Host pin mapping: v2.** Legs 1 to 5 go to H2 **GPIO13, GPIO14, GPIO12,
  GPIO10, GPIO11** (module pins 12, 13, 16, 20, 21). Adopted, signed off, and
  applied to the schematic, `hardware/DESIGN.md` and the docs.
  **`docs/pin-mapping-v2.md` is the authority.** Summary of why, kept because
  the reasoning is the valuable part:
  - v1 put the legs on GPIO1 to GPIO5. Two of them, GPIO2 and GPIO3, are
    **boot-mode strapping pins** (TRM section 8.2.2; `GPIO_STRAP_REG` bits 0
    and 1 in Register 6.7). That is what forced the review.
  - **GPIO4 is vetoed outright.** Its MTCK after-reset pull-up is present on
    every factory part and lasts 200 to 400 ms, from reset release through app
    init, which is long enough to energise a relay or a MOSFET gate. Since no
    one yet knows what any leg does, every leg is potentially the actuator line,
    so the per-leg concern becomes a per-pin veto.
  - **v1's stated reason was wrong at its root.** It spent all five legs on
    ADC1_CH0 to CH4 to "keep ADC on every leg". The ESP8266EX has exactly one
    analogue input, TOUT, a dedicated input-only pin that the TYWE2L does not
    break out. **No host can ever present analogue on a leg**, so the constraint
    that drove v1 never existed. All five ADC channels moved to the spare field,
    where the carrier can actually use one.
  - Gained along the way: all five host lines are **LP pins**, so any of them
    can wake the chip from deep sleep, which none could under v1; and GPIO11 is
    the analogue comparator input, a hardware zero-cross capability the ESP8285
    never had.
  - **Naming hazard.** ESP8285 GPIO12/13/14 are leg nets, H2 GPIO12/13/14 are
    module pins, and under v2 they cross: ESP8285 GPIO14 goes to H2 GPIO13, and
    ESP8285 GPIO12 goes to H2 GPIO14. Always say which chip a number belongs to.
    The schematic keeps `HOST_GPIOxx` and `H2_IOxx` separate for this reason.
  - **New hard constraint: never feed VBAT from anything other than 3V3.**
    GPIO12, GPIO13 and GPIO14 sit on the VDDA_PMU/VBAT domain rather than
    VDDPST1, so a separate VBAT supply would give three host lines different
    logic thresholds and output swings from the other two. The module ties VBAT
    to 3V3 through a 0 Ω link and the carrier leaves it NC, so it is inert as
    built, and it must stay that way. On the schematic sheet next to the
    existing VBAT no-connect note.
  - **Falsifier, stated plainly:** if anyone finds a TYWE2L-based device that
    presents an analogue signal on one of the five legs, this mapping is unsafe
    for that device and must be revisited.
- Matter identity: **test VID 0xFFF1**, SDK test DACs, PID in 0x8000-0x801F.
  Architect for migration to a real VID at zero cost now (reserve
  `esp_secure_cert` 0x2000 and `fctry` 0x6000, set
  `CONFIG_CHIP_FACTORY_NAMESPACE_PARTITION_LABEL=fctry`, per-device
  discriminator and passcode from day one, burn no eFuses).
- Flashing tool: **pure web installer on GitHub Pages**, esptool-js >= 0.6.1
  used directly rather than via esp-web-tools. No native desktop app, no code
  signing spend.
- RF re-certification exposure of swapping the radio: **accepted**. Project is
  published open source as a design, not sold as a product. Documented in
  `LICENSE.md`.
- Licensing: CERN-OHL-S-2.0 hardware, Apache-2.0 software, CC-BY-4.0 docs,
  CC0-1.0 registry profiles.

## Backlogged

### Programming jig (deferred, agreed)
A small companion PCB carrying a USB-C socket and a 7-way 2.0 mm socket that the
carrier plugs into, wired to GPIO26/GPIO27 (native USB Serial/JTAG), EN and
GPIO9. Lets people flash a board on the bench with no adapter and no driver
install, which research identified as the single biggest reduction in support
burden. There is no room for a USB-C connector on the 15 x 17 mm carrier itself,
so this has to be a separate board. Test pads on the carrier remain the in-situ
fallback in the meantime.

**Why deferred:** the carrier has to exist first, and the pad positions the jig
mates with are not fixed until the carrier layout is done.

### Firmware dump analysis
Determine what each TYWE2L leg actually does in the target device by examining a
firmware dump from the original module. Deferred by decision until the hardware
is testable. This is what turns a pin map into a working device profile.

## Open questions

- **Leg diameter and exact leg span on the TYWE2L.** Still open, and it is the
  standing blocker. Restated at the top of this file because it is the only item
  that stops a board being fabricated.
- **CSA Associate membership at USD 0/yr: can it obtain a Vendor ID?** Sources
  conflict. If yes, it removes the USD 7,500/yr Adopter fee from the migration
  path entirely and changes the whole certification calculus. Worth an hour of
  someone's time.
- **Antenna keepout extent** for ESP32-H2-MINI-1, from Espressif's ESP32-H2
  Hardware Design Guidelines rather than the datasheet. Datasheet-derived figure
  is 5.4 mm, which forces a 13.6 mm carrier and a 19.0 mm assembly.
- **Leg order versus routing.** `docs/pin-mapping-v2.md` section 5.4 sets out an
  alternative leg-to-pin assignment that removes the ESP8285-versus-H2 naming
  hazard at the cost of two trace crossings. It is a genuine choice and it
  belongs to the project owner. The mapping above is the default until it is
  overridden.

## NEVER BURN THESE eFUSES

eFuses are irreversible. Burning the wrong one bricks the board's usefulness or
creates a hardware contention path. The flashing tool must never burn any eFuse
automatically, and this list must be enforced in code, not just documented.

- **`EFUSE_DIS_USB_JTAG`** is the dangerous one. With `DIS_PAD_JTAG` = 0 and
  `DIS_USB_JTAG` = 1, the chip is forced into pad JTAG and the GPIO25 strap is
  ignored entirely, permanently seizing GPIO2 to GPIO5 and making MTDO a chip
  *output*. **Under v1 those four were host lines**, so this was a contention and
  damage path into a host-driven net. **Under v2 all four are spares**, which
  demotes it to losing four test pads. That is a real improvement and it is not a
  licence: it is still irreversible, and it is still wrong. If debug lockdown is
  ever wanted, **both JTAG eFuses must be burnt together, never singly.**
- **Matter attestation eFuses.** Burning these forecloses the migration path
  from test VID 0xFFF1 to a real Vendor ID.

## Silicon revision constraints

- **Pin the production spec to rev v1.2 or later.** Erratum ADC-7227 kills
  ADC1_CH4 on pre-v1.2 parts, and LEDC-253 restricts PWM duty range on v0.0 and
  v0.1. Both are fixed in v1.2. Sort incoming modules by the `MF XXXX` marking
  to confirm revision on receipt, since second-sourced stock may be older.
  Enforce it in the build with `CONFIG_ESP32H2_REV_MIN_102`, which makes the
  bootloader refuse to start on anything older. Setting the Kconfig is the
  guarantee; the marking check is the receiving-dock version of it.
  **Under v2, ADC-7227 no longer bears on the pin mapping at all**, only on the
  production specification: GPIO5 is a spare pad rather than a host line, and no
  leg uses the ADC. It still matters if a future revision puts an analogue input
  on a spare.
- **Firmware rule that costs nothing:** run LEDC at 12 to 13-bit duty
  resolution, never `SOC_LEDC_TIMER_BIT_WIDTH` (20). This keeps even a
  second-sourced v0.1 part safe from LEDC-253 with no visible quality loss.
  LEDC-253 needs no open question of its own: `docs/ledc-erratum.md` closed it.
  Affected on v0.0 and v0.1, fixed in v1.2, and it has no pin dimension, since
  LEDC on ESP32-H2 routes purely through the GPIO matrix with no per-pin
  restriction. Any of GPIO10 to GPIO14 can carry any of the six channels, in any
  assignment, changeable in firmware without a board change.
- **Clamp `PAD_DRIVER` on the five host lines.** The H2 default drive is
  `DRV` = 2, roughly 20 mA, which is already above the ESP8285's 12 mA I_MAX.
  Firmware should reduce it rather than leave it at the default.
  `docs/pin-mapping-v2.md` section 2.9.
- **Erratum ECDSA_DS-836 affects v1.2 and has no fix scheduled.** Signatures
  with invalid r and s values are incorrectly accepted. The stated workaround is
  to use RSA_DS Secure Boot rather than ECDSA_DS Secure Boot. This needs its own
  investigation before the Matter secure boot scheme is locked down. Out of
  scope for the carrier design, but it must not be forgotten.

## Resolved (kept as the record of what was decided and why)

- ~~VBAT (pin 15) external tie~~. Internally tied to 3V3 by default (Table 3,
  p.10). Left unconnected with an NC flag.
- ~~GPIO25 pull direction~~. Pull **up**. Note that in factory state
  (`EFUSE_JTAG_SEL_ENABLE` = 0) the GPIO25 column is marked "Ignored", so the
  resistor is insurance rather than the load-bearing protection. The actual boot
  protection comes from the GPIO8 pull-up. Do not read R4 as a stray pull-up or
  a depopulation candidate.
- ~~ESP32-H2 strapping pin count~~. **Five, not three. Closed.** GPIO2 and GPIO3
  have dedicated latch bits in `GPIO_STRAP_REG` (Register 6.7, bit 0 and bit 1)
  and TRM section 8.2.2 states their reset values determine boot mode alongside
  GPIO8 and GPIO9. The chip and module datasheets say three and are wrong on
  this point; ESP-IDF's GPIO reference said five all along and was right. Note
  the shape of the argument, because it is reusable: Register 6.7 enumerates
  every bit and marks bits 5 to 15 invalid, so an exhaustive register bit map is
  a much stronger negative than an absent table row. This invalidated the
  original GPIO1-GPIO5 pin mapping and produced v2. No bench test is needed and
  the one `docs/devkit-bringup.md` used to prescribe has been withdrawn. See
  `docs/h2-strapping-and-reset-states.md` and `docs/pin-mapping-v2.md`.
- ~~Whether any TYWE2L leg can carry an analogue signal~~. **No, and it never
  could.** The ESP8266EX has one analogue input, TOUT, which is a dedicated
  input-only pin and is not brought out on the TYWE2L. This is why v2 does not
  keep ADC on the legs, and it retrospectively removes the entire justification
  v1 was built on. `docs/pin-mapping-v2.md`, "Why v1 had to change".
- ~~What real TYWE2L devices do with the legs~~. **In the visible community
  record, PWM light channels and nothing else.** Across `blakadder/templates`
  and `esphome/esphome-devices`, every TYWE2L device is a light: two channels on
  ESP8285 GPIO12 and GPIO14 for CCT, or all five for RGBCCT. Not one uses a leg
  for a relay, button, LED, TuyaMCU, I2C, one-wire, IR, counter or zero-cross.
  This is a lower bound, not a census, because the `chip:` field is only
  populated when a contributor filled it in, so the worst-case discipline stands
  and the firmware dump analysis under Backlogged is still needed. What it
  changes is where
  the importer's effort belongs: PWM is the code that must be flawless.
  `docs/pin-mapping-v2.md` section 2.7.
- ~~Whether all four JTAG pads carry GPIO4's reset pull-up~~. No. TRM Table
  6.13-1 gives MTMS, MTDO and MTDI reset code 1 (input enabled, no pull), and
  only MTCK gets a pull. The deviation from the ESP8285 is one line, not four.

## Known risks carried forward

- **esptool-js has no reconnect logic** and the device re-enumerates mid-flash.
  The web app state machine must handle port reacquisition from day one, not as
  a later fix.
- **Google Home hard-blocks test-cert devices** unless each user registers the
  device in their own Developer Console project. Does not scale. Document the
  limitation prominently rather than pretending otherwise.
- **Home Assistant tightened in June 2026.** The matter.js-based server refuses
  test-cert devices by default. There is an opt-in setting that must be
  documented at the top of the user guide.
- **Snap and Flatpak packaged browsers are sandboxed** and cannot reach serial
  ports at all, with no obvious diagnosis for the user. Needs a detection and a
  clear error message in the web installer.
- **Wrong pin maps are a physical hazard.** Registry profiles drive relays in
  mains appliances. Safe boot pin states, profile checksums and a recovery mode
  belong in the firmware, not just in review process. v2 adds a specific way to
  get one wrong: the ESP8285 and H2 GPIO namespaces both contain 12, 13 and 14,
  and they cross over. Validate on pad number, which equals the leg number, and
  derive the H2 number from a single table rather than transcribing it.
