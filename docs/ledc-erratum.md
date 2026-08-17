# LEDC-253 on ESP32-H2: resolved

Scope: does erratum LEDC-253 constrain the PWM design of the TYWE2L-to-Thread module, which uses an
ESP32-H2-MINI-1-H4S and surfaces five host GPIOs (GPIO10, GPIO11, GPIO12, GPIO13, GPIO14), given
that the dominant real-world workload is dimmable PWM lighting under Matter.

This document resolves the conflict flagged in `docs/pin-mapping-v2.md` section 3.3 and repeated in
its open-questions list at line 1072.

---

## 1. Verdict

**LEDC-253 does not bite on rev v1.2 silicon. The errata is current, and the earlier reading of the
ESP-IDF documentation was taken from a stale documentation version.** The v5.5 documentation, which
is the version esp-matter pins, carries the revision qualifier explicitly.

Answering the five decision questions directly:

- **Does the erratum bite?** No. LEDC-253 affects chip revisions v0.0 and v0.1 only, and is stated
  "Fixed in chip revision v1.2" (errata v1.3, section 3.7, page 10). ESP-IDF v5.5.4 agrees, in the
  driver source and in the documentation source.
- **GPIOs, channels, timers, frequencies?** None of them. Even on affected silicon the erratum
  restricts exactly one thing: the single duty value `2**duty_resolution` when the bound timer is
  configured at the part's maximum duty resolution, which on ESP32-H2 means a 20-bit timer. It has
  no pin dependency, no channel dependency, no timer dependency and no frequency dependency.
- **Fade and gamma-curve operation?** Unaffected by LEDC-253. ESP32-H2 supports hardware gamma fade
  with up to 16 linear segments, which is exactly what Matter smooth dimming wants.
- **Duty of exactly 0 or 100 percent?** Duty 0 is untouched by any erratum. Duty 100 percent is
  reachable on v1.2 silicon. There is one unrelated driver behaviour worth knowing about at the top
  endpoint, described in section 6.3: a step fade that starts from exactly max duty is nudged down
  by one LSB by the driver, on every target, to avoid a counter overflow. At a realistic 10 to
  14-bit resolution that is between 0.006 and 0.1 percent of full scale and is not visible.

**Practical rule for the firmware, which costs nothing:** configure lighting timers at 10 to 14-bit
duty resolution, never at `SOC_LEDC_TIMER_BIT_WIDTH` (20). That keeps the design correct even if a
v0.1 part is ever second-sourced. Verify incoming modules carry the `MF XXXX` specification
identifier, which is the v1.2 module marking (errata v1.3, section 1, Table 3).

**No consequence for the PCB.** LEDC on ESP32-H2 routes purely through the GPIO matrix, so pin
choice is unconstrained by this erratum or by LEDC generally. See section 8.

The one genuine LEDC-shaped constraint on this design has nothing to do with errata: the part has
**six channels but only four timers** (`soc_caps.h:306-307` at v5.5.4). Five host pins can all be
independent PWM channels, but they can carry at most four distinct frequencies. For lighting, where
every channel normally runs the same frequency, this never binds.

---

## 2. Sources used

| Ref | Document | Version and access |
| --- | --- | --- |
| E | *ESP32-H2 Series SoC Errata* | v1.3, dated 2026-06-09 (Revision History, page 15). PDF `esp-chip-errata-en-master-esp32h2.pdf` from `https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32h2/`, 19 pages, fetched 13 August 2026. HTML mirror at the same base URL. |
| S | ESP-IDF source | Tag `v5.5.4` on `github.com/espressif/esp-idf`, which is the version esp-matter pins. Cross-checked against `release/v5.5` head `1a1a5aa6513592623d6b841ab8c9f3ef61eb9dfd` and `master` head `08e0d30a`. |
| D | ESP-IDF rendered documentation | `https://docs.espressif.com/projects/esp-idf/en/<ver>/esp32h2/api-reference/peripherals/ledc.html` for `latest`, `v5.5`, `v5.4` and `v5.3`. |
| T | *ESP32-H2 Technical Reference Manual* | Version 1.1, Chapter 35 "LED PWM Controller (LEDC)". |

Where a printed page number is given for the errata, it is the printed page. The PDF page index is
two higher, because of the cover and contents.

---

## 3. Question 1: what LEDC-253 is, verbatim

From [E], section 3.7, printed page 10, reproduced in full:

> **3.7 [LEDC-253] Unable to Reach 100% Duty Cycle at Maximum Duty Resolution**
>
> Affected revisions: v0.0 v0.1
>
> **Description**
>
> When the timer selects the maximum duty resolution, in such case, 100% duty cycle is not
> achievable. Setting duty to (2^MAX_DUTY_RES) will break the internal duty calculation.
>
> **Workarounds**
>
> No workaround.
>
> **Solution**
>
> Fixed in chip revision v1.2.

(The superscript in "2^MAX_DUTY_RES" renders as "2MAX_DUTY_RES" in extracted text. The intended
meaning, confirmed by the ESP-IDF wording quoted in section 5, is 2 raised to the power of the
maximum duty resolution.)

**Failure mode**: the duty value that means a permanently high output cannot be written. The
internal duty calculation breaks, so instead of full brightness the channel produces something
wrong. In the closest analogous case, an overflow that wraps to near-zero output, which for a light
means full off instead of full on.

**Trigger**: two conditions together. The timer must be configured at the part's maximum duty
resolution, which is 20 bits on ESP32-H2 (`SOC_LEDC_TIMER_BIT_WIDTH`, `soc_caps.h:308` at v5.5.4).
And the channel duty must be set to `2**duty_resolution`, that is 1048576. Any lower resolution, or
any duty below the maximum, is unaffected.

**Stated workaround**: "No workaround." Note that this is Espressif's wording for the hardware. In
practice the constraint is trivially avoided by not configuring the timer at 20-bit resolution,
which no lighting application would do anyway. The ESP-IDF example code says this out loud
(`examples/peripherals/ledc/ledc_basic/main/ledc_basic_example_main.c`, header comment, as amended
by commit `19fec9f455`).

---

## 4. Question 2: affected revisions

From [E], section 2, Table 2.1 "Errata summary", printed page 6. The table has three revision
columns, v0.0, v0.1 and v1.2, and marks affected cells with Y. The LEDC row reads:

> LEDC | LEDC-253 | [LEDC-253] Unable to Reach 100% Duty Cycle at Maximum Duty Resolution | Y | Y |

Two Y marks, under v0.0 and v0.1. **The v1.2 column is blank for LEDC-253.** The per-erratum entry
at section 3.7 states the same twice over, once as "Affected revisions: v0.0 v0.1" and once as
"Solution: Fixed in chip revision v1.2".

**v1.2 is confirmed fixed.** For contrast, the same table marks PCNT-249 with three Y values,
including v1.2, so the table does distinguish, and a blank cell is meaningful rather than an
oversight.

On the "current production parts are v1.2" point, the errata does not say so in as many words, but
[E] section 1, Table 3 "Chip Revision Identification by Module Marking" gives the specification
identifiers as: v0.0 = "—", v0.1 = "MB XXXX", v1.2 = "MF XXXX", with the footnote:

> Missing specification identifier "—" means modules with this chip revision are not mass produced.

So v0.0 modules were never mass produced, and incoming goods can be sorted by marking. That is the
check to put in the production specification. In software, `EFUSE_RD_MAC_SYS_3_REG[22:18]` encodes
the revision ([E], section 1, Table 1), which is what `efuse_hal_chip_revision()` reads.

Document currency: [E] is at v1.3, dated 2026-06-09, and its own revision history shows chip
revision v1.2 information was added in errata v1.0 dated 2025-03-06. The most recent edits, in v1.2
and v1.3 of the document, were to the ECDSA_DS entries. The LEDC entry has not been revisited since
the v1.2 fix was documented, which is what you would expect of a closed item.

---

## 5. Question 3: what ESP-IDF documentation says, and why the conflict appeared

The relevant text lives in `docs/en/api-reference/peripherals/ledc.rst`, in the "Change PWM Duty
Cycle Using Software" area. At **tag v5.5.4**, lines 272 to 285:

```rst
.. only:: esp32 or esp32s2 or esp32s3 or esp32c3 or esp32c2 or esp32c6 or esp32h2 or esp32p4

    .. warning::

        On {IDF_TARGET_NAME}, when channel's binded timer selects its maximum duty resolution, the
        duty cycle value cannot be set to ``(2 ** duty_resolution)``. Otherwise, the internal duty
        counter in the hardware will overflow and be messed up.

    .. only:: esp32h2

        The hardware limitation above only applies to chip revision before v1.2.

    .. only:: esp32p4

        The hardware limitation above only applies to chip revision before v3.0.
```

Line 276 is the warning. **Line 280 is the revision qualifier, and it is present.**

The documentation is versioned per release and per target, and that is precisely what explains the
apparent conflict. The qualifier was added by commit **`19fec9f455`, "fix(ledc): updated docs for
esp32h2 eco5 bugfix", dated 2024-12-20**. ESP32-H2 ECO5 is chip revision v1.2. Presence of the
sentence by version:

| ESP-IDF version | Qualifier in `ledc.rst`? |
| --- | --- |
| v5.3.3 | No |
| v5.4 | No |
| v5.4.1 | No |
| v5.4.2 | Yes (backported) |
| v5.5, v5.5.1, v5.5.4 | Yes |
| `release/v5.5` head, `master` head | Yes |

Confirmed on the rendered pages too, [D], target esp32h2. The `latest` and `v5.5` pages both end the
paragraph with "The hardware limitation above only applies to chip revision before v1.2." The `v5.4`
and `v5.3` pages stop after "will overflow and be messed up."

**So the ESP-IDF documentation was stale, was fixed in December 2024, and the fix is in the version
we are pinned to.** The earlier finding of an "unconditional" restriction came from reading a v5.3
or v5.4 era page, or from stopping at the end of the yellow warning box. The qualifier sits
immediately after the warning as ordinary body text rather than inside it, which makes it easy to
miss when skimming the rendered page or when grepping only for the warning string.

One caveat on the docs: the qualifier applies to the ESP32-H2 target build only. The generic warning
text still reads unconditionally for the other affected targets, several of which have no fix.

---

## 6. Question 4: what the ESP-IDF source actually does

This is the decisive evidence, and it is unambiguous. All line numbers are at tag **v5.5.4**.

### 6.1 There is no workaround code to gate, on any target

Searching `components/esp_driver_ledc/`, `components/hal/esp32h2/`, `components/hal/ledc_hal.c` and
`components/soc/esp32h2/` for `efuse_hal_chip_revision`, `ESP_CHIP_REV`, `ESP_CHIP_REV_ABOVE` or any
`#if` keyed to silicon revision returns **zero hits related to LEDC**. The only hit anywhere nearby
is `components/hal/esp32h2/efuse_hal.c:84`, which is unrelated to LEDC.

The reason there is nothing to gate is that the driver never restricted the duty range in the first
place. It permits the full inclusive range `[0, 2**duty_res]` and always has. The argument checks
are `<=`, not `<`:

- `components/esp_driver_ledc/src/ledc.c:1444` and `:1460`, `ledc_set_fade_with_step` and
  `ledc_set_fade_with_time`: `LEDC_ARG_CHECK(target_duty <= ledc_get_max_duty(speed_mode, channel), "target_duty");`
- `ledc.c:1620` and `:1638`, the "and start" variants: the same check.
- `ledc.c:1650-1651`, `ledc_set_multi_fade`: `uint32_t max_duty = ledc_get_max_duty(...); LEDC_ARG_CHECK(start_duty <= max_duty, "start_duty");`

And `max_duty` is `2**duty_res` exactly, not `2**duty_res - 1`:

- `components/hal/esp32h2/include/hal/ledc_ll.h:267-270`
  ```c
  static inline void ledc_ll_get_max_duty(ledc_dev_t *hw, ledc_mode_t speed_mode, ledc_timer_t timer_sel, uint32_t *max_duty)
  {
      *max_duty = (1 << (LEDC.timer_group[speed_mode].timer[timer_sel].conf.duty_res));
  }
  ```

So the "workaround" for LEDC-253 was only ever advice to the application, never driver logic. There
is no unconditional clamp, no revision-gated clamp, and no `SOC_LEDC_*` capability macro covering
this behaviour. The full LEDC capability block for ESP32-H2 is `soc_caps.h:304-313` and contains no
such macro.

### 6.2 The driver comment is revision-qualified

`components/esp_driver_ledc/src/ledc.c:877-880`, inside `ledc_channel_config()`:

```c
/*   Note: On ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2 (rev < 1.2), ESP32P4 (rev < 3.0), due to a hardware bug,
 *         100% duty cycle (i.e. 2**duty_res) is not reachable when the binded timer selects the maximum duty
 *         resolution. For example, the max duty resolution on ESP32C3 is 14-bit width, then set duty to (2**14)
 *         will mess up the duty calculation in hardware.
*/
```

Same text at `master` head, `ledc.c:917`. The `(rev < 1.2)` on ESP32H2 came from the same commit
`19fec9f455` that fixed the documentation, whose diff reads:

```diff
-    /*   Note: On ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2, ESP32P4, due to a hardware bug,
+    /*   Note: On ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2 (rev < 1.2), ESP32P4, due to a hardware bug,
```

The `(rev < 3.0)` on ESP32P4 was added later by `fbdb9413de`, "feat(ledc): ESP32P4 ECO5 LEDC related
updates", 2025-09-04. Espressif is maintaining this note actively and per silicon revision.

The LEDC test suite, `components/esp_driver_ledc/test_apps/ledc/main/test_ledc.c` (683 lines at
v5.5), contains no revision-gated cases and no full-duty-at-max-resolution case at all, so it neither
supports nor contradicts the fix.

### 6.3 One unconditional behaviour at the top endpoint, unrelated to LEDC-253

`ledc.c:1335-1344`, in `_ledc_set_fade_with_step()`:

```c
uint32_t duty_cur = 0;
ledc_hal_get_duty(&(p_ledc_obj[speed_mode]->ledc_hal), channel, &duty_cur);
// When duty == max_duty, meanwhile, if scale == 1 and fade_down == 1, counter would overflow.
if (duty_cur == ledc_get_max_duty(speed_mode, channel)) {
    assert(duty_cur > 0);
    duty_cur -= 1;
}
```

This is unconditional, applies on every target and every revision, and is a different hardware
quirk: a hardware step fade downward from exactly max duty with a scale of 1 would overflow the fade
counter. The driver avoids it by treating the starting point as one LSB below maximum. Consequences
for a Matter light:

- Setting a static 100 percent duty is unaffected. This code path is only entered when starting a
  hardware fade.
- A fade that starts from a currently-full-on channel begins from `max_duty - 1`. At 13-bit
  resolution that is one part in 8192, or 0.012 percent. Not visible.
- The multi-fade (gamma) API carries a related caution in the header,
  `components/esp_driver_ledc/include/driver/ledc.h:692` and `:715`: "This function does not prohibit
  from duty overflow. User should take care of this by themselves. If duty overflow happens, the PWM
  signal will suddenly change from 100% duty cycle to 0%, or the other way around." That is a
  parameter-construction hazard when hand-building `ledc_fade_param_config_t` lists, and it is
  avoided by using `ledc_fill_multi_fade_param_list()` to build them.

### 6.4 Corroboration from the TRM

[T] version 1.1, Chapter 35, Register 35.10 `LEDC_CHn_DUTY_REG` (printed page 1050): the register
field `LEDC_DUTY_CHn` occupies bits [24:0]. The integer part used by the comparator is
`LEDC_DUTY_CHn[24:4]`, which is 21 bits wide, and `[3:0]` is the dither fraction (printed page
1048). The driver writes `duty_val << 4` (`ledc_ll.h:328`). With a 20-bit maximum duty resolution, a
duty of `2**20` needs 21 bits of integer part, which is exactly what the field provides. The
register layout on this part has room for the 100 percent value, which is consistent with the v1.2
fix being real. This is corroboration, not proof, since the erratum describes a break in the
internal duty calculation rather than a register width limit.

### 6.5 Reading of the source evidence

The source does **not** contradict the errata, and does **not** contradict the current
documentation. All three now agree:

- Errata [E] v1.3: affects v0.0 and v0.1, fixed in v1.2.
- ESP-IDF v5.5.4 documentation: same, stated explicitly for esp32h2.
- ESP-IDF v5.5.4 source: full duty range permitted, no clamp, comment says `ESP32H2 (rev < 1.2)`.

The stale artefact was the ESP-IDF v5.3 and v5.4 documentation, superseded in December 2024. If any
future conflict of this shape appears, the ranking to apply is: driver source first, then the
errata, then the versioned documentation, and always check which documentation version is being
read before concluding anything.

---

## 7. Questions 5 and 6: impact on this design, and other LEDC errata

### 7.1 Impact, itemised

| Question | Answer for rev v1.2 with ESP-IDF v5.5.x |
| --- | --- |
| Does LEDC-253 bite at all? | No. |
| Restricts which GPIOs? | None. The erratum has no pin dimension. |
| Restricts which channels? | None. It is a timer-configuration and duty-value condition. All 6 channels behave alike. |
| Restricts which timers? | None. Any of the 4 timers, at any resolution below 20 bits, is unaffected even on v0.1 silicon. |
| Restricts which frequencies? | No, indirectly at most. Duty resolution and frequency trade off against the source clock, so avoiding 20-bit resolution puts a floor on achievable frequency far below anything relevant. A 20-bit timer off the 32 MHz XTAL would run at about 30 Hz, which is unusable for lighting regardless. |
| Restricts duty resolution? | Only the single maximum value, 20 bits, and only on v0.0 and v0.1. Lighting uses 10 to 14 bits. |
| Affects fade or gamma? | Not from LEDC-253. ESP32-H2 has hardware gamma fade, `SOC_LEDC_GAMMA_CURVE_FADE_SUPPORTED (1)` with `SOC_LEDC_GAMMA_CURVE_FADE_RANGE_MAX (16)` (`soc_caps.h:310-311`), which suits Matter `MoveToLevel` transitions. See 6.3 for the separate one-LSB fade-start behaviour. |
| Duty exactly 0? | No erratum touches it. Duty 0 gives a constant low output. |
| Duty exactly 100 percent? | Reachable on v1.2. Set duty to `2**duty_resolution`; the driver accepts it (`ledc.c:1444` and related). At 13-bit resolution that is 8192. |

Recommended firmware settings for the lighting path, all comfortably clear of the erratum:

- Duty resolution 12 or 13 bits, which gives 4096 or 8192 steps, ample for Matter's 254-step level
  control after gamma correction.
- Timer clock from the 32 MHz XTAL or the 96 MHz PLL divider, both available
  (`soc_caps.h:304-305`).
- PWM frequency in the usual 1 kHz to 20 kHz band for LED drivers, which at 13 bits is achievable
  from either source.
- Use `ledc_find_suitable_duty_resolution()` if the frequency is configurable at runtime. Note it
  can return up to `SOC_LEDC_TIMER_BIT_WIDTH`, so clamp its result to 14 bits if the belt-and-braces
  rule against 20-bit operation is to hold for a second-sourced v0.1 part.

### 7.2 Other LEDC errata

None. The full errata list [E] Table 2.1 has exactly one row under the LEDC category, LEDC-253.
The 15 documented errata are CPU-206, CLK-6996, ADC-7227, ADC-1477, I2C-308, SPI-304, LEDC-253,
RMT-176, BOOT-9537, AES-11401, ECC-11400, ECDSA_DS-836, ECDSA_DS-837, 802.15.4-9538 and PCNT-249.
Checked one by one against a PWM lighting product on v1.2 silicon:

- **Nothing else touches PWM output.** RMT-176 is the nearest neighbour, since RMT also drives
  pin-level waveforms, and it is limited to RMT continuous TX mode, affects v0.0 and v0.1 only, and
  has been bypassed in ESP-IDF since v5.1 per the erratum's own note.
- **Only two errata affect v1.2 at all**: PCNT-249 (step interrupts, no fix scheduled, no pin
  dimension, no bearing on LEDC) and ECDSA_DS-836 (signature validation, no fix scheduled,
  workaround "Use RSA_DS Secure Boot instead of ECDSA_DS Secure Boot"). ECDSA_DS-836 remains a live
  question for the Matter secure boot scheme and is already tracked in `BACKLOG.md`. It is not a
  PWM issue.

---

## 8. Question 7: interaction with pin choice

**None. LEDC on ESP32-H2 routes entirely through the GPIO matrix, with no IO MUX affinity and no
per-pin restriction.** The erratum adds nothing to that picture, since it is a timer and duty-value
condition with no pin dimension at all.

Evidence:

- `components/soc/esp32h2/include/soc/gpio_sig_map.h:9-19` at v5.5.4 defines `LEDC_LS_SIG_OUT0_IDX`
  through `LEDC_LS_SIG_OUT5_IDX` as matrix signal indices 0 to 5. There is no `LEDC_HS_*` signal,
  since ESP32-H2 is low-speed mode only.
- [T] version 1.1, Table 6.12-1 "Peripheral Signals via GPIO Matrix", printed page 230, lists
  `ledc_ls_sig_out0` to `ledc_ls_sig_out5` with "Direct Output via IO MUX" marked as not available,
  meaning matrix-only routing.
- `ledc.c` binds the signal at configuration time via `_ledc_set_pin(gpio_num, output_invert, speed_mode, ledc_channel)`,
  with no per-pin table or validation beyond "must be a valid output GPIO".

This matches and confirms `docs/pin-mapping-v2.md` section 2.8. **GPIO10, GPIO11, GPIO12, GPIO13 and
GPIO14 can each carry any of the six LEDC channels, in any assignment, and the assignment can be
changed in firmware without a board change.** No feedback into the PCB.

---

## 9. Bench test, if anyone wants belt and braces

The question is resolved from documentation and source, so no bench test is required before
committing the PCB. If a physical confirmation is wanted later, on an ESP32-H2-DevKitM-1, the
following settles it in about ten minutes and needs only a scope or a logic analyser:

1. Confirm the part is v1.2. `esptool.py chip_id` prints the revision, or call
   `efuse_hal_chip_revision()` and expect 102.
2. Configure timer 0 at `duty_resolution = SOC_LEDC_TIMER_BIT_WIDTH` (20), clock source XTAL,
   `freq_hz` around 30, which is the highest achievable at 20 bits. Bind channel 0 to any GPIO.
3. Sweep duty across `2**20 - 2`, `2**20 - 1`, then `2**20`. Watch the pin.
   - **Fixed silicon**: the output goes to a constant high at `2**20` and stays high.
   - **Affected silicon**: the output collapses, wraps, or produces a glitching waveform at `2**20`,
     which is the "internal duty calculation" breaking.
4. Repeat at `duty_resolution = 13` with duty `8192`. This must give a constant high on every
   revision, since the erratum only applies at the maximum resolution. If step 4 fails, something
   other than LEDC-253 is wrong.
5. Optional endpoint check for lighting: at 13 bits, hold duty 0 and confirm a constant low, then
   run `ledc_set_fade_with_time` from 8192 down to 0 and back and confirm no discontinuity at either
   end. This exercises the section 6.3 behaviour, where the expected result is a fade that starts
   one LSB below full and shows no visible step.

---

## 10. Corrections to existing documents

Not applied here, since this document was asked to leave those files alone. Recording them so the
drift is not lost.

1. `docs/pin-mapping-v2.md` section 3.3 "LEDC-253, and a live documentation conflict" states that
   the ESP-IDF documentation carries the restriction "unconditionally with no revision qualifier"
   and that the conflict "is not resolved". Both statements are incorrect for ESP-IDF v5.4.2 and
   later, including the pinned v5.5.4. The qualifier is at `ledc.rst:280`. The section's practical
   advice, which is to stay off the maximum duty resolution, is still sound and costs nothing.
2. `docs/pin-mapping-v2.md` open question 1 at line 1072 should be closed, pointing here.
3. `docs/pin-mapping-v2.md` section 3.3 says "No revision-gated logic was found in the LEDC driver
   either way." That observation is correct but was read as evidence of doubt. The correct reading is
   that no gating is possible, because there is no workaround code in the driver to gate. The driver
   has always allowed the full duty range, and the workaround was always advice to the application.
4. `docs/h2-strapping-and-reset-states.md` line 392 lists LEDC-253 among errata affecting v0.0 and
   v0.1 only, and is correct. No change needed.
