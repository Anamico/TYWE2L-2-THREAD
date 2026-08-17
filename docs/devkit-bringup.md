# ESP32-H2-DevKitM-1 bring-up guide

Prototyping the TYWE2L replacement on a dev board, before any PCB exists.

> **The shipping pin mapping is v2, and it is settled.** Legs 1 to 5 go to H2 GPIO13, GPIO14,
> GPIO12, GPIO10 and GPIO11. [`docs/pin-mapping-v2.md`](./pin-mapping-v2.md) is the authority and
> [§1.1](#11-what-ships-the-v2-mapping) restates it.
>
> **A stock DevKitM-1 cannot carry that mapping in full**, because GPIO13 and GPIO14 are taken by the
> on-board 32.768 kHz crystal. So the bench uses a separate **prototyping pin set**
> ([§1.2](#12-what-to-wire-on-the-bench-the-prototyping-pin-set)), which is a deliberate choice and
> costs the discovery work nothing ([§1.3](#13-why-the-bench-set-differs-from-the-shipping-set-and-why-that-is-fine)).

Everything below is grounded in the official Espressif documents listed at the bottom. The board in
hand is the **ESP32-H2-DevKitM-1 with 4 MB flash**, which carries an
[ESP32-H2-MINI-1-H4S](https://documentation.espressif.com/esp32-h2-mini-1_mini-1u_datasheet_en.pdf)
(4 MB Quad SPI flash, in-package). That is the same module part the carrier board is designed
around, so the prototype is not an approximation of the target. It is the target module on a
different PCB.

---

## 1. Two pin sets, and why they differ

This guide carries **two** pin tables and they are deliberately not the same. Read both before you
strip a wire.

### 1.1 What ships: the v2 mapping

This is the mapping the carrier board uses. It is settled, signed off, and already applied to
`hardware/carrier/tywe2l-h2-carrier.kicad_sch` and `hardware/DESIGN.md`. The full reasoning lives in
[`docs/pin-mapping-v2.md`](./pin-mapping-v2.md) and is not repeated here.

| TYWE2L leg | TYWE2L net (ESP8285) | **Shipping ESP32-H2 GPIO** | MINI-1 module pin |
|---|---|---|---|
| 1 | GPIO14 | **GPIO13** | 12 |
| 2 | GPIO12 | **GPIO14** | 13 |
| 3 | GPIO13 | **GPIO12** | 16 |
| 4 | GPIO5  | **GPIO10** | 20 |
| 5 | GPIO4  | **GPIO11** | 21 |
| 6 | GND    | GND | 1, 2, 11, 14, 36 to 53 |
| 7 | 3V3    | 3V3 | 3 |

Two things about that table catch people out.

**The numbering crosses over.** ESP8285 GPIO12, GPIO13 and GPIO14 are *leg nets*. H2 GPIO12, GPIO13
and GPIO14 are *module pins*. Both appear in the same design and they do not line up: the leg
carrying ESP8285 GPIO14 goes to H2 GPIO13, and the leg carrying ESP8285 GPIO12 goes to H2 GPIO14.
Say which chip a number belongs to, every time.

**This mapping cannot be reproduced in full on a stock DevKitM-1.** Legs 1 and 2 ship on GPIO13 and
GPIO14, and those two pins are taken by the devkit's on-board 32.768 kHz crystal X1. That is what
§1.2 is for.

### 1.2 What to wire on the bench: the prototyping pin set

Use these five on the DevKitM-1. Four of them land on **J3**, one on **J1**. Read the silkscreen
rather than counting pins: each GPIO pad is labelled with its bare GPIO number.

| TYWE2L leg | TYWE2L net (ESP8285) | **Prototyping H2 GPIO** | Header | Pin no. | Silkscreen | Stands in for | ADC |
|---|---|---|---|---|---|---|---|
| 1 | GPIO14 | **GPIO22** | J3 | 9  | `22`  | GPIO13 | — |
| 2 | GPIO12 | **GPIO1**  | J1 | 4  | `1`   | GPIO14 | ADC1_CH0 |
| 3 | GPIO13 | **GPIO12** | J3 | 7  | `12`  | *(production pin)* | — |
| 4 | GPIO5  | **GPIO10** | J3 | 4  | `10`  | *(production pin)* | — |
| 5 | GPIO4  | **GPIO11** | J3 | 5  | `11`  | *(production pin)* | — |
| 6 | GND    | GND | J3 | 1, 10, 12 or 15 | `G` | — | — |
| 7 | 3V3    | 3V3 | J1 | 1  | `3V3` | — | — |

Read [§4](#4-powering-it-without-destroying-something) before you connect leg 7. In the recommended
bench setup you leave `3V3` **unconnected** and share only GND plus the five signals.

Three of the five are the actual production pins, so legs 3, 4 and 5 are exercised on exactly the
silicon they will ship on. Only legs 1 and 2 use stand-ins, and only because X1 sits on their
production pins.

**Why these particular stand-ins.** Every pin in the set is free on a stock v1.3 board, is absent
from the strapping latch, and comes out of reset with the input buffer disabled and no pull, which
is the cleanest state the part offers. GPIO22 keeps four of the five wires on one header. GPIO1
carries ADC1_CH0, which pays for the diagnostic voltage column in §6 at no cost. Neither is a
strapping pin (GPIO2, GPIO3, GPIO8, GPIO9 and GPIO25 are, and all five are avoided), neither is a
crystal pin, neither is UART0 or USB, and neither is the WS2812B line on GPIO8. The board circuits
these dodge are tabulated in [§2](#2-what-else-is-on-the-devkitm-1).

**GPIO4 is free on the devkit and is still not used**, on the bench or on the board. Its MTCK
after-reset pull-up is active on every factory part and holds the pad towards 3V3 for the 200 to
400 ms it takes to get through the ROM bootloader, the second-stage bootloader and app init. Into a
MOSFET gate or a relay driver on the host you are probing, that is long enough to actuate
something. See [`docs/h2-strapping-and-reset-states.md`](./h2-strapping-and-reset-states.md)
sections 10 and 11.

### 1.3 Why the bench set differs from the shipping set, and why that is fine

The prototype exists to answer one question: **what does the host device do with each TYWE2L leg?**
You answer it by clipping onto a real Tuya board and watching. Whether leg 3 turns out to be a relay
drive, a button input or a PWM channel is a property of *the host*, and it does not change according
to which ESP32-H2 pin happens to sit on the other end of the wire. The discovery is portable; the
mapping is not part of it.

Only the final PCB needs the exact v2 mapping, because only the final PCB has to sit in the socket
and behave like the module it replaced. So the sensible bench choice is the set of pins that are
cleanest and least encumbered on the hardware in front of you, and that is the set in §1.2. Record
the *leg number* in your test log, never the H2 GPIO number, and the results transfer to the board
without translation.

### 1.4 If you want exact fidelity anyway

You can have the real mapping on the bench. Remove **X1**, the 32.768 kHz crystal, and fit **R26**
and **R27** (0 Ω, 0402), which are marked NC on the v1.3 schematic. That connects GPIO13 and GPIO14
through to J1 pins 7 and 8, and the `13/N` and `14/N` silkscreen stops being a lie. One crystal off
and two joints, once, on one board.

**It is not required and it is not recommended for a first pass.** It costs you a board you can
never use for anything needing the 32 kHz oscillator, it is fiddly rework on a 0402 footprint, and
it buys nothing that the discovery work needs. Do it later, if at all, and only to sanity-check the
finished profile against a board wired exactly as the carrier will be.

### 1.5 Header order, for counting along when a label is obscured

```
J1:  1   2   3   4   5   6   7     8     9  10  11   12   13  14  15
    3V3 RST  0   1   2   3  13/N  14/N   4   5   NC  VBAT  G   5V   G
             ^
             leg 2

J3:  1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
     G   TX  RX  10  11  25  12   8  22   G   9   G  27  26   G
                 ^   ^       ^       ^    ^
                 leg leg     leg     leg  GND
                  4   5       3       1
```

Two notes on J1:

- Pins 7 and 8 are silkscreened `13/N` and `14/N`. The `/N` means **not connected**. R26 and R27 are
  0 Ω parts marked NC (not fitted) on the schematic, because GPIO13 and GPIO14 are taken by the
  on-board 32.768 kHz crystal X1. This is the whole reason the two pin sets differ. See §1.4.
- Pin 12 `VBAT` is the module's VBAT pin. The module datasheet says it is "connected to internal 3V3
  power supply (Default) or external battery power supply (3.0 ~ 3.6 V)". Leave it alone. On the
  carrier this matters more than it does here, because three shipping legs sit on the VBAT/VDDA_PMU
  domain, so **VBAT must never be fed from a separate supply** (`pin-mapping-v2.md` section 2.5).

### Practical wiring advice

Put a **330 Ω to 1 kΩ resistor in series with each of the five signal lines**. Both sides are 3.3 V
CMOS, so the levels match, but during discovery you do not yet know which pins the host drives. If
the host drives a pin high and the H2 drives it low, the series resistor turns a dead short into a
few milliamps. It costs nothing and it will save a module.

ESP32-H2 GPIOs are **not 5 V tolerant**. Nothing on a TYWE2L host should present 5 V to these legs,
but confirm with a meter before you connect anything.

---

## 2. What else is on the DevKitM-1

**None of the five prototyping pins is encumbered.** GPIO1 runs straight from the module to J1, and
GPIO10, GPIO11, GPIO12 and GPIO22 run straight to J3, on the v1.3 schematic. No pull resistors, no
LEDs, no buttons, no test points, nothing fitted. The user guide's own header tables list them with
no annotation beyond their alternate functions: J1-4 is "GPIO1, FSPICS0, ADC1_CH0", J3-4 is "GPIO10,
ZCD0", J3-5 is "GPIO11, ZCD1", J3-7 is "GPIO12", J3-9 is "GPIO22".

The things that *are* used on the board, and why each one misses the prototyping set:

| Feature | Pin | Circuit | Clashes with us? |
|---|---|---|---|
| Addressable RGB LED | **GPIO8** | WS2812B (D6), driven through D11 (1N4148W) and R17 (0 Ω), with R25 10 kΩ pull-up to 5 V | **No** |
| GPIO8 strapping pull-up | **GPIO8** | R6, 3.3 kΩ to 3V3 | No |
| BOOT button | **GPIO9** | SW1 to GND | No |
| RESET button | CHIP_PU (EN) | SW2 to GND | No |
| Auto-download transistors | CHIP_PU + **GPIO9** | U6 BC847BDW1T1G dual NPN, from DTR/RTS | No |
| Native USB D-/D+ | **GPIO26 / GPIO27** | to USB-C connector J4 | No |
| UART0 console | **GPIO23 (RX) / GPIO24 (TX)** | to CP2102N | No |
| 32.768 kHz crystal | **GPIO13 / GPIO14** | X1, and R26/R27 to header not fitted | **This is why legs 1 and 2 use stand-ins.** See §1.2 |
| EN pull-up | CHIP_PU | R5 10 kΩ to VBAT, C6 1 µF | No |

The addressable RGB LED is a **WS2812B on GPIO8**, confirmed on the schematic, driven through D11
and R17 with R25 pulling to 5 V. GPIO8 is also a strapping pin and carries a 3.3 kΩ pull-up (R6).
It is nowhere near the prototyping set, and it must not be. Do not be tempted to borrow it.

### Strapping pins: settled, and it is five

This used to be an open contradiction on this page. **It is resolved and the answer went against
the datasheets.** ESP32-H2 has **five** strapping pins: GPIO2, GPIO3, GPIO8, GPIO9 and GPIO25.

The deciding evidence is TRM v1.1 Register 6.7, which enumerates the whole strapping latch:
"bit0: GPIO2, bit1: GPIO3, bit2: GPIO8, bit3: GPIO9, bit4: GPIO25, bit5 ~ bit15: invalid". TRM
section 8.2.2 says the same in prose: "the values of GPIO9, GPIO8, GPIO3 and GPIO2 at reset
determine the boot mode after the reset is released". The chip and module datasheets say three,
listing only GPIO8, GPIO9 and GPIO25, and on this point **the datasheets are wrong**. ESP-IDF's
GPIO reference, which said five all along, was right.

Full working, including why an exhaustive register bit map beats an absent table row, is in
[`docs/h2-strapping-and-reset-states.md`](./h2-strapping-and-reset-states.md) sections 2 and 5.

**What this cost.** GPIO2 and GPIO3 were legs 4 and 3 under the old v1 mapping, so v1 had two host
legs sitting on boot-mode straps. That is what forced the move to v2. It is also why neither pin
appears in the prototyping set: there is no reason to wire a strap to an unknown host, even on the
bench.

The bench test this page used to prescribe (tie GPIO2 and GPIO3 high and low, reset, see if it
boots) is no longer needed to answer the question. If you run it anyway out of curiosity, expect
Joint Download Boot behaviour to change, not a clean boot in all cases.

### JTAG, and why the prototyping set steps around it

GPIO2, GPIO3, GPIO4 and GPIO5 carry the alternate functions **MTMS, MTDO, MTCK and MTDI**. None of
them is in the prototyping set and none is in the shipping mapping, which is deliberate: it keeps
all four JTAG pads clear of host nets.

For the record, they are not live on a factory part anyway. The module datasheet, Table 9 "JTAG
Signal Source Control", lists the default configuration in bold: with `EFUSE_DIS_PAD_JTAG = 0`,
`EFUSE_DIS_USB_JTAG = 0` and `EFUSE_STRAP_JTAG_SEL_ENABLE = 0` (all eFuses ship as 0), the JTAG
signal source is the **USB Serial/JTAG Controller**, and GPIO25 is "Ignored". Pin-based JTAG is not
active out of reset.

That default only changes if someone burns an eFuse. eFuses are one-time programmable. `BACKLOG.md`
carries the never-burn list, and `EFUSE_DIS_USB_JTAG` is the dangerous one: burning it alone forces
pad JTAG and makes MTDO a chip output. Do not burn either JTAG eFuse, and never burn one singly.

### Pin states at reset

Chip datasheet Table 2-1 "Pin Overview" and TRM v1.1 Table 6.13-1 give what each pad does at and
after reset. For the prototyping set:

| Prototyping GPIO | Pad name | Reset code | At reset | After reset | Strapping? |
|---|---|---|---|---|---|
| GPIO1 | `GPIO1` | `0` | (nothing) | (nothing) | No |
| GPIO10 | `GPIO10` | `0` | (nothing) | (nothing) | No |
| GPIO11 | `GPIO11` | `0` | (nothing) | (nothing) | No |
| GPIO12 | `GPIO12` | `0` | (nothing) | (nothing) | No |
| GPIO22 | `GPIO22` | `0` | (nothing) | (nothing) | No |

Reset code `0` is the legend on TRM Table 6.13-1 page 235: `IE = 0`, input disabled, no pull. It is
the cleanest state the part offers, and every pin in the set has it. Nothing on the chip touches
these pads before your firmware does.

**The one pad on the devkit that does not behave, and why it is in neither set.** GPIO4 is `MTCK`,
reset code `1*`, and footnote 4 of Table 2-1 reads "Depends on the value of EFUSE_DIS_PAD_JTAG: 0 -
WPU is enabled; 1 - pin floating". `DIS_PAD_JTAG` ships as 0 on every part and the project has
committed never to burn it, so **GPIO4 carries an internal weak pull-up after reset on every unit
that will ever exist.**

That pull-up is not a few milliseconds. The window runs from reset release through the ROM
bootloader, the second-stage bootloader and app init, on the order of **200 to 400 ms**. Into a
logic-level MOSFET gate with no external pull-down, that energises whatever is on the other side and
holds it energised for the whole window. `h2-strapping-and-reset-states.md` section 11 calls it "a
definite, not marginal, failure". Since nobody yet knows what any leg does, every leg has to be
treated as potentially the actuator line, which turns the per-leg concern into a flat veto:
**no leg goes on GPIO4, on the board or on the bench.**

A secondary nuisance, worth knowing if you ever probe GPIO4 for other reasons: an unconnected GPIO4
reads high rather than floating until firmware disables the pull, so a bare `gpio_get_level` will
mislead you.

---

## 3. The two USB-C ports

Both ports are USB-C. They do completely different things, and the schematic settles which is which.

| | Port **J2**, silkscreened `UART` | Port **J4**, silkscreened `USB` |
|---|---|---|
| Routes to | **CP2102N-A02-GQFN28** (U3) | GPIO26 (D−) and GPIO27 (D+), straight to the module |
| Mechanism | Silicon Labs USB-to-UART bridge, onto UART0 | ESP32-H2 native USB Serial/JTAG peripheral |
| Auto-reset into download mode | **Yes**, via DTR/RTS and U6 | No DTR/RTS circuit |
| JTAG debugging | No | Yes, built in |
| Enumerates as | Silicon Labs CP210x | Espressif USB JTAG/serial debug unit |
| Linux device | `/dev/ttyUSB*` | `/dev/ttyACM*` |
| macOS device | `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART` | `/dev/cu.usbmodem*` |

**The bridge chip is a Silicon Labs CP2102N-A02-GQFN28** (U3 on the v1.3 schematic). Earlier
research left this open; it is now resolved from the schematic itself, not inferred.

### Which one to plug into

**Use the `UART` port (J2, the CP2102N) for both flashing and the serial console.** Reasons:

1. It has the auto-download circuit. `idf.py flash` will reset the chip into download mode by itself
   via DTR/RTS. The native port has no such circuit.
2. The native USB peripheral lives inside the chip. If your firmware crashes, reconfigures GPIO26/27,
   or enters deep sleep, the native port disappears from the host mid-session. A CP2102N console
   keeps printing regardless of what the firmware does to itself, which is exactly what you want
   while probing unknown hardware.
3. UART0 is the default ROM console, so you see the second-stage bootloader output with no config.

Plug into the `USB` port later if you want JTAG single-stepping. You can have both plugged in at
once. The two VBUS rails are OR'd into VCC_5V through Schottky diodes D1 and D7, so neither port
back-feeds the other.

If the silkscreen is hard to read, plug one in and look at what enumerates. The CP210x is the `UART`
port. No guessing required.

### Drivers

- **Linux**: nothing to install. `cp210x` has been in mainline since forever. Add yourself to
  `dialout` (`sudo usermod -aG dialout $USER`, then log out and back in) so you are not fighting
  permissions on `/dev/ttyUSB0`.
- **macOS**: modern macOS ships a CP210x driver. If the port does not appear, install the Silicon
  Labs VCP driver from
  <https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers>.
- **Windows 10/11**: usually picked up over Windows Update. If not, same Silicon Labs VCP link.
- The native `USB` port is CDC-ACM and needs no driver on any of the three.

---

## 4. Powering it without destroying something

This is the part that bites people, so read it before you connect leg 7.

### How the board's power tree actually works

```
J2 VBUS ──D1──┐
              ├── VCC_5V ── U2 (SGM2212-3.3) ── VCC_3V3 ──[ J5 jumper ]── ESP_3V3 ──┬── module 3V3
J4 VBUS ──D7──┘                                    │                                │
                                                   └── CP2102N VDD                  └── J1 pin 1
```

Three facts that follow from this, all from the v1.3 schematic:

- **J1 pin 1 (`3V3`) is on the module side of the J5 jumper.** It is the node `ESP_3V3`, not the
  regulator output.
- **J5 is the only thing between the on-board regulator and the module.** Pull it and the module is
  isolated from the LDO.
- The CP2102N is powered from `VCC_3V3`, upstream of J5. So with J5 removed and the module externally
  powered, the USB-UART bridge still works when USB is plugged in.

### The two ways to get hurt

**Back-powering.** Plug USB into the devkit while J1 pin 1 is wired to a host's 3.3 V rail and J5 is
fitted, and the devkit's LDO will drive current backwards into the host's entire 3.3 V net. On a
switch or dimmer that can partially energise the host MCU and, on some designs, the mains-side
controller, with the host's own supply dead. You get a half-powered board behaving unpredictably,
and possibly a live mains section you did not expect.

**Two supplies fighting.** With both the devkit LDO and the host regulator hard-tied to the same
node, whichever has the higher setpoint sources current into the other. Neither is designed to sink.
The SGM2212 has no reverse-current protection in this configuration. Best case they sit there
warming up while your logic levels drift; worst case one of them fails and takes the module with it.

### Recommended arrangement

**Do not connect the two 3.3 V rails. Ever. Share GND and the five signals only.**

The bench setup that works and is safe:

1. **Unplug the host from mains.** Not switched off at the wall. Physically unplugged. See §7.
2. Feed the host's 3.3 V rail from a **separate bench supply** set to 3.3 V with the current limit
   wound down to ~200 mA. Inject at the TYWE2L 3V3 pad (leg 7) or the host's regulator output, with
   the host's own mains-side supply completely out of circuit.
3. Power the devkit from **USB into the `UART` port**, J5 fitted as shipped.
4. Connect between the two boards: **GND, plus the five signal lines through their series
   resistors**. Leave J1 pin 1 unconnected.
5. Now the only shared node is GND, which is also your scope and laptop reference. That is fine,
   because nothing in the setup is referenced to mains.

If you do not have a bench supply, the devkit's 3V3 can feed the host's low-voltage rail: fit J5,
power the devkit from USB, and run J1 pin 1 to the host's 3V3. This works for a bare MCU, but the
LDO plus USB budget will not carry a relay coil or a mains-side controller. Watch for the red power
LED (D5) dimming, and treat any brownout as a sign to go get a bench supply.

**If the devkit must be powered by the host instead** (host supply is the only supply):

- Remove the **J5 jumper**. The user guide is explicit: "When using 3V3 and GND pin headers to power
  the board, please remove the J5 jumper, and connect an ammeter in series to the external circuit."
- Feed host 3.3 V into J1 pin 1, host GND into J1 pin 13 or 15.
- **Do not plug USB in at the same time** unless J5 is out. With J5 out it is safe, because the LDO
  is isolated from the module.
- Understand that plugging your laptop's USB into that setup ties your laptop's ground to the host's
  ground. If the host is running on mains, that is the lethal case in §7. It is only acceptable when
  the host is running on an isolated bench supply.

---

## 5. BOOT, RESET and manual download mode

From the schematic:

- **SW1 (BOOT)** shorts **GPIO9** to GND. GPIO9 has a weak internal pull-up (bit value 1 by default),
  so the button pulls the strap low.
- **SW2 (RESET)** shorts **CHIP_PU** (the module's EN pin) to GND. CHIP_PU is held up by R5, 10 kΩ to
  VBAT, with C6 1 µF for the power-on delay.
- Auto-download is **U6, a BC847BDW1T1G dual NPN**, driven from the CP2102N's DTR and RTS. The truth
  table printed on the schematic:

  | DTR | RTS | → EN | GPIO9 |
  |---|---|---|---|
  | 1 | 1 | 1 | 1 |
  | 0 | 0 | 1 | 1 |
  | 1 | 0 | 0 | 1 |
  | 0 | 1 | 1 | 0 |

  The pair of transistors is what stops DTR and RTS from ever asserting EN and GPIO9 at the same
  instant, which is the classic ESP auto-reset trick.

### Manual download mode

You should rarely need this on the `UART` port, since esptool drives DTR/RTS for you. When you do:

1. Hold **BOOT** down.
2. Press and release **RESET**.
3. Release **BOOT**.

The chip latches GPIO8 = 1 (floating, pulled up by R6) and GPIO9 = 0, which is Joint Download Boot
per Table 6. The console prints something containing `DOWNLOAD(USB/UART0)` and then sits there.

Reach for this when: you have flashed firmware that immediately crashes or reconfigures the console
pins, auto-reset is not working, or you are flashing over the native `USB` port.

---

## 6. Test firmware: discover what each TYWE2L leg does

The point of this firmware is to answer "what is on leg 3?" on a host board you have no schematic
for. It reads each pin as an input under two pull configurations, counts edges to catch pins that are
being driven with a waveform, and prints a live table. It also takes an ADC reading where the pin
happens to have one, which under the prototyping set is leg 2 only.

The firmware is written against the **prototyping pin set** from §1.2, not the shipping mapping. The
assignments sit in one marked block at the top of the file so you can change them in one place.

Driving pins is deliberately **opt-in behind the BOOT button**, and uses open-drain so the H2 can
only ever pull a line low, never fight a host that is driving it high.

### How to read the results

The two-pull test is the whole trick, and it is what actually identifies a leg. Configure the pin
with an internal pull-up and read it, then with a pull-down and read it again. The internal pulls are
weak (tens of kΩ), so anything the host is actually doing will overpower them:

| pull-up reads | pull-down reads | Verdict |
|---|---|---|
| 1 | 0 | **Floating.** Nothing connected, or a high-Z host input. Probably a button or a host input pin. |
| 1 | 1 | **Driven or pulled HIGH** by the host. A host output sitting high, or a hard pull-up. |
| 0 | 0 | **Driven or pulled LOW.** A host output sitting low, an LED to GND, or a pressed button. |
| 0 | 1 | Inverted, which should not happen. Check your wiring. |

If the edge counter fires, the pin is being **toggled**: PWM to a dimmer or LED, or serial data. On
the community record every TYWE2L leg that does anything is a PWM channel driving a light
(`pin-mapping-v2.md` section 2.7), so expect this column to do most of the work.

### The ADC column is a convenience, not a method

Earlier revisions of this guide leaned on a per-leg ADC reading, and chose pins partly to keep one on
every leg. **That reasoning was wrong at its root and has been withdrawn.** The ESP8266EX has exactly
one analogue input, TOUT, it is a dedicated input-only pin rather than a GPIO, and the TYWE2L does
not bring it out. The module datasheet's own feature list says "Peripherals: five GPIOs" and there is
no TOUT anywhere in the document. **No host device can ever present an analogue signal on a TYWE2L
leg**, so there is nothing for an ADC on a leg to read. Working in `pin-mapping-v2.md`, "Why v1 had
to change".

The reading is kept because it is nearly free and it is occasionally handy: on a toggling pin the
average voltage gives you a rough duty cycle, which is a quicker read than counting edges. Treat it
as a nicety. It influences no pin choice, on the bench or on the board, and the four legs that print
`-` in that column are not worse off for it.

### ESP-IDF version: use v5.5.x, not the newest

"ESP32-H2 needs v5.1" is the usual answer and it is misleading. v5.1 covers chip revisions v0.1 and
v0.2 only. Silicon you buy today is **revision v1.2**, and that needs a newer patch release on
whichever branch you pick. From ESP-IDF's `COMPATIBILITY.md`:

| Release branch | Required for chip rev v1.2 |
|---|---|
| release/v5.1 | v5.1.6 |
| release/v5.2 | v5.2.5 |
| release/v5.3 | v5.3.3 |
| release/v5.4 | v5.4.1 |
| release/v5.5 and above | v5.5 |

**Recommendation for this project: ESP-IDF v5.5.x.** Two reasons:

1. Matter is the whole point of the board, and esp-matter's README pins the toolchain: "For Matter
   projects development with this SDK, it is recommended to utilize ESP-IDF **v5.5.4**". Going
   outside that puts you on an untested combination of esp-matter and connectedhomeip.
2. ESP-IDF v6.x exists and the docs "stable" alias now points at it, but esp-matter has not moved
   there. Do not take v6 just because it is newest.

Check what you have with `idf.py --version`, and confirm the currently supported set against
<https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/versions.html>, since releases age out
on a 30-month policy.

The source below compiles on v5.1 through v6.x. The one version-sensitive item is the ADC
attenuation enum: it was `ADC_ATTEN_DB_11` originally, gained the name `ADC_ATTEN_DB_12` in v5.2 with
the old name deprecated, and in **v6.0 `ADC_ATTEN_DB_11` was removed outright**, so older code will
not compile there. The shim at the top of the file covers every case.

### Project layout

```
tywe2l_probe/
├── CMakeLists.txt
└── main/
    ├── CMakeLists.txt
    └── tywe2l_probe.c
```

**`CMakeLists.txt`** (top level):

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tywe2l_probe)
```

**`main/CMakeLists.txt`**:

```cmake
idf_component_register(SRCS "tywe2l_probe.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_adc driver esp_timer)
```

**`main/tywe2l_probe.c`**:

```c
/*
 * TYWE2L leg prober for ESP32-H2-DevKitM-1.
 *
 * Wire the five TYWE2L legs to the PROTOTYPING pin set in section 1.2 of the
 * bring-up guide, share GND, and leave the 3V3 line disconnected unless you
 * have read the powering section.
 *
 * The prototyping pins are NOT the shipping pins. The carrier board uses the
 * v2 mapping (legs 1..5 -> H2 GPIO13, GPIO14, GPIO12, GPIO10, GPIO11), but
 * GPIO13 and GPIO14 are taken by the devkit's 32.768 kHz crystal X1, so legs 1
 * and 2 use stand-ins here. That is deliberate and harmless: this firmware
 * discovers what the HOST does with each leg, which is a property of the host
 * and not of the H2 pin on the other end of the wire. Log leg numbers, never
 * H2 GPIO numbers, and the results transfer to the board unchanged.
 *
 * All five pins are inputs at all times unless you explicitly enter drive mode
 * by pressing the BOOT button. Drive mode is open-drain: this firmware can pull
 * a line low, never push it high.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"   /* esp_rom_delay_us() */

/* ESP-IDF renamed the 11 dB attenuation to 12 dB partway through 5.x. */
#ifndef ADC_ATTEN_DB_12
#define ADC_ATTEN_DB_12 ADC_ATTEN_DB_11
#endif

static const char *TAG = "tywe2l";

#define BOOT_BTN        GPIO_NUM_9      /* SW1 on the DevKitM-1, active low */
#define EDGE_WINDOW_MS  20              /* how long to watch for toggling */
#define SETTLE_US       200             /* let a pull settle before reading */

/* Sentinel for "this pin has no ADC channel". Most of them do not, and that
 * is fine: see "The ADC column is a convenience, not a method" in the guide. */
#define NO_ADC          ((adc_channel_t)-1)

typedef struct {
    int           leg;    /* TYWE2L leg number, 1..5 */
    const char   *orig;   /* what the ESP8285 called it, i.e. the leg net */
    gpio_num_t    gpio;   /* ESP32-H2 GPIO actually wired on the bench */
    adc_channel_t chan;   /* ADC1 channel, or NO_ADC */
    const char   *hdr;    /* DevKitM-1 header position */
    const char   *ships;  /* the H2 pin this leg uses on the carrier board */
} probe_pin_t;

/* ======================================================================== *
 *                      PIN ASSIGNMENTS: EDIT ONLY HERE                     *
 * ------------------------------------------------------------------------ *
 * The PROTOTYPING set from section 1.2 of the bring-up guide. Every entry   *
 * is free on a stock DevKitM-1 v1.3, absent from the strapping latch        *
 * (GPIO2, GPIO3, GPIO8, GPIO9, GPIO25), clear of X1 (GPIO13, GPIO14),       *
 * UART0 (GPIO23, GPIO24), native USB (GPIO26, GPIO27) and the WS2812B       *
 * (GPIO8), and comes out of reset with IE = 0 and no pull.                  *
 *                                                                          *
 * GPIO4 is free on the devkit and is deliberately NOT used: its MTCK        *
 * after-reset pull-up lasts 200 to 400 ms, which is long enough to actuate  *
 * a relay on the host you are probing.                                     *
 *                                                                          *
 * ADC1 channels, from the ESP32-H2-MINI-1 datasheet Table 3, exist only on  *
 * GPIO1..GPIO5 (CH0..CH4), and ADC1 is the only unit on the part. Under     *
 * this set that is leg 2 alone. Nothing depends on it.                     *
 *                                                                          *
 * If you rework a board per section 1.4 (remove X1, fit R26/R27), change    *
 * leg 1 to GPIO_NUM_13 "J1-7" and leg 2 to GPIO_NUM_14 "J1-8", both NO_ADC, *
 * and you are then running the exact shipping mapping.                      *
 * ======================================================================== */
static const probe_pin_t PINS[] = {
    /*  leg  ESP8285   bench GPIO      ADC1 channel   header    ships on   */
    {   1,  "GPIO14", GPIO_NUM_22,    NO_ADC,        "J3-9",   "GPIO13" },
    {   2,  "GPIO12", GPIO_NUM_1,     ADC_CHANNEL_0, "J1-4",   "GPIO14" },
    {   3,  "GPIO13", GPIO_NUM_12,    NO_ADC,        "J3-7",   "GPIO12" },
    {   4,  "GPIO5",  GPIO_NUM_10,    NO_ADC,        "J3-4",   "GPIO10" },
    {   5,  "GPIO4",  GPIO_NUM_11,    NO_ADC,        "J3-5",   "GPIO11" },
};
/* ==================== END OF PIN ASSIGNMENTS ============================ */

#define NPINS (sizeof(PINS) / sizeof(PINS[0]))

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t         s_cali[NPINS];
static bool                      s_cali_ok[NPINS];

/* ---------- pin mode helpers ---------- */

static void pin_input(gpio_num_t g, gpio_pullup_t up, gpio_pulldown_t down)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << g,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = up,
        .pull_down_en = down,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
}

/* Fully released: no pulls, input buffer off. Required before an ADC read. */
static void pin_analog(gpio_num_t g)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << g,
        .mode         = GPIO_MODE_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
}

/* Open-drain output: can pull low, cannot push high. Safe against a driving host. */
static void pin_open_drain(gpio_num_t g)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << g,
        .mode         = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(g, 1);   /* released */
}

/* ---------- measurement ---------- */

static int read_with_pull(gpio_num_t g, bool pull_up)
{
    pin_input(g,
              pull_up ? GPIO_PULLUP_ENABLE  : GPIO_PULLUP_DISABLE,
              pull_up ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE);
    esp_rom_delay_us(SETTLE_US);
    return gpio_get_level(g);
}

static uint32_t count_edges(gpio_num_t g, uint32_t window_ms)
{
    pin_input(g, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE);
    esp_rom_delay_us(SETTLE_US);

    int64_t  deadline = esp_timer_get_time() + (int64_t)window_ms * 1000;
    int      last     = gpio_get_level(g);
    uint32_t edges    = 0;

    while (esp_timer_get_time() < deadline) {
        int now = gpio_get_level(g);
        if (now != last) {
            edges++;
            last = now;
        }
    }
    return edges;
}

/* Returns millivolts, or -1 if this pin has no ADC channel or the read failed. */
static int read_mv(size_t idx)
{
    const probe_pin_t *p = &PINS[idx];
    int raw = 0, mv = -1;

    if (p->chan == NO_ADC) {
        return -1;
    }

    pin_analog(p->gpio);
    esp_rom_delay_us(SETTLE_US);

    if (adc_oneshot_read(s_adc, p->chan, &raw) != ESP_OK) {
        return -1;
    }
    if (s_cali_ok[idx] && adc_cali_raw_to_voltage(s_cali[idx], raw, &mv) == ESP_OK) {
        return mv;
    }
    /* Uncalibrated fallback: 12-bit result across roughly 3.3 V at 12 dB. */
    return (raw * 3300) / 4095;
}

static const char *verdict(int up, int down, uint32_t edges)
{
    if (edges > 4)               return "TOGGLING (PWM or serial)";
    if (up == 1 && down == 0)    return "floating (nothing driving it)";
    if (up == 1 && down == 1)    return "driven/pulled HIGH";
    if (up == 0 && down == 0)    return "driven/pulled LOW";
    return "inverted?? check wiring";
}

/* ---------- drive mode ---------- */

static bool boot_pressed(void)
{
    pin_input(BOOT_BTN, GPIO_PULLUP_ENABLE, GPIO_PULLDOWN_DISABLE);
    esp_rom_delay_us(SETTLE_US);
    return gpio_get_level(BOOT_BTN) == 0;
}

static void drive_sweep(void)
{
    printf("\n");
    printf("*** DRIVE MODE. Pulling each leg LOW in turn, open-drain, 600 ms each.\n");
    printf("*** Watch the host device for a relay click, an LED, a beep, a state change.\n\n");

    for (size_t i = 0; i < NPINS; i++) {
        const probe_pin_t *p = &PINS[i];
        printf("  leg %d (%s -> GPIO%d, %s): LOW ... ",
               p->leg, p->orig, (int)p->gpio, p->hdr);
        fflush(stdout);

        pin_open_drain(p->gpio);
        gpio_set_level(p->gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(600));
        gpio_set_level(p->gpio, 1);          /* release */
        pin_input(p->gpio, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE);

        printf("released\n");
        vTaskDelay(pdMS_TO_TICKS(400));
    }
    printf("\n*** Drive sweep finished. Back to monitoring.\n\n");
}

/* ---------- setup ---------- */

static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = ADC_UNIT_1,          /* the H2 has ADC1 only */
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,     /* widest input range, ~0 to 3.3 V */
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    for (size_t i = 0; i < NPINS; i++) {
        s_cali_ok[i] = false;

        if (PINS[i].chan == NO_ADC) {
            continue;                    /* most legs have no ADC. Expected. */
        }
        ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, PINS[i].chan, &chan_cfg));

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_curve_fitting_config_t cal = {
            .unit_id  = ADC_UNIT_1,
            .chan     = PINS[i].chan,
            .atten    = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        if (adc_cali_create_scheme_curve_fitting(&cal, &s_cali[i]) == ESP_OK) {
            s_cali_ok[i] = true;
        }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_line_fitting_config_t cal = {
            .unit_id  = ADC_UNIT_1,
            .atten    = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        if (adc_cali_create_scheme_line_fitting(&cal, &s_cali[i]) == ESP_OK) {
            s_cali_ok[i] = true;
        }
#endif
        if (!s_cali_ok[i]) {
            ESP_LOGW(TAG, "no ADC calibration for GPIO%d, millivolts are approximate",
                     (int)PINS[i].gpio);
        }
    }
}

/* ---------- main ---------- */

void app_main(void)
{
    printf("\n\n");
    printf("=========================================================\n");
    printf(" TYWE2L leg prober   ESP32-H2-DevKitM-1\n");
    printf(" All five pins are INPUTS. Hold BOOT to run a drive sweep.\n");
    printf("=========================================================\n");
    printf(" Bench wiring uses the PROTOTYPING pin set, not the\n");
    printf(" shipping v2 mapping. Record LEG numbers in your log.\n");
    printf("   leg  TYWE2L net  bench pin  header  ships on\n");
    for (size_t i = 0; i < NPINS; i++) {
        printf("    %d   %-10s  GPIO%-5d  %-6s  %s\n",
               PINS[i].leg, PINS[i].orig, (int)PINS[i].gpio,
               PINS[i].hdr, PINS[i].ships);
    }
    printf("=========================================================\n\n");

    for (size_t i = 0; i < NPINS; i++) {
        pin_input(PINS[i].gpio, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE);
    }
    adc_init();

    uint32_t pass = 0;

    for (;;) {
        int      up[NPINS], down[NPINS], mv[NPINS];
        uint32_t edges[NPINS];

        for (size_t i = 0; i < NPINS; i++) {
            up[i]    = read_with_pull(PINS[i].gpio, true);
            down[i]  = read_with_pull(PINS[i].gpio, false);
            edges[i] = count_edges(PINS[i].gpio, EDGE_WINDOW_MS);
            mv[i]    = read_mv(i);
            /* leave it benign between passes */
            pin_input(PINS[i].gpio, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE);
        }

        printf("\n--- pass %" PRIu32 "  (t = %.1f s) ---\n",
               ++pass, esp_timer_get_time() / 1e6);
        printf("leg  TYWE2L   bench    hdr     PU  PD  edges     mV  verdict\n");
        printf("---  -------  -------  -----   --  --  -----  -----  -------------------------\n");

        for (size_t i = 0; i < NPINS; i++) {
            const probe_pin_t *p = &PINS[i];
            char mv_txt[8];
            if (mv[i] < 0) {
                snprintf(mv_txt, sizeof mv_txt, "%5s", "-");
            } else {
                snprintf(mv_txt, sizeof mv_txt, "%5d", mv[i]);
            }
            printf(" %d   %-7s  GPIO%-3d  %-5s   %d   %d  %5" PRIu32 "  %s  %s\n",
                   p->leg, p->orig, (int)p->gpio, p->hdr,
                   up[i], down[i], edges[i], mv_txt,
                   verdict(up[i], down[i], edges[i]));
        }

        if (boot_pressed()) {
            drive_sweep();
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### Build and flash

```bash
# once per shell
. $HOME/esp/esp-idf/export.sh

cd tywe2l_probe
idf.py set-target esp32h2
idf.py build

# flash and open the console, over the UART port (CP2102N)
idf.py -p /dev/ttyUSB0 flash monitor
```

Substitute the port for your OS: `/dev/ttyUSB0` on Linux, `/dev/cu.usbserial-XXXX` on macOS, `COM5`
or similar on Windows. `idf.py monitor` exits with `Ctrl+]`.

`idf.py` defaults to **460800 baud** for flashing, not the 115200 that bare esptool uses. If
auto-reset or flashing misbehaves, add `-b 115200` to slow the handshake down, or drop into manual
download mode per §5 and flash again. You can set `ESPPORT` and `ESPBAUD` in your environment
instead of passing `-p` and `-b` every time.

`set-target esp32h2` only needs running once. It wipes the build directory and regenerates
`sdkconfig` from scratch, saving the previous one as `sdkconfig.old`, so run it **before** you change
anything in menuconfig.

Auto-reset works on the `UART` port because esptool asserts DTR and RTS on the CP2102N, and those
lines reach EN and GPIO9 through U6. That is the circuit in §5.

### A note on the ADC numbers

This applies to leg 2 only, the one prototyping pin with an ADC channel. The other four print `-`
and that is correct behaviour, not a fault. See "The ADC column is a convenience, not a method"
above.

ADC1 on the H2 is 12-bit with a maximum sampling rate of 100 kSPS, and the firmware uses the widest
attenuation so it can see the full rail. Calibrated ranges and accuracy, from chip datasheet
Table 5-5:

| Attenuation | Range | Error |
|---|---|---|
| `ADC_ATTEN_DB_0` | 0 to 1000 mV | ±7 mV |
| `ADC_ATTEN_DB_2_5` | 0 to 1300 mV | ±8 mV |
| `ADC_ATTEN_DB_6` | 0 to 1900 mV | ±12 mV |
| `ADC_ATTEN_DB_12` (used here) | 0 to 3300 mV | ±23 mV |

ESP32-H2 uses the **curve fitting** calibration scheme, and it sets
`SOC_ADC_CALIB_CHAN_COMPENS_SUPPORTED`, meaning calibration is per-channel. That is why the code
fills in the `chan` field of `adc_cali_curve_fitting_config_t` for every pin rather than leaving it
zero, which is a genuinely easy mistake to make and quietly costs you accuracy.

### Reading the output

```
--- pass 12  (t = 6.3 s) ---
leg  TYWE2L   bench    hdr     PU  PD  edges     mV  verdict
---  -------  -------  -----   --  --  -----  -----  -------------------------
 1   GPIO14   GPIO22   J3-9     1   0      0      -  floating (nothing driving it)
 2   GPIO12   GPIO1    J1-4     1   1      0   3298  driven/pulled HIGH
 3   GPIO13   GPIO12   J3-7     0   0      0      -  driven/pulled LOW
 4   GPIO5    GPIO10   J3-4     1   0      0      -  floating (nothing driving it)
 5   GPIO4    GPIO11   J3-5     1   1    847      -  TOGGLING (PWM or serial)
```

That example reads as: leg 5 is a PWM output, probably an LED or a dimmer. Leg 3 is held low. Legs 1
and 4 are floating, which on a Tuya switch almost always means button inputs, so press the physical
buttons on the host and watch which row flips to `driven/pulled LOW`. Leg 2 is high, typical of an
active-low relay or LED drive that is currently off.

The `bench` column is the DevKitM-1 pin you wired, and it is **not** the pin that leg will use on the
carrier board. Record the leg number. Leg 5 above is the ESP8285's GPIO4, which ships on H2 GPIO11.

Then press BOOT, and the drive sweep tells you which of them actually makes the host *do* something.

---

## 7. Probing a mains-powered Tuya device: read this properly

**This section is about not dying. Take it literally.**

### The specific hazard

Most Tuya Wi-Fi switches, dimmers, relays and smart plugs use a **non-isolated buck converter** or a
capacitive dropper to generate the low-voltage rail. There is no transformer and no galvanic
barrier. On those designs the net silkscreened `GND`, the net that the TYWE2L calls GND on leg 6, is
**tied directly to one of the mains conductors**, usually neutral, and frequently live depending on
which way round the plug went in.

That means: while the device is plugged into mains, **the "GND" you are about to clip a wire to can
be sitting at full mains potential with respect to earth.** It looks like 0 V because everything on
the board is referenced to it. Your multimeter reading between GND and 3V3 will say a reassuring
3.3 V. It tells you nothing about the potential between that GND and the earth under your feet.

If you connect a USB cable from that board's ground to your laptop, or clip a scope probe's earthed
ground lead to it, you have connected mains to the chassis of your equipment, to the bench, and to
whatever part of you is touching either. This kills people. It is the single most common way
hobbyists are electrocuted working on smart-home hardware, and it happens on the first mistake, not
the tenth.

### The rules

**1. Never probe a live non-isolated supply.** No exceptions, no "just for a second", no "I'll only
touch the low-voltage side". Once mains is applied, the low-voltage side *is* the mains side.

**2. Work on the low-voltage side only, with mains fully removed.** The correct method, and the one
you should use for essentially all of this project:

- Unplug the device from mains. Physically unplug it. Do not trust a wall switch or the device's own
  power button.
- Wait, then **discharge the bulk capacitors** and verify with a meter that the high-voltage
  electrolytic is below a few volts before you touch anything. They hold a dangerous charge for
  minutes after unplugging, and on some designs indefinitely if the bleeder resistor has failed.
- Feed the 3.3 V rail from a bench supply, injected at the TYWE2L 3V3 pad, current-limited to
  ~200 mA.
- Now everything on the board is at bench-supply potential, your ground is a real ground, and you
  can probe, wire and flash freely.

This is not a compromise. You lose almost nothing: relays will click, LEDs will light, buttons work,
and the host MCU runs normally. The only thing you cannot exercise is the mains switching itself,
and you do not need to.

**3. Assume non-isolated until you have proved otherwise.** Do not judge by looks. A device is
isolated only if you can see a real isolation barrier: a transformer with a proper creepage slot, an
optocoupler crossing that slot, and a physical gap in the copper with no traces bridging it. If you
cannot point at all of those, it is non-isolated. Most of these devices are.

A useful check, with the device **unplugged and discharged**: measure resistance between the
low-voltage GND and each mains pin. On a non-isolated buck you will typically find a low resistance
or an obvious diode path to one of them. On a genuinely isolated supply you will read open circuit
in the megohms.

**4. If the device genuinely must run from mains, use a mains isolation transformer.** A proper
isolation transformer, not an autotransformer, not a variac, not an RCD. Feed the device under test
from the transformer's secondary. This removes the earth reference so that touching one conductor no
longer completes a circuit through you to earth.

Understand exactly what that buys you and what it does not:

- It **does** stop a single-point contact between one output conductor and earth from shocking you.
- It **does not** make the circuit safe to touch. Contact **across the two secondary conductors**
  will still kill you, at full mains voltage.
- Your **RCD or RCBO does not protect you on the secondary side.** It cannot see a fault current
  that never returns through earth. You have deliberately removed the mechanism the RCD relies on.
- It does not protect your test equipment from a mistake, only from one particular class of mistake.

So even with an isolation transformer, treat every node on that board as live, and go back to rule
2 wherever you can.

**5. Basic discipline while any mains work is happening.** One hand behind your back, so a shock
cannot cross your chest. No rings, watches or metal bracelets. Insulated tools. Nothing damp. Someone
else in the building who knows what you are doing and where the isolator is. Not when you are tired,
not at 2 am, not while distracted.

**6. Never leave a mains-powered board open on the bench unattended**, and de-energise it before you
walk away, even briefly.

The honest summary: for the whole discovery phase described in this guide, there is no reason to
apply mains at all. Do §7 rule 2, run the host from a bench supply, and the entire hazard disappears.

---

## Sources

All hardware claims above were checked against these documents rather than against memory or
secondary write-ups.

- ESP32-H2-DevKitM-1 user guide:
  <https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32h2/esp32-h2-devkitm-1/user_guide.html>
  (header tables, power supply options, J5 jumper instruction, boot/reset button description)
- ESP32-H2-DevKitM-1 schematic, **v1.3**, dated 19 March 2024:
  <https://dl.espressif.com/dl/schematics/esp32-h2-devkitm-1_v1.3_schematics.pdf>
  (CP2102N-A02-GQFN28 identification, J2 vs J4 routing, WS2812B on GPIO8, X1 on GPIO13/GPIO14 with
  R26/R27 not fitted, absence of any circuit on the five prototyping pins, J5 position in the power
  tree, auto-download truth table, SW1/SW2 connections)
- ESP32-H2-DevKitM-1 schematic v1.2 (older revision, for comparison):
  <https://dl.espressif.com/dl/schematics/esp32-h2-devkitm-1_v1.2_schematics.pdf>
- ESP32-H2-DevKitM-1 PCB layout:
  <https://dl.espressif.com/dl/schematics/esp32-h2-devkitm-1_v1.2_pcb_layout.pdf>
- ESP32-H2-MINI-1 & MINI-1U datasheet, **v1.6**:
  <https://documentation.espressif.com/esp32-h2-mini-1_mini-1u_datasheet_en.pdf>
  (Table 1 part numbers and flash sizes, Table 3 pin definitions and ADC1 channel mapping, Table 4
  strapping pin defaults, Table 6 boot mode control, Table 9 JTAG signal source control, VBAT
  description)
- ESP32-H2 Series Datasheet, **v1.3**:
  <https://documentation.espressif.com/esp32-h2_datasheet_en.pdf>
  (Table 2-1 Pin Overview and footnote 4 for the GPIO4/MTCK pull-up, Table 2-3 IO MUX functions,
  Table 2-5 analog functions, Table 3-1 strapping defaults, Table 3-3 boot mode, Table 3-6 JTAG
  source, Table 5-5 ADC calibration ranges, and the "Three strapping pins" statement)
- ESP-IDF ESP32-H2 GPIO API reference, which said five strapping pins and turned out to be right:
  <https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/api-reference/peripherals/gpio.html>
- ESP32-H2 Technical Reference Manual v1.1, Register 6.7 `GPIO_STRAP_REG` and section 8.2.2, which
  settle the strapping question, and Table 6.13-1 for the reset codes:
  <https://documentation.espressif.com/esp32-h2_technical_reference_manual_en.pdf>
- In-repo companions: [`docs/pin-mapping-v2.md`](./pin-mapping-v2.md) for the shipping mapping and
  the analogue argument, and
  [`docs/h2-strapping-and-reset-states.md`](./h2-strapping-and-reset-states.md) for strapping and
  reset states, which is the authority on both and is not re-derived here.
- ESP-IDF chip revision compatibility, for the minimum patch release per branch:
  <https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY.md>
- ESP-IDF supported versions and support policy:
  <https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/versions.html>
- esp-matter, for the recommended ESP-IDF version for Matter work:
  <https://github.com/espressif/esp-matter/blob/main/README.md>
- ESP-IDF ADC oneshot API:
  <https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/api-reference/peripherals/adc/adc_oneshot.html>
- ESP-IDF ADC calibration, for the curve fitting scheme:
  <https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/api-reference/peripherals/adc/adc_calibration.html>
- ESP-IDF built-in JTAG configuration, confirming USB Serial/JTAG is the default:
  <https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/api-guides/jtag-debugging/configure-builtin-jtag.html>
- esptool boot mode selection, for auto-reset behaviour with a CP210x:
  <https://docs.espressif.com/projects/esptool/en/latest/esp32h2/advanced-topics/boot-mode-selection.html>
- Establishing a serial connection, for port naming and driver expectations:
  <https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/get-started/establish-serial-connection.html>
- Silicon Labs CP210x VCP drivers:
  <https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers>

### What I could not verify

Flagged honestly rather than asserted:

- **Silkscreen text on the two USB-C ports.** The schematic block titles are `UART:` for J2 and
  `USB:` for J4, and the user guide calls them the "USB Type-C to UART Port" and the "ESP32-H2 USB
  Type-C Port". The electrical routing is certain. The exact wording printed on the board is
  inferred, so use the enumeration test in §3 if the labels are unclear.
- **Physical placement of J1 and J3 on the board**, meaning which is left and which is right in any
  given orientation. Identify J1 by its silkscreen sequence (`3V3`, `RST`, `0`, `1`, ...) rather than
  by position. I did not open the PCB layout PDF to confirm placement.
- **Which module variant is fitted to your specific board.** The schematic symbol just says
  `ESP32-H2-MINI-1`. The "4 MB Flash" in the product name implies the `-H4S` variant per datasheet
  Table 1. Confirm with `esptool.py flash_id` once you have it connected.
- ~~**Whether GPIO2 and GPIO3 are strapping pins.**~~ **Resolved: they are.** TRM v1.1 Register 6.7
  enumerates the whole strapping latch and gives GPIO2 bit 0 and GPIO3 bit 1, and section 8.2.2 says
  their reset values help select the boot mode. The datasheets' "three strapping pins" is wrong. This
  invalidated the v1 mapping and produced v2. No bench test needed. See §2 and
  [`docs/h2-strapping-and-reset-states.md`](./h2-strapping-and-reset-states.md).
- **Whether Windows 10/11 and macOS genuinely ship an in-box CP210x driver.** Espressif says drivers
  "should be bundled with an operating system and automatically installed", and the Linux kernel's
  `cp210x` table carries a PID commented as the Windows Update variant, which is suggestive. Neither
  Silicon Labs nor Espressif states it outright, so treat "no manual install needed" as probable
  rather than confirmed. Linux is certain: `cp210x` is mainline.
- **The exact ESP-IDF release to pin.** v5.5.4 is what esp-matter recommends today, but that moves
  with esp-matter. Re-check its README rather than trusting this page in six months.
- **Non-blocking `stdin` behaviour**, which is why the firmware uses the BOOT button rather than
  keyboard hotkeys to trigger drive mode. That decision avoids a console dependency I could not test
  on hardware.
- Nothing in this guide has been run on a physical board. It is derived from official documents.
