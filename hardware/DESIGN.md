<!-- SPDX-License-Identifier: CERN-OHL-S-2.0 -->

# TYWE2L-2-THREAD carrier: design record

A carrier PCB that mounts an Espressif **ESP32-H2-MINI-1-H4S** and is a physical and electrical
drop-in replacement for a **Tuya TYWE2L** ESP8285 Wi-Fi module, converting Wi-Fi Tuya devices to
Matter-over-Thread.

Licence: CERN-OHL-S-2.0.

Sources used, both in `hardware/datasheets/`:

* *TYWE2L Module Datasheet*, Tuya, version 20210119 (20 PDF pages, printed pagination 1..18).
* *ESP32-H2-MINI-1 & ESP32-H2-MINI-1U Datasheet*, Espressif, v1.6 (45 PDF pages).

Every figure below was pulled out of those PDFs rather than taken on trust. The Tuya mechanical
drawings are raster images and were read visually; the Espressif figures are vector art, so the
land-pattern geometry was recovered from the PDF content stream and is exact rather than scaled off
a picture. Page numbers cited are the printed page numbers on each document.

---

## Read this first: four findings that shape the design

### The pin mapping changed. v2 puts the five host legs on GPIO10 to GPIO14.

The v1 mapping put two host legs on **boot-mode strapping pins**, and it did so in order to buy ADC
capability that no TYWE2L device can ever have used. Both halves are now settled in
[`docs/pin-mapping-v2.md`](../docs/pin-mapping-v2.md), which is the authority on the mapping.
Section 3 below and `carrier/tywe2l-h2-carrier.kicad_sch` both carry v2.

| Leg | TYWE2L net | v1, superseded | v2 | Module pin |
| --- | --- | --- | --- | --- |
| 1 | GPIO14 | GPIO4 | **GPIO13** | 12 |
| 2 | GPIO12 | GPIO5 | **GPIO14** | 13 |
| 3 | GPIO13 | GPIO3, a strapping pin | **GPIO12** | 16 |
| 4 | GPIO5 | GPIO2, a strapping pin | **GPIO10** | 20 |
| 5 | GPIO4 | GPIO1 | **GPIO11** | 21 |

The v1 reasoning is kept in section 3 rather than deleted, because the way it failed is the part
worth carrying forward. It optimised hard against a constraint that was never binding, and it took
one datasheet table as the complete strapping-pin list when it was not.

One new constraint comes with v2: **VBAT must never be fed from a separate supply**, because three
of the five host legs now sit on the VBAT/VDDA_PMU power domain. See section 2.

### The antenna keepout is 5.4 mm, not 4.1 mm

The obvious way to size the keepout is to subtract the MINI-1U's 12.5 mm body length from the
MINI-1's 16.6 mm and call it 4.1 mm. **That is wrong.** The MINI-1U retains roughly 1.3 mm of body
past the end of its pad field for the U.FL connector, so the length difference understates the
keepout. Read off the vector geometry of Figure 11 (page 38), the pad field ends and the antenna
area begins 11.2 mm from the far edge on both variants, which the drawing's own `11.2` callout
confirms. **The keepout is the top 5.4 mm of the module, across the full 13.2 mm width.**

### Honouring it makes the assembly about 1.7 mm taller, and that is forced

Keeping all FR4 and all copper out from under a 5.4 mm keepout pins the geometry:

| | TYWE2L | This carrier |
| --- | --- | --- |
| Width | 15.0 mm | **15.0 mm**, exact match |
| Thickness | 3.0 mm | **0.6 mm PCB + 2.4 mm module = 3.0 mm**, exact match |
| Carrier height | n/a | **13.6 mm** |
| Overall board height | 17.272 mm | **19.0 mm** |

That is **roughly 1.7 mm taller than the part being replaced**, and it is a consequence of the
keepout rather than a design choice. The brief's target of a ~15 mm carrier and a ~18.6 mm assembly
cannot both hold once the keepout is 5.4 mm: a 15 mm carrier gives a 20.4 mm board, and an 18.6 mm
board needs the carrier down at 13.2 mm with FR4 under the antenna. Width and stack thickness still
match the original exactly, so only the height changes. Full working in section 7.

### GPIO25 must be pulled UP. Safety-critical, not a component preference.

Table 9 on page 14 shows that when `EFUSE_STRAP_JTAG_SEL_ENABLE` has been burnt to 1,
**GPIO25 = 0 selects the JTAG pads, and those pads are MTDI, MTCK, MTMS and MTDO, which on
ESP32-H2 are GPIO5, GPIO4, GPIO2 and GPIO3.** Under the v1 mapping that was four of the five pins
this carrier handed to the host device.

So a pull-down on GPIO25 would, on any v1 board with that eFuse burnt, hand four live host nets to
the JTAG peripheral during early boot. In a mains product those nets may be driving relays or triacs.
**R4 is a 10 kOhm pull-UP.** On a factory part the resistor changes nothing; on a burnt part it
keeps the host lines as ordinary GPIO. There is no configuration of this product in which pulling
GPIO25 low is acceptable. Do not "tidy" it to a pull-down. Reasoning in full in section 2.

**Amended by pin mapping v2, and the conclusion does not change.** Under v2 the four JTAG pads sit
in the spare test-pad field rather than on host legs, so a pull-down could no longer reach a host
net. R4 stays a pull-up all the same: the pin has no internal pull and must not float, and keeping
the spare pads ordinary GPIO is worth the same nothing it always cost. What changed is that R4 is
no longer the thing standing between a burnt eFuse and a live relay line. The mapping is.

---

## Deliverables in this directory

| File | What it is |
| --- | --- |
| `lib/TYWE2L-2-THREAD.kicad_sym` | Symbol library: `ESP32-H2-MINI-1-H4S` (53 pins), `TYWE2L_Host_Legs_7P`, plus `R`, `C`, `TestPoint`, `+3V3`, `GND` so the project is self-contained |
| `lib/TYWE2L-2-THREAD.pretty/ESP32-H2-MINI-1.kicad_mod` | Module land pattern from Figure 11 |
| `lib/TYWE2L-2-THREAD.pretty/TYWE2L_Legs_7P_2.0mm.kicad_mod` | 7-leg through-hole host interface |
| `carrier/tywe2l-h2-carrier.kicad_sch` | Full schematic, A3, with the mapping table and rationale on the sheet |
| `carrier/tywe2l-h2-carrier.kicad_pro` | Project file |
| `carrier/sym-lib-table`, `carrier/fp-lib-table` | So the project resolves `TYWE2L-2-THREAD:*` |

The KiCad S-expression format has no comment syntax, so the SPDX identifier is carried in the
schematic title block (`comment 1`), in each footprint's `descr`, in the symbol `Description` and
`License` properties, in the library table `descr` fields, and in `text_variables` in the project
file.

---

## 1. TYWE2L facts, verified

From the mechanical drawings, Figure 2 (page 4) and Figure 3 (page 14 of the printed document,
PDF page 16):

| Fact | Value | Where |
| --- | --- | --- |
| Board size | 15.0 mm W x 17.272 mm H x 3.0 mm thick | Fig. 2 and Fig. 3 dimension callouts; body text on page 4 rounds this to "3 mm (W) x 15 mm (L) x 17.3 mm (H)" |
| Legs | 7, single row along the bottom edge, 2.0 mm pitch | Fig. 2, Table 1 page 4 |
| Leg protrusion | 3.454 mm below the board edge | Fig. 3 |
| Outer leg centres | 1.436 mm from the left edge, 1.564 mm from the right | Fig. 3 |
| Leg span | 6 x 2.0 mm = 12.000 mm, and 1.436 + 12.000 + 1.564 = 15.000 exactly | derived, self-consistent |
| Leg order, front (antenna) face, left to right | GPIO14, GPIO12, GPIO13, GPIO5, GPIO4, GND, 3V3 | Fig. 2 front view, Table 1 |
| Rear test pads | GND, TX, RX, GND (upper), IO0, IO2, RST (lower) | Fig. 2 rear view, Table 2 page 5 |
| Supply | 3.0 / 3.3 / 3.6 V min/typ/max | Table 4, page 7 |
| VIH / VIL | 0.75 x VCC min / 0.25 x VCC max | Table 4 continued, page 8 |
| VOH / VOL | 0.8 x VCC min / 0.1 x VCC max | page 8 |
| I/O drive | 12 mA max | page 8 |
| Peak supply current | 451 mA (AP mode maximum) | Table 7, page 9 |

Because the leg row spans 12.000 mm inside a 15.000 mm board with unequal edge offsets, the row
sits **0.064 mm to the left of centre**. That asymmetry is in the real part and is reproduced in
`TYWE2L_Legs_7P_2.0mm.kicad_mod`. It has deliberately not been "corrected"; the centre leg is at
x = -0.064 mm relative to the board centreline.

### One disagreement found in the TYWE2L datasheet

Page 4 states "TYWE2L provides two rows of pins with a distance of 2.0 mm between every two pins."
Figures 2 and 3 and Table 1 all show a **single** row of seven legs. The rear face carries test
pads, not a second leg row. The brief's "seven legs in a single row" is right and the prose on
page 4 is wrong. Treated as a Tuya documentation defect, no design impact.

## 2. ESP32-H2-MINI-1-H4S facts, verified

| Fact | Value | Where |
| --- | --- | --- |
| Part | ESP32-H2-MINI-1-H4S, 4 MB Quad SPI flash, -40 to 105 degC | Table 1, page 6 |
| Size | 13.2 x 16.6 x 2.4 mm | Table 1, page 6; Figure 8, page 35 |
| MINI-1U for comparison | 13.2 x 12.5 x 2.4 mm | Table 1, page 6 |
| Pin count | 53 | Section 3.2, page 9 |
| GND | 1, 2, 11, 14, 36..53 | Table 3, page 10 |
| 3V3 | 3 | Table 3 |
| NC | 4, 7, 17, 28, 29, 32..35 | Table 3 |
| EN | 8, "Do not leave the EN pin floating" | Table 3 |
| VBAT | 15 | Table 3 |
| IO2=5, IO3=6, IO0=9, IO1=10, IO13=12, IO14=13, IO12=16, IO4=18, IO5=19, IO10=20, IO11=21, IO8=22, IO9=23, IO22=24, IO25=25, IO26=26, IO27=27 | | Table 3 |
| RXD0 (GPIO23) = 30, TXD0 (GPIO24) = 31 | | Table 3 |
| ADC1 | IO1=CH0, IO2=CH1, IO3=CH2, IO4=CH3, IO5=CH4, and no others | Table 3 |
| USB Serial/JTAG | IO26 = USB_D-, IO27 = USB_D+ | Table 3 |
| JTAG pads | MTMS=IO2, MTDO=IO3, MTCK=IO4, MTDI=IO5 | Table 3 |
| Strapping (per this datasheet) | GPIO8 floating, GPIO9 weak pull-up, GPIO25 floating | Table 4, page 11 |
| Boot mode | SPI Boot when GPIO9 = 1; Joint Download Boot when GPIO8 = 1 and GPIO9 = 0 | Table 6, page 12 |
| Pull-up / pull-down resistor | 45 kOhm typical | Table 12, page 24 |
| Supply current required | 0.35 A minimum from the external supply | Table 11, page 24 |
| Peak TX current | 123 mA at 802.15.4 @ 18 dBm | Table 14, page 25 |

Every number the brief supplied matched the datasheet. Nothing in the pin list needed correcting.

**One row above is incomplete, and it cost the v1 pin mapping.** The strapping row reads "per this
datasheet" for a reason. Module datasheet Table 4 names GPIO8, GPIO9 and GPIO25, and the chip
datasheet section 3 agrees, but the **Technical Reference Manual does not**. TRM v1.1 Register 6.7
enumerates the strapping latch as "bit0: GPIO2, bit1: GPIO3, bit2: GPIO8, bit3: GPIO9, bit4:
GPIO25", and section 8.2.2 says the reset values of GPIO9, GPIO8, GPIO3 and GPIO2 select the boot
mode. The register bit map is the authority; the datasheet table is a summary that omits two pins.
Read it as a lower bound, never as the complete list.

### VBAT, answered

Table 3, page 10, describes pin 15 as *"Connected to internal 3V3 power supply (Default) or
external battery power supply (3.0 ~ 3.6 V)."* The module ties VBAT to its own 3V3 rail unless you
choose to feed it separately. **No external tie is required and none is fitted.** VBAT carries a
no-connect flag in the schematic so ERC stays clean, with a note on the sheet explaining why.

#### Under v2 this stops being a default and becomes a rule

H2 GPIO12, GPIO13 and GPIO14, which are legs 3, 1 and 2, are powered from the **VBAT / VDDA_PMU**
domain rather than VDDPST1. The evidence is the "Pin Providing Power" column of ESP32-H2 datasheet
v1.3 Table 2-1, Table 2-7 "Power Pins", and Figure 2-2 "ESP32-H2 Power Scheme". Table 5-3 note 1
defines VIH, VIL, VOH and VOL against "a power pin of a *respective power domain*", so a pin's
thresholds track its own domain and nothing else.

Today that is inert. The module schematic (page 32) ties VBAT to VDD33 through a 0 Ohm link, this
carrier leaves pin 15 unconnected, and all five legs therefore sit at the same 3.3 V.

**The rule: VBAT must never be fed from a separate supply while host lines sit on GPIO12 to
GPIO14.** Feed it from a battery and three of the five host legs take different input thresholds
and different output swings from the other two, on the same host connector, with the host circuit
none the wiser. Under v1 the situation could not arise, because all five legs were on VDDPST1. It
can now, so it is written on the schematic sheet as a hard rule rather than left as a default that
looks optional. Changing it means re-doing the pin mapping first. Full working in
`docs/pin-mapping-v2.md` section 2.5.

### GPIO25, and why the pull resistor goes up (safety-critical)

Section 4.3, page 13: *"The strapping pin GPIO25 can be used to control the source of JTAG signals
during the early boot process. This pin does not have any internal pull resistors and the strapping
value must be controlled by the external circuit that cannot be in a high impedance state."* So the
resistor is mandatory, which the brief already had.

The direction comes from Table 9, page 14. With factory eFuses (`EFUSE_DIS_PAD_JTAG` = 0,
`EFUSE_DIS_USB_JTAG` = 0, `EFUSE_STRAP_JTAG_SEL_ENABLE` = 0) GPIO25 is ignored and the JTAG source
is the USB Serial/JTAG controller. Once `EFUSE_STRAP_JTAG_SEL_ENABLE` is burnt to 1, GPIO25 chooses:

* GPIO25 = 0 selects the **JTAG pins**, footnoted as MTDI, MTCK, MTMS and MTDO.
* GPIO25 = 1 selects the **USB Serial/JTAG controller**.

**MTDI, MTCK, MTMS and MTDO are GPIO5, GPIO4, GPIO2 and GPIO3, which under the v1 mapping was four
of the five pins this design handed to the host device.** A pull-down on GPIO25 would therefore have
meant that any part with `EFUSE_STRAP_JTAG_SEL_ENABLE` burnt hands four live host nets to the JTAG
peripheral during early boot, with the host's own circuitry still attached to them. In a mains
product those nets may be driving relays or triacs, so this was a safety consideration and not a
component preference.

**R4 is a 10 kOhm pull-UP.** On a factory part, where that eFuse is 0, the resistor changes nothing
at all. On a part where the eFuse has been burnt it keeps those four lines as ordinary GPIO. The
pull-up costs nothing and removes the failure mode entirely, so there is no configuration of this
product in which pulling GPIO25 low is acceptable. Anyone reviewing this schematic and reading R4 as
a stray pull-up that should be a pull-down, or as a part that could be depopulated, should read this
section first: it is neither.

**What v2 changes, and what it does not.** Pin mapping v2 moves the host legs to GPIO10 to GPIO14,
so all four JTAG pads now sit in the spare test-pad field and a GPIO25 pull-down could no longer
reach a host net. The remedy has moved from a resistor to the mapping itself, which is the stronger
place for it: it holds unconditionally rather than depending on a part being fitted. R4 does not
change. GPIO25 still has no internal pull, still must not float, the pull-up still costs nothing,
and keeping the spare pads as ordinary GPIO is still worth having. The same goes for
`EFUSE_DIS_USB_JTAG`, which `BACKLOG.md` calls the dangerous one: it stays on the never-burn list
even though it can no longer seize a host line, because it would still take GPIO2 to GPIO5 in the
spare field and would still make MTDO a chip output.

## 3. Pin mapping, v2

Authority: [`docs/pin-mapping-v2.md`](../docs/pin-mapping-v2.md). Implemented in
`carrier/tywe2l-h2-carrier.kicad_sch`.

| Leg | TYWE2L net | Series link | ESP32-H2 signal | Module pin | Reset code | Power domain | LP pin |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | GPIO14 | R10 | GPIO13 | 12 | `0` | VDDA_PMU/VBAT | yes |
| 2 | GPIO12 | R11 | GPIO14 | 13 | `0` | VDDA_PMU/VBAT | yes |
| 3 | GPIO13 | R12 | GPIO12 | 16 | `0` | VDDA_PMU/VBAT | yes |
| 4 | GPIO5 | R13 | GPIO10 | 20 | `0` | VDDPST1 | yes |
| 5 | GPIO4 | R14 | GPIO11 | 21 | `0` | VDDPST1 | yes |
| 6 | GND | - | GND | 1, 2, 11, 14, 36..53 | - | - | - |
| 7 | 3V3 | - | 3V3 | 3 | - | - | - |

Reset codes are TRM v1.1 Table 6.13-1 page 235, where `0` means input disabled, output disabled and
no pull. Power domains are datasheet v1.3 Table 2-1 and Table 2-7. Module pin numbers are
ESP32-H2-MINI-1 datasheet v1.6 Table 3 page 10.

The 0 Ohm links R10 to R14 and the DNP pull-downs R20 to R24 sit on the host lines and simply follow
them to the new pins. Nothing else in the schematic moves.

### Rationale

1. **None of the five is a strapping pin.** TRM v1.1 section 8.2.2: "the values of GPIO9, GPIO8,
   GPIO3 and GPIO2 at reset determine the boot mode after the reset is released". Register 6.7
   enumerates the whole strapping latch as "bit0: GPIO2, bit1: GPIO3, bit2: GPIO8, bit3: GPIO9,
   bit4: GPIO25, bit5 ~ bit15: invalid", so the list is exhaustive and GPIO10 to GPIO14 have no bit
   to set. Nothing the host does to a leg at power-up can change boot mode or force download mode.
2. **All five come up at reset code `0`.** The pad neither drives nor pulls from power-on until
   firmware configures it, so the host sees an open circuit and there is no window in which the chip
   contests the net. Under v1, three legs sat at code `1` (input buffer enabled) and one at `1*`
   (input enabled *and* pulled up).
3. **None is a JTAG pad.** MTMS, MTDO, MTCK and MTDI are GPIO2 to GPIO5, and v2 leaves all four in
   the spare field. `EFUSE_DIS_USB_JTAG`, which `BACKLOG.md` calls the dangerous one, can no longer
   reach a host net whether it is burnt or not. Under v1 it could seize four of the five.
4. **GPIO4 is vetoed outright as a host line.** Its MTCK after-reset weak pull-up is present
   whenever `EFUSE_DIS_PAD_JTAG` = 0, which is every factory part, and it holds from reset release
   through ROM bootloader, second-stage bootloader and app init, on the order of 200 to 400 ms.
   Into a relay driver or a logic-level MOSFET gate that is long enough to actuate. See section 4.
5. **All five are LP pins** (GPIO8 to GPIO14 per datasheet section 2.2), so every leg can wake the
   chip from deep sleep via EXT1. Not one v1 leg could. The product does not need it today, being a
   mains-powered Thread router node, but it is capability the design now has for nothing.

Contiguity is a small bonus in its own right: a block of five consecutive numbers is harder to
mis-transcribe into a firmware pin table than a scattered set, and `BACKLOG.md` names wrong pin maps
as a physical hazard.

### The v1 rationale, recorded because the failure is the reusable part

v1 mapped the five legs onto GPIO1 to GPIO5 for two stated reasons, and **both were wrong**. They
are kept here rather than deleted.

**"Keep an ADC-capable pin on every leg" was answering a need that has never existed.** GPIO1 to
GPIO5 are indeed the only ADC1-capable pins on ESP32-H2, and that part was right. What was never
checked is whether a host could present an analogue signal in the first place. It cannot. The
ESP8266EX has exactly one ADC pin, TOUT, given a single row in Datasheet v7.1 Table 4-9 and typed
as input-only in Table 2-1, and **the TYWE2L does not bring it out**: the TYWE2L datasheet section
2.2 Table 1 lists seven interface pins (GPIO14, GPIO12, GPIO13, GPIO5, GPIO4, GND, 3V3), section
2.3 Table 2 lists three test pins, and section 1.1 says "Peripherals: five GPIOs". There is no TOUT
anywhere in the document. The project's own corpus work had already recorded the consequence without
drawing the conclusion, in `docs/tasmota-import.md` section 3: every one of the 202 templates asking
for ADC0 is classified as not fitting a TYWE2L "because a template that asks for ADC0 is asking for
the ESP8285's TOUT pin, which the TYWE2L does not break out".

There is a sharper version of the point, worth keeping. An ADC input whose net is driven by a host
output stage is not an ADC input. Analogue capability on a host-facing leg is dead weight in both
directions. So v1 spent all five legs on a constraint that was never binding, and the price it paid
was two legs on strapping pins.

**"None of the five is a strapping pin" rested on an incomplete list.** The module datasheet Table 4
(page 11) names GPIO8, GPIO9 and GPIO25 and nothing else, and that table was taken as the whole
list. The TRM says otherwise and the TRM is the authority. GPIO2 and GPIO3 are latched, so legs 4
and 3 of v1 were sitting on boot-mode straps. Evidence in
[`docs/h2-strapping-and-reset-states.md`](../docs/h2-strapping-and-reset-states.md) sections 2 and 5.

Two lessons, both cheap to apply next time. A datasheet table that lists a set is not evidence that
the set is complete; find the register that enumerates it. And a capability argument needs the
other end of the wire checked before it is allowed to drive a decision.

**Falsifier, stated plainly.** If anyone finds a TYWE2L-based device that presents an analogue
signal on one of the five legs, the v2 mapping is unsafe for that device and must be revisited. The
ESP8285 could not have read such a signal, so the device would have to be doing something unusual,
but the claim is falsifiable and should be treated that way.

### What v1's mitigations become under v2

None of them changes on the board, and none is removed.

* **R20 to R24**, the DNP 10 kOhm host-line pull-downs, were fitted because of the GPIO4 pull-up.
  With GPIO4 off the host interface they revert to what they were originally described as: optional
  insurance for an unknown host. Keep them DNP.
* **R4**, the GPIO25 pull-up, can no longer reach a host line. Keep it. The pin still must not float
  and the pull-up still costs nothing.
* **R2**, the GPIO8 pull-up, was load-bearing for host-line safety under v1. It is not now. Keep it:
  pulling GPIO8 high is independently correct and a floating strap is not acceptable in a shipping
  product.

Under v1, three resistors were the only things standing between an uncontrolled host and a boot
failure or a spurious actuation. Under v2 they are all belt to the mapping's braces. That is the
real improvement, more than any individual pin.

### The naming hazard v2 introduces, and how the schematic handles it

ESP8285 GPIO12, GPIO13 and GPIO14 are **leg nets**. H2 GPIO12, GPIO13 and GPIO14 are **module
pins**. Under v2 both appear in the same design and they are **crossed**: the leg carrying ESP8285
GPIO14 goes to H2 GPIO13, and the leg carrying ESP8285 GPIO12 goes to H2 GPIO14. The schematic's
`HOST_GPIOxx` versus `H2_IOxx` net naming keeps the two namespaces apart and must be kept. Anyone
writing a profile, a firmware pin table or a test script has to say which namespace a number belongs
to, every time. There is a note on the sheet saying so.

`docs/pin-mapping-v2.md` section 5.4 sets out an alternative assignment that removes the hazard at
the cost of two trace crossings. It is a genuine choice and it belongs to the project owner. This
document records the monotonic assignment, which is what has been drawn.

### Rear test pads

| TYWE2L rear pad | Goes to | Module pin | Why |
| --- | --- | --- | --- |
| TX | TXD0 | 31 | same role |
| RX | RXD0 | 30 | same role |
| RST | EN | 8 | same role |
| IO0 | GPIO9 | 23 | GPIO9 is the ESP32-H2 download-mode strap, so "hold IO0 low through reset to flash" still works |
| IO2 | GPIO22 | 24 | spare / secondary log, matching IO2's role as the Tuya log pin |
| GND x2 | GND | - | as original |

Added beyond the original: TP8 and TP9 bring GPIO26 and GPIO27 out as USB D- and D+ for native USB
Serial/JTAG flashing, and TP10..TP15 are a labelled spare field.

**The spare field was recut for v2 and is strictly better than it was.** It used to carry GPIO0 and
GPIO10 to GPIO14. Those five are now host legs, so the field picks up the pins v2 freed:

| Pad | Signal | Module pin | Notes |
| --- | --- | --- | --- |
| TP10 | GPIO0 | 9 | Reset code `0`. No boot, strapping or reset role of any kind on ESP32-H2 |
| TP11 | GPIO1 | 10 | ADC1_CH0 |
| TP12 | GPIO2 | 5 | ADC1_CH1, MTMS. **Boot-mode strapping pin, `GPIO_STRAP_REG` bit 0** |
| TP13 | GPIO3 | 6 | ADC1_CH2, MTDO. **Boot-mode strapping pin, `GPIO_STRAP_REG` bit 1** |
| TP14 | GPIO4 | 18 | ADC1_CH3, MTCK. Weak internal pull-up after reset on every factory part |
| TP15 | GPIO5 | 19 | ADC1_CH4, MTDI. ADC1_CH4 is dead on chip revisions v0.0 and v0.1, erratum ADC-7227, fixed in v1.2 |

Under v1 the carrier had no analogue pin available to itself. It now has five, which is the one
place the ADC1 pins are genuinely worth something: a future revision adding a sensor has somewhere
to put it. TP12 and TP13 are labelled `GPIO2 STRAP` and `GPIO3 STRAP` on the schematic and in their
`Assembly` fields, because a load, a pull or a driven signal on either of them changes boot mode.
TP14 is not a strap but is not clean either, and the sheet note says so.

## 4. The deviation from the ESP8285 that v2 removes at source

**The GPIO4 pull-up is confirmed, and the response is to stop using the pin.** TRM v1.1 Table 6.13-1
carries the footnote verbatim: "1* - If `EFUSE_DIS_PAD_JTAG` = 1, the pin MTCK is left floating
after reset, i.e., IE = 1. If `EFUSE_DIS_PAD_JTAG` = 0, the pin MTCK is connected to internal
pull-up resistor, i.e., IE = 1, WPU = 1." ESP32-H2 datasheet v1.3 Table 2-1 footnote 4 says the
same. The eFuse ships as 0 on every part and `BACKLOG.md` carries a commitment never to burn it, so
the pull-up is present on every unit that will ever ship. What was recorded here as reported and
unverified is now verified.

The window is not the reset assertion. It runs from reset release through ROM bootloader, second
stage bootloader and app init, on the order of **200 to 400 ms**. Into a logic-level MOSFET gate
with no external pull-down, `docs/h2-strapping-and-reset-states.md` sections 10 and 11 put it
plainly: the load "energises and stays energised for the entire few hundred milliseconds until
firmware drives the pin low. This is a definite, not marginal, failure."

Under v1, GPIO4 was leg 1 and this was a real break in electrical equivalence that the ESP8285 did
not have. **Under v2 GPIO4 is not a host line at all.** Because the project has deliberately
deferred finding out what any leg actually does until the firmware dump work, every leg has to be
treated as potentially the actuator line, so the per-leg recommendation generalises into a per-pin
veto: no leg goes on GPIO4, on any revision of this board. All five v2 legs are reset code `0`,
neither driving nor pulling, which is a cleaner state than the ESP8285 leg it replaces, whose reset
state cannot be established at all (see open item 4).

The community device record sharpens what "worst case" means here without softening it. Every
TYWE2L device found in the `blakadder` and `esphome-devices` corpora is a light driving PWM channels
into MOSFET gates, which is exactly the topology the pull-up defeats. The failure mode is a lamp at
full brightness for a few hundred milliseconds on every power-up rather than a mains relay clicking.
Less dangerous, more visible, still a defect, and a relay device nobody has flashed with Tasmota
would not appear in that evidence at all.

**R20 to R24 stay, DNP.** They are 10 kOhm pull-downs, one on each of the five host lines, on the
host side of the 0 ohm links. Host side was chosen deliberately, because the net whose state matters
to the host circuit is the host net, and putting the pull-down there means pulling a 0 ohm link to
isolate the module does not also remove the hold-down the host relies on. They were fitted as the
mitigation for the GPIO4 pull-up; with GPIO4 off the host interface they revert to optional
insurance for a host that dislikes an open line. Nothing on our side now depends on them. Against
the 45 kOhm typical internal pull-up (Table 12, page 24) a 10 kOhm pull-down still divides 3.3 V to
about 0.6 V, comfortably under VIL of 0.25 x VDD = 0.825 V, so the option remains good if it is ever
wanted. On the PCB they are five 0402 pads on the rear face; if the layout cannot carry all five,
drop from R24 downwards and record what was dropped.

## 5. Support circuitry

| Ref | Value | Function | Justification |
| --- | --- | --- | --- |
| R1 | 10k | EN pull-up to 3V3 | Table 3 page 10: "Do not leave the EN pin floating" |
| C1 | 1uF | EN to GND | Power-up RC delay. Espressif reference value, page 34. **See below.** |
| R2 | 10k | GPIO8 pull-up | GPIO8 defaults to floating (Table 4, page 11) and must read high for Joint Download Boot (Table 6, page 12). A floating strap is not acceptable in a shipping product. |
| R3 | 10k | GPIO9 pull-up | GPIO9 already has a weak internal pull-up, but the rear IO0 test pad is exposed to the outside world. An external pull-up makes "hold low through reset to flash, release for normal boot" deterministic rather than dependent on a 45 kOhm internal device. |
| R4 | 10k | GPIO25 pull-**up** | Mandatory, no internal pull resistor, must not float. Direction derived in section 2 above. |
| C2 | 10uF | 3V3 decoupling | Espressif reference |
| C3 | 100nF | 3V3 decoupling | Espressif reference |
| C4 | 22uF | 3V3 bulk | Espressif reference; also covers the 123 mA TX peak (Table 14, page 25) against host wiring inductance |
| R10..R14 | 0R 0402 | series links on the five host lines | Bring-up isolation, see below |
| R20..R24 | 10k 0402, DNP | host-line pull-downs | Section 4. Optional insurance under v2, not a mitigation for anything on our side |

### The EN capacitor: 1 uF, following the Espressif reference

Page 34 of the module datasheet says: *"it is advised to add an RC delay circuit at the EN pin. The
recommended setting for the RC delay circuit is usually R = 10 kOhm and C = 1 uF. However, specific
parameters should be adjusted based on the power-up timing of the module and the power-up and reset
sequence timing of the chip."* Espressif's own Figure 7 peripheral schematic draws that capacitor as
`TBD`, so the page 34 text is the only value the manufacturer commits to.

**C1 is 1 uF**, giving the reference 10 ms time constant with R1 at 10 kOhm.

An earlier revision of this design carried 100 nF, on a brief that turned out to be mistaken. It was
drawn as specified and flagged as a disagreement with the datasheet rather than changed silently.
The project owner reviewed the flag and confirmed the datasheet wins: the recollection was wrong,
not the manufacturer. The value, the BOM field and the on-sheet note all now record 1 uF as the
Espressif reference value rather than as a deviation from it. Recorded here because the reasoning
matters more than the value, and because the same judgement applies next time.

If bring-up on a particular host shows the module starting before its 3V3 rail has settled, page 34
explicitly invites tuning this RC against the host's power-up timing. Change it with measurements in
hand, not by preference.

### The 0 ohm links, and what "DNP-optional-populated" was taken to mean

R10 to R14 are 0 ohm 0402 links, one per host-facing line, so a contentious host net can be isolated
during bring-up. They are **fitted by default** (`dnp` is `no`) and each carries an `Assembly`
property reading "FITTED 0 ohm link. Optional: remove to isolate this host net during bring-up."

The brief's phrase "mark them DNP-optional-populated" is ambiguous. Setting the KiCad `dnp` flag
would tell the assembler to leave them off, which would ship a board with five open-circuit host
lines. The reading taken is the opposite: they are populated, and their optionality is documented
rather than encoded as a do-not-populate instruction. If the intent was actually to have the
assembler leave them off and hand-fit them, flip `dnp` on R10..R14 and say so; it is a one-field
change per part.

R20 to R24 are the genuinely DNP parts and do carry `dnp yes`.

## 6. Fit analysis

| | TYWE2L | This carrier |
| --- | --- | --- |
| Width | 15.0 mm | 15.0 mm, exact match |
| Board height | 17.272 mm | 19.0 mm (see below) |
| Thickness | 3.0 mm | 0.6 mm PCB + 2.4 mm module = 3.0 mm, plus rear-side parts |
| Legs | 7, 2.0 mm pitch, 3.454 mm protrusion | identical footprint |
| Supply | 3.0 to 3.6 V, 451 mA peak | 3.0 to 3.6 V, 123 mA peak TX, 350 mA minimum supply capability required |
| I/O logic | VIH 0.75 x VCC, VIL 0.25 x VCC, VOH 0.8 x VCC, VOL 0.1 x VCC | identical thresholds (Table 12, page 24) |
| I/O drive | 12 mA max | 40 mA source / 28 mA sink typical at PAD_DRIVER = 3 |

The logic thresholds are identical between the two parts, expressed as the same fractions of VCC,
so no level translation is needed anywhere. The current budget improves: the host already supplied
451 mA for Wi-Fi, and 802.15.4 peaks at 123 mA, well inside the 350 mA the module asks for.

Drive strength goes the other way and is worth watching. The ESP8285 specifies I_MAX of 12 mA per
I/O with **no programmable drive strength at all**: `pin_mux_register.h` in `ESP8266_RTOS_SDK`
exposes pull-up, pull-down, sleep-pull-up, sleep-pull-down, sleep-OE and OE bits and no
drive-strength field. All five v2 pins carry `DRV` = 2 in TRM Table 6.13-1, which is roughly 20 mA
by default and rises to 40 mA source and 28 mA sink at `PAD_DRIVER` = 3. **The H2 default already
exceeds the ESP8285's maximum**, so firmware should consider clamping `PAD_DRIVER` on the five host
lines rather than leaving it at the default. See open item 3.

## 7. Mechanical, Option A

### The antenna keepout, measured

Figure 11 on page 38 is vector art, so its geometry was recovered from the PDF content stream rather
than estimated. Relative to a module body 13.2 mm wide and 16.6 mm long:

* The pad ring is centred 2.7 mm off the body centre, away from the antenna.
* A full-width line at 11.2 mm from the far edge marks the antenna area boundary, which matches the
  drawing's own `11.2` callout.
* The block above that line carries the label **"Antenna Area"**.

**The antenna keepout is therefore the top 5.4 mm of the module, across the full 13.2 mm width.**

Note that the naive comparison, MINI-1 at 16.6 mm against MINI-1U at 12.5 mm, gives 4.1 mm and is
**wrong**. The MINI-1U keeps roughly 1.3 mm past the end of the pad field for its U.FL connector, so
the difference in overall length understates the keepout. The pad field ends at 11.2 mm from the far
edge on both variants. Take 5.4 mm, not 4.1 mm.

### Consequence for the carrier height

The brief asked for a carrier of approximately 15 mm with a total assembly around 18.6 mm, and for
the carrier height to be set so that no copper and no FR4 sits under the keepout. With the keepout
measured at 5.4 mm those two requests cannot both hold: a 15 mm carrier plus a 5.4 mm overhang is a
20.4 mm board, and an 18.6 mm board needs the carrier down at 13.2 mm. The keepout constraint was
treated as binding, since that is what the brief asked to be derived from the datasheet.

Design as drawn:

| Dimension | Value |
| --- | --- |
| Carrier width | 15.0 mm (exact TYWE2L match) |
| Carrier height | **13.6 mm** |
| Carrier thickness | 0.6 mm target |
| Leg hole centres | 1.25 mm above the carrier bottom edge |
| Module body lower edge | 2.4 mm above the carrier bottom edge |
| Carrier top edge | 2.4 + 11.2 = 13.6 mm, exactly on the keepout boundary |
| Module overhang above the carrier | 5.4 mm, into free air |
| Overall board height | 2.4 + 16.6 = **19.0 mm** (TYWE2L: 17.272 mm) |
| Overall height including legs | 19.0 + 3.454 = 22.45 mm (TYWE2L: 20.726 mm) |
| Stack thickness | 0.6 + 2.4 = 3.0 mm, matching the TYWE2L exactly, before rear-side parts |

So the assembly is **1.73 mm taller than the original board**, not the 1.33 mm the 18.6 mm estimate
implied. 18.6 mm is not reachable while honouring a 5.4 mm keepout. If the host enclosure genuinely
cannot take 19.0 mm, the options are, in order of preference: accept FR4 under part of the keepout
and pay for it in radiated performance; or move to the MINI-1U with an external antenna, which has
no keepout at all and would allow a 13.2 x 12.5 mm module on a shorter carrier.

Clearance check at the bottom edge: leg pads are 1.4 mm diameter on 0.9 mm holes centred 1.25 mm up,
so pad copper runs from 0.55 mm to 1.95 mm above the board edge. The module body starts at 2.4 mm,
leaving 0.45 mm of clear FR4, and the module's lowest pads (the 0.7 mm corner pads) sit 2.7 mm above
the board edge, leaving 0.75 mm for routing between the leg pads and the module.

### Component placement

The module covers 13.2 x 16.6 mm of a 15.0 x 13.6 mm carrier, so the front face has no usable area
at all. **All support parts and all test pads go on the rear face**, which is the right side anyway:
the TYWE2L's own test pads are on its rear face, so a technician's habits carry over unchanged.

This has a thickness consequence that the 3.0 mm figure hides. 0402 parts add about 0.5 mm and an
0603 22 uF adds about 0.9 mm, so the real local thickness is nearer **3.5 to 3.9 mm** against the
TYWE2L's 3.0 mm. If the host enclosure is tight against the module face, use 0402 throughout,
including a 0402 10 uF for C4 in place of the 22 uF, and re-measure. This is recorded rather than
solved because no PCB layout has been committed yet.

## 8. Footprints as drawn

### `ESP32-H2-MINI-1.kicad_mod`

Origin at the centre of the 13.2 x 16.6 mm body, antenna at -Y. Straight from the Figure 11 vector
geometry:

* 48 pads at 0.4 x 0.8 mm, arranged as a closed ring. Two rows of 13 run across the 13.2 mm width,
  9.8 mm apart along the long axis. Two columns of 11 run along the 16.6 mm axis, 11.8 mm apart
  across the width. Pitch is 0.8 mm throughout.
* 4 corner pads at 0.7 x 0.7 mm, 11.9 mm and 9.9 mm apart, pins 50 to 53, all GND.
* Thermal pad, pin 49: continuous 5.4 x 5.4 mm copper on `F.Cu` and `F.Mask`, with solder paste
  applied as a 3 x 3 array of 1.45 mm windows on 1.975 mm pitch, exactly as the figure draws it.
* 12 thermal vias, 0.25 mm drill, placed in the gaps between the paste windows so paste cannot wick
  down them. Copper layers only, no mask opening, so they are tented.
* Courtyard 0.25 mm outside the body. Antenna keepout drawn as a dashed and hatched rectangle on
  `Dwgs.User` with text on `Cmts.User`.

Pin numbering runs anticlockwise viewed from the top, starting with pin 1 as the pad nearest the
antenna on one long side: pins 1..11 down one column, 12..24 along the far row, 25..35 up the other
column, 36..48 along the antenna-side row, then the four corners and the thermal pad. This was
derived from Figure 3 on page 9 by matching the pad counts (11, 13, 11, 13) against the land pattern
and cross-checking against Table 3. The check that settles it: pins 36..48 are the thirteen pads of
the antenna-side row and Table 3 says pins 36 to 53 are all GND, which is exactly what you would
expect of the row nearest the antenna. Pin 49 is the centre pad in Figure 3, so it is the EPAD.

Pins 49 to 53 are all GND, so any permutation among the four corner pads is electrically identical;
the assignment used is a convention, not a claim.

### `TYWE2L_Legs_7P_2.0mm.kicad_mod`

Origin at the midpoint of the carrier's bottom edge, x increasing to the right as seen from the
**front** (module) face, so pad 1 = GPIO14 is the leftmost leg in that view and pad 7 = 3V3 the
rightmost. Pad centres at x = -6.064, -4.064, -2.064, -0.064, 1.936, 3.936, 5.936, which is 2.000 mm
pitch, 12.000 mm span, 1.436 mm from the left edge and 1.564 mm from the right. Pads sit 1.25 mm
above the bottom edge. Pad 1 is square as the pin-1 marker.

> ### ASSUMPTION: 0.9 mm hole, 1.4 mm pad, 0.25 mm annular ring
>
> **The TYWE2L datasheet does not state the leg diameter anywhere.** It is not in Figure 3, not in
> the tables, not in the packaging section. 0.9 mm was chosen because the legs read as roughly
> 0.6 mm formed pin stock in the drawing and a 0.3 mm clearance is normal for hand or wave assembly
> of a formed lead. This is a guess with a stated basis, not a measurement.
>
> **Caliper a physical TYWE2L before ordering boards.** Both the hole diameter and the actual leg
> span want checking; the span in particular is derived from three drawing callouts that happen to
> sum exactly to 15.000 mm, which is reassuring but is still drawing arithmetic, not metrology.
> This warning is repeated in the footprint `descr` and on `Cmts.User` inside the footprint itself.

## 9. Format and validation notes

The files were hand-authored as KiCad 9 S-expressions and then parsed with an independent Python
S-expression reader. What was checked and passed:

* Balanced parentheses with correct string escaping, exactly one top-level form per file.
* Correct top-level keys: `kicad_symbol_lib`, `footprint`, `kicad_sch`, and for the schematic the
  presence of `version`, `generator`, `uuid`, `paper`, `lib_symbols` and `sheet_instances`.
* Every `uuid` is a well-formed RFC 4122 string and no UUID is reused anywhere in a file.
* Symbol: 53 pins, numbers 1 to 53 with none missing or duplicated; 24 `power_in` (3V3, VBAT and the
  22 grounds), 1 `input` (EN), 19 `bidirectional`, 9 `no_connect`.
* Module footprint: 74 pad records covering 53 distinct pad numbers, 12 vias, 9 paste windows, and
  the pad ring geometry re-checked against the 11.8 / 9.8 / 11.9 / 9.9 mm figures from Figure 11.
* Leg footprint: pitch exactly 2.0 mm, span exactly 12.0 mm, edge offsets exactly 1.436 and
  1.564 mm.
* Schematic: 57 placed symbols with no duplicate reference, every instance `lib_id` present in the
  cached `lib_symbols`, all 52 labels lying on a wire, every wire endpoint on the 1.27 mm grid, and
  every symbol instance path matching the root sheet UUID.
* Project file: valid JSON, `meta.version` 3, and `sheets[0]` UUID matching the schematic root.

**That residual risk is now closed for the schematic.** `carrier/tywe2l-h2-carrier.kicad_sch` has
been opened and re-saved by **KiCad 10.0.5**, so the file on disk is KiCad's own native 10.x output:
`(version 20260306)`, `(generator "eeschema")`, `(generator_version "10.0")`. That is the
authoritative on-disk state and it is what every later edit works from. Do not regenerate the file
from an earlier hand-authored version and do not downgrade the version stamps.

Two format defects were found and fixed by hand during that round trip, both worth remembering
because a permissive S-expression reader passes them and KiCad does not:

* In an embedded `lib_symbols` block, the **parent** symbol carries the library nickname
  (`TYWE2L-2-THREAD:R`) but the **child unit** symbols must not (`R_0_1`, never
  `TYWE2L-2-THREAD:R_0_1`). KiCad rejects the prefixed form.
* **Literal newlines are not allowed inside quoted strings.** Multi-line sheet notes must use `\n`
  escape sequences; a raw newline gives "Unterminated delimited string". This one bites every time
  the on-sheet notes are edited.

The symbol library still declares `20241209` and the footprints `20241229`, and neither has been
through KiCad yet. If either stamp is off, KiCad will migrate the file rather than reject it. Only
optional tokens whose spelling is stable across KiCad 6 to 9 were used; tokens whose syntax changed
between versions, notably graphic `fill` on `fp_rect` and `fp_circle`, were avoided entirely by
drawing every rectangle and marker from `fp_line` segments.

---

## OPEN ITEMS TO VERIFY BEFORE FABRICATION

**1. TYWE2L leg hole diameter and exact span. BLOCKER FOR FABRICATION. Still open.**
The leg diameter is not published anywhere in the TYWE2L datasheet: not in Figure 3, not in the
tables, not in the packaging section. The footprint assumes a **0.9 mm hole with a 1.4 mm pad**, and
that is a guess with a stated basis, not a measurement. **Caliper a physical TYWE2L before ordering
boards.** Measure the leg diameter or cross-section, the actual centre-to-centre pitch across all
six gaps, and the distance from each outer leg centre to its board edge, then re-cut
`TYWE2L_Legs_7P_2.0mm.kicad_mod` from the measurements. Also confirm the 3.454 mm protrusion against
the host boards actually being converted, since insertion depth decides whether the carrier sits
where the original sat.

This is not a detail that can be deferred to layout, because every routing figure scales directly
off it. The channel between the top of the leg pad copper and the bottom of the module pad copper is
0.750 mm on the current assumption, which holds one 0.15 mm trace comfortably. A 1.0 mm hole with a
1.5 mm pad drops it to 0.700 mm and kills the two-trace case outright.

**2. Antenna keepout extent.**
Measured here as the top 5.4 mm of the 16.6 mm module, full 13.2 mm width, from Figure 11's vector
geometry and its own 11.2 mm callout. The datasheet defers the base-board rules to *ESP32-H2
Hardware Design Guidelines*, section "Positioning a Module on a Base Board", which is not in
`hardware/datasheets/`. Cross-check it, in particular whether the guidelines require clearance
beyond the module footprint, sideways or in front of the antenna, rather than only underneath. The
carrier height of 13.6 mm depends directly on this number.

**3. GPIO drive strength.**
The TYWE2L is specified at 12 mA maximum per I/O with no programmable drive strength at all (page 8,
and the `ESP8266_RTOS_SDK` IO MUX header has no drive-strength field). All five v2 host pins carry
`DRV` = 2 in TRM Table 6.13-1, roughly 20 mA, rising to 40 mA source and 28 mA sink at `PAD_DRIVER`
= 3 (module Table 12, page 24). The H2 default is therefore already above the ESP8285's maximum.
Decide whether firmware should clamp `PAD_DRIVER` on the five host lines so a converted device does
not push harder into host circuitry than the part it replaces did, and check the sink current
against any host LED or opto driven directly from a leg.

**4. ESP8285 reset states on the five legs.**
Not established, and not establishable from public sources. ESP8266EX Datasheet section 4.1
documents only the pad's *capability*, and the reset defaults lived in the *ESP8266 Pin List*
spreadsheet cited by Appendix A, which no longer exists anywhere: every URL variant returns the CDP
app shell, the Wayback Machine has no usable snapshot, and ESP8266 Technical Reference v1.7's own
release note records "Deleted the ESP8266 Pin List in Section 2.1". The defensible reading is that a
competently designed Tuya-era host circuit cannot have relied on a defined internal pull state, so
it must already tolerate an **indeterminate** line at power-up, which is a weaker assumption than
"weakly pulled high". Bench measurement on a real TYWE2L is the only resolution. This cannot make v2
unsafe, only more conservative than strictly necessary.

**5. Assembly thickness on the rear face.**
The 0.6 + 2.4 = 3.0 mm stack matches the TYWE2L only before rear-side parts. With 0402 support parts
and an 0603 bulk capacitor the real figure is nearer 3.5 to 3.9 mm. Confirm against a real host
enclosure before committing the layout, and switch C4 to an 0402 10 uF if clearance is tight.

---

### Closed items, kept with their reasoning

**Strapping pin count: are GPIO2 and GPIO3 straps? CLOSED. They are.**
This was the blocker on the pin mapping, and it resolved against the module datasheet. TRM v1.1
section 8.2.2 names GPIO9, GPIO8, GPIO3 and GPIO2 as the pins whose reset values select boot mode,
and Register 6.7 gives GPIO2 and GPIO3 dedicated latch bits in `GPIO_STRAP_REG`. The module
datasheet Table 4 lists only GPIO8, GPIO9 and GPIO25, and that table is simply incomplete. Legs 4
and 3 of v1 sat on the two missing pins. Settled in
[`docs/h2-strapping-and-reset-states.md`](../docs/h2-strapping-and-reset-states.md), and designed
around by pin mapping v2, which takes both pins off the host legs entirely. The old reroute path
(cut R12 and R13) is no longer needed, because the reroute has been done.

**GPIO4 reset-time internal pull-up. CLOSED. Confirmed present on every factory part.**
Verified against TRM v1.1 Table 6.13-1 footnote `1*` and ESP32-H2 datasheet v1.3 Table 2-1 footnote
4, both of which say MTCK is tied to an internal pull-up after reset whenever `EFUSE_DIS_PAD_JTAG`
= 0. It ships as 0 on every part. None of GPIO1, GPIO2, GPIO3 or GPIO5 behaves the same way: GPIO1
is reset code `0`, and GPIO2, GPIO3 and GPIO5 are code `1`, input enabled with no pull. v2 responds
by keeping GPIO4 off the host interface rather than by fitting a resistor, so R20 to R24 revert to
optional insurance. Section 4 has the full working.

**EN capacitor value. CLOSED.**
Was open while C1 sat at 100 nF against the datasheet's 1 uF. C1 is now 1 uF, matching the Espressif
reference on page 34, so this is no longer a discrepancy to resolve. Page 34 still invites tuning the
RC against a specific host's power-up timing, so measure the 3V3 rise time on the first converted
device during bring-up, but do so as characterisation rather than as an open question.
