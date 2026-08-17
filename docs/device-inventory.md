# Device inventory

The running record of the owner's own devices, one entry per device, decoded from its Tasmota
template and ruled on for fit against the TYWE2L replacement board.

This is a *device* record, not a corpus study. [`tasmota-import.md`](./tasmota-import.md) covers
the template format, the encodings and the community corpora, and it is the authority for anything
general. [`pin-mapping-v2.md`](./pin-mapping-v2.md) is the authority for the leg-to-H2 mapping.

Every decode below was produced by the tool, not by hand:

```
python3 tools/tasmota-import/decode_template.py '<template JSON>'
python3 tools/tasmota-import/decode_template.py --json '<template JSON>'   # for tooling
```

Decode tables come from Tasmota at commit `db56cd62aa455714e4a9a3043b8f835addf771a2`, extracted by
`tools/tasmota-import/extract_tables.py` into `tools/tasmota-import/tables/tasmota-db56cd62aa45.json`.
Tasmota citations give file and line at that commit.

Evidence tags, used the same way as in `tasmota-import.md`. **[V]** verified against a primary
source (Tasmota source, a screenshot of the device's own web UI, the owner's inspection of the
hardware). **[C]** corroborated by a community corpus only. **[U]** unverified, or my inference,
and I say why.

---

## Two things every entry has to answer

**Does it fit the pins, and does it fit the firmware?** These are separate questions with separate
fixes, and the second one was discovered late enough to change the shape of this document.

**Pin fit** is what the verdict column reports, and it is purely mechanical:

- **COMPATIBLE**: every assigned function lands on one of the five legs.
- **NEEDS TEST PADS**: nothing lands on an unavailable pin, but something lands on GPIO0, GPIO1,
  GPIO2 or GPIO3, which the TYWE2L presents as rear test pads rather than legs. On a vertically
  mounted module those pads face nothing, so this verdict really means "the host is contacting a
  pin a TYWE2L cannot offer", which is itself evidence that the module is not a TYWE2L.
- **INCOMPATIBLE**: something lands on GPIO9, GPIO10, GPIO15, GPIO16 or ADC0, none of which the
  TYWE2L brings out anywhere.

**Driver architecture** is how the device actually drives its load, and it decides which firmware
the device needs. A device can fit the five legs perfectly and still be unservable, because the
legs are not driving LEDs at all, they are clocking a protocol into a driver chip. The values the
tool emits:

| `driver_architecture` | What it means | Firmware consequence |
| --- | --- | --- |
| `pwm_direct` | the module's own pins drive the load through PWM | LEDC channels, our current design |
| `two_wire_led_driver` | the module clocks a serial protocol into a constant-current driver IC, usually on a separate LED board | a bit-banged driver per chip family, plus per-chip current registers. Not written |
| `gpio_direct` | relays, buttons and indicator LEDs on plain GPIOs | the simple on/off firmware |
| `addressable_led` | WS2812 style strip | different light model, strict timing |
| `mixed` | more than one of the above on one device | usually a light with an extra relay or button |
| `input_only`, `none` | nothing drives anything | almost always a TuyaMCU device or a broken template |

The decoder reports both, and refuses to fold one into the other:

```
DRIVER ARCHITECTURE: two_wire_led_driver (SM2135)
VERDICT (pins): COMPATIBLE
SERVABLE: no, see blockers below
  firmware blocker: GPIO4 uses GPIO_SM2135_DAT ...
```

### Consequence for the profile schema, flagged not fixed

[`flasher-and-registry.md`](./flasher-and-registry.md) section 4.2 specifies the registry profile as
a list of pins, each with a `role`, an `index`, an active level and a boot state, and section 3.2
specifies the compiled blob the firmware reads. **That schema cannot express a two-wire LED driver
device.** Its light channels are not pins: five colour channels can sit behind two wires, the
channel order and the drive currents live in the driver IC's registers, and `role: "pwm"` on leg 1
would be actively wrong.

**The schema needs a device-level driver field, first class, with the pin roles subordinate to
it.** Something of the shape `"driver": { "type": "pwm_direct" }` against
`"driver": { "type": "sm2135", "model": 1, "channel_order": "WCGRB" }`, where the second form
carries the chip, the variant and the channel order, and the pins carry only clock and data. The
firmware variant list in that document needs the same split, because a variant built for LEDC
cannot run a bit-banged driver.

This document does not change `flasher-and-registry.md`. **It flags it as needing revision**, and
the revision should happen before any profile is written, not after, because retrofitting a device
type into a schema that is already in flashed devices is the expensive kind of change.

---

## Compatibility summary

| Device | Family | Driver architecture | Legs used | Verdict (pins) | Servable now | Matter device type |
| --- | --- | --- | --- | --- | --- | --- |
| **Bath1**, Connect SmartHome CSH-240RGB10W downlight | A | **`pwm_direct`**, 5 channels | 1, 2, 3, 4, 5 | **COMPATIBLE** | **yes** | Extended Colour Light (0x010D) |
| **Mirabella Genio colour change** | B | **`two_wire_led_driver`**, chip not yet identified | 1, 2 only | **COMPATIBLE**, pending its template | **no**, no driver firmware exists | Extended Colour Light (0x010D), presumed |
| **EnsuiteSwitch**, 3 gang wall switch | switch | `gpio_direct` | needs GPIO16 and GPIO2, which are not legs | **INCOMPATIBLE** | no | 3 × On/Off Light (0x0100), if it were servable |
| **Bed2Switch**, 1 gang wall switch | switch | `gpio_direct` | 2, 3, and a button on GPIO0 | **NEEDS TEST PADS**, in practice not servable | no | On/Off Light (0x0100), if it were servable |

**Read the first two rows together, because they are the whole design problem.** Both families are
TYWE2L modules in Australian retail lighting, both are the owner's, and they need two different
firmwares. Family A drives five colour channels straight off the five legs. Family B uses two legs
to talk to a driver IC that does the actual driving. Pin compatibility says nothing about which.

The switches confirm what [`tasmota-import.md`](./tasmota-import.md) section 4.6 predicted: lights
fit the TYWE2L footprint, multi-gang wall switches do not, because a switch needs more inputs and
outputs than five pins can carry and therefore ships on a bigger module.

---

## 1. Bath1, Connect SmartHome CSH-240RGB10W downlight (family A)

The anchor device and the first target. The owner has 100 or more, bought as a single batch.

| | |
| --- | --- |
| Owner's Tasmota name | `Bath1` |
| Product | Connect SmartHome / Laser CSH-240RGB10W, 240 V RGB LED downlight, sold through Harvey Norman **[C]** |
| Module | TYWE2L, owner-confirmed **[U]**, in the sense that I have not seen the board myself |
| **Driver architecture** | **`pwm_direct`, five channels** **[V]**, confirmed by the Tasmota screenshot and by the owner's physical inspection: a 16.5 V supply feeds a board carrying W, WW, R, G, B, V and GND, one PWM channel per leg |
| Firmware seen | Tasmota 8.2.0 **[V]**, `hardware/tasmota/Bath1.png` |
| Template | `{"NAME":"Bath1","GPIO":[0,0,0,0,37,40,0,0,38,41,39,0,0],"FLAG":0,"BASE":18}` |
| Encoding | **legacy 8-bit** **[V]**, 13 entries and every value below 256, which is Tasmota's own test in `JsonTemplate()`, `tasmota/tasmota_support/support.ino:1912` |
| `BASE` | 18, `WEMOS`, displayed as "Generic" **[V]** (`tasmota/include/tasmota_template.h:1612` and `:1623`) |

### Decoded functions

| ESP8285 pin | Raw | Packed | Tasmota component | Leg | H2 GPIO (v2) |
| --- | --- | --- | --- | --- | --- |
| GPIO14 | 39 | 418 | PWM3 | 1 | GPIO13 |
| GPIO12 | 38 | 417 | PWM2 | 2 | GPIO14 |
| GPIO13 | 41 | 420 | PWM5 | 3 | GPIO12 |
| GPIO5 | 40 | 419 | PWM4 | 4 | GPIO10 |
| GPIO4 | 37 | 416 | PWM1 | 5 | GPIO11 |
| all others | 0 | 0 | None | | |

Read the leg column, not the pin numbers. ESP8285 GPIO12 becomes H2 GPIO14 and ESP8285 GPIO13
becomes H2 GPIO12. The two namespaces cross over, so a row where the two numbers agree is a red
flag rather than a reassurance ([`pin-mapping-v2.md`](./pin-mapping-v2.md)).

The five raw values are consecutive because the legacy enum gave each PWM instance its own ordinal
(`GPI8_PWM1` through `GPI8_PWM5`, `tasmota/include/tasmota_template_legacy.h:27` onwards). The
modern encoding instead packs one function ordinal with a five-bit instance index,
`AGPIO(x) ((x)<<5)` at `tasmota/include/tasmota_globals.h:533`, which is why 37 converts to 416 and
41 to 420 through `kGpioConvert` at `tasmota/include/tasmota_template_legacy.h:257`.

### Verdict: COMPATIBLE, and servable

All five functions land on legs. Nothing touches a rear test pad, nothing touches GPIO9, GPIO10,
GPIO15, GPIO16 or ADC0, `BASE` is 18, and the driver architecture is the one our firmware is being
built for. This is the best case the board has.

Five PWM channels are comfortable on the ESP32-H2: six LEDC channels and four timers, and lighting
runs every channel at one frequency ([`ledc-erratum.md`](./ledc-erratum.md) section 1) **[V]**.

### Corrections to earlier notes on this device

An earlier brief recorded these units as **two channel**, with only legs 1 and 2 connected, and
recorded blakadder's CSH-240RGB10W template as not matching. **Both statements are wrong** and are
superseded by the screenshot and by the owner's inspection. Bath1 uses **five** PWM channels across
all five legs, and its GPIO array is **byte-identical** to blakadder's CSH-240RGB10W entry at corpus
commit `30d9c9b16014817641f83a0cf8f2f266a0dbaef2` **[V]**:

```
blakadder _templates/connect_smarthome_CSH-240RGB10W
  template: '{"NAME":"CSH-240RGB10W","GPIO":[0,0,0,0,37,40,0,0,38,41,39,0,0],"FLAG":0,"BASE":18}'
```

The same array appears on **77 other corpus entries** **[V]**, so it is the common five-channel
RGBCCT shape rather than anything specific to this product. The two-channel note was probably an
echo of the genuinely two-channel CCT TYWE2L lights in the corpus
([`pin-mapping-v2.md`](./pin-mapping-v2.md) section 2.7), or of the family B units below, which do
use only legs 1 and 2.

### Open questions

1. **Is every downlight in the fleet wired like this one?** One screenshot from one unit. The owner
   is being asked. Until an answer arrives, treat the fleet as one verified unit plus 99 or more
   assumed, not as 100 verified.
2. **Which channel is which colour.** The template says five channels and nothing else. Tasmota
   keeps colour order, PWM frequency, dimmer curve and the minimum-brightness cutoff in settings
   rather than in the template ([`tasmota-import.md`](./tasmota-import.md) section 4.3). The
   owner's wiring inspection gives the board's labels, W, WW, R, G, B, but not which label is on
   which leg **[V]**, so the mapping still needs one channel driven at a time with someone
   watching **[U]**.
3. **Colour temperature range in mireds**, needed for the Matter Color Control cluster. Not in the
   template.
4. **Confirm the module by eye on one unit.** The corpus records a module on 4 of 2,871 entries, so
   it can never confirm this ([`tasmota-import.md`](./tasmota-import.md) section 4.6).
5. **Watch for a silicon change across the batch.** blakadder's CSH-240RGB10W page carries an
   unsupported-hardware note saying later units ship a BK7231 part instead of an ESP **[V]**. The
   owner's units run Tasmota, so those are ESP8285, but a mixed fleet is possible if any were bought
   later.

### Matter mapping

Extended Colour Light, device type 0x010D, with On/Off, Level Control and Color Control carrying
both the ColorTemperature and HueSaturation features. Provisional until question 2 is answered, and
the choice is load-bearing rather than cosmetic: the cluster vocabulary is fixed at build time, so
a wrong light type means the profile asks for a cluster the flashed variant does not contain
([`tasmota-import.md`](./tasmota-import.md) section 4.4).

---

## 2. Mirabella Genio colour change (family B)

**No template yet.** This entry records what the hardware says, and it is here because the
architecture finding changes the firmware plan whether or not the template ever arrives.

| | |
| --- | --- |
| Owner's Tasmota name | not yet supplied |
| Product | Mirabella Genio colour change unit, exact model not yet identified **[U]** |
| Module | TYWE2L **[V]**, owner's inspection |
| **Driver architecture** | **`two_wire_led_driver`** **[V]** by inspection, **chip not identified** **[U]** |
| Interconnect | the module sits on a power board; four wires carry **SDA, SCL, GND and V** through to a separate LED board that holds the driver **[V]**, owner's inspection, silkscreen labels |
| Legs used | **legs 1 and 2 only**, i.e. ESP8285 GPIO14 and GPIO12, plus GND and 3V3 **[V]** |
| Template | not yet supplied. Requested, along with a photo of the driver IC |

### What the SDA and SCL labels almost certainly are not

They are very unlikely to be standard I2C **[U]**. Tuya lighting boards routinely silkscreen a
bespoke clock-and-data LED driver interface that way, and Tasmota implements several such chips as
named components: SM2135, SM2235, SM2335, BP5758D, BP1658CJ, SM16716 and the MY92x1 family. Which
one this is **is open, and is not being guessed**. The photo of the driver IC settles it, and so
does the device's own Tasmota template if it was ever configured correctly.

### What its template will look like, so it can be recognised on sight

Two-wire drivers appear in a template as a **clock pin and a data pin**, never as PWM channels. The
complete set of codes reachable in the **legacy 8-bit** encoding, which is what Tasmota 8.2.0
writes, is short **[V]**:

| Legacy 8-bit value | Converts to | Tasmota UI label | Chip |
| --- | --- | --- | --- |
| 140 | `GPIO_SM16716_CLK` | SM16716 CLK | SM16716 |
| 141 | `GPIO_SM16716_DAT` | SM16716 DAT | SM16716 |
| 142 | `GPIO_SM16716_SEL` | SM16716 PWR | SM16716 |
| 143 | `GPIO_DI` | MY92x1 DI | MY92x1 (MY9231, MY9291) |
| 144 | `GPIO_DCKI` | MY92x1 DCKI | MY92x1 |
| 180 | `GPIO_SM2135_CLK` | SM2135 Clk | SM2135 |
| 181 | `GPIO_SM2135_DAT` | SM2135 Dat*n* | SM2135 |

**[V]** for every row, decoded through `kGpioConvert` at
`tasmota/include/tasmota_template_legacy.h:257`, and confirmed against the enum in the owner's own
firmware version by parsing `tasmota/tasmota_template.h` at tag **v8.2.0**, where the same ordinals
appear and the whole enum is 202 names long **[V]**. There is a test that walks all 256 legacy
values and asserts that no other value is a two-wire driver
(`tools/tasmota-import/tests/test_decode_template.py`, `TestLegacyDriverCodeInventory`).

**The important negative: SM2235, SM2335, BP5758D, BP1658CJ and P9813 do not exist in the legacy
encoding at all** **[V]**. They were added to Tasmota after the 8-bit enum was frozen, and they are
absent from v8.2.0. So if the Mirabella really carries one of those, a template captured from
Tasmota 8.2.0 **cannot say so**. It would show either the wrong chip, or nothing at all on legs 1
and 2. Ask which Tasmota version that device is running before reading anything into its template.

`GPIO_SM2135_DAT` carries an instance index and **the index is not cosmetic**: Tasmota's driver
reads it as a model selector that sets both the channel order (WCGRB against WCBGR) and the drive
currents, at `tasmota/tasmota_xlgt_light/xlgt_04_sm2135.ino:231` **[V]**. `SM2135 Dat1` and
`SM2135 Dat5` are materially different configurations of the same chip.

### What the corpus says about Mirabella Genio units

The corpus holds 37 Mirabella Genio entries, and they are not one architecture **[V]**:

| Architecture | Entries | Shape |
| --- | --- | --- |
| `pwm_direct` | 21 | one to five PWM channels, various pins |
| `gpio_direct` | 10 | relay, button and LED plugs and switches, one of them TuyaMCU |
| **`two_wire_led_driver`, SM2135** | **5** | **GPIO12 and GPIO14 only, which is legs 2 and 1** |
| `mixed` | 1 | PWM plus relays and a buzzer |

The five SM2135 entries are `mirabella_genio_I002741`, `I002608`, `I003293`, `1002339` and
`42905684` **[V]**. Every one of them uses exactly the two legs the owner's unit uses, and nothing
else. That is strong circumstantial support for SM2135, and it is **not** proof, for two reasons.
The corpus disagrees with itself on the orientation, four entries putting Clk on GPIO14 and one
putting Clk on GPIO12 **[V]**, and it disagrees on the model index, which appears as Dat1, Dat3 and
Dat5 across the five **[V]**. Getting either wrong gives wrong colours or wrong currents, and
currents drive real LEDs.

Across the whole corpus, taking one template per file and preferring the modern key, **90 two-wire
driver devices fit our five legs**: SM16716 on 47, SM2135 on 38 and MY92x1 on 5 **[V]**. Family B is
not a Mirabella quirk. It is a large slice of the retrofit market we currently cannot serve at all,
and it is close to the 85 figure `tasmota-import.md` section 3.2 already counted from the other
direction.

### Verdict: pin-compatible, not servable

Two legs, both of them legs we have, so on pins alone this is the easiest device in the inventory.
It is unservable anyway, because no firmware variant we have or plan bit-bangs an LED driver
protocol. Building one is a real project: the protocol, the per-chip current registers, the gamma
handling and the channel order, per chip family
([`tasmota-import.md`](./tasmota-import.md) section 3.2 lists these as outcome X).

### Open questions

1. **Which driver chip.** Photo of the IC, or the device's Tasmota template plus its version.
2. **Which leg is clock and which is data**, which the corpus contradicts itself about.
3. **The model index**, if it is an SM2135, which sets channel order and current.
4. **How many colour channels the LED board actually has**, which the template cannot tell us at all
   because the channels live behind the driver.
5. **How many family B devices the owner has.** This decides whether a second firmware variant is
   worth building.

### Matter mapping

Presumed Extended Colour Light (0x010D), on the strength of "colour change" in the product name
**[U]**. It cannot be confirmed until the channel count is known.

---

## 3. EnsuiteSwitch, 3 gang wall switch

| | |
| --- | --- |
| Owner's Tasmota name | `EnsuiteSwitch` |
| Product | described by the owner as a 3 toggle switch module in a wall plate. Very probably a Connect SmartHome CSH-SWTCH3, see below **[C]** |
| Module | **not a TYWE2L** **[V]**, by the pin evidence below. Most likely an ESP-12 footprint module, the TYWE3S family **[C]** |
| **Driver architecture** | `gpio_direct`: three relays, three buttons, one inverted status LED |
| Firmware seen | Tasmota 8.2.0 **[V]**, `hardware/tasmota/EnsuiteSwitch.png` |
| Template | `{"NAME":"EnsuiteSwitch","GPIO":[255,255,56,255,19,18,0,0,22,21,23,255,17],"FLAG":0,"BASE":18}` |
| Encoding | **legacy 8-bit** **[V]**, same test as above |
| `BASE` | 18, Generic **[V]** |

### Decoded functions

| ESP8285 pin | Raw | Packed | Tasmota component | Availability on a TYWE2L |
| --- | --- | --- | --- | --- |
| GPIO0 | 255 | user | User | rear test pad (IO0) |
| GPIO1 | 255 | user | User | rear test pad (TXD) |
| GPIO2 | 56 | 320 | **Led1i**, inverted indicator LED | **rear test pad (IO2), not a leg** |
| GPIO3 | 255 | user | User | rear test pad (RXD) |
| GPIO4 | 19 | 34 | Button3 | leg 5 |
| GPIO5 | 18 | 33 | Button2 | leg 4 |
| GPIO12 | 22 | 225 | Relay2 | leg 2 |
| GPIO13 | 21 | 224 | Relay1 | leg 3 |
| GPIO14 | 23 | 226 | Relay3 | leg 1 |
| GPIO15 | 255 | user | User | not brought out |
| GPIO16 | 17 | 32 | **Button1** | **not brought out at all** |
| GPIO9, GPIO10, ADC0 | 0 | 0 | None | not brought out |

Three relays, three buttons and one status LED, so the "textbook 3-gang switch" reading is right in
substance. Three details are worth stating exactly:

- **The gang-to-relay pairing is by index and it is not in pin order.** Button1 on GPIO16 pairs with
  Relay1 on GPIO13, Button2 on GPIO5 with Relay2 on GPIO12, Button3 on GPIO4 with Relay3 on GPIO14.
  Tasmota pairs by the instance index in the low five bits of the packed value, so dropping the
  index wires the wrong button to the wrong load.
- **The LED is inverted.** Legacy 56 converts to 320, `GPIO_LED1_INV`, which Tasmota displays as
  `Led1i` **[V]**. "An LED function" understates it: the polarity is part of the function, and
  getting it backwards leaves the indicator lit whenever the load is off. This is the first hard
  evidence in this project that the per-channel active level in the profile schema is load-bearing.
- **255 is not a distinct wire value.** In legacy it is simply out of range of `kGpioConvert`, so
  `GpioConvert()` returns `AGPIO(GPIO_USER)` (`tasmota/tasmota_support/support.ino:1496`). The tool
  prints it as "User" and treats it exactly like 0: nothing is connected.

### Verdict: INCOMPATIBLE

Two independent reasons, and the first one alone is fatal:

1. **Button1 is on GPIO16.** The TYWE2L brings out GPIO14, GPIO12, GPIO13, GPIO5 and GPIO4 on legs,
   and GPIO0, GPIO1, GPIO2, GPIO3 and RST as rear test pads. **GPIO16 is not present anywhere on
   the module**, per the datasheet in `hardware/datasheets/`. There is no pad to solder to and no
   way to reach the signal.
2. **The status LED is on GPIO2**, which the TYWE2L offers only as a rear test pad, and on a
   vertically mounted module that pad faces nothing.

Both facts point the same way, and the conclusion is about the *device* rather than about our
board: a host PCB that drives GPIO16 and GPIO2 is wired to a module that brings them out, so **this
device does not contain a TYWE2L**, and our board cannot serve it as designed.

### Which module it most likely uses

**An ESP-12 footprint module, the TYWE3S family, is the obvious candidate [C], corroborated rather
than proven.**

- blakadder groups module pinouts in `_data/modules.yaml`, and TYWE3S sits in the **`esp12`** group
  alongside TYWE3L, ESP-12 and the Beken WB3 parts, while TYWE2L sits in the two-pin-per-side
  `dt-light` group **[V]** (corpus commit `30d9c9b1`).
- Across the 63 corpus entries whose contributor recorded `chip: TYWE3S`, the assigned pins include
  GPIO0 on 22 entries, GPIO3 on 21, **GPIO16 on 17**, GPIO15 on 14, GPIO1 on 13 and GPIO2 on 6
  **[V]**. A module that did not bring those out could not appear that way. The four `chip: TYWE2L`
  entries use only GPIO4, GPIO5, GPIO12, GPIO13 and GPIO14 **[V]**.
- **[U]** I could not open a Tuya TYWE3S datasheet from this environment, so the pad list above is
  community evidence rather than a manufacturer document. Chip-down ESP8285 on the host PCB, with no
  module at all, is also perfectly plausible for a wall switch and would look identical in the
  template. **A photograph of the board settles it in a minute**, and nothing else will.

### Which product this is, probably

Searching the pinned corpus for the same shape found **8 entries**, and one matches on every
function including the inverted LED **[V]**:

```
zemismart_KS-811_3gang        GPIO2=Led1i GPIO4=Button3 GPIO5=Button2 GPIO12=Relay2 GPIO13=Relay1 GPIO14=Relay3 GPIO16=Button1
connect_smarthome_CSH-SWTCH3  GPIO2=Led1  GPIO4=Button3 GPIO5=Button2 GPIO12=Relay2 GPIO13=Relay1 GPIO14=Relay3 GPIO16=Button1
```

The Connect SmartHome CSH-SWTCH3 is the Australian retail product, and its sibling CSH-SWTCH2 is
annotated in the corpus as "Rebadged Zemismart KS-811 2 Gang Switch" **[V]**, which ties the two
together. Note that the two entries **disagree on the LED polarity**, `Led1` against `Led1i`, while
the owner's own screenshot says `Led1i`. That is a live example of the corpus contradicting itself,
measured at 38 disagreements in 64 second opinions
([`tasmota-import.md`](./tasmota-import.md) section 2.4). The screenshot wins.

The KS-811 corpus entry carries `chip: BK7231N`, a Beken part **[V]**. That is a *later* revision:
the owner's unit runs Tasmota on an ESP8285, so theirs is from the ESP era of the same product line.

### What it would take to serve this device

Three routes, in increasing order of honesty about the effort:

1. **A different carrier board**, a drop-in for an ESP-12 footprint bringing out at least GPIO0,
   GPIO2, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15 and GPIO16. The ESP32-H2-MINI-1 has enough
   usable GPIOs, so this is a new PCB and a new pin map rather than new silicon. It is a sibling
   product, not a variant of this one.
2. **Reuse the TYWE2L board with flying leads.** Physically possible, and not defensible on a mains
   wall switch: two of seven signals would be hand-wired.
3. **Leave it.** Retire the switch, or replace it with a Matter-native one.

Route 1 is the only real answer, and the decision it forces is whether this project is a TYWE2L
replacement or a family of Tuya module replacements. That is the owner's call.

### Matter mapping, if it were servable

Three On/Off Light endpoints (0x0100), one per gang, each bound to its relay with the matching
button as local control, plus a status LED driven by firmware rather than by a cluster. Optionally
three Generic Switch endpoints if the buttons should also be visible to the fabric.

---

## 4. Bed2Switch, 1 gang wall switch

| | |
| --- | --- |
| Owner's Tasmota name | `Bed2Switch` |
| Product | not yet identified **[U]** |
| Module | **almost certainly not a TYWE2L** **[V]**, see the verdict |
| **Driver architecture** | `gpio_direct`: one relay, one button, one inverted indicator LED |
| Firmware seen | Tasmota 8.2.0 **[V]**, `hardware/tasmota/Bed2Switch.png` |
| Template | `{"NAME":"Bed2Switch","GPIO":[17,0,0,0,0,0,0,0,21,56,0,0,0],"FLAG":0,"BASE":18}` |
| Encoding | **legacy 8-bit** **[V]** |
| `BASE` | 18, Generic **[V]** |

### Decoded functions

| ESP8285 pin | Raw | Packed | Tasmota component | Availability on a TYWE2L |
| --- | --- | --- | --- | --- |
| GPIO0 | 17 | 32 | **Button1** | **rear test pad (IO0), not a leg** |
| GPIO12 | 21 | 224 | Relay1 | leg 2, to H2 GPIO14 |
| GPIO13 | 56 | 320 | **Led1i**, inverted | leg 3, to H2 GPIO12 |
| all others | 0 | 0 | None | |

One relay, one button, one inverted indicator LED. This is the single most common shape in the
whole Tasmota corpus ([`tasmota-import.md`](./tasmota-import.md) section 3.4), and two of its three
functions sit on legs we have.

### Verdict: NEEDS TEST PADS, and in practice not servable

The tool returns NEEDS TEST PADS, because nothing lands on GPIO9, GPIO10, GPIO15, GPIO16 or ADC0
and the only problem is Button1 on GPIO0. For this device that is worse news than it sounds:

- The TYWE2L presents GPIO0 as a **rear test pad**, not a leg. On a vertically mounted module the
  rear face does not contact the host board, so there is no route from a host button to GPIO0
  **[V]**, per the datasheet in `hardware/datasheets/`.
- A host PCB with its button on GPIO0 is therefore not talking to a TYWE2L. The same reasoning as
  for EnsuiteSwitch applies: **the device is very probably built on a larger module** **[C]**, and
  22 of the 63 corpus entries tagged `chip: TYWE3S` do exactly this, putting a button on GPIO0
  **[V]**.
- The two functions that do land on legs are not enough on their own. Without the button the device
  has no local control, which is not acceptable on a wall switch.

If a photograph ever shows a genuine TYWE2L in this device, then the template is describing a button
wired to a rear test pad, and the right response is to inspect the board and revise this entry. I
would treat that as a surprise rather than a possibility worth planning for.

### Open questions

1. **Identify the product.** The pin shape is too common to identify it from the corpus, so this
   needs a photograph or a model number.
2. **Confirm the module**, which decides whether this device joins the EnsuiteSwitch sibling-board
   case.

### Matter mapping, if it were servable

On/Off Light (0x0100), with the button as local toggle and the inverted LED driven by firmware.

---

## Adding an entry

Keep the summary table at the top in step with the entries, and keep driver architecture in both.

1. Get the template. Tasmota web UI, Configuration, Configure Other, or the `Template` command. A
   screenshot of Configuration, Configure Template is better than the JSON alone, because Tasmota
   renders the function names itself and that turns an inference into a verified decode. Save it
   under `hardware/tasmota/<name>.png`, and **record the Tasmota version shown in the footer**,
   because it decides which component codes that firmware could even express.
2. Run the decoder and paste its output rather than your reading of it:
   ```
   python3 tools/tasmota-import/decode_template.py '<template JSON>'
   ```
3. Copy this skeleton:

```markdown
## N. <Tasmota name>, <what it is>

| | |
| --- | --- |
| Owner's Tasmota name | |
| Product | |
| Module | |
| **Driver architecture** | pwm_direct / two_wire_led_driver (chip) / gpio_direct / mixed |
| Firmware seen | Tasmota <version>, `hardware/tasmota/<name>.png` |
| Template | `{...}` |
| Encoding | legacy 8-bit or modern packed, and how it was detected |
| `BASE` | |

### Decoded functions
| ESP8285 pin | Raw | Packed | Tasmota component | Leg | H2 GPIO (v2) |

### Verdict: COMPATIBLE / NEEDS TEST PADS / INCOMPATIBLE, and servable or not
### Open questions
### Matter mapping
```

4. Tag every claim **[V]**, **[C]** or **[U]**. If the tool and a human disagree, say so in the
   entry rather than resolving it silently.
5. Never promote a decode to "verified" on the strength of a template alone. A template names a
   device, not a module ([`tasmota-import.md`](./tasmota-import.md) section 4.6), and the module is
   what our board replaces.
6. If a new device introduces a driver architecture not already in the summary table, say so
   loudly. It probably means another firmware variant, and it probably means another revision of
   the profile schema in [`flasher-and-registry.md`](./flasher-and-registry.md).
