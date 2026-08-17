<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Pin mapping v2: the five TYWE2L host legs on ESP32-H2

**Status: ADOPTED AND IMPLEMENTED.** This SUPERSEDES the GPIO1 to GPIO5 mapping. It has been signed
off by the project owner and applied to `hardware/carrier/tywe2l-h2-carrier.kicad_sch` (host nets,
spare test-pad field, on-sheet notes) and to
[`hardware/DESIGN.md`](../hardware/DESIGN.md) sections 2, 3, 4, 5, 6 and the open items.
Every knock-on update is now applied across the repository. Nothing carries the superseded mapping.
See "Knock-on updates needed elsewhere" at the end for the record of what changed where.

Prerequisite for the LEDC material: [`docs/ledc-erratum.md`](./ledc-erratum.md), whose corrections
to section 3.3 and to open question 1 have been folded in.

**Why, in two sentences.** GPIO2 and GPIO3 are latched at reset to select a boot mode, so legs 4 and
3 of v1 were sitting on strapping pins. And the ADC1 argument that justified putting all five legs on
GPIO1 to GPIO5 was answering a need that has never existed, because the ESP8285 has no ADC on any
TYWE2L leg. Both are worked through under "Why v1 had to change", after the mapping.

Prerequisite reading: [`docs/h2-strapping-and-reset-states.md`](./h2-strapping-and-reset-states.md).
That is the authority on ESP32-H2 strapping and pin reset states and is not re-derived here.

Primary sources used, all fetched and checked rather than taken on trust: ESP32-H2 Series Datasheet
v1.3; ESP32-H2 Technical Reference Manual v1.1; ESP32-H2-MINI-1 & MINI-1U Datasheet v1.6; ESP32-H2
Series SoC Errata v1.3 dated 9 June 2026; ESP-IDF `master` as at 13 August 2026 plus `release/v5.5`
where master has refactored; ESP8266EX Datasheet v7.1; ESP8285 Datasheet; ESP8266 Technical
Reference v1.7; ESP8266 Hardware Design Guidelines v2.8; the `ESP8266_RTOS_SDK` IO MUX header; the
Tuya TYWE2L Module Datasheet 20210119 held in `hardware/datasheets/`; and the `blakadder/templates`
and `esphome/esphome-devices` device databases.

---

## Recommended mapping

| Leg | TYWE2L net (ESP8285) | v2: H2 GPIO | Module pin | Reset code | Power domain | Capabilities retained | Change from v1 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | GPIO14 | **GPIO13** | 12 | `0` (IE = 0, no pull) | VDDA_PMU/VBAT | full digital via GPIO matrix: LEDC PWM, RMT, UART, I2C, SPI2, PCNT, edge interrupt, internal pull either way. **LP pin: deep-sleep wake capable.** XTAL_32K_P if ever wanted. | was H2 GPIO4 (pin 18). Loses ADC1_CH3. **Loses the MTCK after-reset pull-up, which is the point.** Gains LP. |
| 2 | GPIO12 | **GPIO14** | 13 | `0` | VDDA_PMU/VBAT | as above. **LP pin.** XTAL_32K_N. | was H2 GPIO5 (pin 19). Loses ADC1_CH4. No longer an MTDI pad. Gains LP. |
| 3 | GPIO13 | **GPIO12** | 16 | `0` | VDDA_PMU/VBAT | as above. **LP pin.** | was H2 GPIO3 (pin 6). Loses ADC1_CH2. **No longer a strapping pin.** No longer MTDO. Gains LP. |
| 4 | GPIO5 | **GPIO10** | 20 | `0` | VDDPST1 | as above. **LP pin.** ZCD0, the comparator's optional external reference input. | was H2 GPIO2 (pin 5). Loses ADC1_CH1. **No longer a strapping pin.** No longer MTMS. Gains LP. |
| 5 | GPIO4 | **GPIO11** | 21 | `0` | VDDPST1 | as above. **LP pin.** **ZCD1, the comparator's signal input.** | was H2 GPIO1 (pin 10). Loses ADC1_CH0. Gains LP and the comparator. |
| 6 | GND | GND | 1, 2, 11, 14, 36..53 | — | — | — | unchanged |
| 7 | 3V3 | 3V3 | 3 | — | — | — | unchanged |

Leg order is the physical TYWE2L order, left to right on the front (antenna) face, per
`hardware/DESIGN.md` section 1 and TYWE2L datasheet Figure 2 and Table 1, page 4. Reset codes are
TRM v1.1 Table 6.13-1 page 235. Power domains are datasheet v1.3 Table 2-1 "Pin Providing Power"
and Table 2-7. Module pin numbers are ESP32-H2-MINI-1 datasheet v1.6 Table 3 page 10.

**Everything else stays.** GPIO22 remains the rear `IO2` log pad, GPIO9 the rear `IO0` download strap
pad, GPIO23 and GPIO24 the UART0 console pads, GPIO26 and GPIO27 the native USB Serial/JTAG pads. R2,
the 10 kΩ GPIO8 pull-up already in the schematic, stays and is still independently correct, though
under v2 it is no longer load-bearing for host-line safety.

**The new spare field is strictly better than the old one.** GPIO0 through GPIO5 all become spares,
which puts every one of ADC1_CH0 to CH4 and all four JTAG pads back where they can actually be used.
Under v1 the carrier had no analogue pin available to itself. Under v2 it has five. TP10 to TP15 need
recutting to carry GPIO0 to GPIO5 in place of GPIO0 and GPIO10 to GPIO14.

**One new hard constraint that v2 introduces.** Three of the five legs (GPIO12, GPIO13, GPIO14) sit
on the VDDA_PMU/VBAT domain rather than VDDPST1. See section 2.5. The module ties VBAT to its own
3V3 through a 0 Ω link, so in our configuration nothing changes, but **VBAT must never be fed from a
separate supply while host lines sit on these pads**. That needs to go on the schematic sheet next to
the existing VBAT no-connect note.

### Naming hazard, and it needs a note on the schematic

ESP8285 GPIO12, GPIO13 and GPIO14 are leg nets. H2 GPIO12, GPIO13 and GPIO14 are module pins. Under
v2 both appear in the same design and they are **crossed**: the leg carrying ESP8285 GPIO14 goes to
H2 GPIO13, and the leg carrying ESP8285 GPIO12 goes to H2 GPIO14. The schematic's existing
`HOST_GPIOxx` versus `H2_IOxx` net naming already separates the two namespaces and must be kept.
(This document originally wrote that second prefix as `IOxx`; the schematic as built uses `H2_IOxx`.)
Anyone writing a profile, a firmware pin table or a test script has to say which namespace a number
belongs to, every time. `BACKLOG.md` already carries "wrong pin maps are a physical hazard" as a
known risk, so this belongs on the schematic sheet rather than only here.

Section 5.4 sets out an alternative assignment that removes the hazard at the cost of two trace
crossings. It is a genuine choice and it belongs to the project owner.

---

## Why v1 had to change, and the second reason nobody was looking for

**Reason one, the one that forced the review.** TRM v1.1 section 8.2.2 states that "the values of
GPIO9, GPIO8, GPIO3 and GPIO2 at reset determine the boot mode after the reset is released", and
Register 6.7 gives GPIO2 and GPIO3 dedicated latch bits in `GPIO_STRAP_REG` (bit 0 = GPIO2, bit 1 =
GPIO3). Legs 4 and 3 of v1 sat on exactly those two pins. Evidence in
`h2-strapping-and-reset-states.md` sections 2 and 5.

**Reason two, which this review turned up on its own.** v1 put all five legs on ADC1_CH0 to CH4 so
that, per `hardware/DESIGN.md` section 3, "a host device that used a TYWE2L pin as an analogue input
keeps an analogue-capable pin".

No host device can ever have done that, and this is now confirmed from primary sources rather than
inferred. ESP8266EX Datasheet v7.1 section 4.9 and Table 4-9 give the ADC exactly one row: "TOUT | 6
| ADC Interface". Table 2-1 lists pin 6 TOUT as type **I**, input only, a dedicated pin rather than a
GPIO. The complete IO MUX function set for the five legs, from `ESP8266_RTOS_SDK`
`components/esp8266/include/esp8266/pin_mux_register.h` and corroborated by Datasheet Table 2-1,
contains no analogue function on any of them:

| TYWE2L leg | ESP8285 GPIO | Chip pin | IO MUX pad | F0 | F1 | F2 | F3 | F4 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | GPIO14 | 9 | `MTMS` | MTMS | I2SI_WS | HSPI_CLK | **GPIO14** | UART0_DSR |
| 2 | GPIO12 | 10 | `MTDI` | MTDI | I2SI_DATA | HSPIQ/MISO | **GPIO12** | UART0_DTR |
| 3 | GPIO13 | 12 | `MTCK` | MTCK | I2SI_BCK | HSPID/MOSI | **GPIO13** | UART0_CTS |
| 4 | GPIO5 | 24 | `GPIO5` | **GPIO5** | CLK_RTC | — | — | — |
| 5 | GPIO4 | 16 | `GPIO4` | **GPIO4** | CLK_XTAL | — | — | — |

And TOUT is not brought out. TYWE2L datasheet section 2.2 Table 1 lists seven interface pins
(GPIO14, GPIO12, GPIO13, GPIO5, GPIO4, GND, 3V3) and section 2.3 Table 2 lists three test pins
(`IO2`, `RST`, `IO0`). Section 1.1 Features says plainly "Peripherals: five GPIOs". There is no
TOUT anywhere in the document.

This project's own corpus work already recorded the consequence without drawing the conclusion. From
[`docs/tasmota-import.md` section 3](./tasmota-import.md), the entire Tasmota ADC function family is
marked unusable, verbatim: "a template that asks for ADC0 is asking for the ESP8285's TOUT pin, which
the TYWE2L does not break out". ADC0 appears in 202 corpus templates and every one is classified as
not fitting a TYWE2L for that reason (section 2.2).

So v1 spent all five legs buying back a capability the part being replaced never had, and paid for it
by landing two legs on strapping pins. The ADC1 constraint was never binding.

There is a sharper way to put it. An ADC input whose net is driven by a host output stage is not an
ADC input. Analogue capability on a host-facing leg is dead weight in both directions, because the
host cannot present an analogue signal and we cannot read one through a driven net. The right home for
the ADC1 pins is the spare field, and v2 puts them there.

---

## 1. Screening table: all ten candidates

Reset codes come from TRM v1.1 Table 6.13-1 "IO MUX Functions List" (page 235). The legend on that
page reads: `0 - IE=0 (input disabled)`, `1 - IE=1 (input enabled)`, `2 - IE=1, WPD=1`,
`3 - IE=1, WPU=1`, `4 - OE=1, WPU=1`. The `R` note in the Notes column marks LP pins (page 236). At
Reset and After Reset come from datasheet v1.3 Table 2-1 "Pin Overview", pages 13 to 14.

The "Latched?" column is settled by TRM v1.1 Register 6.7, which enumerates the entire strapping
latch: "bit0: GPIO2, bit1: GPIO3, bit2: GPIO8, bit3: GPIO9, bit4: GPIO25, bit5 ~ bit15: invalid".
There is no sixth strapping bit, so a pin absent from that list cannot be latched at reset on this
silicon. Note that the ESP-IDF register header
`components/soc/esp32h2/register/soc/gpio_reg.h:109-119` defines only an opaque 16-bit
`GPIO_STRAPPING` field with no per-bit names, so TRM Register 6.7 is the sole bit map.

| GPIO | Pad name | Module pin | Reset code | At Reset | After Reset | Latched? | Chip pull before firmware? | Power domain | LP? | Verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | `GPIO0` | 9 | `0` | (blank) | (blank) | **No** | No | VDDPST1 | no | **PASS** |
| 1 | `GPIO1` | 10 | `0` | (blank) | (blank) | No | No | VDDPST1 | no | **PASS** |
| 4 | **`MTCK`** | 18 | **`1*`** | (blank) | **IE, footnote 4** | No | **YES: WPU after reset whenever `EFUSE_DIS_PAD_JTAG` = 0, which is every factory part** | VDDPST1 | no | **REJECT** |
| 5 | `MTDI` | 19 | `1` | (blank) | IE | No | No | VDDPST1 | no | PASS, with reservations |
| 10 | `GPIO10` | 20 | `0` | (blank) | (blank) | No | No | VDDPST1 | **yes** | **PASS** |
| 11 | `GPIO11` | 21 | `0` | (blank) | (blank) | No | No | VDDPST1 | **yes** | **PASS** |
| 12 | `GPIO12` | 16 | `0` | (blank) | (blank) | No | No | **VDDA_PMU/VBAT** | **yes** | **PASS** |
| 13 | **`XTAL_32K_P`** | 12 | `0` | (blank) | (blank) | No | No | **VDDA_PMU/VBAT** | **yes** | **PASS** |
| 14 | **`XTAL_32K_N`** | 13 | `0` | (blank) | (blank) | No | No | **VDDA_PMU/VBAT** | **yes** | **PASS** |
| 22 | `GPIO22` | 24 | `0` | (blank) | (blank) | No | No | **VDDPST2** | no | **PASS** |

All ten carry `DRV` = 2 in TRM Table 6.13-1, which is approximately 20 mA default drive.

Reference rows, same sources, for the pins already reserved:

| GPIO | Pad name | Reset code | Why reserved |
| --- | --- | --- | --- |
| 2 | MTMS | `1` | Latched, `GPIO_STRAP_REG` bit 0 |
| 3 | MTDO | `1` | Latched, bit 1 |
| 8 | GPIO8 | `1` | Latched, bit 2. Boot mode and ROM print control |
| 9 | GPIO9 | **`3`** (IE + **WPU**) | Latched, bit 3. Download strap |
| 23 | U0RXD | **`3`** (IE + WPU) | UART0 console |
| 24 | U0TXD | **`4`** (**OE** + WPU) | UART0 console, and an **active output out of reset** |
| 25 | GPIO25 | `1` | Latched, bit 4. JTAG select |
| 26 | GPIO26 | — | USB Serial/JTAG D−, IE after reset |
| 27 | GPIO27 | — | USB Serial/JTAG D+, **IE and USB_PU** after reset |

One refinement worth recording. The datasheet distinguishes At Reset from After Reset, and for MTCK
and MTDI the At Reset cell is blank with IE appearing only in the After Reset column. TRM's single
"Reset" column corresponds to the after-reset state. So GPIO4 and GPIO5 are not input-enabled
*during* reset, only after it. It does not change any conclusion here, but it matters to anyone
reasoning about behaviour inside the reset window itself.

### The GPIO4 rejection, in full

GPIO4 is the only candidate that fails, and it fails on the criterion the brief set. A 45 kΩ pull-up
is not an output stage, so GPIO4 passes the letter of "cannot be seized as an output". It fails the
intent, and the intent governs.

TRM v1.1 Table 6.13-1 footnote, verbatim: "1* - If `EFUSE_DIS_PAD_JTAG` = 1, the pin MTCK is left
floating after reset, i.e., IE = 1. If `EFUSE_DIS_PAD_JTAG` = 0, the pin MTCK is connected to
internal pull-up resistor, i.e., IE = 1, WPU = 1." Datasheet Table 2-1 footnote 4 says the same.
`EFUSE_DIS_PAD_JTAG` ships as 0 on every part and we have committed never to burn it (`BACKLOG.md`
never-burn list), so the pull-up is present on every unit that will ever ship.

`h2-strapping-and-reset-states.md` sections 10 and 11 establish the two facts that decide it. The
window is not the reset assertion, it is reset release through ROM bootloader, second stage
bootloader and app init, on the order of 200 to 400 ms. And into a logic-level MOSFET gate with no
external pull-down, "the relay energises and stays energised for the entire few hundred milliseconds
until firmware drives the pin low. This is a definite, not marginal, failure."

That document's remedy is "do not put the relay drive line on GPIO4 at all". Because we do not know
what any leg does, and the project has deliberately deferred finding out to the firmware dump work
(`BACKLOG.md`), every leg must be treated as potentially the actuator line. The per-leg
recommendation therefore generalises into a per-pin veto. **No leg goes on GPIO4.**

The fallback remedy, a 2.2 kΩ pull-down, is self-cancelling. The same document notes it "destroys the
pin's usefulness as an ADC input for anything but a low-impedance source", so fitting it removes the
only reason anyone wanted GPIO4. With ADC on the legs now known to be worthless anyway, nothing is
left on GPIO4's side of the ledger.

### GPIO5 passes and is still not used

GPIO5 (MTDI) carries reset code `1`, input enabled with no pull, which is electrically
indistinguishable from code `0` as far as a host net is concerned. Nothing drives, nothing pulls. It
is a legitimate PASS and it keeps ADC1_CH4. It is left out for three reasons, none of them
electrical:

1. **It is the last JTAG pad.** Leaving it out means all four of MTMS, MTDO, MTCK and MTDI (GPIO2 to
   GPIO5) are clear of host nets. That converts `EFUSE_DIS_USB_JTAG`, which `BACKLOG.md` calls "the
   dangerous one" because burning it alone "permanently seizes four host lines" and makes MTDO a chip
   output driving into a host-driven net, from a hardware-damage path into an inconvenience. The same
   applies to the `EFUSE_STRAP_JTAG_SEL_ENABLE` plus GPIO25 scenario that `hardware/DESIGN.md`
   section 2 calls safety-critical.
2. **It is not an LP pin**, so it cannot wake the chip from deep sleep. GPIO10 to GPIO14 can.
3. Its ADC channel is the one with the erratum history (ADC-7227, section 3), and the ADC is
   worthless on a host leg regardless.

Keeping GPIO5 free costs nothing, because six clean candidates remain and only five are needed.

---

## 2. Peripheral value of each candidate

### 2.1 IO MUX alternate functions, confirmed

From datasheet v1.3 Table 2-3 "IO MUX Pin Functions" pages 15 to 16, TRM v1.1 Table 6.13-1 page 235,
and `components/soc/esp32h2/register/soc/io_mux_reg.h` where the `FUNC_*` defines are literally the
`MCU_SEL` field values. Note that `io_mux_reg.h` has moved on master and now lives under
`register/soc/` rather than `include/soc/`.

| GPIO | Pad name | F0 | F1 | F2 | Analogue function |
| --- | --- | --- | --- | --- | --- |
| 0 | `GPIO0` | GPIO0 | GPIO0 | **FSPIQ** | none |
| 1 | `GPIO1` | GPIO1 | GPIO1 | **FSPICS0** | **ADC1_CH0** |
| 4 | `MTCK` | **MTCK** | GPIO4 | **FSPICLK** | **ADC1_CH3** |
| 5 | `MTDI` | **MTDI** | GPIO5 | **FSPID** | **ADC1_CH4** |
| 10 | `GPIO10` | GPIO10 | GPIO10 | — | **ZCD0** |
| 11 | `GPIO11` | GPIO11 | GPIO11 | — | **ZCD1** |
| 12 | `GPIO12` | GPIO12 | GPIO12 | — | none |
| 13 | `XTAL_32K_P` | GPIO13 | GPIO13 | — | **XTAL_32K_P** |
| 14 | `XTAL_32K_N` | GPIO14 | GPIO14 | — | **XTAL_32K_N** |
| 22 | `GPIO22` | GPIO22 | GPIO22 | — | none |

Corroborating defines: `FUNC_GPIO0_FSPIQ 2` (`io_mux_reg.h:174`), `FUNC_GPIO1_FSPICS0 2` (`:179`),
`FUNC_MTCK_FSPICLK 2` (`:194`), `FUNC_MTDI_FSPID 2` (`:199`), and for GPIO10 to GPIO14 and GPIO22
only functions 0 and 1 exist, both being the plain GPIO function (`:220-237`, `:268-269`).

Two points that matter. F3 and F4 are empty for every ESP32-H2 pin, so the IO MUX is at most three
deep on this part. And for the six recommended-set candidates, F0 and F1 are both the GPIO function,
with TRM section 6.16 page 237 instructing "Configure `IO_MUX_GPIOy_MCU_SEL` to 1, to select
Function1" as the canonical GPIO selection. There is nothing else on those pads to conflict with.

**The FSPI correction.** FSPIQ, FSPICS0, FSPICLK and FSPID are the **SPI2** IO MUX signals. They were
never the flash bus. The in-package flash on the ESP32-H2FH4S uses SPI0 and SPI1 on GPIO15 to GPIO21,
which are not fanned out to any package pin (TRM section 6.1 page 217: the usable pins are "GPIO0~GPIO5,
GPIO8~GPIO14, and GPIO22~GPIO27", 19 in total; ESP-IDF `esp32h2.inc:141` says the same). So using
GPIO0 to GPIO5 as plain GPIOs takes nothing from the flash, and the F2 functions are simply idle
unless SPI2 is deliberately configured through the IO MUX.

### 2.2 Analogue: confirmed, and confirmed to be irrelevant here

ADC1_CH0 to CH4 are GPIO1 to GPIO5 and nothing else. `components/soc/esp32h2/include/soc/adc_channel.h`
lines 9 to 22 contain exactly five channel defines and **no ADC2 defines whatsoever**.
`soc_caps.h:128-129` gives `SOC_ADC_PERIPH_NUM (1U)` and `SOC_ADC_CHANNEL_NUM(PERIPH_NUM) (5)`.
Datasheet section 4.2.2.1 page 48: "The pins for the SAR ADC are multiplexed with GPIO1 ~ GPIO5, JTAG
interface, and SPI2 interface." Confirmed against TRM Table 6.14-1 page 236 and datasheet Table 2-5
page 17.

With GPIO2 and GPIO3 excluded as strapping pins and GPIO4 excluded above, only **GPIO1 and GPIO5**
retain analogue capability among the candidates. Neither is used, for the reason in the preamble. All
five ADC1 pins go to the spare field, where the carrier could actually use one. Sampling is capped at
100 kSPS and DNL is −8/+12 LSB (datasheet Table 5-4 page 54), which is worth knowing before anyone
plans anything precise with them.

### 2.3 ZCD0 and ZCD1: real, useful, and not what the brief assumed

The brief asked to confirm "GPIO10 = ZCD0 and GPIO11 = ZCD1 (zero-crossing detection)". The pad names
are confirmed. **The semantics are not two channels, and this changes what can be claimed.**

Datasheet v1.3 Table 2-5 page 17 gives GPIO10 analogue F0 = ZCD0 and GPIO11 analogue F0 = ZCD1.
Table 2-4 page 17 defines `ZCDn` as "Voltage from GPIO Pad — Analog Pad voltage comparator interface".
TRM Table 6.14-1 page 236 matches, with footnote 1 pointing at TRM section 6.15.

TRM v1.1 section 6.15 "Function of Analog PAD Voltage Comparator", page 237, verbatim:

> GPIO10 and GPIO11 pads have the function of analog PAD voltage comparator, which can be enabled by
> setting `GPIO_EXT_XPD_COMP` to 1. After enabling the function of analog PAD voltage comparator,
> when the voltage on GPIO11 pad is higher than the reference voltage, the `PAD_COMP_OUT` signal
> indicating the comparison result will be high, otherwise it will be low.
> Set the value of `GPIO_EXT_MODE_COMP` as follows: 0: the reference voltage is
> (`GPIO_EXT_DREF_COMP` * VDDPST2)/10. 1: the reference voltage is the voltage on GPIO10 PAD.

**So there is one comparator. GPIO11 is the signal input. GPIO10 is the optional external reference
input**, needed only when you do not want the internal reference. The zero-crossing name comes from
`GPIO_EXT_ZERO_DET_MODE` (0 = no interrupt, 3 = any edge of `PAD_COMP_OUT_sync` as an interrupt
source) with `GPIO_EXT_ZERO_DET_FILTER_CNT` masking new interrupts for a number of IO MUX clock
cycles. The whole thing lives inside the GPIO block, which is why its interrupt shares the GPIO
interrupt source.

ESP-IDF agrees and is unambiguous. `soc_caps.h:263` on `release/v5.5` gives `SOC_ANA_CMPR_NUM (1U)`,
and `:264` `SOC_ANA_CMPR_INTR_SHARE_WITH_GPIO (1)`. `components/soc/esp32h2/ana_cmpr_periph.c:10-17`
has a single unit entry. `components/soc/esp32h2/include/soc/ana_cmpr_pins.h:9-10` is explicit:

```
#define ANA_CMPR0_EXT_REF_GPIO    10   /* The GPIO that can be used as external reference voltage */
#define ANA_CMPR0_SRC_GPIO        11   /* The GPIO that used for inputting the source signal to compare */
```

The ESP-IDF Analog Comparator API reference for esp32h2 says "Analog comparator on ESP32-H2 has 1
unit(s) ... UNIT0 — Source Channel: GPIO11 — External Reference Channel: GPIO10". The driver is
`esp_driver_ana_cmpr` (`ana_cmpr_new_unit()`, `ana_cmpr_set_internal_reference()`,
`ana_cmpr_set_debounce()`, `ana_cmpr_register_event_callbacks()`). The internal reference is 0 to
70 % of VDD in 10 % steps.

**The path is hard-wired analogue and is not routable through the GPIO matrix.** The GPIO number is a
compile-time constant per unit, and `esp_driver_ana_cmpr/ana_cmpr.c:79-83` calls
`gpio_config_as_analog()` on the fixed pins with no `esp_rom_gpio_connect_in_signal()` anywhere in the
file. `ana_cmpr_get_gpio()` exists purely to tell you which fixed pin it is. Table 6.14-1 lists ZCD0
and ZCD1 as **Analog Function 0**, an IO MUX analogue path, and the GPIO matrix carries digital
signals only. So if you want the comparator, GPIO11 is not negotiable.

**Flagged contradiction in Espressif's own documents.** Datasheet v1.3 section 4.2.2.3 page 48 states
"ESP32-H2 integrates **two** analog voltage comparators ... Each analog voltage comparator has two
pads associated with it". Its own pin assignment paragraph on page 49 then says "The pins for the
analog voltage pad comparators are multiplexed with GPIO10 ~ GPIO11", which is two pads, meaning one
comparator. The TRM, the soc caps and the IDF driver all say one. **Treat ESP32-H2 as having one
comparator unit.** No register or driver evidence of a second was found, and no Espressif erratum
acknowledges the discrepancy.

**A second cross-domain oddity, flagged.** TRM section 6.15 derives the internal reference from
**VDDPST2** (`(GPIO_EXT_DREF_COMP * VDDPST2)/10`), while the GPIO10 and GPIO11 pads are powered from
**VDDPST1** per datasheet Table 2-1. On the MINI-1 both come from the same 3V3 net so this is
theoretical, but it is a genuine cross-domain reference and is worth knowing before trusting the
internal reference to track the pad supply.

**How much is this worth? Less than the brief assumed, and it is still free.** The brief's premise is
that Tuya AC dimmers commonly feed zero-cross detect to the module. Two independent lines of evidence
say that is not true of TYWE2L devices specifically.

First, this project's own corpus. Across the 902 TYWE2L-compatible Tasmota templates analysed in
`tasmota-import.md` section 2.3, the complete function vocabulary is 40 component codes and
`GPIO_ZEROCROSS` is not one of them. Tasmota does support zero-cross AC dimming on ESP8266
(`USE_AC_ZERO_CROSS_DIMMER`, `SetOption99`, `ZCDimmerSet`, with the zero-cross input on any GPIO), so
the absence is a real signal rather than a missing feature.

Second, and more striking, a direct search of both community device databases (see section 2.7)
found **no TYWE2L device using any leg for zero-cross detect**, and no TYWE2L device using any leg for
anything other than PWM.

And in any case, zero-cross detection does not need the comparator. The ESP8266 has no analogue
comparator at all, and every ESP8266 AC dimmer implementation detects zero cross as a plain digital
edge interrupt. Any of the ten candidates does that.

So the honest weighting: **ZCD is a free option, not a requirement.** GPIO10 and GPIO11 earn their
places on reset behaviour, LP capability and routing. The comparator comes along for nothing, and
because GPIO11 is on a leg, a converted device gets a hardware zero-cross input it could not have had
on the ESP8285. That is a genuine capability gain, and it is a bonus rather than a driver.

### 2.4 XTAL_32K_P and XTAL_32K_N on GPIO13 and GPIO14

Confirmed. Datasheet v1.3 Table 2-1 rows 15 and 16 give the pin names `XTAL_32K_P` and `XTAL_32K_N`;
Table 2-5 page 17 lists them as analogue F0; Table 2-4 page 17 describes them as "32 kHz external
clock input/output connected to ESP32-H2's oscillator/crystal". TRM Table 6.13-1 page 235 gives the
same pad names. Module datasheet v1.6 Table 3 page 10: pin 12 = `IO13` "GPIO13, XTAL_32K_P", pin 13 =
`IO14` "GPIO14, XTAL_32K_N".

**No 32.768 kHz crystal is fitted on the MINI-1.** The module's "Integrated Components on Module" list
on page 2 contains only a 32 MHz crystal, and the module schematic on page 32 routes GPIO13 and
GPIO14 straight from the SoC to the module pads with nothing on them.

**Cost of using them as ordinary GPIOs: none from a pin-function standpoint.** They are
Function0/Function1 = GPIO13/GPIO14 with reset code `0`, the cleanest state available. The only cost
is opportunity: the carrier can never fit a 32.768 kHz crystal, so the RTC slow clock must come from
an internal RC source. That matters only if the product wants the low-power sleep modes an external
32 kHz reference supports, and it does not. This is a mains-powered Matter over Thread device expected
to act as a router node and stay awake. No crystal is fitted on the carrier today
(`hardware/DESIGN.md` section 5 lists the whole support BOM), so the cost is the loss of an option
never taken up. Note that erratum CLK-6996 concerns RC_FAST_CLK rather than RC_SLOW, and is fixed in
v1.2 in any case.

**One practical consequence that is not free, and it bites the bring-up path.** The
ESP32-H2-DevKitM-1 does fit that crystal. From `docs/devkit-bringup.md`, header J1 pins 7 and 8 are
silkscreened `13/N` and `14/N`, and the doc explains: "The `/N` means not connected. R26 and R27 are
0 Ω parts marked NC (not fitted) on the schematic, because GPIO13 and GPIO14 are taken by the
on-board 32.768 kHz crystal X1."

So under v2, two of the five legs cannot be exercised on a stock DevKitM-1. Options, in order:

1. **Rework one devkit**: remove X1 and fit R26 and R27 (0 Ω, 0402). One crystal removal and two
   joints, once, on one board. Document it in `devkit-bringup.md`.
2. **Substitute for bring-up only**: exercise legs 1 and 2 on GPIO0 (J1 pin 3) and GPIO1 (J1 pin 4),
   both reset code `0` and both non-strapping, so they are honest electrical stand-ins. Record in the
   test log that they are stand-ins, because this does not test the production pins and neither is an
   LP pin.
3. Choose a different five. Section 4.2 says what that costs.

GPIO10, GPIO11, GPIO12 and GPIO22 are all on devkit header J3 (`G, TX, RX, 10, 11, 25, 12, 8, 22, G,
9, G, 27, 26, G`) and need no rework.

### 2.5 Power domains: resolved, and it produces a new design rule

Datasheet v1.3 Table 2-7 "Power Pins" page 20 and the "Pin Providing Power" column of Table 2-1:

| Domain | Pins it powers, among our candidates |
| --- | --- |
| VDDPST1 (datasheet pin 9) | GPIO0, GPIO1, GPIO4, GPIO5, GPIO10, GPIO11 |
| VDDPST2 (pin 20) | GPIO22 |
| **VBAT (pin 18) / VDDA_PMU (pin 19)** | **GPIO12, XTAL_32K_P (GPIO13), XTAL_32K_N (GPIO14)** |

Table 2-7 lists VBAT as "Analog power domain or battery power supply" and VDDA_PMU as "Analog power
domain", both with "Other IO Pins" = GPIO12, XTAL_32K_P, XTAL_32K_N. Figure 2-2 "ESP32-H2 Power
Scheme" page 21 shows the same split. This confirms the claim in `hardware/DESIGN.md` that was
previously unverified.

**Why it matters.** Datasheet Table 5-3 pages 53 to 54 defines VIH = 0.75 × VDD, VIL = 0.25 × VDD,
VOH = 0.8 × VDD and VOL = 0.1 × VDD, with footnote 1: "VDD – voltage from a power pin of a
**respective power domain**". If VBAT ever runs from a battery at a voltage different from VDDPST1,
GPIO12, GPIO13 and GPIO14 will have different thresholds and different output swings from every other
pin on the module, including from GPIO10 and GPIO11 on the same host connector.

**Why it is inert in our configuration.** Module pin 15 `VBAT` is documented as "Connected to
internal 3V3 power supply (Default) or external battery power supply (3.0 ~ 3.6 V)" (module datasheet
Table 3 page 10), and the module schematic on page 32 ties VBAT to VDD33 through **R2 = 0 Ω**, with
the schematic note "When VBAT is powered by external battery, R2 can be NC." Our carrier leaves VBAT
unconnected with an NC flag (`hardware/DESIGN.md` section 2), so GPIO12 to GPIO14 sit at the same
3.3 V as everything else.

**The new rule this creates: never feed VBAT from anything other than 3V3 while host lines sit on
GPIO12 to GPIO14.** Under v1 (all five legs on VDDPST1) this could not have arisen. It should go on
the schematic sheet next to the existing VBAT note, and into `BACKLOG.md`.

One naming inconsistency, flagged rather than resolved. ESP-IDF `soc_caps.h` comments call the
GPIO7 to GPIO14 supply `VDD3V3_LP` (line 216) and the digital pads `VDD3P3_CPU or VDD_SPI` (line 229),
while the datasheet uses VDDPST1, VDDPST2, VDDA_PMU and VBAT. The datasheet Table 2-1 per-pin column
is the most specific evidence and is what is reported above. Relatedly,
`SOC_GPIO_VALID_DIGITAL_IO_PAD_MASK 0x000000000FFF807FULL` (`soc_caps.h:230`) **excludes GPIO7 to
GPIO14**, meaning ESP-IDF treats GPIO10 to GPIO14 as non-digital-domain pads. This has no functional
consequence for ordinary GPIO use, but it is worth knowing if any IDF API ever rejects one of these
pins on that mask.

### 2.6 LP capability: a genuine gain that was not in the brief

This was not asked for and it is the largest capability the recommended set picks up.

Datasheet v1.3 section 2.2 page 13, verbatim:

> **Digital pins (GPIO0 ~ GPIO5, GPIO22 ~ GPIO27)**: are unable to work in Deep-sleep mode, but can
> work in Light-sleep mode only if the power domain controlled by the XPD TOP does not power off.
> **LP pins (GPIO8 ~ GPIO14)**: are able to work in any chip mode.

Corroboration: TRM Table 6.13-1's Notes column carries `R` (LP pins) on GPIO8 through GPIO14 and on
no other pin. `components/soc/esp32h2/include/soc/rtc_io_channel.h:10-32` gives RTCIO channels 0 to 7
as GPIO7 through GPIO14, matching `SOC_RTCIO_PIN_COUNT (8U)`. ESP-IDF `esp32h2.inc:142`: "RTC:
GPIO7–GPIO14 can be used to wake up the chip from Deep-sleep mode. Note that although GPIO7 is an RTC
GPIO, it cannot be used for external wake-up since it is not led out. Other GPIOs can only wake up
the chip from Light-sleep mode."

**All five recommended pins are LP pins. None of GPIO0, GPIO1, GPIO4, GPIO5 or GPIO22 is.** So the
recommended set is the only five-pin selection from the candidate list where every host line can wake
the chip from deep sleep via EXT1. Under v1, not one leg could.

Whether the product ever uses it is another matter, and today it will not, because a Thread router
node stays awake. But it is capability the design now has and did not before, and it is free.

Two operational notes for whoever writes the firmware. EXT1 wakeup configures the pad as an RTC IO, so
`rtc_gpio_deinit()` must be called before reusing the pad as a digital GPIO, and `rtc_gpio_hold_dis()`
after a light-sleep wake. Hold masks for all GPIOs live in LP_AON registers
(`gpio_periph.c:43-72`).

Minor discrepancy, flagged: the datasheet says LP pins are GPIO8 to GPIO14 (seven), ESP-IDF says
GPIO7 to GPIO14 (eight RTCIO channels). Reconciled by GPIO7 existing on the die but not being bonded
out on the package, which `esp32h2.inc:142` states directly.

### 2.7 What real TYWE2L devices actually do with these legs

A direct search of the two largest community device corpora, at source rather than through their
search pages: `blakadder/templates` (the database behind templates.blakadder.com) and
`esphome/esphome-devices` (behind devices.esphome.io).

**The TYWE2L is classified as a lighting module.** `blakadder/templates` `_data/modules.yaml` groups
module pinouts, and TYWE2L sits in the `dt-light` group alongside `BW2L`, `CB2L`, `DT-BL200`, `WB2L`,
`WBR2L` and `WR2L`. The TYWE3L and TYWE3S sit in the separate `esp12` group.

**Every TYWE2L device in both databases is a light, and every leg used is a PWM channel.** Six Tasmota
device entries and two ESPHome entries name the module explicitly:

| Device | Legs used | Function |
| --- | --- | --- |
| Nedis WIFILAC30WT RGBCCT 1200 lm | all five | GPIO4 = PWM2, GPIO5 = PWM1, GPIO12 = PWM5, GPIO13 = PWM3, GPIO14 = PWM4 |
| Simply Conserve L9W-A19-CCT-RGB-WIFI | all five | same five-channel mapping |
| Deta DET902HA 10 W downlight (ESPHome) | all five | GPIO4 green, GPIO5 red, GPIO12 warm white, GPIO13 blue, GPIO14 cold white |
| HeyLight Plafoniera 30 W CCT | two | GPIO12 = PWM2, GPIO14 = PWM1 |
| LEDLite LTTD10WIFI CCT downlight | two | GPIO12 = PWM2, GPIO14 = PWM1 |
| Mirabella Genio 1002742 | two | GPIO12 = PWM2, GPIO14 = PWM1 |
| BrilliantSmart 20812 Salisbury 12 W CCT | two | GPIO12 and GPIO14, legacy encoding, same positions |
| Barcelona LED B1295-SMART CCT (ESPHome) | two | GPIO12 white temperature, GPIO14 brightness |

Not one of them uses a leg for a relay, a button or switch, a status LED, TuyaMCU serial, I2C,
one-wire, WS2812, IR, a counter, or zero-cross detect. The pattern is uniform: **two PWM channels for
CCT on ESP8285 GPIO12 and GPIO14, five PWM channels for RGBCCT across all five legs.** The two
databases agree independently.

**This does not relax the worst-case discipline, and here is why.** `blakadder`'s `chip:` field is
only populated when a contributor filled it in, so eight device entries is a lower bound rather than a
census. The defensible claim is that in the visible community record the TYWE2L is used exclusively
as a multi-channel PWM light driver. The claim that no TYWE2L product has ever driven a relay is not
supported and is not made.

What it does do is sharpen the picture of what "worst case" means in practice. The most likely thing
on the other side of a leg is a MOSFET gate driving an LED channel. That is exactly the topology that
`h2-strapping-and-reset-states.md` section 11 identifies as a guaranteed failure against the GPIO4
pull-up. The failure mode is a light flashing at full brightness for a few hundred milliseconds on
every power-up rather than a mains relay clicking, which is less dangerous and more visible, but it is
still a defect and the GPIO4 veto still stands. And a relay device that nobody has flashed with
Tasmota would not appear in this evidence at all.

### 2.8 LEDC PWM: confirmed on every candidate

PWM is the single most-used function in the corpus at 519 uses of `GPIO_PWM1` plus 37 of
`GPIO_PWM1_INV` across 902 templates (`tasmota-import.md` section 2.3), and section 2.7 shows it is
the *only* function on real TYWE2L hardware. Every leg must do it, and every leg does.

**LEDC on ESP32-H2 is GPIO-matrix-only, so pin choice is unconstrained.** Independently confirmed in
[`docs/ledc-erratum.md`](./ledc-erratum.md) section 8, which adds that `ledc.c` binds the signal at
configuration time through `_ledc_set_pin()` with no per-pin table and no validation beyond "must be
a valid output GPIO". Any of GPIO10 to GPIO14 can carry any of the six channels, in any assignment,
and the assignment can be changed in firmware without a board change. TRM v1.1 Table 6.12-1
"Peripheral Signals via GPIO Matrix"
page 230, first six rows, gives `ledc_ls_sig_out0` through `ledc_ls_sig_out5` with "Direct Output via
IO MUX" = **no** for all six. Matching indices in
`components/soc/esp32h2/include/soc/gpio_sig_map.h:9-19` (`LEDC_LS_SIG_OUT0_IDX 0` through
`LEDC_LS_SIG_OUT5_IDX 5`). So any GPIO that can be an output can be an LEDC channel, and all ten
candidates qualify.

Counts, from `soc_caps.h:283-286` and TRM Chapter 35 section 35.2 page 1029:

| Property | ESP32-H2 |
| --- | --- |
| LEDC channels | **6** (`SOC_LEDC_CHANNEL_NUM`) |
| LEDC timers | **4** (`SOC_LEDC_TIMER_NUM`), all sharing one clock source on this part |
| Duty resolution | up to **20 bits** (`SOC_LEDC_TIMER_BIT_WIDTH`) |
| Speed modes | **low speed only.** There is no `LEDC_HS_*` signal in `gpio_sig_map.h` and no high-speed mode in TRM Chapter 35 |
| Extras | hardware fade with stop, gamma-curve fading, sleep retention, ETM |

Six channels against five legs is enough for the five-channel RGBCCT case, with one spare.

**The ESP32-H2 LEDC is a strict superset of what the ESP8285 offered on these legs**, and by a wide
margin, because the ESP8266 has no true hardware PWM at all. ESP8266EX Datasheet section 4.7,
verbatim: "ESP8266EX has four PWM output interfaces. They can be extended by users themselves ... The
functionality of PWM interfaces can be implemented via software programming. For example, in the LED
smart light demo, the function of PWM is realized by interruption of the timer ... PWM frequency range
is adjustable from 1000 μs to 10000 μs, i.e., between 100 Hz and 1 kHz." Tasmota's practical envelope
is 5 channels, 40 Hz to 4 kHz, default 977 Hz since v8.3.0, 10-bit range.

| | ESP8285 (TYWE2L) | ESP32-H2 LEDC |
| --- | --- | --- |
| Channels | 5 in Tasmota, software timer ISR | 6, hardware generators |
| Frequency | 100 Hz to 1 kHz documented, 40 Hz to 4 kHz in Tasmota | 2 Hz to 50 kHz in Tasmota |
| Resolution | ~10-bit practical | up to 20-bit |
| Jitter | interrupt-driven, degrades under radio load | hardware, glitch-free |
| Fading | software | hardware fade plus gamma curve |
| In light sleep | no | yes |

One ambiguity in Espressif's own ESP8266 documentation, recorded because it does not change the
conclusion: functional block diagram Figure 3-1 shows a discrete "PWM" block, which contradicts the
plain reading of section 4.7. There is no PWM register set in any surviving public ESP8266
documentation. Either way the H2 is the superset.

The same matrix argument covers every other function the corpus needs. TRM section 6.1 page 217:
"Through GPIO matrix and IO MUX, peripheral input signals can be from **any** IO pins, and peripheral
output signals can be routed to **any** IO pins." Section 6.2 page 217 quantifies it: "78 peripheral
input signals sourced from the input of any GPIO pins. 99 peripheral output signals routed to the
output of any GPIO pins." RMT, UART, I2C, SPI2, PCNT and GPIO interrupts are all matrix-routed and all
available on all ten candidates. The exceptions are analogue: ADC on GPIO1 and GPIO4 and GPIO5, and
the comparator on GPIO10 and GPIO11, are IO MUX analogue paths and are not routable.

### 2.9 Drive strength, and a note for `DESIGN.md` open item 5

The ESP8285 specifies I_MAX of **12 mA** per I/O with **no programmable drive strength at all**.
`pin_mux_register.h` exposes only pull-up, pull-down, sleep-pull-up, sleep-pull-down, sleep-OE and OE
bits, and there is no drive-strength field. ESP32-H2 candidates all carry `DRV` = 2 in TRM Table
6.13-1, roughly 20 mA by default, rising to 40 mA source and 28 mA sink at `PAD_DRIVER` = 3 (module
datasheet Table 12). Logic thresholds are identical between the parts as fractions of VDD
(VIH ≥ 0.75 VDD, VIL ≤ 0.25 VDD, VOH ≥ 0.8 VDD, VOL ≤ 0.1 VDD in both), so no level translation is
needed.

This is direct input to `hardware/DESIGN.md` open item 5. The default H2 drive is already above the
ESP8285's maximum, so firmware should consider clamping `PAD_DRIVER` on the five host lines rather
than leaving it at the default. Neither part is 5 V tolerant.

---

## 3. Errata

Source: ESP32-H2 Series SoC Errata **v1.3, released 9 June 2026**,
<https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32h2/>. Fifteen errata. Chip
revisions identified in the errata's own tables are **v0.0, v0.1 and v1.2** only. Table 1.3 footnote
1: "Missing specification identifier '—' means modules with this chip revision are not mass
produced", applied to v0.0. v0.1 modules are marked `MBXXXX` and v1.2 modules `MFXXXX`.

**There is no GPIO-category erratum on ESP32-H2 at all.** No erratum touches the strapping pin list,
the reset pull states, or any of GPIO0, GPIO1, GPIO10, GPIO11, GPIO12, GPIO13, GPIO14 or GPIO22 as
pins.

### 3.1 ADC-7227, "Unavailable Channel 4 in SRA ADC1"

Verbatim and complete, errata section 3.3 page 10:

> **Affected revisions: v0.0 v0.1**
> **Description**: Channel 4 (ADC1_CH4) of ADC1 is not operational in the ESP32-H2 chip.
> **Workarounds**: Use other channels instead of ADC1_CH4.
> **Solution**: Fixed in chip revision v1.2.

ADC1_CH4 = GPIO5 is confirmed (`adc_channel.h:21-22`, TRM Table 6.14-1, datasheet Table 2-5). The
scope is exactly one channel on one pin. **Does pinning the production spec to v1.2 or later fully
resolve it? Yes on the silicon**, since the errata says "Fixed in chip revision v1.2" and the summary
table marks v1.2 blank. Two qualifications:

1. It resolves it for parts you actually receive at v1.2, so the residual risk is supply chain rather
   than silicon. Module marking is checkable on receipt (`MFXXXX`). `BACKLOG.md` already carries the
   revision pin.
2. Enforcement belongs in the build. `CONFIG_ESP32H2_REV_MIN_102` makes the bootloader refuse to boot
   on anything older, which is how you guarantee the precondition. Setting the Kconfig does not itself
   fix anything.

**Under v2 this erratum no longer bears on the pin mapping**, because GPIO5 is not a host line and no
leg uses ADC. It still bears on the production specification, because GPIO5 becomes a spare that a
future revision might use as an analogue input.

The errata title contains a typo, "SRA ADC1" rather than "SAR ADC1", in both the contents and the
section heading. Worth knowing if anyone greps for it.

### 3.2 ADC-1477, "Loss of Precision in Lower Four Bits of SAR ADC"

Verbatim, errata section 3.4 page 10: affected revisions v0.0 and v0.1; "The lower four bits of the
SAR ADC data bits are missing, causing a loss of precision in the corresponding bits"; workarounds
"No workaround"; "Fixed in chip revision v1.2." On affected parts the 12-bit ADC is effectively
8-bit, on all channels. Irrelevant to v2 legs, relevant to any future analogue use of the spare
field.

### 3.3 LEDC-253. RESOLVED: it does not bite on v1.2, and the apparent conflict was a stale doc.

Verbatim, errata section 3.7 page 12: affected revisions v0.0 and v0.1; "When the timer selects the
maximum duty resolution, in such case, 100% duty cycle is not achievable. Setting duty to
(2^MAX_DUTY_RES) will break the internal duty calculation"; "No workaround"; "Fixed in chip revision
v1.2."

This one matters more under v2 than under v1, because LEDC is now known to be the only function real
TYWE2L hardware uses (section 2.7), and because a light that cannot reach 100 % brightness is a
user-visible defect. It was worth chasing down, and it has been:
[`docs/ledc-erratum.md`](./ledc-erratum.md) settles it in full and is the authority.

**The conflict this section originally flagged is closed.** It read that the ESP-IDF LEDC
documentation states the restriction "unconditionally with no revision qualifier". **That was true
of the ESP-IDF v5.3 and v5.4 pages and is not true of the version this project pins.** The qualifier
"The hardware limitation above only applies to chip revision before v1.2" sits at `ledc.rst:280` at
tag v5.5.4, added by commit `19fec9f455` "fix(ledc): updated docs for esp32h2 eco5 bugfix" on
2024-12-20, backported as far as v5.4.2. ESP32-H2 ECO5 is chip revision v1.2. The earlier reading
came from a stale rendered page, or from stopping at the end of the yellow warning box, since the
qualifier sits immediately after the warning as ordinary body text rather than inside it.

**The second observation in this section was correct but read the wrong way.** It said "no
revision-gated logic was found in the LEDC driver either way", and treated that as evidence of doubt.
There is no gating because **there is nothing to gate**: the driver never restricted the duty range,
and the "workaround" was always advice to the application rather than driver code. The argument
checks permit the full inclusive range, `LEDC_ARG_CHECK(target_duty <= ledc_get_max_duty(...))` at
`ledc.c:1444` and `:1460` at v5.5.4, and `ledc_ll_get_max_duty()` returns `2**duty_res` exactly, not
`2**duty_res - 1` (`ledc_ll.h:267-270`). The driver comment at `ledc.c:877-880` is itself
revision-qualified, reading "ESP32H2 (rev < 1.2)", from the same commit that fixed the docs.

**Net: errata, current documentation and driver source all agree. Affected on v0.0 and v0.1, fixed
in v1.2.** All three now say the same thing, so the ranking to apply if a conflict of this shape
appears again is driver source first, then the errata, then the versioned documentation, and always
check which documentation version is being read before concluding anything.

**No consequence for the PCB, and none for this mapping.** The erratum has no pin dimension at all,
being a timer-configuration and duty-value condition. LEDC on ESP32-H2 routes **purely through the
GPIO matrix** with no IO MUX affinity and no per-pin restriction (section 2.8), so pin choice is
unconstrained by this erratum or by LEDC generally. GPIO10 to GPIO14 can each carry any of the six
channels, in any assignment, changeable in firmware without a board change.

The practical rule stands and still costs nothing: **configure lighting timers at 10 to 14-bit duty
resolution, never at `SOC_LEDC_TIMER_BIT_WIDTH` (20)**. That keeps the design correct even if a v0.1
part is ever second-sourced, and 20-bit operation is useless for lighting anyway, since a 20-bit
timer off the 32 MHz XTAL runs at about 30 Hz. Verify incoming modules carry the `MF XXXX`
specification identifier, which is the v1.2 marking. If `ledc_find_suitable_duty_resolution()` is
used at runtime, clamp its result to 14 bits, because it can return up to
`SOC_LEDC_TIMER_BIT_WIDTH`.

One unrelated driver behaviour is worth knowing at the top endpoint, and it is not LEDC-253. A
hardware step fade that starts from exactly max duty is nudged down by one LSB by the driver
(`ledc.c:1335-1344`), unconditionally and on every target, to avoid a fade-counter overflow. At 10
to 14 bits that is 0.006 to 0.1 % of full scale and is not visible. Setting a static 100 % duty is
untouched by it. Details in `ledc-erratum.md` section 6.3.

### 3.4 The two that affect v1.2

- **PCNT-249, "Unable to Trigger Step Interrupts"**, errata section 3.15 page 16, affects **v0.0, v0.1
  and v1.2**, no fix scheduled. Verbatim: "When the step counter is enabled and the step counter
  reaches the low limit or high limit, step interrupts are not generated properly." The stated
  workaround is to notify the application every time the counter changes by a specified value and read
  the counter in the handler. **It touches no pins**, since PCNT inputs are matrix-routed
  (`gpio_sig_map.h:192`, TRM Table 6.12-1 shows `pcnt_sig_ch0_in0` with "Direct Input via IO MUX" =
  no). It limits only the step-notify feature. Relevant because the corpus tail contains `GPIO_CNTR1`
  and `GPIO_CNTR1_NP`, so a pulse counter on a host leg is plausible. A Tasmota-style pulse counter
  does not use step interrupts. **A second flagged conflict**: `soc_caps.h:302` reads
  `#define SOC_PCNT_SUPPORT_STEP_NOTIFY 1 /* Only avliable in chip version above 1.2 */`, which
  implies the feature works above v1.2, while the errata marks v1.2 affected. Unresolved. Do not rely
  on step notify.
- **ECDSA_DS-836**, errata section 3.12 page 15, affects **v1.2 only**, no fix scheduled. Signatures
  with invalid `r` and `s` values are incorrectly accepted; workaround "Use RSA_DS Secure Boot instead
  of ECDSA_DS Secure Boot". No pins. Already carried in `BACKLOG.md` as a Matter secure boot question
  and it stays there. It is the one erratum where v1.2 is worse than v0.x.

### 3.5 The rest, all fixed in v1.2

CPU-206 (deadlock on out-of-order execution involving LP SRAM writes), CLK-6996 (inaccurate
RC_FAST_CLK calibration), I2C-308 (I2C **slave** multiple-read in non-FIFO mode; I2C master is
unaffected), SPI-304 (flash auto-suspend, which concerns the in-package SPI0/1 bus on GPIO15 to
GPIO21 and not our pins), RMT-176 (idle state level in RMT continuous TX mode, already bypassed in
ESP-IDF v5.1 and later by forcing `RMT_IDLE_OUT_EN_CHn = 1`), AES-11401, ECC-11400, ECDSA_DS-837 and
802.15.4-9538 (TX power variation on certain certification channels, still meeting requirements).

**BOOT-9537**, "Accidentally Enter USB Download Boot Mode If the Power-up Duration Is Too Long",
affects v0.0 and v0.1, fixed in v1.2. It earns a mention because it is exactly the failure class this
whole exercise guards against. Keeping the supply rise under 12 ms remains cheap insurance regardless
of revision.

**Net for a v1.2 part using GPIO, LEDC, RMT, I2C master, SPI2 and PCNT on the recommended five pins:
the only live errata are PCNT-249 (step notify, avoidable) and ECDSA_DS-836 (irrelevant unless ECDSA
Secure Boot is used).**

### 3.6 Two revision anomalies, flagged

1. **A revision v0.2 (ECO2) appears in ESP-IDF but not in the errata.**
   `components/esp_hw_support/port/esp32h2/Kconfig.hw_support:12-19` offers four minimum-revision
   choices: v0.0, v0.1 (ECO1), **v0.2 (ECO2)** and v1.2. No v0.2 appears anywhere in the errata
   document's revision tables. Unexplained.
2. **"v1.2 is the current mass-production revision" is inferred, not quoted.** No Espressif statement
   saying so verbatim was found. The supporting evidence is errata Table 1.3 (v0.0 not mass produced,
   v1.2 has a module specification identifier) plus `SOC_CAPS_ECO_VER_MAX 102` (`soc_caps.h:42`).
   Treat it as very likely rather than as a cited fact.

---

## 4. Reasoning for the recommended five

### 4.1 The screen, applied in order

**Test 1, boot latch.** Is the pin in `GPIO_STRAP_REG`? TRM Register 6.7 enumerates the complete latch
as bits 0 to 4 with bits 5 to 15 marked invalid. All ten candidates pass by construction. This is the
criterion that killed v1, and it now excludes nothing because the reserved list already absorbed the
casualties.

**Test 2, chip-driven or chip-pulled state before firmware runs.** Reset code `0` is a genuinely dead
pad: input disabled, output disabled, no pull. Code `1` adds only an input buffer. Anything above code
`1` either pulls the net or drives it. GPIO4 fails on the after-reset pull-up. Eight of the remaining
nine sit at code `0` and GPIO5 sits at code `1`.

**Test 3, seizure by a peripheral alive before firmware.** This reserves GPIO23 and GPIO24 (UART0
console, with GPIO24 at reset code `4`, an active output out of reset) and GPIO26 and GPIO27 (USB
Serial/JTAG, with GPIO27 carrying USB_PU after reset). No candidate is touched by it in the factory
eFuse state. GPIO4 and GPIO5 are touchable only by burning `EFUSE_DIS_USB_JTAG`, which is not a
default state and is on the never-burn list, and which v2 removes from the equation by not using
either pin.

**Survivors at code `0` and free of every JTAG pad: GPIO0, GPIO1, GPIO10, GPIO11, GPIO12, GPIO13,
GPIO14, GPIO22.** Eight pins for five legs.

### 4.2 Choosing five from eight

**Routing removes GPIO0 and GPIO1.** Section 5 has the numbers. In short, GPIO0 (module pad 9) and
GPIO1 (pad 10) sit on the module's **left column**, while GPIO10, GPIO11, GPIO12, GPIO13, GPIO14 and
GPIO22 all sit on the **bottom row**, the row facing the legs. A bottom-row pad is one via and about
two millimetres of rear trace from its leg. A left-column pad escapes sideways into a 1.2 mm margin,
runs the height of the board and then across it, and three such vias would consume a margin that can
hold either three traces or a via column, not both. That is roughly a threefold difference in copper
and the difference between zero crossings and seven.

**LP capability removes GPIO22.** Section 2.6 establishes that LP pins are GPIO8 to GPIO14 and no
others, so GPIO22 is the one remaining candidate that cannot wake the chip from deep sleep. It is also
already doing a job as the rear `IO2` log pad, and it is the furthest bottom-row pad from the legs at
x = 12.3 mm.

**That leaves exactly five: GPIO10, GPIO11, GPIO12, GPIO13, GPIO14.** They are a contiguous run, all
reset code `0`, none a strapping pin, none a JTAG pad, all LP-capable, all on the row facing the legs,
and two of them carry the analogue comparator. Contiguity is worth something on its own, because a
contiguous block is materially harder to mis-transcribe into a profile or a firmware pin table than a
scattered set, and `BACKLOG.md` names wrong pin maps as a physical hazard.

**What the alternative would be.** If the DevKitM-1 crystal rework in section 2.4 is judged
unacceptable, the next-best set is GPIO14, GPIO12, GPIO10, GPIO11 and GPIO22, dropping GPIO13. That
halves the devkit rework, at the cost of consuming GPIO22 so the `IO2` log pad has to move to GPIO0 or
GPIO13, giving up the contiguous numbering, and giving up LP capability on one leg. It routes
essentially as well. It is a legitimate second choice and the deciding factor is how much the project
values a stock devkit.

### 4.3 What is lost relative to v1

| | Lost | Gained |
| --- | --- | --- |
| Leg 1 | ADC1_CH3 | reset code `0` instead of `1*`, so **the MTCK after-reset pull-up is gone**. Not a JTAG pad. LP pin. |
| Leg 2 | ADC1_CH4 | Not a JTAG pad. LP pin. |
| Leg 3 | ADC1_CH2 | **Not a strapping pin.** Not a JTAG pad. LP pin. |
| Leg 4 | ADC1_CH1 | **Not a strapping pin.** Not a JTAG pad. LP pin. Comparator external reference. |
| Leg 5 | ADC1_CH0 | Not a JTAG pad. LP pin. **Comparator signal input**, a hardware zero-cross capability the ESP8285 never had. |
| All legs | the option of a 32 kHz crystal on GPIO13 and GPIO14 | all five ADC1 channels move to the spare field, where they are usable |
| All legs | GPIO10 to GPIO14 stop being spares | GPIO0 to GPIO5 become spares, a strictly richer set |
| Constraint | — | **new: VBAT must never be fed separately** (section 2.5) |
| Bring-up | two legs no longer exercisable on a stock DevKitM-1 | — |

The ADC losses are the whole of the functional cost, and the preamble is the case that they are paper
losses. If that argument is wrong then v2 is wrong, so the falsifier is worth stating plainly: **if
anyone finds a TYWE2L-based device that presents an analogue signal on one of the five legs, this
mapping is unsafe for that device and must be revisited.** The ESP8285 could not have read such a
signal, so the device would have to be doing something unusual, but the claim is falsifiable and
should be treated that way.

Three things that were mitigations under v1 can now be reconsidered. None needs to change and this
document does not propose changing any of them:

- **R20 to R24**, the DNP 10 kΩ host-line pull-downs, were fitted because of the GPIO4 pull-up
  (`hardware/DESIGN.md` section 4). With GPIO4 off the host interface they revert to what they were
  originally described as, optional insurance. Keep them DNP.
- **R4**, the GPIO25 pull-up, was justified as safety-critical because GPIO25 could hand four host
  lines to JTAG. It cannot reach a host line under v2. Keep it: it is still correct, still costs
  nothing, and the pin still must not float per module datasheet section 4.3.
- **R2**, the GPIO8 pull-up, was the load-bearing protection for GPIO2 and GPIO3 under the conditional
  recommendation in `h2-strapping-and-reset-states.md`. Those pins are no longer host lines. Keep it:
  pulling GPIO8 high is independently correct, it makes the download strap deterministic, and a
  floating strap is not acceptable in a shipping product.

The pattern is worth naming. Under v1, three resistors were the only things standing between an
uncontrolled host and a boot failure or a spurious actuation. Under v2 they are all belt to the
mapping's braces. That is the real improvement, more than any individual pin.

---

## 5. The leg-order question

Geometry is computed from the two KiCad footprints in `hardware/lib/TYWE2L-2-THREAD.pretty/`, which
were cut from Figure 11 of the ESP32-H2-MINI-1 datasheet page 38 by recovering the vector geometry
from the PDF content stream rather than scaling off a picture (`hardware/DESIGN.md` sections 7 and 8).
Carrier coordinates are **X** = distance right of the left board edge and **H** = height above the
bottom board edge, viewed from the front (module) face.

Placement per `hardware/DESIGN.md` section 7: 15.0 × 13.6 mm carrier, 13.2 × 16.6 mm module body
centred horizontally with its lower edge at H = 2.4, antenna overhanging the top by 5.4 mm. The
transform is X = 7.5 + x_local, H = 10.7 − y_local. Checks: body edges land at X = 0.90 and 14.10, and
the keepout boundary lands at H = 13.600, exactly the carrier top edge, which is the number the
carrier height was reverse-engineered from.

### 5.1 Where the pads are

The pad ring occupies X 1.200 to 13.800 and H 2.700 to 13.300. **Every pad is on the board and no
candidate GPIO is unreachable for geometric reasons.** The keepout imposes no copper-avoidance
constraint, because the keepout region is entirely off the carrier by construction. The thinnest
clearance anywhere is 0.300 mm from the top row (pads 36 to 48, all GND) to the board edge, which
passes most fab minima with nothing to spare.

Which side each candidate sits on is the whole answer:

| Signal | Pad | X | H | Side |
| --- | --- | --- | --- | --- |
| GPIO0 | 9 | 1.600 | 5.600 | **left column** |
| GPIO1 | 10 | 1.600 | 4.800 | **left column** |
| GPIO13 | 12 | 2.700 | 3.100 | bottom row |
| GPIO14 | 13 | 3.500 | 3.100 | bottom row |
| GPIO12 | 16 | 5.900 | 3.100 | bottom row |
| GPIO4 | 18 | 7.500 | 3.100 | bottom row |
| GPIO5 | 19 | 8.300 | 3.100 | bottom row |
| GPIO10 | 20 | 9.100 | 3.100 | bottom row |
| GPIO11 | 21 | 9.900 | 3.100 | bottom row |
| GPIO22 | 24 | 12.300 | 3.100 | bottom row |

Leg pads sit at H = 1.250 with copper from H 0.550 to 1.950, at X = 1.436, 3.436, 5.436, 7.436, 9.436,
11.436 and 13.436 for legs 1 to 7.

### 5.2 The channel, and why it is not the binding constraint

Between the top of the leg pad copper (H 1.950) and the bottom of the module pad copper (H 2.700)
there is a **0.750 mm** channel running the full board width. At 0.15 mm trace and 0.15 mm clearance
it holds exactly two traces with zero margin, or one comfortably with 0.300 mm to spare. Design for
one.

It is not the binding constraint, because the legs are through-hole and therefore present on every
copper layer, and because R10 to R14 sit on the rear anyway, so every host line reaches the rear
regardless of which GPIO it lands on. Each host line is one escape via plus a short front stub plus a
short rear run.

Two numbers to carry into layout. A 0.45 mm escape via placed directly above a leg pad has
**0.000 mm** of margin against the leg pad copper, so put the five escape vias in the five inter-leg
gaps (X = 2.436, 4.436, 6.436, 8.436, 10.436) instead, which buys 0.39 mm of slack for free. And the
gap between adjacent bottom-row module pads is 0.400 mm, which will not pass a 0.15/0.15 trace
(needing 0.450 mm), so there is no front-side inward escape except the two 0.60 mm slots beside the
bottom corner pads.

### 5.3 Does the assignment matter? Within the bottom row, barely. Against the left column, a lot.

Legs 1 to 5 are at X = 1.436, 3.436, 5.436, 7.436, 9.436 in fixed physical order. The recommended
mapping in module-pad-X order is 2.700, 3.500, 5.900, 9.100, 9.900, which is monotonic increasing
against the leg order. **Zero crossings.** Lateral runs are 1.26, 0.06, 0.46, 1.66 and 0.46 mm, and
legs 2 and 4 are close enough to concentric with their pads to be near-vertical stubs. Total copper is
roughly 18 mm.

For comparison, v1's target X sequence was 7.5, 8.3, 1.6, 1.6, 1.6, which is **seven crossings** and
roughly 51 mm of copper, with three vias competing for the 1.2 mm left margin, on a rear face that
already has to hold 18 passives and 15 test pads in about 155 mm² of usable area. Doable, but the kind
of doable that pushes a 2-layer board to 4 layers.

**Within the bottom-row set, routing does not discriminate and should not be used to.** Any monotonic
five-subset of {GPIO13, GPIO14, GPIO12, GPIO4, GPIO5, GPIO10, GPIO11, GPIO22} routes with zero
crossings, and the gap between the best and the runner-up is about three millimetres of copper on a
15 mm board carrying five low-speed signals. That is noise. Pick on reset behaviour and let routing
follow, which is what section 4 does.

**Where routing speaks clearly is against GPIO0 and GPIO1**, and it agrees with the functional
argument. Both point at the bottom row. The agreement is luck rather than design and is worth
recording as luck.

### 5.4 The one assignment that would justify overriding routing

There is an alternative that is worse for routing and better for safety in a different dimension.

Assign leg 1 (ESP8285 GPIO14) to H2 **GPIO14**, leg 2 (ESP8285 GPIO12) to H2 **GPIO12** and leg 3
(ESP8285 GPIO13) to H2 **GPIO13**, keeping legs 4 and 5 on GPIO10 and GPIO11. Three of the five legs
then carry the same GPIO number on both sides of the conversion. And because H2 GPIO4 and GPIO5 are
not host lines under v2, a namespace confusion on legs 4 or 5 refers to a pin that is not in the
profile at all, so it fails loudly rather than silently actuating the wrong leg.

The property this buys: **any confusion between ESP8285 numbering and H2 numbering either lands on the
correct physical leg or fails visibly.** Under any other assignment it silently lands on the wrong
leg. Given `BACKLOG.md` carries "wrong pin maps are a physical hazard. Registry profiles drive relays
in mains appliances", that is not trivial.

The price: the target X sequence becomes 3.5, 5.9, 2.7, 9.1, 9.9, which is non-monotonic, and leg 3's
run crosses legs 1 and 2. On two layers one of the three has to use the front channel, and the front
channel is where legs 1 and 2 want their escape vias, so it gets fiddly. On four layers it disappears.

**This document recommends the monotonic assignment**, because the brief asked routing to act as the
tiebreak among otherwise-equivalent pins, and because the naming hazard has a cheaper mitigation in
the `HOST_GPIOxx` versus `H2_IOxx` net naming the schematic already uses, plus the profile checksums
already planned. The trade is real and the decision belongs to the project owner. If the stackup goes
to four layers for the EPAD ground plane, which is independently a good idea on an 802.15.4 module,
the price drops to nothing and the mnemonic assignment becomes the better choice.

### 5.5 The layout problem that is not a GPIO problem

Recorded because it surfaced in the same analysis and is bigger than anything above. Module 3V3 is pad
3 at (1.600, 10.400), top of the left column. The 3V3 leg is at X = 13.436, bottom right, and GND
corner pad 51 sits directly over that leg blocking a front escape. That is a roughly 21 mm rear route
diagonally across the entire rear face, straight through where C2, C3, C4 and R1 to R4 want to live,
and it exists no matter which GPIOs are chosen. Decouple locally at pad 3 and run the leg feed as a
wide rear trace or an inner-layer pour. **This, not the GPIO assignment, is what is worth spending
four-layer money on.**

---

## 6. Sanity check against the failure mode

For each host behaviour, four hazards have to be cleared: (H1) change boot mode, (H2) force download
mode, (H3) enable JTAG on a host net, (H4) output contention between the chip pad and a host output
stage.

### Case 1: the host holds a leg high through power-up

- **H1 and H2.** None of GPIO10 to GPIO14 appears in `GPIO_STRAP_REG`. TRM Register 6.7 lists bits 0
  to 4 as GPIO2, GPIO3, GPIO8, GPIO9 and GPIO25 and marks bits 5 to 15 invalid, so there is no latch
  for these pins to set. Boot mode is decided by GPIO9, which has an internal weak pull-up (reset code
  `3`) and an external 10 kΩ pull-up (R3), so it reads 1 and selects SPI Boot. TRM Table 8.2-2 row 1
  marks GPIO8, GPIO3 and GPIO2 all `x` in that row. **Cleared.**
- **H3.** None of the five is a JTAG pad. MTMS, MTDO, MTCK and MTDI are GPIO2 to GPIO5, all of which
  v2 leaves in the spare field. This holds even under a burnt `EFUSE_DIS_USB_JTAG` or
  `EFUSE_STRAP_JTAG_SEL_ENABLE`, which is the structural difference from v1. **Cleared, and cleared
  unconditionally rather than by eFuse policy.**
- **H4.** Reset code `0` is IE = 0 and OE = 0. The pad neither drives nor pulls, so the host sees an
  open circuit. **Cleared.**

### Case 2: the host holds a leg low through power-up

Identical reasoning at every step. Nothing about any of the four hazards is polarity-dependent once
the pin is absent from the strapping latch and its pad is inert. **All four cleared.**

### Case 3: the host drives a leg from a push-pull output during our reset

H1, H2 and H3 are as above and unaffected by drive strength or edge rate. For H4 the pad is high
impedance from power-on through reset and on until firmware configures it, so the host's output stage
sees an open circuit. The only current is pad leakage plus ESD structure leakage, in microamps. There
is no window in which the chip contests the net, because there is no default state in which the chip
drives it. **Cleared.**

### Residual risks, stated rather than waved away

1. **Firmware contention is not solved by pin choice and this document does not claim it is.** Once
   the application runs, a wrong profile that configures a leg as an output while the host also drives
   it is direct contention. That is exactly the hazard `BACKLOG.md` records. The defences are the
   profile checksum, the recovery mode, and R10 to R14 as isolation links, all already planned. Pin
   selection buys the window from power-on to firmware and nothing after it.
2. **The 3 ms strapping hold window still exists** (datasheet Table 3-2, t_H minimum 3 ms), but no host
   line is sampled during it under v2, so it is no longer a host-facing concern.
3. **ESP8285 reset states on the five legs are not established, and cannot be from public sources.**
   ESP8266EX Datasheet section 4.1 documents only the pad's *capability* ("Each GPIO PAD can be
   configured with internal pull-up or pull-down ... or set to high impedance", with pull-down
   available on XPD_DCDC alone). The reset default was published in the *ESP8266 Pin List* spreadsheet
   cited by Appendix A of both datasheets, in its *Buffer Sheet* and *Register List* tabs. **That file
   is no longer retrievable**: every URL variant returns the CDP single-page-app shell and the Wayback
   Machine has no usable snapshot, and ESP8266 Technical Reference v1.7's own release note records
   "Deleted the ESP8266 Pin List in Section 2.1". Notably, Espressif *does* call out GPIO2's internal
   weak pull-up specifically in the esptool boot-mode documentation, which is weak circumstantial
   evidence that it is not universal. The defensible engineering reading is that a competently designed
   Tuya-era host circuit cannot have relied on a defined internal pull state, so it must already
   tolerate an **indeterminate** line at power-up, which is a weaker assumption than "weakly pulled
   high". **Flagged as an open question, not resolved by assertion.** It cannot make v2 unsafe, only
   more conservative than strictly necessary. Bench measurement on a real TYWE2L is the only
   resolution.
4. **The device corpora are a sample, not a census.** `blakadder`'s `chip:` field is optional, so the
   eight TYWE2L entries in section 2.7 are a lower bound. A device nobody has ever flashed could use a
   leg in a way the record does not show. This is the falsifier named in section 4.3.
5. **The routing figures are a placement study, not a routed board.** No layout has been committed. All
   the channel arithmetic scales directly with the leg pad diameter, which is still the unverified
   0.9 mm hole and 1.4 mm pad assumption flagged in `hardware/DESIGN.md` section 8 and in
   `BACKLOG.md`. A 1.0 mm hole with a 1.5 mm pad drops the channel from 0.750 to 0.700 mm and kills
   the two-trace case outright.
6. **`EFUSE_DIS_USB_JTAG` stays on the never-burn list** even though it can no longer reach a host
   line. It would still seize GPIO2 to GPIO5 in the spare field and MTDO would still become an output.
   The list in `BACKLOG.md` does not change.
7. **The VBAT domain rule in section 2.5 is new and unenforced.** Nothing in the schematic today stops
   someone feeding VBAT separately in a future revision, and three host lines depend on it not
   happening. It needs a sheet note.

---

## 7. Is GPIO0 genuinely safe on ESP32-H2?

**Yes. GPIO0 has no boot-time, strapping or reset role of any kind on ESP32-H2.** This needs answering
carefully, because on ESP8266, ESP8285 and the original ESP32, GPIO0 is *the* download-mode strap, and
that association is strong enough that people reject the pin from habit.

Five independent lines of evidence, in descending order of strength.

**1. The strapping latch has no bit for it, and its bit list is exhaustive.** TRM v1.1 Register 6.7
`GPIO_STRAP_REG`, offset 0x0038, describes every bit: "bit0: GPIO2, bit1: GPIO3, bit2: GPIO8, bit3:
GPIO9, bit4: GPIO25, bit5 ~ bit15: invalid". A pin latched at reset has a latch bit. GPIO0 has none
and there is no unallocated bit for it to occupy. This is the argument that settles it, and it is the
same argument that proved GPIO2 and GPIO3 *are* latched, so it cuts both ways honestly.

**2. It is absent from the boot mode table.** TRM v1.1 Table 8.2-2 "Boot Mode Control" has four
columns: GPIO9, GPIO8, GPIO3 and GPIO2. Section 8.2.2 page 346 names the same four. TRM section 8.1
page 345 opens "ESP32-H2 has three strapping pins: GPIO8, GPIO9, GPIO25". **GPIO0 appears nowhere in
TRM Chapter 8.**

**3. It is absent from both datasheets' strapping lists.** Datasheet v1.3 section 3 page 22 lists the
strapping pins per boot parameter: chip boot mode GPIO8 and GPIO9, ROM message printing GPIO8, JTAG
signal source GPIO25. Table 3-1 "Default Configuration of Strapping Pins" page 22 has three rows.
Table 3-3 page 23 uses only GPIO8 and GPIO9. Table 3-6 page 25 uses only GPIO25. The module datasheet
v1.6 Table 4 is a verbatim copy. Note that the datasheets are the *less* conservative source here,
since they omit GPIO2 and GPIO3, so their silence on GPIO0 is weak on its own. It matters because it
agrees with the strong evidence rather than contradicting it.

**4. ESP-IDF, the source that was right about GPIO2 and GPIO3, does not flag it.**
`docs/en/api-reference/peripherals/gpio/esp32h2.inc:138` reads "GPIO2, GPIO3, GPIO8, GPIO9, and
GPIO25 are strapping pins", and the GPIO0 row of the summary table at lines 24 to 26 has an **empty
Analog Function cell and an empty Comments cell**, where GPIO2 and GPIO3 say "Strapping pin", GPIO8
and GPIO9 say "Strapping pin, RTC" and GPIO25 says "Strapping pin". This is the strongest *negative*
evidence available, precisely because this file over-reports relative to the datasheet. It caught
GPIO2 and GPIO3 and it does not catch GPIO0. A false negative here would require the one source with
a demonstrated bias toward listing more strapping pins to have missed one.

**5. esptool has no GPIO0 handling for this target.** A case-insensitive search of
`esptool/targets/esp32h2.py` on master for `gpio`, `strap`, `boot`, `USB_JTAG` and `UART_DOWNLOAD`
returns **no GPIO0 reference at all**. The only GPIO mention is line 86, about GPIO26 and GPIO27 being
used by USB Serial/JTAG. There is no download-mode GPIO0 handling because there is no such function.
This is the practical corroboration: the tool that actually puts these chips into download mode has
never heard of GPIO0 on H2.

**Its reset state is the cleanest available.** TRM Table 6.13-1 gives GPIO0 reset code `0`, input
disabled with no pull, `DRV` 2, and datasheet Table 2-1 row 3 has both the At Reset and After Reset
cells blank. `h2-strapping-and-reset-states.md` section 10 notes this as "an even cleaner state than
the datasheet's blank cells implied". There is no window in which the chip touches this pad before
firmware does. Power domain is VDDPST1, the same as GPIO1, GPIO4, GPIO5, GPIO10 and GPIO11.

**Its alternate function is inert on our part, and the reason is not the one usually given.** GPIO0's
F2 is FSPIQ, which is **SPI2**, the general-purpose SPI master and slave. It was never the flash bus.
The in-package flash on the ESP32-H2FH4S uses SPI0 and SPI1 on GPIO15 to GPIO21, which are not fanned
out to any package pin at all (TRM section 6.1 page 217; `esp32h2.inc:141`). So the correct statement
is that GPIO0's F2 is a SPI2 IO MUX path that is free to use as a plain GPIO because nothing on the
module drives SPI2, and the flash is on an entirely separate non-fanned-out bus. Using GPIO0 steals
nothing from the flash.

**Why v2 does not use it anyway.** Two reasons, neither of them safety. GPIO0 is module pad 9 on the
left column at (1.600, 5.600) in carrier coordinates, and reaching it from a bottom-edge leg costs an
order of magnitude more copper than reaching a bottom-row pad (section 5.3). And it is not an LP pin,
so it cannot wake the chip from deep sleep, where all five recommended pins can (section 2.6). **GPIO0
is rejected on routing and capability, not on safety**, and it remains an excellent choice for a test
pad, a spare, or the new home for the `IO2` log pad if GPIO22 is ever promoted to a host line.

**The residual uncertainty, stated.** The answer above rests on documentation, ESP-IDF source and
esptool source rather than on ROM disassembly. No sweep of the boot ROM for a GPIO0 read was done.
That is worth naming rather than glossing over, because the ESP32-C6 precedent in
`h2-strapping-and-reset-states.md` section 9 is a case where a real undocumented behaviour was found
only by disassembling the boot ROM. The difference is that in the C6 case the strapping register *did*
have a bit for the pin in question and the documentation had omitted it. Here there is no bit, and an
exhaustive register bit map is a stronger negative than an absent table row.

---

## What this document does not settle

In rough order of how much each could change the answer.

0. **THE TYWE2L LEG HOLE DIAMETER. STILL OPEN, AND IT BLOCKS FABRICATION RATHER THAN THE MAPPING.**
   Listed first because it is the only item here that must be closed before boards are ordered. The
   0.9 mm hole and 1.4 mm pad in `TYWE2L_Legs_7P_2.0mm.kicad_mod` are an assumption: Tuya publishes
   no leg diameter anywhere in the datasheet. Every routing figure in section 5 scales directly off
   it, and a 1.0 mm hole with a 1.5 mm pad drops the leg-to-module channel from 0.750 to 0.700 mm
   and kills the two-trace case outright. **Caliper a physical TYWE2L before fabrication.**
   `hardware/DESIGN.md` open item 1 and section 8, and `BACKLOG.md`.
1. ~~**LEDC-253 on v1.2 silicon.**~~ **CLOSED.** Resolved in
   [`docs/ledc-erratum.md`](./ledc-erratum.md). LEDC-253 affects v0.0 and v0.1 only and is fixed in
   v1.2. The apparent conflict was a stale ESP-IDF documentation version: the revision qualifier was
   added in December 2024 and is present at v5.4.2 and later, including the pinned v5.5.4. There is
   no revision-gated driver logic because there is no workaround code in the driver to gate, the
   full duty range always having been permitted. The erratum has no pin dimension and LEDC routes
   purely through the GPIO matrix, so nothing here constrains the mapping. No bench check is needed
   before committing the PCB, though `ledc-erratum.md` section 9 gives a ten-minute belt-and-braces
   procedure. The mitigation still costs nothing: do not run at the timer's maximum duty resolution.
   Section 3.3.
2. **PCNT-249 versus `SOC_PCNT_SUPPORT_STEP_NOTIFY`.** The errata marks v1.2 affected, the ESP-IDF
   soc cap comment says the feature is "only avliable in chip version above 1.2". Do not rely on step
   notify until this is resolved. Section 3.4.
3. **ESP8285 pin reset states on the five legs.** Unresolvable from public documentation, because the
   source spreadsheet no longer exists anywhere. Bench measurement is the only route. Section 6,
   residual risk 3.
4. **How many analogue comparators the ESP32-H2 has.** Datasheet section 4.2.2.3 says two, the TRM,
   `SOC_ANA_CMPR_NUM` and the IDF driver all say one, and the datasheet's own pin assignment paragraph
   contradicts its own count. Treated as one. Section 2.3.
5. **The VDDPST2-derived comparator reference against VDDPST1-powered pads.** A genuine cross-domain
   reference in TRM section 6.15, theoretical on the MINI-1 because both rails come from the same 3V3
   net. Section 2.3.
6. **Chip revision v0.2 (ECO2)** appears in `Kconfig.hw_support` but nowhere in the errata. Unexplained.
   And "v1.2 is the current mass-production revision" is inferred rather than quoted. Section 3.6.
7. **The `SOC_GPIO_VALID_DIGITAL_IO_PAD_MASK` exclusion of GPIO7 to GPIO14.** No functional consequence
   found for ordinary GPIO use, but it means ESP-IDF classifies all five recommended pins as
   non-digital-domain pads, and some API could in principle reject one on that basis. Worth a smoke
   test during bring-up. Section 2.5.
8. **Whether the TYWE2L's rear pads are TX, RX, RST, IO0, IO2 and GND as the project believes.** Tuya's
   own Table 2 lists only `IO2`, `RST` and `IO0`, and omits TX, RX and GND, which community teardowns
   confirm physically exist. Tuya's table is incomplete rather than the project being wrong, but the
   figures on the datasheet's pages 6 and 7 are raster images that could not be read to confirm the
   silkscreen labels.

## Knock-on updates needed elsewhere

**DONE.**

- `hardware/DESIGN.md`: mapping table and rationale rewritten in section 3 with the v1 reasoning
  kept as a recorded failure; section 4 rewritten around the GPIO4 veto; section 2 carries the VBAT
  rule and an amendment to the GPIO25 argument; section 6 carries the drive-strength data from
  section 2.9; the rear-pad table shows the recut spare field; open items 1 and 4 are closed and
  moved to a "Closed items" block, and the leg hole diameter is now open item 1 and prominent.
- `hardware/carrier/tywe2l-h2-carrier.kicad_sch`: the five host nets moved to H2 GPIO13, GPIO14,
  GPIO12, GPIO10 and GPIO11; TP10 to TP15 recut to GPIO0 to GPIO5 with GPIO2 and GPIO3 labelled as
  strapping pins; the "HOST LEG MAPPING" and "WHY THESE FIVE PINS" notes rewritten; new notes on the
  ESP8285-versus-H2 naming hazard and on VBAT never being fed separately; the R4, R20 to R24, rear
  test pad and open-item notes brought into line.

**ALSO DONE.** The following were outstanding at the time this document was written and have since
been applied.

- `docs/devkit-bringup.md`: rewritten around a separate **bench prototyping pin set**, because a
  stock ESP32-H2-DevKitM-1 cannot carry v2 in full: GPIO13 and GPIO14 are taken by the on-board
  32.768 kHz crystal X1. Legs 3, 4 and 5 use the real production pins (GPIO12, GPIO10, GPIO11);
  legs 1 and 2 use GPIO22 and GPIO1 as stand-ins. GPIO4 is excluded from the bench set as well as
  the board, since its 200 to 400 ms MTCK pull-up can actuate a host relay on a bench rig exactly as
  readily as on the finished carrier. The substitution costs the discovery work nothing, because the
  prototype exists to find out what the host device does with each leg, which is independent of
  which H2 pin is on the other end of the wire.
- `docs/flasher-and-registry.md`: sections 1.5, 3.4 and 5.1.
- `docs/tasmota-import.md`: sections 1.2, 3.1, 3.2, the section 3.4 worked example, and 5.4.
- `tools/tasmota-import/README.md`: the v1 map appeared twice, in the converter's `pin_map` config
  block and in the emitted-profile JSON example. That second one mattered most, being the
  specification a tool would be written from.
- `BACKLOG.md`: restructured. The leg hole diameter is now stated at the top as the only item
  blocking fabrication, v2 is recorded as settled with the reasoning retained, and the VBAT rule and
  the document index are added.

A crossed-namespace warning was added wherever a profile record appears, since ESP8285 GPIO12 maps
to H2 GPIO14 and ESP8285 GPIO13 maps to H2 GPIO12. Profile validation should key on `pad`, which
equals the leg number and is unambiguous.
