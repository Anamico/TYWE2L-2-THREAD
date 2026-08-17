# ESP32-H2 strapping pins and pin reset states

Scope: resolving two hardware questions for the TYWE2L drop-in replacement module, both of which
affect copper and cannot be fixed after fabrication.

1. Are GPIO2 and GPIO3 strapping pins, and is it safe to route an uncontrolled host signal to them?
2. Does GPIO4 have an internal pull-up around reset, and is that a hazard on a relay drive line?

All evidence below is from primary sources: the ESP32-H2 Technical Reference Manual v1.1, the
ESP32-H2 Series Datasheet v1.3, the ESP32-H2-MINI-1 & MINI-1U Datasheet v1.6, and the ESP-IDF
source tree at commit `08e0d30a74ad0bfd5a34933142b80f45619ee410` (master, 3 August 2026).

---

## Verdict

### Question 1: GPIO2 and GPIO3

**The ESP-IDF documentation is right and the datasheet is incomplete.** GPIO2 and GPIO3 are
genuinely latched at reset. They occupy bits 0 and 1 of `GPIO_STRAP_REG`, and the Technical
Reference Manual's boot mode table lists them alongside GPIO8 and GPIO9. The chip datasheet and the
module datasheet both omit them because both omit the entire boot mode they select.

**Routing an uncontrolled host signal to GPIO2 and GPIO3 is SAFE, conditional on one change.**

The condition: **fit a 10 kΩ pull-up from GPIO8 to 3V3 on the carrier board.** GPIO8 is currently
floating by default, and that is the only thing that exposes GPIO2 and GPIO3 at all.

Reasoning in one line: GPIO2 and GPIO3 are only consulted when GPIO9 = 0 **and** GPIO8 = 0. GPIO9
has an internal weak pull-up and is high in normal operation. Pulling GPIO8 high makes the
GPIO9 = 0 case resolve to Joint Download Boot regardless of GPIO2 and GPIO3, so the two lines
become true don't-cares in every reachable state.

**The pin mapping does not need to change.** GPIO1 through GPIO5 stay as the five host-facing
lines. No ADC1 capability is lost. The cost is one 0402 resistor.

This is not just my reading of the tables. Espressif staff gave the same answer for the
architecturally identical ESP32-C6 case in June 2026, verbatim: "When GPIO8 is 1 and GPIO9 is 0, the
chip enters Joint Download Boot mode. In this, GPIO2 does not matter, and you can do with this pin
whatever you want." See Section 9.

Residual caveat, stated plainly: without that pull-up, a host driving
GPIO2 high while the module is put into download mode lands the chip in the single-interface
download branch. On ESP32-C6 that exact condition has been observed on real hardware to leave the
chip printing `wait sdio download` and going no further. The application never runs. Nudge GPIO2 the
other way and you land in what the TRM labels "Invalid Combination", which it says "can trigger
unexpected behavior and should be avoided".

Severity: this needs GPIO9 = 0, which on our design means someone is deliberately trying to flash.
So it is a failed-flashing risk in the factory or a service depot, not a field hazard, and the
appliance in a customer's house is not exposed. But "the board hangs and nobody knows why" is an
expensive fault to chase on a production line, and the C6 reporter had to disassemble the boot ROM
to work out what had happened to him. One resistor avoids all of it.

### Question 2: GPIO4 internal pull-up

**The premise is confirmed but the timing in it is wrong, and the error matters.**

The footnote is real. GPIO4 (pad name MTCK) does get a weak internal pull-up whenever
`EFUSE_DIS_PAD_JTAG` = 0, which is every factory part. It is roughly 45 kΩ.

But it is **not** active *during* reset. The datasheet places it in the **After Reset** column, and
the *At Reset* cell for that pin is empty. The pull-up switches on when reset releases and stays on
until firmware reconfigures the pad. That is a window of roughly 200 to 400 ms covering ROM
bootloader, second stage bootloader and app startup, not a few microseconds of reset. It is a
longer and more dangerous window than the question assumed.

It applies to **GPIO4 alone**. GPIO2, GPIO3 and GPIO5 do not have it, despite also being JTAG pads.

**Yes, this can turn on a relay driver.** Into a logic-level MOSFET gate with no pull-down, 45 kΩ to
3.3 V charges the gate fully on. Into a bipolar base it delivers about 58 µA, which is marginal but
not safely off.

**Recommendation: do not put the relay drive line on GPIO4 at all.** Use GPIO0, GPIO1, GPIO10,
GPIO11, GPIO12 or GPIO22, all of which have both reset columns blank. If GPIO4 must carry an
actuator line, fit a **2.2 kΩ pull-down** to ground on the carrier.

**Do not burn `EFUSE_DIS_PAD_JTAG` to solve this, and the flashing tool must never burn it
automatically.** It is irreversible, it does not help parts already built, and a 2.2 kΩ resistor
solves the same problem for a fraction of a cent with no permanent consequences.

### Question 3: the "GPIO2/GPIO3 are only JTAG pads" hypothesis

Tested explicitly. **The hypothesis is false, but the practical conclusion it points at is still
correct, for a different reason.**

**GPIO2 and GPIO3 are genuinely latched at reset to configure a boot parameter.** They are not
merely pins that JTAG can commandeer. Two independent proofs, both primary:

1. `GPIO_STRAP_REG` has dedicated latch bits for them: bit 0 is GPIO2, bit 1 is GPIO3 (TRM v1.1
   Register 6.7). JTAG alternate functions do not get strapping latch bits. Nothing else in the IO
   MUX does either.
2. TRM v1.1 Section 8.2.2: "The values of GPIO9, GPIO8, **GPIO3 and GPIO2** at reset determine the
   boot mode after the reset is released." That is a boot parameter, sampled at reset, in as many
   words.

**The cleanest disproof is in the ESP-IDF file itself.** All four JTAG pads are MTMS (GPIO2), MTDO
(GPIO3), MTCK (GPIO4) and MTDI (GPIO5). If ESP-IDF were listing pins because they are JTAG pads,
all four would be marked. Only GPIO2 and GPIO3 are. `esp32h2.inc:40-46` gives GPIO4 and GPIO5 an
empty Comments column. The two that are marked are exactly the two with `GPIO_STRAP_REG` bits, and
that correspondence is not a coincidence.

**So GPIO25 being pulled high does not do the work.** It is good practice and worth keeping, but it
is not what makes GPIO2 and GPIO3 safe. By default it does nothing at all: with
`EFUSE_JTAG_SEL_ENABLE` = 0 (factory state), datasheet Table 3-6 marks the GPIO25 column "Ignored"
and the JTAG source is the USB Serial/JTAG controller regardless of the pull. GPIO25 only acquires a
vote if we burn `EFUSE_JTAG_SEL_ENABLE`.

**The override you asked me to chase exists, and it is not GPIO25.** Datasheet Table 3-6 row 4:
with `EFUSE_DIS_PAD_JTAG` = 0 and **`EFUSE_DIS_USB_JTAG` = 1**, the JTAG source is forced to the
**JTAG pins**, with both `EFUSE_JTAG_SEL_ENABLE` and GPIO25 marked "Ignored". Burning
`DIS_USB_JTAG` commandeers GPIO2, GPIO3, GPIO4 and GPIO5 as JTAG permanently and no external pull
can prevent it. MTDO (GPIO3) is a chip **output** in that state, so a host also driving that net
means direct contention. **Add `EFUSE_DIS_USB_JTAG` to the never-burn list alongside
`EFUSE_DIS_PAD_JTAG`.** It is not currently on our list and it should be.

The mapping is still safe, on the GPIO8 pull-up argument in Question 1, not on the JTAG argument.

**On electrical equivalence to the ESP8285: it is one line, not four.** GPIO2, GPIO3 and GPIO5 do
**not** share GPIO4's reset pull-up. TRM v1.1 Table 6.13-1 gives reset code `1` (input enabled, no
pull) for MTMS, MTDO and MTDI, and `1*` for MTCK alone. Being a JTAG pad is not what causes the
pull-up. Only the JTAG **clock** input gets one. So the deviation from the ESP8285 is confined to
GPIO4, and the "materially bigger deviation" case does not arise.

---

## Evidence

### 1. What ESP-IDF documentation says

Both the `latest` and the `v5.5` builds carry the same text. Verbatim, from
<https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-reference/peripherals/gpio.html>
and <https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32h2/api-reference/peripherals/gpio.html>:

> Strapping pin: GPIO2, GPIO3, GPIO8, GPIO9, and GPIO25 are strapping pins. For more information,
> please refer to ESP32H2 datasheet.

The GPIO summary table on the same page marks GPIO2 "Strapping pin", GPIO3 "Strapping pin", GPIO8
"Strapping pin, RTC", GPIO9 "Strapping pin, RTC" and GPIO25 "Strapping pin".

The docs version does not matter. Both branches agree.

Source file in the ESP-IDF tree, which is the origin of that rendered text:

- `docs/en/api-reference/peripherals/gpio/esp32h2.inc:138` holds the note text quoted above.
- `docs/en/api-reference/peripherals/gpio/esp32h2.inc:32-38` are the GPIO2 and GPIO3 table rows,
  each with `Strapping pin` in the Comments column and `ADC1_CH1` / `ADC1_CH2` in the Analog
  Function column.

Note the irony: the ESP-IDF note points the reader at the datasheet for details, and the datasheet
does not mention GPIO2 or GPIO3 as strapping pins anywhere. That circular reference is the whole
source of the confusion.

### 2. What the Technical Reference Manual says

ESP32-H2 Technical Reference Manual v1.1, Chapter 8 "Chip Boot Control".

Section 8.1 Overview opens by agreeing with the datasheet:

> ESP32-H2 has three strapping pins:
> - GPIO8
> - GPIO9
> - GPIO25

Then Section 8.2.2 "Boot Mode Control", two pages later, contradicts its own overview:

> The values of GPIO9, GPIO8, GPIO3 and GPIO2 at reset determine the boot mode after the reset is
> released.

Table 8.2-2 "Boot Mode Control" is the authoritative statement. Reproduced exactly:

| Boot Mode | GPIO9 | GPIO8 | GPIO3 | GPIO2 |
|---|---|---|---|---|
| SPI Boot mode | 1 | x¹ | x | x |
| Joint Download Boot mode² | 0 | 1 | x | x |
| SPI Download Boot mode³ | 0 | 0 | 0 | 1 |
| Invalid Combination⁴ | 0 | 0 | x | 0 |

With footnotes, verbatim:

> ¹ x: values that have no effect on the result and can therefore be ignored.
>
> ³ SPI Download Boot mode: GPIO3 and GPIO2 need to be reserved only when using SPI Download Boot
> mode. GPIO3 and GPIO2 are floating by default and are in a high-impedance state at reset.
>
> ⁴ Invalid Combination: This combination can trigger unexpected behavior and should be avoided.

Section 8.1 also defines what "latched" means here, verbatim:

> During Chip Reset (see Chapter 7 Reset and Clock), hardware captures samples and stores the
> voltage level of strapping pins as strapping bit of "0" or "1" in latches, and holds these bits
> until the chip is powered down or next chip reset. Software can read the latch status (strapping
> value) from GPIO_STRAPPING.

And Section 8.2 carries a general warning that applies directly to any undocumented row of Table
8.2-2:

> Notice:
> Only documented patterns should be used. If an undocumented pattern is used, it may trigger
> unexpected behaviors.

The register description settles it beyond argument. TRM v1.1, Chapter 6 "IO MUX and GPIO Matrix",
Register 6.7 `GPIO_STRAP_REG` (offset 0x0038), verbatim:

> GPIO_STRAPPING Represents the values of GPIO Strapping pins.
> - bit0: GPIO2 (GPIO2 should be reserved as a Strapping pin only when using SPI Download Boot mode.)
> - bit1: GPIO3 (GPIO3 should be reserved as a Strapping pin only when using SPI Download Boot mode.)
> - bit2: GPIO8 (this value will also be affected by EFUSE_DIS_FORCE_DOWNLOAD and LP_AON_FORCE_DOWNLOAD_BOOT.)
> - bit3: GPIO9 (this value will also be affected by EFUSE_DIS_FORCE_DOWNLOAD and LP_AON_FORCE_DOWNLOAD_BOOT.)
> - bit4: GPIO25
> - bit5 ~ bit15: invalid

GPIO2 and GPIO3 have dedicated latch bits in the strapping register. They are latched. That is
settled hardware fact, not documentation opinion.

### 3. What the datasheets say, and why they disagree

ESP32-H2 Series Datasheet v1.3, Section 3 "Boot Configurations", verbatim:

> The chip allows for configuring the following boot parameters through strapping pins, eFuse bits,
> and registers at power-up or a hardware reset, without microcontroller interaction.
> - Chip boot mode
>   - Strapping pin: GPIO8 and GPIO9
> - ROM message printing
>   - Strapping pin: GPIO8
>   - eFuse bits: EFUSE_UART_PRINT_CONTROL and EFUSE_DIS_USB_SERIAL_JTAG_ROM_PRINT
>   - Register: LP_AON_STORE4_REG[0]
> - JTAG signal source
>   - Strapping pin: GPIO25
>   - eFuse bits: EFUSE_DIS_PAD_JTAG, EFUSE_DIS_USB_JTAG, and EFUSE_JTAG_SEL_ENABLE

Datasheet Table 3-3 "Chip Boot Mode Control" has only two rows, SPI Boot (GPIO9 = 1, GPIO8 any
value) and Joint Download Boot (GPIO9 = 0, GPIO8 = 1). Table 3-1 "Default Configuration of
Strapping Pins" lists only GPIO8 (Floating), GPIO9 (Weak pull-up, bit value 1) and GPIO25
(Floating).

The ESP32-H2-MINI-1 & MINI-1U Datasheet v1.6, Section 4 "Boot Configurations", is a verbatim copy.
It says so itself:

> Note: The content below is excerpted from ESP32-H2 Series Datasheet > Section Boot Configurations
> via Strapping Pins and eFuses.

Its feature list on page 2 says "Up to 19 GPIOs – 3 strapping pins".

**The reconciliation.** The datasheet does not describe a *different* strapping set. It describes a
*subset of boot modes*. Both datasheets omit SPI Download Boot mode entirely. GPIO2 and GPIO3 are
the pins that select SPI Download Boot, so once that mode is dropped, the two pins have nothing
left to do and drop out of the list with it. There is no contradiction about the silicon, only a
difference in editorial scope. The TRM is the superset and is correct.

This also explains why the omission is defensible from Espressif's side. SPI Download Boot on
ESP32-H2 loads firmware over the SPI bus, which is GPIO15 to GPIO21. On the ESP32-H2FH2S and
ESP32-H2FH4S parts used in the MINI-1 module, those pins are consumed by the in-package flash and
are not brought out. ESP-IDF's own documentation states this at
`docs/en/api-reference/peripherals/gpio/esp32h2.inc:141`:

> For chip variants with an SiP flash built in, GPIO15–GPIO21 are dedicated to connecting the SiP
> flash and are not fan-out to the external pins. In addition, GPIO6–GPIO7 are also not fan-out to
> the external pins. In conclusion, only GPIO0–GPIO5, GPIO8–GPIO14, GPIO22–GPIO27 are available to
> users.

**SPI Download Boot is not reachable on our module.** The bus it would download over does not leave
the package. That is a significant part of why GPIO2 and GPIO3 are safe in practice.

### 4. ESP-IDF source

There is no `SOC_GPIO_STRAPPING_MASK`, `ESP_GPIO_REVERSED_MASK` or equivalent strapping bitmask
anywhere in the tree. A repo-wide grep for `STRAPPING_PIN`, `STRAP_PIN`, `strapping_pin`,
`is_strapping` and `STRAP_MASK` across all `.c`, `.h` and `.py` files returns no strapping-related
hits at all. ESP-IDF does not encode the strapping pin list in software. The documentation table is
hand-maintained prose, and the register header is the machine-readable truth.

What does exist:

- `components/soc/esp32h2/register/soc/gpio_reg.h:109-119` defines `GPIO_STRAP_REG` at
  `DR_REG_GPIO_BASE + 0x38` with a 16-bit read-only `GPIO_STRAPPING` field. The header carries no
  per-bit comment, so the TRM Register 6.7 description quoted above is the only bit map.
- `components/soc/esp32h2/register/soc/gpio_struct.h:119-131` mirrors it as `gpio_strap_reg_t` with
  `uint32_t strapping:16`.
- `components/soc/esp32h2/include/soc/boot_mode.h:42` reads it:
  `#define BOOT_MODE_GET() (GPIO_REG_READ(GPIO_STRAP_REG))`.

`components/hal/esp32h2/` and `components/esp_hw_support/port/esp32h2/` contain nothing relevant. A
grep for `strap` in both returns only hits on the unrelated `rtc_clk_32k_bootstrap()` function.
`components/soc/esp32h2/gpio_periph.c:13-16` is a plain IO MUX register address table with no
strapping annotation.

**One caution about `boot_mode.h`.** That header is generic boilerplate copied across Espressif
targets and its macro names do not match the ESP32-H2 boot table. Reading TRM Register 6.7's bit
order, the low four bits of `GPIO_STRAP_REG` are `{GPIO9, GPIO8, GPIO3, GPIO2}`. On that reading:

- `IS_1XXX` at `boot_mode.h:13` correctly matches SPI Boot (GPIO9 = 1).
- `ETS_IS_SPI_DOWNLOAD_BOOT()` at `boot_mode.h:75` expands to `IS_0110`, meaning GPIO9 = 0,
  GPIO8 = 1, GPIO3 = 1, GPIO2 = 0. TRM Table 8.2-2 says SPI Download Boot is `0001`. These do not
  agree.
- `ETS_IS_JOINT_DOWNLOAD_BOOT()` at `boot_mode.h:80` expands to `IS_00XX` (GPIO9 = 0, GPIO8 = 0),
  where TRM Table 8.2-2 says Joint Download Boot is GPIO9 = 0, GPIO8 = 1.

Do not design against `boot_mode.h`. It is legacy, it is not referenced by the ESP32-H2 boot path,
and it disagrees with the TRM. TRM Table 8.2-2 and Register 6.7 are the authority.

### 5. What GPIO2 and GPIO3 actually control, and what "latched" means for us

They select **SPI Download Boot mode**, and nothing else. They do not touch flash voltage, flash
drive strength, ROM UART download selection, secure boot, download-mode disable, USB or JTAG pad
selection, or any SDIO or slave configuration. Every one of those is accounted for elsewhere:

| Boot parameter | Controlled by | Citation |
|---|---|---|
| Chip boot mode | GPIO9, GPIO8, GPIO3, GPIO2 | TRM Table 8.2-2 |
| ROM message printing | GPIO8, `EFUSE_UART_PRINT_CONTROL`, `LP_AON_STORE4_REG[0]` | TRM Table 8.2-3, datasheet Table 3-4 |
| JTAG signal source | GPIO25, `EFUSE_DIS_PAD_JTAG`, `EFUSE_DIS_USB_JTAG`, `EFUSE_JTAG_SEL_ENABLE` | TRM Section 1.10.4, datasheet Table 3-6 |
| Force download from software | `EFUSE_DIS_FORCE_DOWNLOAD`, `LP_AON_FORCE_DOWNLOAD_BOOT` | TRM Section 8.2.2 |
| Download mode disable | `EFUSE_DIS_DOWNLOAD_MODE` | TRM Section 8.2.2 |
| Direct Boot disable | `EFUSE_DIS_DIRECT_BOOT` | TRM Section 8.2.2 |
| Flash voltage / drive strength | not strapped on ESP32-H2 | absent from TRM Chapter 8 |

Flash voltage strapping does not exist on this part. ESP32-H2 has 3.3 V flash inside the package,
as the datasheet cover states, so there is no `VDD_SPI` selection pin of the kind ESP32 and ESP32-S3
have.

**What "latched" means for the host circuitry, precisely.** The latch samples GPIO2 and GPIO3 at
every chip reset and holds the sampled bits for the life of the power cycle. An external pull, or a
host output stage, absolutely does change the latched value. The latch is not decorative. But the
latched value is then fed into Table 8.2-2, and Table 8.2-2 marks both bits `x` in the SPI Boot row
and `x` in the Joint Download Boot row. So the value changes, and then the boot logic ignores it.

The value remains readable by software from `GPIO_STRAPPING` bits 0 and 1 for the whole power cycle.
Our firmware must not read those bits and infer anything about the host, since they will reflect
whatever the host happened to be doing at reset, but nothing in ESP-IDF reads them on this target.

After the hold time expires the pads revert to ordinary IO. Datasheet Table 3-2:

> t_H — Hold time is the time reserved for the chip to read the strapping pin values after CHIP_EN
> is already high and before these pins start operating as regular IO pins. Min 3 ms.

So there is a 3 ms window after CHIP_EN goes high during which GPIO2 and GPIO3 are being sampled.
Our firmware must not try to drive them as outputs inside that window, which it cannot do anyway
since no code is running yet.

### 6. Errata and silicon revisions

Source: "ESP32-H2 Series SoC Errata", **v1.3, dated 9 June 2026**,
<https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32h2/>. Note that the standalone
errata PDF no longer exists. The old
`www.espressif.com/sites/default/files/documentation/esp32-h2_errata_en.pdf` URL now redirects to
the HTML documentation, and the "Download PDF" link on that page returns 404.

**Revisions that exist:** v0.0, v0.1 and v1.2, per the Chip Revision Identification chapter. There
is no v1.0 or v1.1 for this part. Our parts are v1.2, which is the current and only
mass-production revision. Module marking for v1.2 reads "MF XXXX", chip marking "X F XXXXXXXX".
v0.0 was never mass produced.

**No erratum touches the strapping pin list, the reset pull states, or GPIO2, GPIO3, GPIO4, GPIO8,
GPIO9 or GPIO25 as pins.** The strapping behaviour described in this document is identical across
all three revisions. The full list is fifteen items. **Two of them affect v1.2**, per the
by-revision index at
<https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32h2/_tags/v1-2.html>:

- **PCNT-249**, "Unable to Trigger Step Interrupts", affects v0.0, v0.1 and v1.2. No fix scheduled.
  A pulse counter defect with no bearing on strapping, boot or pin reset states.
- **ECDSA_DS-836**, "Signatures with Invalid r and s Values Are Incorrectly Accepted", affects
  **v1.2 only**. No fix scheduled. See the warning below.

The boot-related one, fixed in our silicon:

- **BOOT-9537**, "Accidentally Enter USB Download Boot Mode If the Power-up Duration Is Too Long",
  affects v0.0 and v0.1, **fixed in v1.2**. Verbatim: "During power-on, if the voltage rises from
  0 V to 3.3 V in more than 12 ms, the chip may accidentally enter USB Download Boot mode."
  Workaround for affected revisions: "Ensure that the power-up duration is less than 12 ms."

BOOT-9537 deserves a note even though our silicon is fixed, because it is the exact class of failure
this whole exercise is guarding against: a chip entering download mode when it should have booted
the application, in a mains appliance where that means the relay logic never runs. It is fixed in
v1.2, so it is not a design constraint for us. But it is a reason to pin the silicon revision in the
production specification and to re-check this document if anyone ever second-sources v0.1 parts.
Keeping the supply rise under 12 ms is cheap insurance regardless.

> **Flagged outside the scope of this document, because it bears on the Matter work.**
> **ECDSA_DS-836 affects v1.2, which is our silicon, and has no scheduled fix.** Verbatim from the
> erratum: "When the signature `{r = 0 or n, s = 0 or n}` with invalid `r` and `s` values is
> submitted to the ECDSA_DS peripheral against any message and public key, the peripheral
> incorrectly reports the signature as 'valid'." The stated workaround is equally blunt: "Use RSA_DS
> Secure Boot instead of ECDSA_DS Secure Boot."
>
> A Matter product doing attestation and secure boot is precisely the use case this touches. It does
> not change any pin decision in this document, and I have not assessed how far it reaches into
> Matter DAC verification as opposed to secure boot image verification. **It needs its own
> investigation before the secure boot scheme is locked in.** Raising it here because it surfaced
> while checking the errata for pin issues and should not be lost.

The remaining twelve errata (CPU-206, CLK-6996, ADC-7227, ADC-1477, I2C-308, SPI-304, LEDC-253,
RMT-176, AES-11401, ECC-11400, ECDSA_DS-837, 802.15.4-9538) affect v0.0 and v0.1 only and are
irrelevant to v1.2 parts. Two are worth noting in passing because they bear directly on this
design's reliance on ADC1 across GPIO1 to GPIO5: **ADC-7227 "Unavailable Channel 4 in SAR ADC1"**
(ADC1_CH4 is GPIO5/MTDI, so that pin's analogue function simply did not work) and **ADC-1477 "Loss
of Precision in Lower Four Bits of SAR ADC"**. Both are fixed in v1.2. A second reason to specify
the revision in the production documentation rather than accepting whatever a broker ships.

**Minimum ESP-IDF for v1.2 silicon**, from
<https://raw.githubusercontent.com/espressif/esp-idf/master/COMPATIBILITY.md>: v5.1.6 on
release/v5.1, v5.2.5 on release/v5.2, v5.3.3 on release/v5.3, v5.4.1 on release/v5.4, and v5.5 on
release/v5.5 and later. Support for v1.2 silicon landed in ESP-IDF v5.5 ("Added support for
ESP32-H2 revision v1.2"). Doc drift worth knowing: `COMPATIBILITY.md` heads the older revisions
"v0.1, v0.2" while the errata document and the eFuse table call them v0.0 and v0.1.

### 7. Datasheet revision history: the omission is longstanding, not a recent slip

ESP32-H2 Series Datasheet v1.3, "Revision History" on pages 67 and 68, covers every release from
v0.5 (24 May 2023) through v1.3 (3 July 2026). No entry mentions the strapping pin list, GPIO2,
GPIO3, or the Table 2-1 reset-state footnotes. The only strapping-adjacent entry is under v1.2
(13 October 2025):

> Updated Figure 3-1 Visualization of Timing Parameters for the Strapping Pins

That is a redrawn timing diagram, not a change of substance. The v1.1 entry (28 February 2025)
records the updates made "based on chip revision v1.2", and none of them touch pin behaviour.

The same holds for the module datasheet. ESP32-H2-MINI-1 & MINI-1U Datasheet v1.6, Revision History
on page 44, has one strapping entry, under v0.6 (17 October 2023):

> Updated the description about Boot Mode Control in Section Strapping Pins.

and its v1.1 entry (28 February 2025) is an ordering-code change "according to chip revision v1.2".

Conclusion: the datasheet has described three strapping pins since its first official release and
has never described anything else. This is a stable editorial scope decision, not a documentation
bug that might be corrected out from under us, and not something that changed between silicon
revisions. Designing against the TRM's four-pin boot table is designing against the more
conservative of the two, which is the right direction to err.

### 8. Cross-target check: ESP32-H2 is the only target where the two disagree

My first reading of this was that ESP-IDF applies a house convention of listing every pin present in
the strapping latch register, while each datasheet lists only the pins its documented boot modes
need. That theory is wrong, and it is worth recording why, because the correct answer is less
comfortable.

Sweeping every target's `.inc` file against its datasheet:

| Target | ESP-IDF `.inc` list | Datasheet | Agree? |
|---|---|---|---|
| ESP32-C2 | GPIO8, GPIO9 | same | yes |
| ESP32-C3 | GPIO2, GPIO8, GPIO9 (`esp32c3.inc:114`) | same | yes |
| ESP32-C5 | GPIO2, GPIO7, GPIO25, GPIO27, GPIO28 (`esp32c5.inc:172`) | same | yes |
| ESP32-C6 | GPIO4, GPIO5, GPIO8, GPIO9, GPIO15 (`esp32c6.inc:182`) | same, five | **yes** |
| ESP32-S3 | GPIO0, GPIO3, GPIO45, GPIO46 | same | yes |
| ESP32-P4 | GPIO34–GPIO38 | same | yes |
| ESP32-H21 | GPIO8, GPIO13, GPIO14 (`esp32h21.inc:157`) | same | yes |
| **ESP32-H2** | **GPIO2, GPIO3, GPIO8, GPIO9, GPIO25** | **GPIO8, GPIO9, GPIO25** | **no** |

The ESP32-C6 row is the one that kills the convention theory. ESP32-C6 Series Datasheet v1.5,
Chapter 3, Table 3-1 lists **five** strapping pins: MTMS, MTDI, GPIO8, GPIO9 and GPIO15. MTMS is
GPIO4 and MTDI is GPIO5 on that part, so the C6 datasheet and the C6 ESP-IDF page name the same five
pins. There is no discrepancy on C6 to generalise from. On C6 the extra pair straps SDIO sampling
and driving clock edge (C6 datasheet Section 3.2), a peripheral ESP32-H2 does not have.

**ESP32-H2 is the sole Espressif target where ESP-IDF lists more strapping pins than its own
datasheet.** What is systematic is the silicon, not the documentation: `GPIO_STRAP_REG` bits 0 and 1
map to the same architectural pair on C3, C6 and H2 alike, and on every one of them that pair
selects a single-interface download boot mode. C3 documents them. C6 documents them. H2 documents
them in its TRM boot table and denies them in its TRM prose and its datasheet.

The most likely explanation, and it is inference rather than fact: the H2 `.inc` file was ported from
the C3 file, where a five-pin framing was correct and documented, and it happens to describe the H2
silicon more honestly than the H2 datasheet does. That does not weaken the conclusion. The TRM boot
table and the register bit map are primary evidence and they stand on their own.

### 9. Community reports and Espressif's own position

**There is no ESP32-H2-specific issue or forum thread on this.** Searches across `espressif/esp-idf`,
`org:espressif` and all of GitHub found nothing asking about H2 GPIO2 and GPIO3. Nothing on
esp32.com, developer.espressif.com or the Chinese forum either, though that is a soft negative:
esp32.com puts its search behind a bot challenge, so forum coverage rests on external search rather
than a full sweep.

**But the identical question was asked and answered for ESP32-C6, and the answer is directly
applicable.** This is the single most valuable piece of corroboration in this document, because it is
Espressif staff addressing exactly our design decision, and because it comes with a real board
failure attached.

[espressif/esp-technical-reference-manual-latex#4](https://github.com/espressif/esp-technical-reference-manual-latex/issues/4),
"Mention GPIO2 as a strapping pin for ESP32-C6", opened 21 June 2026 by satyamedh, originally filed
against `espressif/esptool` and transferred, closed as completed 30 June 2026.

The reporter bricked custom C6-MINI-1 boards by putting a pull-up on GPIO2. His boot log:

> ```
> rst:0x1 (POWERON),boot:0x61 (SDIO_REI_FEO_V1_BOOT)
> wait sdio download
> ```

He found `GPIO_STRAP_REG (0x60091038) & 0xf == 1`, which is GPIO2 = 1, GPIO3 = 0, GPIO8 = 0,
GPIO9 = 0, and had to disassemble the ROM to work out why. **Note what that failure actually is: the
chip does not misboot, it hangs waiting for a download that never comes.** The application never
runs.

Espressif's reply, from **espwangning**, 23 June 2026, verbatim:

> Besides pulling up GPIO2, may I know if you have also pulled up GPIO8?
>
> According to the GPIO_STRAP_REG value, GPIO8 and GPIO9 are read 0. When they are 0, the chip may
> enter download boot mode via either USB, UART, or SDIO interface solely. When GPIO8 is 1 and
> GPIO9 is 0, the chip enters Joint Download Boot mode. In this, GPIO2 does not matter, and you can
> do with this pin whatever you want.

That last sentence is Espressif independently arriving at the recommendation in this document. Pull
GPIO8 high and the question disappears.

**espwangning**, 24 June 2026, on why the pins are undocumented, verbatim:

> GPIO2 is intentionally left undocumented. Here is the reasoning:
>
> - To accidentally enter the download boot mode via a single interface, you need to pull down both
>   GPIO8 and GPIO9, and pull up GPIO2 or GPIO3 up or down. This falls outside the valid boot mode
>   configurations documented in the datasheet and TRM.
> - The Joint Download Boot mode supports all three interfaces (USB, UART, and SDIO) simultaneously,
>   with the chip automatically selecting the active one. Most users never need to manually control
>   GPIO2 or GPIO3.
>
> Adding GPIO2 to the boot mode table would increase documentation complexity without benefit for
> the vast majority of users. Instead of documenting GPIO2, we would add this "invalid combination".

So the omission is a deliberate editorial policy, confirmed by the vendor, not an oversight. It will
not be corrected in a future datasheet revision. Design accordingly.

**How it was resolved:** commit
[`99d7062`](https://github.com/espressif/esp-technical-reference-manual-latex/commit/99d706290d42de21887320ad3f6e65b991a8e2d0),
30 June 2026, "ESP32-C6/BOOTCTRL: Add invalid strapping combination". It adds a single
`Invalid Combination | 0 | 0` row to the C6 boot mode table and still does not name GPIO2 or GPIO3.

**dobairoland** (Espressif), 22 June 2026, on where this belongs:

> Thank you for your report. I forwarded it to the documentation team. We won't keep it open here
> because we don't intend to document everything in the esptool documentation. We will leave this up
> to the TRMs.

Two weaker near-misses, both unanswered publicly:

- [espressif/esp-idf#18969](https://github.com/espressif/esp-idf/issues/18969), "Online wiki doc
  might be wrong for ESP32-S31 strapping pins (IDFGH-18133)", opened 12 August 2026. Same shape of
  complaint as ours, against ESP32-S31. No Espressif reply yet.
- [espressif/esp-idf#18074](https://github.com/espressif/esp-idf/issues/18074), "what is the
  definition of all esp32s3 GPIO_STRAPPING bits, especially bit1, bit0 (IDFGH-17032)", 3 January
  2026. Asks precisely the "what are the undocumented low strap bits" question. Closed as
  Resolution: Done **with no public comment**, so whatever answer was given is not on the record.

**Also confirmed: GPIO2 and GPIO3 were never added to the ESP-IDF docs. They were there from the
start.** The file was created by commit `4c8fdc31f98c5b21103947107bbfe7ee18dd60a5`, "gpio: Add
support for esp32h2", 18 January 2023. The blob at that SHA already reads "GPIO2, GPIO3, GPIO8,
GPIO9, and GPIO25 are strapping pins" (with "infomation" misspelt) and already marks both table rows
"Strapping pin". GitHub blame attributes line 138 to `719b75da9b51af56bd00911c7fbba9cf9a7ab791`
(19 April 2023), which is misleading: that commit's only change to the line was fixing the spelling
and swapping a hardcoded URL for the `{IDF_TARGET_DATASHEET_EN_URL}` macro. So there is no commit
message anywhere justifying the five-pin list, and it has stood unreviewed for three and a half
years. Treat it as corroborating the TRM, not as independent evidence.

### 10. Reset and after-reset pin states, from the primary table

ESP32-H2 Series Datasheet v1.3, Table 2-1 "Pin Overview" and Appendix A "ESP32-H2 Consolidated Pin
Overview". Note that Table 2-1 identifies pins 5 to 8 by **pad name**, not GPIO number, which is the
main reason this footnote is easy to misread. The mapping comes from datasheet Table 2-3 "IO MUX Pin
Functions": pin 5 MTMS = GPIO2, pin 6 MTDO = GPIO3, pin 7 MTCK = GPIO4, pin 8 MTDI = GPIO5.

Extracted with column coordinates from the PDF to remove any ambiguity about which of the two
columns each entry sits in. The "At Reset" column begins at x = 319.7 pt and "After Reset" at
x = 365.8 pt on page 13.

| Pin | Pad name | GPIO | At Reset | After Reset |
|---|---|---|---|---|
| 3 | GPIO0 | GPIO0 | (blank) | (blank) |
| 4 | GPIO1 | GPIO1 | (blank) | (blank) |
| 5 | MTMS | **GPIO2** | IE | IE |
| 6 | MTDO | **GPIO3** | IE | IE |
| 7 | MTCK | **GPIO4** | **(blank)** | **IE, footnote 4** |
| 8 | MTDI | **GPIO5** | **(blank)** | IE |
| 10 | GPIO8 | GPIO8 | IE | IE |
| 11 | GPIO9 | GPIO9 | IE, WPU | IE, WPU |
| 12 | GPIO10 | GPIO10 | (blank) | (blank) |
| 13 | GPIO11 | GPIO11 | (blank) | (blank) |
| 14 | GPIO12 | GPIO12 | (blank) | (blank) |
| 21 | GPIO22 | GPIO22 | (blank) | (blank) |
| 22 | U0RXD | GPIO23 | (blank) | IE, WPU |
| 23 | U0TXD | GPIO24 | (blank) | IE, WPU |
| 24 | GPIO25 | GPIO25 | IE | IE |
| 25 | GPIO26 | GPIO26 | (blank) | IE |
| 26 | GPIO27 | GPIO27 | (blank) | IE, USB_PU |

Appendix A on page 65 renders the same data with GPIO numbers visible and shows MTCK as `IE*` in a
single column, confirming that one of the two cells is empty. The footnote legend reads:

> \* For details, see Section 2 Pins. Regarding highlighted cells, see Section 2.3.3 Restrictions
> for GPIOs.

The abbreviations, from Table 2-1 footnote 3, verbatim:

> Column Pin Settings shows predefined settings at reset and after reset with the following
> abbreviations:
> - IE – input enabled
> - WPU – internal weak pull-up resistor enabled
> - WPD – internal weak pull-down resistor enabled
> - USB_PU – USB pull-up resistor enabled

And the footnote the question asked about, Table 2-1 footnote 4, verbatim and complete:

> 4. Depends on the value of EFUSE_DIS_PAD_JTAG
> - 0 - WPU is enabled
> - 1 - pin floating

That footnote marker sits at x = 375.4 pt, raised above the baseline, immediately following the
"IE" at x = 365.8 pt. It is attached to the **After Reset** cell of pin 7 (MTCK / GPIO4). Nothing
else on the page carries footnote 4.

**Independent confirmation from the TRM, which states it far less ambiguously.** TRM v1.1 Table
6.13-1 "IO MUX Functions List" (page 235) carries a "Reset" column, and the legend below it reads,
verbatim:

> "Reset" column shows the default configuration of each pin **after reset**:
> - 0 - IE = 0 (input disabled)
> - 1 - IE = 1 (input enabled)
> - 2 - IE = 1, WPD = 1 (input enabled, pull-down resistor enabled)
> - 3 - IE = 1, WPU = 1 (input enabled, pull-up resistor enabled)
> - 4 - OE = 1, WPU = 1 (output enabled, pull-up resistor enabled)

The reset codes for the pins that matter here:

| GPIO | Pin name | Reset code | Meaning |
|---|---|---|---|
| 0, 1, 10, 11, 12, 13, 14, 22 | various | 0 | input disabled, no pull |
| **2** | **MTMS** | **1** | input enabled, **no pull** |
| **3** | **MTDO** | **1** | input enabled, **no pull** |
| **4** | **MTCK** | **1\*** | **see footnote below** |
| **5** | **MTDI** | **1** | input enabled, **no pull** |
| 8 | GPIO8 | 1 | input enabled, no pull |
| 9 | GPIO9 | 3 | input enabled, **WPU** |
| 23 | U0RXD | 3 | input enabled, WPU |
| 24 | U0TXD | 4 | output enabled, WPU |
| 25 | GPIO25 | 1 | input enabled, no pull |

And the `1*` footnote, page 236, verbatim:

> 1* - If EFUSE_DIS_PAD_JTAG = 1, the pin MTCK is left floating after reset, i.e., IE = 1. If
> EFUSE_DIS_PAD_JTAG = 0, the pin MTCK is connected to internal pull-up resistor, i.e., IE = 1,
> WPU = 1.

This settles both halves of question 2a beyond argument. The TRM says "**after reset**" in plain
words, twice, which removes any dependence on my column-coordinate reconstruction of the datasheet
table. And it assigns plain code `1` to MTMS, MTDO and MTDI while reserving `1*` for MTCK alone,
which confirms that **GPIO2, GPIO3 and GPIO5 have no pull at any point** despite being JTAG pads.
Only the JTAG clock gets one.

Note also that GPIO0, GPIO1, GPIO10, GPIO11, GPIO12 and GPIO22 all carry reset code `0`, input
disabled and no pull. That is an even cleaner state than the datasheet's blank cells implied, and it
is why those pins are the right home for an actuator line.

**Answers to question 2a.** The footnote applies to GPIO4 alone. GPIO2 (MTMS) and GPIO3 (MTDO) have
IE in both columns and no pull in either. GPIO5 (MTDI) has IE after reset only, no pull. No other
pin in Table 2-1 references footnote 4. Being a JTAG pad is not what triggers the pull-up. Only MTCK
gets it, which is consistent with MTCK being the JTAG **clock** input, where a floating input is the
one case that genuinely misbehaves.

**Answers to question 2b.** Resistance, from datasheet Table 5-3 "DC Characteristics (3.3 V,
25 °C)", verbatim:

> R_PU  Internal weak pull-up resistor  —  45  —  kΩ

The datasheet gives a typical of 45 kΩ with **no minimum and no maximum**. Design for a worst case
well below 45 kΩ. Espressif quotes wide tolerances on this structure across its other parts, so
treat 20 kΩ as a defensive lower bound rather than trusting 45 kΩ.

Duration: it is active from reset release until firmware writes the pad configuration. It is *not*
active during reset, since the At Reset cell is empty. Working backwards from the datasheet timing:
CHIP_EN goes high after t_STBL ≥ 50 µs (Table 2-9), strapping hold is t_H ≥ 3 ms (Table 3-2), and
then ROM bootloader, second stage bootloader and app init run before any application code touches
the pad. On a typical ESP32-H2 with in-package flash that is on the order of 200 to 400 ms.

**This is the important correction.** A pull-up confined to the reset assertion window would be a
minor concern. A pull-up that persists for several hundred milliseconds after reset release, on a
line that may be gating a relay, is a real one.

### 11. Assessing the GPIO4 hazard on a relay drive line (question 2c)

Take the pull-up as R_PU to 3.3 V with the ESP32-H2 pad otherwise high-impedance.

**Into a logic-level N-channel MOSFET gate with no external pull-down.** The gate is capacitive with
essentially no DC path. 45 kΩ charges it to the full 3.3 V within a few microseconds. A logic-level
FET with V_GS(th) between 1 V and 2 V turns **hard on**. The relay energises and stays energised for
the entire few hundred milliseconds until firmware drives the pin low. This is a definite, not
marginal, failure. In a mains appliance that is a contact closure the user did not ask for on every
power-up and every reset.

**Into a bipolar NPN base, common configuration with a series base resistor.** V_BE clamps at
roughly 0.7 V, so available base current is about (3.3 − 0.7) / 45 kΩ ≈ 58 µA, less the drop across
the base resistor, which is negligible at that current. With h_FE of 100 that supports about 5.8 mA
of collector current; with h_FE of 300, about 17 mA. A typical small signal relay coil wants 30 to
80 mA to pull in, so a low-gain part is probably safe. A high-gain part driving a sensitive
low-power relay is not. Worse, using the 20 kΩ defensive bound instead of 45 kΩ gives 130 µA of base
drive and up to 39 mA of collector current, which will pull in many relays.

Conclusion: **marginal at best on a BJT, a guaranteed fault on a MOSFET.** Do not rely on the
driver topology to save it.

**External pull-down sizing.** Requirement is to hold the node below the driver's turn-on threshold
against R_PU. With a pull-down R_PD the node sits at 3.3 × R_PD / (R_PU + R_PD).

| R_PD | Node voltage at R_PU = 45 kΩ | Node voltage at R_PU = 20 kΩ (defensive) |
|---|---|---|
| 10 kΩ | 600 mV | 1.10 V |
| 4.7 kΩ | 313 mV | 628 mV |
| **2.2 kΩ** | **154 mV** | **327 mV** |
| 1.0 kΩ | 72 mV | 157 mV |

**Use 2.2 kΩ.** At the defensive 20 kΩ bound it holds 327 mV, comfortably below a BJT's ~0.6 V
conduction knee and far below any MOSFET V_GS(th). 10 kΩ is not enough: at the defensive bound it
reaches 1.10 V, which will partially turn on a logic-level FET.

**Loading in normal operation.** A 2.2 kΩ pull-down draws 1.5 mA when the pin drives high. Against
I_OH of 40 mA typical (datasheet Table 5-3) that is negligible, and V_OH ≥ 0.8 × VDD is unaffected.

**But note what it costs.** GPIO4 is ADC1_CH3. A 2.2 kΩ resistor to ground is a hard load on any
analogue source and destroys the pin's usefulness as an ADC input for anything but a low-impedance
source. If GPIO4 is meant to be both a relay line and an analogue input, that is already a design
conflict independent of this footnote.

**The better answer is to not use GPIO4 for the actuator line.** From the Table 2-1 extract above,
GPIO0, GPIO1, GPIO10, GPIO11, GPIO12 and GPIO22 all have both reset columns blank: no input enable,
no pull, high impedance throughout reset and after it. Any of those is a strictly better home for a
signal that must be guaranteed low at power-up. GPIO1 additionally keeps ADC1_CH0.

Also worth stating, since the question mentioned it: the ESP8285 being replaced has no JTAG pads at
all, so there was never an equivalent behaviour to inherit. This is a new hazard introduced by the
part change, and any carrier design carried over unchanged from the ESP8285 will not have accounted
for it.

### 12. Burning EFUSE_DIS_PAD_JTAG (question 2d)

**Does it eliminate the pull-up?** Yes. Datasheet Table 2-1 footnote 4: `1 - pin floating`. GPIO4
becomes high impedance after reset like the other pins.

**Is it reversible?** No. Datasheet Section 3, verbatim:

> The default values of all the above eFuse bits are 0, which means that they are not burnt. Given
> that eFuse is one-time programmable, once an eFuse bit is programmed to 1, it can never be
> reverted to 0.

Confirmed as requested. It is permanent and per-chip.

**What else does it disable?** It permanently removes PAD_to_JTAG, meaning external JTAG probe
access via MTDI, MTCK, MTMS and MTDO. From the ESP-IDF eFuse table,
`components/efuse/esp32h2/esp_efuse_table.csv:121`:

> DIS_PAD_JTAG, EFUSE_BLK0, 51, 1, [] Represents whether JTAG is disabled in the hard way
> (permanently). 1: disabled. 0: enabled

TRM v1.1 Section 1.10.4 gives the full truth table. The relevant row, with `Temporary disable
JTAG` = 0, `EFUSE_DIS_USB_JTAG` = 0, `EFUSE_DIS_USB_SERIAL_JTAG` = 0, `EFUSE_DIS_PAD_JTAG` = **1**:

> USB_to_JTAG Status: Available. PAD_to_JTAG Status: Unavailable.

**So USB_to_JTAG survives.** Burning `DIS_PAD_JTAG` alone does not disable the USB Serial/JTAG
controller.

**Does it interfere with USB Serial/JTAG flashing?** No, on two counts. Flashing goes over the
CDC-ACM serial endpoint of the USB Serial/JTAG controller, which is a different function from JTAG
debug entirely, and USB_to_JTAG stays available regardless per the table above.

**Does it interfere with ESP-IDF debugging?** Only external-probe debugging. `idf.py openocd` and
`idf.py gdb` over the built-in USB JTAG continue to work. A JTAG probe wired to GPIO2, GPIO3, GPIO4
and GPIO5 stops working permanently. For our design that is a smaller loss than it sounds, since
those four pins are already allocated to host-facing lines and could not carry a probe anyway.

**Does it interfere with Matter attestation storage?** No. `DIS_PAD_JTAG` is a single bit at BLOCK0
offset 51. Matter DAC, PAI and CD material lives in a flash partition, either NVS or the
`esp_secure_cert` partition, and any associated key protection uses eFuse key blocks BLOCK4 to
BLOCK10. `DIS_PAD_JTAG` touches none of those.

One adjacent gotcha worth recording. Its write-protection bit is shared. From
`components/efuse/esp32h2/esp_efuse_table.csv:23`:

> WR_DIS.DIS_PAD_JTAG, EFUSE_BLK0, 2, 1, [] wr_dis of DIS_PAD_JTAG

TRM Table 5.3-1 shows WR_DIS bit 2 also write-protects `DIS_ICACHE`, `DIS_USB_JTAG`,
`POWERGLITCH_EN`, `DIS_FORCE_DOWNLOAD`, `DIS_TWAI`, `JTAG_SEL_ENABLE` and
`DIS_DOWNLOAD_MANUAL_ENCRYPT`. Burning `DIS_PAD_JTAG` does not itself set WR_DIS bit 2, but anything
in our provisioning flow that does will freeze that whole group at once.

**Recommendation on automatic burning: no. The flashing tool must never burn `DIS_PAD_JTAG`
automatically.** Reasons, in order of weight:

1. It is irreversible and per-chip. A tool bug that burns it on the wrong batch cannot be undone,
   and the parts are scrap for any purpose that later needs pad JTAG.
2. It does not solve the problem it appears to solve. A 2.2 kΩ pull-down costs a fraction of a cent,
   works on every part regardless of eFuse state, and protects units already built and units flashed
   by anyone else. Choosing a permanent silicon change over a resistor to fix a board-level issue is
   the wrong layer.
3. It removes a diagnostic avenue for field returns at exactly the moment it is most valuable.
4. It is a security decision, not a manufacturing step. If we want JTAG locked down for production,
   that belongs in a deliberate hardening package considered as a whole (`DIS_PAD_JTAG`,
   `SOFT_DIS_JTAG`, `DIS_DOWNLOAD_MODE`, secure boot v2, flash encryption), applied at a defined
   point in the production flow with explicit operator confirmation, and validated against Matter
   certification requirements before it ships.

If the tool supports it at all, it should be behind an explicit flag, off by default, with a
confirmation prompt naming the chip's MAC.

**`EFUSE_DIS_USB_JTAG` belongs on the same never-burn list, and for a sharper reason.** Datasheet
Table 3-6 "JTAG Signal Source Control", row 4: with `EFUSE_DIS_PAD_JTAG` = 0 and
`EFUSE_DIS_USB_JTAG` = 1, the JTAG signal source is the **JTAG pins**, and both
`EFUSE_STRAP_JTAG_SEL_ENABLE` and GPIO25 are marked "Ignored". TRM v1.1 Section 1.10.4 gives the
same result (`0 1 0 0 x x` → USB_to_JTAG Unavailable, PAD_to_JTAG Available).

The consequence for this design is specific and bad. Burning `DIS_USB_JTAG` alone permanently
commandeers GPIO2, GPIO3, GPIO4 and GPIO5 as MTMS, MTDO, MTCK and MTDI, which is **four of our five
host-facing lines**. No external pull defeats it, because GPIO25 is explicitly ignored in that row.
MTDO (GPIO3) becomes a chip output, so a host driving that net puts two output stages in direct
contention. That is a hardware-damage path, not just a functional one.

Note the asymmetry: burning `DIS_PAD_JTAG` frees the pads and is merely wasteful, while burning
`DIS_USB_JTAG` seizes them and is destructive. If either is ever burnt deliberately, it must be
`DIS_PAD_JTAG`, and if the goal is to disable debug access entirely then **both** must be burnt
together (Table 3-6 row 6: `1`/`1` → "JTAG is disabled"), never `DIS_USB_JTAG` on its own.

---

## Recommendation

### Keep the pin mapping. Add one resistor.

GPIO1, GPIO2, GPIO3, GPIO4 and GPIO5 remain the five host-facing lines. All five keep ADC1. Nothing
is lost.

**Required change: 10 kΩ pull-up from GPIO8 to 3V3 on the carrier.**

This is the whole safety argument, restated to survive the GPIO2 and GPIO3 finding:

| GPIO9 at reset | GPIO8 at reset | Result | Does GPIO2 or GPIO3 matter? |
|---|---|---|---|
| 1 (internal WPU, normal case) | anything | SPI Boot | No. Table 8.2-2 row 1 marks both `x` |
| 0 (download strap asserted) | 1 (our new pull-up) | Joint Download Boot | No. Table 8.2-2 row 2 marks both `x` |
| 0 | 0 (current design, GPIO8 floating) | depends on GPIO2 and GPIO3 | **Yes.** This is the case the pull-up removes |

With GPIO8 pulled high, every reachable state falls in row 1 or row 2, and both rows ignore GPIO2
and GPIO3 by the TRM's own notation. The host can hold those two lines high, hold them low, or drive
them from a push-pull output stage at power-up, and boot behaviour is unaffected.

Without the pull-up, GPIO8 is floating (datasheet Table 3-1, "GPIO8 — Floating"). A floating input
at reset is indeterminate. If it reads 0 while the download strap is asserted, GPIO2 selects between
SPI Download Boot and the row the TRM calls "Invalid Combination". Note also that
GPIO9 = 0, GPIO8 = 0, GPIO3 = 1, GPIO2 = 1 appears in **no** row of Table 8.2-2, which puts it under
the Section 8.2 warning about undocumented patterns.

Pulling GPIO8 high is good practice on this part independently of any of this. It is what makes the
download strap deterministic.

**Side effects of pulling GPIO8 high: none in our configuration.** GPIO8 has a second job, ROM
message printing. Datasheet Table 3-4 "UART0 ROM Message Printing Control" shows that with
`LP_AON_STORE4_REG[0]` = 0 and `EFUSE_UART_PRINT_CONTROL` = 0, both of which are the factory
defaults, the GPIO8 column reads "Ignored" and printing is Enabled. So a hard pull-up on GPIO8
changes nothing about console output unless we later burn `EFUSE_UART_PRINT_CONTROL`, at which
point GPIO8 = 1 would select "Print is disabled during boot" under setting 0b01. Worth knowing
before anyone burns that eFuse, and another reason the flashing tool should not burn eFuses on its
own initiative.

### Move the relay drive line off GPIO4.

Preferred: put it on **GPIO0, GPIO1, GPIO10, GPIO11, GPIO12 or GPIO22**. All six are high impedance
with no pull both at reset and after reset per datasheet Table 2-1 and Appendix A.

**What is lost.** GPIO0 has no ADC. GPIO10 and GPIO11 have no ADC1 but do carry ZCD0 and ZCD1, the
analogue comparator inputs, which is arguably useful for a mains product doing zero-cross detection.
GPIO12, GPIO13 and GPIO14 sit on the VDDA_PMU/VBAT domain rather than VDDPST1, which is a separate
consideration if VBAT is ever fed from something other than 3V3. GPIO22 is plain digital with no
analogue function.

If moving the relay line to GPIO0 or GPIO22, the arithmetic is: one of the five host lines gives up
ADC1 capability, and GPIO4 becomes free for an input-only or analogue-only role where a 45 kΩ
pull-up after reset is harmless.

Fallback if GPIO4 must stay: **2.2 kΩ pull-down to ground**, accepting that it makes GPIO4
unsuitable as an ADC input for any source with meaningful output impedance.

### Do not burn any eFuse to fix either problem.

Both issues are solved with two resistors. Neither warrants a permanent, irreversible change to the
silicon.

Never-burn list for the flashing tool, in priority order:

1. **`EFUSE_DIS_USB_JTAG`** — burning this alone permanently seizes GPIO2, GPIO3, GPIO4 and GPIO5 as
   JTAG pads regardless of GPIO25, putting a chip output onto host-driven nets. Hardware damage
   path. See Section 12.
2. **`EFUSE_DIS_PAD_JTAG`** — irreversible, removes field diagnostics, and does not solve the
   pull-up problem that a 2.2 kΩ resistor solves better.
3. **`EFUSE_UART_PRINT_CONTROL`** — interacts with the new GPIO8 pull-up to disable boot console
   output. See Section 6.

If debug lockdown is ever wanted for production, `DIS_PAD_JTAG` and `DIS_USB_JTAG` must be burnt
**together** (datasheet Table 3-6 row 6, "JTAG is disabled"), as a deliberate hardening step with
operator confirmation, never singly and never automatically.

---

## What is not resolved from documentation

Stated plainly rather than guessed at.

**The minimum value of R_PU.** The datasheet gives a typical of 45 kΩ with no min or max
(Table 5-3). The 20 kΩ defensive bound used in the pull-down sizing above is my assumption, not an
Espressif figure. The 2.2 kΩ recommendation has enough margin that this does not change the answer,
but the number itself is not sourced.

**Bench test that would settle it:** on a bare ESP32-H2-MINI-1 with no firmware driving GPIO4,
release reset and measure the pad's open-circuit voltage and its voltage with a known 10 kΩ to
ground. Two measurements give R_PU directly. Repeat across the temperature range if the margin ever
looks tight.

**Exact duration of the after-reset window on our firmware.** The 200 to 400 ms figure is inferred
from typical ESP32-H2 boot times, not measured on our build.

**Bench test:** scope GPIO4 against CHIP_EN from power-up, with the app's GPIO init instrumented.
The interval between reset release and the pad going low is the exposure. Worth doing regardless,
since it also bounds how long any relay line floats before firmware takes control, which is a
question the design should have a measured answer to.

**Whether a floating GPIO8 actually reads 0 or 1 in our layout.** Indeterminate by definition, which
is the point. The 10 kΩ pull-up removes the question rather than answering it. No test needed.
