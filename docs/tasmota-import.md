# Importing Tasmota templates as device profiles

Research and design document for the TYWE2L-to-ESP32-H2 retrofit project.
Answers the question: "273 of roughly 900 Tasmota templates fit our module. How easily could
these simply be imported to configure a device for us?"

Date: 13 August 2026.

This document **extends** [`flasher-and-registry.md`](./flasher-and-registry.md). It does not
restate it. Section 3.3 there covers how ESPHome, Tasmota and OpenBeken each describe a pin map
as data, section 3.4 gives the first-pass template count and the TYWE2L pad table, and section 4
covers the registry repository, its schema-in-CI model and the two-tier `community` / `verified`
trust model. Read those first. Everything here assumes them.

Claims are tagged the same way. **[V]** means I verified it against the actual source, repository
or downloaded corpus during this research, and the citation points at what I read. **[U]** means
unverified or partly verified, and I say why.

**Exactly what I read.** Tasmota at `master`, commit `db56cd62aa455714e4a9a3043b8f835addf771a2`,
and the tags v6.6.0, v7.2.0, v8.1.0, v8.3.1 and v8.5.1. The blakadder template corpus at commit
`30d9c9b16014817641f83a0cf8f2f266a0dbaef2`, whose last commit is dated 6 July 2026, so the corpus
is about five weeks stale relative to this document. Line numbers cited below are from those
commits and will drift.

---

## Bottom line

Yes, and the work is small, but the honest number is **235 templates that convert with no human
pin decision**, not 273, and they collapse to only **124 distinct pin maps** **[V]**. Everything
that transfers cleanly is a relay-plus-button-plus-LED plug or switch, and the other 440 that fit
our five pads are almost all bulbs and lights needing a human to decide the light model, because
Tasmota keeps colour channel order, PWM frequency and dimmer curve in settings the template does
not carry **[V]**. The single biggest correction to the earlier pass: **two thirds of the ESP8266 corpus uses
a completely different, pre-Tasmota-9 integer encoding** (1,640 of 2,539 entries) **[V]**, and where
the same device carries both encodings the two disagree after conversion 38 times out of 64 **[V]**.
Import the pin maps as *drafts* with provenance, never as finished profiles.

### The six things that matter most

1. **The corpus is bigger and older than the earlier pass measured.** 2,539 ESP8266/ESP8285
   templates, of which 1,640 are in the legacy 8-bit encoding and 899 in the modern one. 902 fit
   our five pads, not 273. See section 2.2.
2. **Tasmota ships the legacy-to-modern conversion table itself**, in
   `tasmota/include/tasmota_template_legacy.h` **[V]**. We do not have to reconstruct it. Lift
   `kGpioConvert` and the problem is solved exactly. See section 1.4.
3. **The component-code enum is platform-conditional and it caught me out.** `UserSelectablePins`
   contains inline `#ifdef ESP32` blocks, so 205 of its names have different ordinals on ESP8266
   than on ESP32 **[V]**. My own first decode used the ESP32 numbering and produced plausible,
   wrong function names. See section 1.3. This is the mistake the converter is most likely to
   repeat.
4. **`BASE` is a trap.** 32 of the 902 pin-fitting templates declare `BASE: 54` (Tuya MCU) and 24
   of those have a completely empty GPIO array **[V]**. They pass a naive pin filter perfectly
   because they use no pins at all, and they are serial-bridge devices we cannot support. Reject
   any template whose `BASE` is not 18. See sections 1.5 and 4.1.
5. **The corpus cannot tell us which module is inside the device.** `chip: TYWE2L` appears on
   **4 of 2,871 entries** **[V]**, and all four are CCT downlights. Pin compatibility is necessary
   and nowhere near sufficient. See section 4.6.
6. **A wrong relay polarity energises mains at boot.** The firmware must hold all five pins as
   inputs until a CRC-checked profile has been read, and must never latch a relay on an
   unverified profile. See section 5.

### Correcting the brief and the earlier pass

| Claim | Status |
| --- | --- |
| "273 of roughly 900 templates fit" | **Partly right, badly scoped.** The ~900 is only the modern-encoded subset. I measure 300 of 899 there, and 902 of 2,539 across the whole ESP8266 corpus **[V]** |
| "our five pins are the corpus's most-used" | **Confirmed**, and by a wide margin. GPIO13 1,852 uses, GPIO12 1,731, GPIO5 1,547, GPIO14 1,531, GPIO4 1,452; next is GPIO0 at 705 **[V]** |
| "no machine-readable module field exists" | **Partly wrong.** blakadder has a `chip:` front-matter field that actually carries the *module* part number. It is present on 366 of 2,871 entries, and says `TYWE2L` on 4 **[V]** |
| "a human must confirm each" | **Confirmed, and the case is stronger than stated.** Section 2.4 |
| "Tasmota is AGPL-3.0" (from the brief) | **Wrong at HEAD.** `LICENSE.txt` is the GNU GPL v3 and the README says "This program is licensed under GPL-3.0-only" **[V]**. Every source file header also says GPL, not AGPL. See section 6 |
| (not previously known) | **Tasmota already redistributes blakadder's corpus in-tree.** `TEMPLATES.md` holds 2,809 templates generated from `templates.blakadder.com/list.json`, with 100% name overlap and no EPL notice **[V]**. It changes the licence discussion materially. See sections 2.1 and 6.6 |

---

## 1. The template format, exactly

### 1.1 Anatomy

A Tasmota template is a single JSON object, pasted into the web UI or sent as
`Template {...}`. Two real examples from the corpus **[V]**:

```
# blakadder/_templates/2nice_SP111, front-matter key "template"
{"NAME":"2NICE SP111","GPIO":[56,0,57,0,0,0,0,0,0,17,0,21,0],"FLAG":2,"BASE":18}

# blakadder/_templates/2ch_smart_switch, front-matter key "template9"
{"NAME":"Generic","GPIO":[32,1,1,1,1,225,33,1,224,288,1,1,1,1],"FLAG":0,"BASE":18}
```

Fields, all optional, because Tasmota allows partial updates **[V]**
(`tasmota/tasmota_support/support.ino`, `JsonTemplate()`, lines 1883 to 1978 at commit HEAD of
13 August 2026):

| Field | Meaning |
| --- | --- |
| `NAME` | Free text, up to `TOPSZ`. Stored in `SET_TEMPLATE_NAME`. Purely cosmetic |
| `GPIO` | The array of component codes, one per configurable pin slot. Length and encoding vary; section 1.2 |
| `FLAG` | Legacy ADC0 selector in the old encoding, effectively dead in the new one; section 1.6 |
| `BASE` | 1-based base module number; section 1.5 |
| `ARCH` | Newest addition. If present and it does not match `ArchName()`, **the whole template is rejected** **[V]** |
| `CMND` | Optional `Backlog` of setup commands, run only if the current module is the user template **[V]** |

The source comment above `JsonTemplate()` documents all three generations in one place, which is
the single most useful three lines in the whole codebase for our purposes **[V]**:

```c
bool JsonTemplate(char* dataBuf) {
  // Old: {"NAME":"Shelly 2.5","GPIO":[56,0,17,0,21,83,0,0,6,82,5,22,156],"FLAG":2,"BASE":18}
  // New: {"NAME":"Shelly 2.5","GPIO":[320,0,32,0,224,193,0,0,640,192,608,225,3456,4736],"FLAG":0,"BASE":18}
  // Newest: {"NAME":"Shelly 2.5","ARCH":"ESP8266","GPIO":[320,0,32,0,224,193,0,0,640,192,608,225,3456,4736],"FLAG":0,"BASE":18}
```

### 1.2 The GPIO array: slot index to physical pin

The array is indexed by **configurable slot**, not by GPIO number. On ESP8266 and ESP8285 the
flash pins are removed from the list, so the mapping is not the identity. From
`tasmota/include/tasmota_template.h` lines 1396 to 1401 **[V]**:

```c
#define MAX_GPI8_PIN       17   // Number of supported GPIO (0..16)
#define MAX_GPIO_PIN       18   // Number of supported GPIO (0..16 + ADC0)
#define ADC0_PIN           17   // Pin number of ADC0
#define MIN_FLASH_PINS     4    // Number of flash chip pins unusable for configuration (GPIO6, 7, 8 and 11)
#define MAX_USER_PINS      14   // MAX_GPIO_PIN - MIN_FLASH_PINS
```

The expansion happens in `TemplateGpios()` in `tasmota/tasmota_support/support.ino`, lines 1680 to
1740 **[V]**:

```c
#ifdef ESP8266
    if (6 == i) { j = 9; }
    if (8 == i) { j = 12; }
    dest[j] = src[i];
    j++;
#endif  // ESP8266
```

Composing that with the TYWE2L pad table already established in
[`flasher-and-registry.md` §3.4](./flasher-and-registry.md), and with our ESP32-H2 mapping:

| Slot | ESP8285 GPIO | TYWE2L pad | Our ESP32-H2 GPIO (v2) | On our board? |
| --- | --- | --- | --- | --- |
| 0 | GPIO0 | test point only | | **no** |
| 1 | GPIO1 (TXD) | not broken out | | **no** |
| 2 | GPIO2 | test point (UART1 TX) | | **no** |
| 3 | GPIO3 (RXD) | not broken out | | **no** |
| **4** | **GPIO4** | **pad 5** | **GPIO11** | yes |
| **5** | **GPIO5** | **pad 4** | **GPIO10** | yes |
| 6 | GPIO9 | not broken out | | **no** |
| 7 | GPIO10 | not broken out | | **no** |
| **8** | **GPIO12** | **pad 2** | **GPIO14** | yes |
| **9** | **GPIO13** | **pad 3** | **GPIO12** | yes |
| **10** | **GPIO14** | **pad 1** | **GPIO13** | yes |
| 11 | GPIO15 | not broken out | | **no** |
| 12 | GPIO16 | not broken out | | **no** |
| 13 | ADC0 (TOUT) | not broken out | | **no** |

**Read that fourth column carefully, because the numbering crosses over.** ESP8285 GPIO12, GPIO13
and GPIO14 are leg nets; H2 GPIO12, GPIO13 and GPIO14 are module pins; and they do not line up. The
leg carrying ESP8285 GPIO14 goes to H2 GPIO13, and the leg carrying ESP8285 GPIO12 goes to H2
GPIO14. A converter that treats the two namespaces as one will silently produce a pin map that is
wrong on three legs out of five, which `BACKLOG.md` classes as a physical hazard. Keep the two
namespaces distinct in code and in every emitted record.

This column is the **v2** mapping, adopted and implemented. It supersedes the GPIO1 to GPIO5
mapping that earlier drafts of this document used.
[`docs/pin-mapping-v2.md`](./pin-mapping-v2.md) is the authority. Note that pad number equals leg
number, so `pad` alone is an unambiguous key: the safest thing a profile can carry is the pad, with
the H2 number derived from this one table at build time rather than transcribed by hand.

The legacy 13-slot array is the same list without the ADC0 slot. Slot 13 exists only in the
modern encoding.

The other platforms, for completeness **[V]** (`tasmota_template.h` lines 1436 to 1550,
`TemplateGpios()` lines 1725 to 1738):

| Platform | `MAX_USER_PINS` | Mapping |
| --- | --- | --- |
| ESP32-C2 | 21 | `dest[i] = src[i]`, straight identity |
| ESP32-C3 | 22 | identity |
| ESP32-C6 | 31 | identity |
| ESP32-S2 | 36 | identity, skipping physical 22 to 32 |
| ESP32-S3 | 38 | identity, skipping physical 22 to 32 |
| ESP32 | 36 | indirect, through the `Esp32TemplateToPhy[]` table |

This matters because it means an ESP32-family template is not merely differently numbered, it is
differently *indexed*, and any converter that treats them uniformly will silently produce garbage.

### 1.3 The component code

In Tasmota 9 and later, each array entry is a packed 16-bit value. From
`tasmota/include/tasmota_globals.h` lines 533 and 534 **[V]**:

```c
#define AGPIO(x) ((x)<<5)
#define BGPIO(x) ((x)>>5)
```

So a value is `(function_ordinal << 5) | instance_index`, with the instance index zero-based. Five
bits of index means at most 32 instances of any one function, which is why `Pin()` masks with
`0xFFE0` when the caller does not care about the index **[V]** (`support.ino` lines 1533 to 1545).

The function ordinal is the position in `enum UserSelectablePins`, declared at
`tasmota/include/tasmota_template.h` line 26 and terminated by `GPIO_SENSOR_END` at line 245.
At the commit I read, **`GPIO_SENSOR_END` evaluates to 358 in an ESP8266 build and 385 in an
ESP32 build** **[V]**, guarded by:

```c
static_assert(GPIO_SENSOR_END < 2000, "Too many UserSelectablePins");
```

Two values are special and neither is a packed code:

- **`0`** is `GPIO_NONE`, meaning the pin is deliberately unused.
- **`1`** is the on-the-wire spelling of "user configurable", i.e. free. `JsonTemplate()` rewrites
  it on input and `TemplateJson()` rewrites it on output **[V]**:

```c
        if (gpio == (AGPIO(GPIO_NONE) +1)) {
          gpio = AGPIO(GPIO_USER);
        }
```

```c
    if (gpio == AGPIO(GPIO_USER)) {
      gpio = AGPIO(GPIO_NONE) +1;
    }
```

`GPIO_USER` itself is 2047, declared in a separate `enum ProgramSelectablePins` starting at
`GPIO_FIX_START = 2046` **[V]** (lines 250 to 253). Treat `0` and `1` identically for our
purposes: nothing is connected.

**The caveat that caught me out, and it will catch the converter out too.** The enum is **not**
a flat list. At the commit I read it contains six inline `#ifdef ESP32` blocks (at lines 104, 119,
161, 166 and 235 among others) holding webcam, Ethernet, hall-effect, e-paper, pull-down-button and
SDIO codes, plus a trailing `#ifdef USE_MODBUS_RELAY` block **[V]**. Evaluating the enum both ways:

| | ESP8266 build | ESP32 build |
| --- | --- | --- |
| Names in the enum | 358 | 385 |
| Names present only in this build | 0 | **27** |
| Names whose ordinal differs between the two | **205** | |
| First ordinal at which they diverge | **154** | |

**[V]** for every cell. Everything at ordinal 153 or below is identical on both platforms, which
happily covers `GPIO_REL1` (7), `GPIO_KEY1` (1), `GPIO_PWM1` (13), `GPIO_LED1` (9),
`GPIO_LEDLNK` (17), `GPIO_TUYA_TX` (71) and the whole ADC family (147 to 153). Above that they
diverge, and three codes matter to us specifically: **`GPIO_KEY1_PD`, `GPIO_KEY1_INV_PD` and
`GPIO_SWT1_PD` are ESP32-only and cannot legitimately appear in an ESP8266 template at all**
**[V]**, and `GPIO_INPUT` is ordinal 222 on ESP8266 against 239 on ESP32 **[V]**.

I state this with feeling because my own first pass at this analysis parsed the enum with the
preprocessor lines stripped, which yields the ESP32 numbering, and it produced perfectly plausible
output: it reported webcam and MCP2515 codes on TYWE2L-sized Tuya plugs. Nothing downstream flags
that as wrong. **A converter must evaluate the ESP8266 arm specifically, and it must assert that
no code it emits is one of the 27 ESP32-only names.** Selecting the table from the source field
name (`template9` versus `template32` versus `templatec3`) is necessary but not sufficient; the
table itself has to be built correctly in the first place.

One piece of good news from the same investigation: the only non-platform conditional inside the
enum is `USE_MODBUS_RELAY`, and it sits immediately before `GPIO_SENSOR_END`, so it adds four codes
at the end and shifts nothing **[V]**. **Ordinals are not build-flag dependent.** One table per
platform is enough.

### 1.4 The renumbering, and the fact Tasmota ships the converter

This is the finding that changes the shape of the work.

**Before Tasmota 9.0 the array held raw 8-bit enum ordinals with no packing and no instance
index.** The old enum began `GPIO_NONE, GPIO_DHT11, GPIO_DHT22, GPIO_SI7021, GPIO_DSB,
GPIO_I2C_SCL, ...` and gave each instance its own name: `GPIO_REL1` through `GPIO_REL8` were eight
consecutive ordinals, 21 to 28. The modern enum begins `GPIO_NONE, GPIO_KEY1, GPIO_KEY1_NP,
GPIO_KEY1_INV, GPIO_KEY1_INV_NP, GPIO_SWT1, GPIO_SWT1_NP, GPIO_REL1, GPIO_REL1_INV, ...` and
carries the instance in the low five bits instead. The two orderings have nothing in common.

**Tasmota froze the old enum and shipped the mapping.** The file is
[`tasmota/include/tasmota_template_legacy.h`](https://github.com/arendst/Tasmota/blob/master/tasmota/include/tasmota_template_legacy.h)
**[V]**, 480 lines, ESP8266 only, and it contains exactly two useful things:

```c
// ATTENTION: No additions are supported
enum LegacyUserSelectablePins {
  GPI8_NONE,           // Not used
  GPI8_DHT11,          // DHT11
  ...
  GPI8_SENSOR_END };
```

```c
// Indexed by LegacyUserSelectablePins to convert legacy (8-bit) GPIOs
const uint16_t kGpioConvert[] PROGMEM = {
  GPIO_NONE,
  AGPIO(GPIO_DHT11),          // DHT11
  ...
  AGPIO(GPIO_REL1),           // Relay
  AGPIO(GPIO_REL1) +1,
  ...
```

I parsed it: **217 entries, exactly matching `GPI8_SENSOR_END` = 217** **[V]**. Every entry is
either `GPIO_NONE` or `AGPIO(NAME)` with an optional `+n`, so it evaluates mechanically against
the modern enum with no judgement calls. Spot-checking it against the source comment in
`JsonTemplate()`: legacy 56 converts to 320 (`GPIO_LED1_INV#1`), 17 to 32 (`GPIO_KEY1#1`), 21 to
224 (`GPIO_REL1#1`), all matching the "Old:"/"New:" pair quoted in section 1.1 **[V]**.

The runtime path is `GpioConvert()` / `Adc0Convert()` / `TemplateConvert()` in `support.ino`
lines 1495 to 1531 **[V]**, and out-of-range legacy values fall back to `AGPIO(GPIO_USER)`:

```c
uint16_t GpioConvert(uint8_t gpio) {
  if (gpio >= nitems(kGpioConvert)) {
    return AGPIO(GPIO_USER);
  }
  return pgm_read_word(kGpioConvert + gpio);
}
```

**How Tasmota decides which era it is looking at**, from `JsonTemplate()` **[V]**:

```c
    bool old_template = false;
    uint8_t template8[sizeof(mytmplt8285)] = { GPIO_NONE };
    if (13 == arr.size()) {  // Possible old template
      uint32_t gpio = 0;
      for (uint32_t i = 0; i < nitems(template8) -1; i++) {
        gpio = arr[i].getUInt();
        if (gpio > 255) {    // New templates might have values above 255
          break;
        }
        template8[i] = gpio;
      }
      old_template = (gpio < 256);
    }
```

Thirteen elements **and** every value below 256. That is the exact test, and our converter should
use the identical one so that we never disagree with Tasmota about what a string means.

**How much of the corpus this affects.** I measured the front-matter keys across all 2,871
blakadder template files **[V]**:

| Front-matter key | Files | GPIO array length | Encoding |
| --- | --- | --- | --- |
| `template` | 1,705 (1,710 occurrences; 5 files carry the key twice) | 13 in 1,708 cases | **legacy 8-bit** |
| `template9` | 915 | 14 in 899 cases | modern packed, ESP8266 |
| `template32` | 169 | 36 | modern packed, ESP32 |
| `templatec3` | 82 | 22 | modern packed, ESP32-C3 |
| `templates3` | 27 | 36 or 38 | modern packed, ESP32-S3 |
| `templates2` | 11 | 36 | modern packed, ESP32-S2 |
| `templatec2` | 10 | 21 or 22 | modern packed, ESP32-C2 |
| `templatec6` | 7 | 31 | modern packed, ESP32-C6 |

Sanity check on the encoding split: of the 1,708 thirteen-element `template` entries, 1,708 have
every value at or below 255 **[V]**. The split is clean.

**The legacy enum was stable for years, which is the good news.** I pulled
`sonoff/sonoff_template.h` or `tasmota/tasmota_template.h` at tags v6.6.0, v7.2.0, v8.1.0, v8.3.1
and v8.5.1 and diffed the parsed ordinals. **Of the 161 names present in v6.6.0, exactly zero
changed ordinal by v8.5.1**; only `GPIO_SENSOR_END` moved, because new codes were appended
**[V]**. So one table decodes every legacy template regardless of which Tasmota version the
contributor was running. That removes the worst fear about the old corpus.

Two caveats on that. The frozen `GPI8_` copy at HEAD is a verbatim snapshot of the **v8.5.1**
enum, so `kGpioConvert` is only *documented* as correct for 8.x-era templates; my measurement
extends the guarantee back to 6.6.0, but it is my measurement rather than Tasmota's promise.
And Tasmota itself removed support for direct upgrade from versions before 7.0, so pre-7.0
templates are out of scope for the project that wrote the table **[V]**.

**The release is v9.1.0, and the trail is exact [V].** The `<<5` macro first appears in commit
`ef61668037ed8667ec438a874339d52f3632c14d` (29 April 2020, first tagged v8.3.0), gated behind
`FINAL_ESP32` and dormant for ESP8266. The break itself is
`1ae9adc6426f3bff7399b26cd454c1aba040f7a4`, "Change redesigning ESP8266 GPIO internal numbering in
line with ESP32" (30 September 2020, first tagged **v9.1.0**), which uncommented the macros and
bumped `tasmota_version.h` from `0x08050001` to `0x09000001`. `RELEASENOTES.md` at v9.1.0 states it
plainly:

> Redesigned ESP8266 GPIO internal representation in line with ESP32 changing ``Template`` layout too

and, under removals, "Support for downgrade to versions before 9.0 keeping current GPIO
configuration". Forward conversion works; backward does not. That is why the community database
key is `template9` and why both keys still exist side by side six years later.

Corroborating detail from the 8.x struct at v8.5.1 **[V]**: `MAX_USER_PINS` was **13**, and the
flag was `typedef union { uint8_t data; struct { uint8_t adc0 : 4; ... } } gpio_flag`. The fourteenth
slot and the death of the ADC0 nibble are the same change.

**[U]** One caution about relying on Tasmota's own migration path rather than on the string
conversion. `ConvertGpios()` reads 14 bytes at the old `ex_user_template8` offset, and later
settings fields have since reclaimed part of that window, so a direct jump from an 8.x settings
blob to a current build can mangle several slots of a *custom* user template. This is a static
reading of offsets and call ordering, not a runtime test, and it does not affect us, because we
convert template *strings* and never touch a Tasmota settings blob. It is a good reason not to tell
users "just upgrade Tasmota and copy the template out".

### 1.5 BASE, and why it is a trap

`BASE` is the **1-based** index into `enum SupportedModulesESP8266`, declared at
`tasmota_template.h` line 1612 with 75 members and a matching `kModuleNames` string table at
line 1623 **[V]**. `JsonTemplate()` stores it as `base - 1` and falls back to 18 on anything
invalid **[V]**:

```c
    if ((0 == base) || !ValidTemplateModule(base -1)) { base = 18; }
    Settings->user_template_base = base -1;  // Default WEMOS
```

Selected values, decoded from the enum and the name table **[V]**:

| `BASE` | Enum member | Display name |
| --- | --- | --- |
| 1 | `SONOFF_BASIC` | Sonoff Basic |
| **18** | `WEMOS` | **Generic** (the neutral one, and the default) |
| 20 | `H801` | H801, a five-channel PWM controller |
| 27 | `AILIGHT` | AiLight, a MY9231 serial-driver bulb |
| 45 | `BLITZWOLF_BWSHP` | BlitzWolf SHP, energy-metering plug |
| 48 | `PHILIPS` | Xiaomi Philips |
| **54** | `TUYA_DIMMER` | **Tuya MCU** |
| 62 | `YTF_IR_BRIDGE` | YTF IR Bridge |

**Why it is a trap.** `BASE` does not merely name the device. It selects which built-in module's
special-case driver behaviour Tasmota enables. `BASE: 54` turns on the TuyaMCU serial driver, and
such a template legitimately carries an empty or near-empty GPIO array, because the device's
entire function lives behind a UART rather than on host GPIOs.

In our corpus, **32 of the 902 pin-fitting templates have `BASE: 54`, and 24 of those use zero
pins at all** **[V]**. A naive "does it fit five pads" filter scores them perfectly. They are
`ME81H Thermostat_8266`, `ZY-M100`, `Arlec DCF4002WHA`, `Tuya IR`, `Kogan Difuser` and similar:
devices we cannot support at all. In total **128 of the 902 pin-fitting templates carry
`BASE != 18`** **[V]**, distributed as `BASE` 54 (32), 48 (24), 62 (23), 1 (17), 20 (7), 27 (5),
61 (4) and 34 (3).

**Rule: reject any template whose `BASE` is not 18, and report it as a distinct outcome rather
than folding it into a general failure.** The 32 TuyaMCU ones in particular are worth naming in
the tool's output, because a user who owns one deserves an explanation rather than "unsupported".

### 1.6 FLAG

In the **legacy** encoding `FLAG` carried the ADC0 function, and it is genuinely load-bearing.
`JsonTemplate()` takes the low nibble and appends it as a fourteenth pseudo-slot **[V]**:

```c
      val = root[PSTR(D_JSON_FLAG)];
      if (val) {
        template8[nitems(template8) -1] = val.getUInt() & 0x0F;
      }
      TemplateConvert(template8, Settings->user_template.gp.io);
      Settings->user_template.flag.data = 0;
```

and `Adc0Convert()` turns 1 through 7 into `AGPIO(GPIO_ADC_INPUT + adc0 - 1)`, with 0 meaning
none and anything above 7 meaning user-configurable **[V]**. So `"FLAG":2` in the `2nice_SP111`
example above is not decoration, it declares an analogue temperature sensor on TOUT.

**178 of the 1,640 legacy ESP8266 templates carry a non-zero FLAG** **[V]**, so a converter that
ignores it will silently drop a sensor on roughly 11% of the old corpus. Since TOUT is not brought
out on the TYWE2L, our filter must treat a non-zero legacy FLAG as "uses a pin we do not have".

In the **modern** encoding `FLAG` is stored raw and is effectively dead. The header says so
directly **[V]** (`tasmota_template.h` line 1571):

```c
#define GPIO_FLAG_USED       0  // Currently no flags used
```

and only **2 of the 899 modern ESP8266 templates have a non-zero FLAG** **[V]**. Ignore it in the
modern path, and log a warning if it is non-zero.

### 1.7 What the parser does with a code it does not know

Nothing defensive. `JsonTemplate()` writes `val.getUInt()` straight into
`Settings->user_template.gp.io[i]` with no range check **[V]**. Validation happens later and
per-driver, when `PinUsed()` fails to find the pin. In the legacy path an out-of-range 8-bit value
becomes `GPIO_USER`, i.e. "free", which is the safe direction.

**Our converter must be stricter than Tasmota, not equally strict.** An unrecognised code is a
hard failure, not a silently-free pin, because a pin we believe to be free is a pin our firmware
may drive.

---

## 2. The corpora, measured

Everything in this section is a real count from a real download on 13 August 2026. The commands
are in the appendix so the numbers can be reproduced or disputed.

### 2.1 What each source actually is

| Source | Records | Machine-readable? | Licence | Verdict as an import source |
| --- | --- | --- | --- | --- |
| [blakadder/templates](https://github.com/blakadder/templates) | **2,871** files under `_templates/`, plus 180 under `_unsupported/` **[V]** | Jekyll front matter, no schema, no CI. The published `templates.json` is **invalid JSON**; `list.json` is Markdown served as JSON **[V]** | **EPL-2.0** (`LICENSE.md` at HEAD, byte-identical to the canonical Eclipse text) **[V]** | **The only viable ESP pin-map source.** Section 2.2 |
| [arendst/Tasmota](https://github.com/arendst/Tasmota), built-in modules | **75** names in `enum SupportedModulesESP8266`; **62** distinct ESP8266 pin structs (`kModules8266` 56 plus `kModules8285` 6) and 12 ESP32-family, so **87 names over 74 distinct pin maps** **[V]** | C source, fully machine-readable | **GPL-3.0** **[V]** | Tiny as device data. It is the authoritative *decode tables*: take `kGpioConvert` and the enums |
| **`Tasmota/TEMPLATES.md`** | **2,809 templates, all parsing cleanly, 69 categories, 355 KB** **[V]** | the cleanest Markdown dump of the corpus that exists | in a GPL-3.0 repo, but see section 6.3 | **It is a generated copy of blakadder**, produced by `tools/templates/templates.py` from `templates.blakadder.com/list.json`. Not an independent source |
| Tasmota "Device Template" issues and discussions | the label **no longer exists**; the live one is `template missing/incomplete` with **1,355** issues, and there is **no Device Template Discussions category** **[V]** | free text in issue bodies | repo terms | Poor, and superseded. Submission moved wholly to blakadder's Google form |
| [OpenBeken `devices.json`](https://openbekeniot.github.io/webapp/devices.json) | **889** devices, **712** with a non-empty `pins` object, **869** with `board`; ESP8266 + ESP8285 = **37 entries, 0 with pin maps** **[V]** | clean JSON, and it ships a draft-04 `schema.json` | **no licence file anywhere** **[V]** | Module identity and schema shape. **No ESP pin data at all.** Pin data is independently extracted from Tuya `user_param_key` blobs, not copied |
| [tuya-cloudcutter.github.io](https://github.com/tuya-cloudcutter/tuya-cloudcutter.github.io) | **777 devices, 372 profiles**; **433 devices with at least one `*_pin` field, 484 with the module identified** **[V]** | clean JSON with a live static API | **no licence, no README** in that repo; the tool repo is MIT **[V]** | **The best-provenance pin data reviewed**, lifted from the device's own OEM flash. Chips are BK7231N/T and RTL87xx. **Nothing for ESP8266** |
| [zigbee-herdsman-converters](https://github.com/Koenkk/zigbee-herdsman-converters) | **4,551** unique models across 382 device files **[V]** | clean TypeScript | **MIT** **[V]** | **Not comparable.** Two incidental "gpio" string hits in the whole of `src/`, both unrelated. Zigbee only |
| [devices.esphome.io](https://github.com/esphome/devices.esphome.io) | **778** devices; 618 pages yield at least one pin, 6,810 assignments extractable **[V]** | Zod-validated front matter, best CI of any source here, but pins live in prose YAML blocks | **GPL-3.0** **[V]** | **Licence blocks a CC0 import.** Also, a pin's function is implied by the enclosing ESPHome component, so it is a real transform rather than a field copy |
| [tuya-convert](https://github.com/ct-Open-Source/tuya-convert) | **no device database in the repo** (34 files, all tooling) **[V]** | n/a | MIT | Dormant since 6 September 2024. Its wiki holds roughly 280 hand-typed rows and is superseded |

**Three of the four best pin corpora are blocked on licensing rather than on data quality.**
tuya-cloudcutter (433 OEM-derived pin maps) and OpenBeken (712) carry **no licence at all**, which
is worse than a restrictive one because the default is all rights reserved, held per contributor.
ESPHome is GPL-3.0. Asking the cloudcutter and OpenBeken maintainers for an explicit CC0 or MIT
dedication is probably the highest-value single action available on this whole topic, and it costs
an email.

**The finding that reframes section 6.** Tasmota's own repository redistributes blakadder's corpus.
`tools/templates/templates.py`, itself GPL-headered, has
`LIST_URL = "https://templates.blakadder.com/list.json"` and writes `TEMPLATES.md`, regenerated
every two to three months, last on 17 June 2026 **[V]**. Of its 2,616 distinct template names,
**100% are present in blakadder and none are Tasmota-only**, and 1,174 of 2,809 entries are
byte-identical JSON **[V]**. Tasmota carries **no EPL notice**, though it does credit blakadder in
the README and in the `TEMPLATES.md` header. Whatever position we take on importing, a GPL-3.0
project has already taken a more aggressive one.

**The published index is broken, confirmed independently.** I downloaded
`https://templates.blakadder.com/templates.json` (1,458,062 bytes, HTTP 200) and it fails to parse
at byte 31,582 **[V]**:

```
    "template": Module 1,
```

The Liquid generator writes the `template` value unquoted, which is fine when it is a JSON object
and fatal when a contributor typed free text. **Use the repository files, not the published
index.** That also gets us the `chip`, `category`, `type` and `standard` fields that the index
partly drops.

A second pass over the same file counted **26 defects in total**: 25 malformed `template` values
(19 bare commas, five `Module 1`, one `Module 18`) plus one stray brace. Repaired, it yields 2,861
records of which 2,836 carry a GPIO object **[V]**.

Its companion `list.json`, which is the file **Tasmota itself consumes**, is worse: served as
`Content-Type: application/json`, 358,586 bytes, and the content is **Markdown**, opening
`# Templates` / `## Adapter Board` **[V]**. It is not JSON at all.

**There is a second, subtler reason to avoid the index, and it matters more.** The generator
flattens every architecture-specific key down to a single `template` field through a Liquid
precedence chain (`templates.json` line 18, and identically in `list.json` line 11) **[V]**:

```liquid
{% if template.templatec6 != nil %}"template": {{ template.templatec6 }}
{% elsif template.templates3 != nil %}...
{% elsif template.template32 != nil %}"template": {{ template.template32 }}
{% elsif template.template9 != nil %}"template": {{ template.template9 }}
{% else %}"template": {{ template.template }}{% endif %},
```

So the published API hands you one `template` key whose *architecture and encoding you cannot
determine from the key*. It might be an ESP32-C6 array, an ESP8266 packed array, or a legacy 8-bit
array, and the field name is identical in all three cases. A consumer has to reconstruct the
encoding from the array length and value range, and cannot reconstruct the architecture at all
without the length heuristic. The repository files keep the distinction, which is the whole reason
our converter can route correctly.

### 2.2 The real count for our five pads

Method: clone `blakadder/templates`, parse every file's front matter, prefer `template9` over
`template` where a file carries both, classify the encoding using Tasmota's own 13-elements-and-all-under-256
test, decode legacy entries through `kGpioConvert`, expand slots to physical GPIOs, and ask whether
the set of *assigned* pins (excluding `GPIO_NONE` and `GPIO_USER`) is a subset of {4, 5, 12, 13, 14}.

| | Count |
| --- | --- |
| Template files in `_templates/` | **2,871** |
| Files with no template field, or an unparseable one | 24 |
| ESP32-family templates (`template32`, `templatec3`, `templates2`, `templates3`, `templatec2`, `templatec6`, and modern arrays of non-ESP8266 length) | 308 |
| **ESP8266 / ESP8285 templates** | **2,539** |
| of which legacy 8-bit encoding | 1,640 |
| of which modern packed encoding | 899 |
| **Fit our five pads** | **902 (35.5%)** |
| of which legacy | 602 |
| of which modern | 300 |
| Fit **and** `BASE` is 18 (Generic) | **774** |
| Need at least one pin we do not have | 1,637 |

**[V]** for every row.

Physical pin usage across the 2,539 ESP8266 templates, confirming and sharpening the earlier
finding **[V]**:

| GPIO | Templates using it | On our board? |
| --- | --- | --- |
| 13 | 1,852 | yes |
| 12 | 1,731 | yes |
| 5 | 1,547 | yes |
| 14 | 1,531 | yes |
| 4 | 1,452 | yes |
| 0 | 705 | no |
| 3 (RXD) | 644 | no |
| 15 | 572 | no |
| 1 (TXD) | 431 | no |
| 16 | 339 | no |
| 2 | 336 | no |
| ADC0 (TOUT) | 202 | no |
| 9 | 70 | no |
| 10 | 41 | no |

The five pads really are the five most-used pins, by a factor of roughly two over the next one.

### 2.3 The function vocabulary is tiny

Across all 902 pin-fitting templates there are only **40 distinct component codes** **[V]**. That
is the number that makes this project tractable: the conversion table in section 3 is not a
358-row monster, it is 40 rows plus a deny-list.

| Code | Templates using it |
| --- | --- |
| `GPIO_PWM1` | 519 |
| `GPIO_KEY1` | 259 |
| `GPIO_REL1` | 255 |
| `GPIO_LED1_INV` | 169 |
| `GPIO_LED1` | 102 |
| `GPIO_SM16716_CLK` / `_DAT` | 42 each |
| `GPIO_SM2135_CLK` / `_DAT` | 38 each |
| `GPIO_SM16716_SEL` | 38 |
| `GPIO_PWM1_INV` | 37 |
| `GPIO_IRRECV` | 34 |
| `GPIO_IRSEND` | 30 |
| `GPIO_LEDLNK` | 29 |
| `GPIO_LEDLNK_INV` | 27 |
| `GPIO_SWT1` | 22 |
| `GPIO_KEY1_NP` | 11 |
| `GPIO_DI` / `GPIO_DCKI` (MY92x1) | 5 each |
| `GPIO_REL1_INV` | 4 |
| `GPIO_SWT1_NP` | 3 |
| `GPIO_I2C_SCL`, `GPIO_I2C_SDA`, `GPIO_WS2812`, `GPIO_KEY1_INV` | 2 each |
| 15 further codes | 1 each |

The long tail of ones is worth listing because it tells you what the edge cases look like:
`GPIO_BL6523_TX`/`_RX`, `GPIO_BOILER_OT_TX`/`_RX`, the four `GPIO_SHIFT595_*` codes,
`GPIO_RFRECV`, `GPIO_CNTR1`, `GPIO_CNTR1_NP`, `GPIO_DSB`, `GPIO_OUTPUT_LO`, `GPIO_OUTPUT_HI` and
`GPIO_HJL_CF` **[V]**.

### 2.4 Corpus quality, measured rather than asserted

Three measurements, and the third one is the reason a human has to be in the loop.

**One.** 24 of 2,871 files have no usable template at all **[V]**.

**Two.** 78 distinct template `NAME` values appear with more than one pin map across the corpus,
out of 2,352 distinct names **[V]**. Some of that is genuine hardware revisions. Some of it is not.

**Three, and this is the important one.** 64 files carry **both** a legacy `template` and a modern
`template9` for the same device. If both are correct, converting the legacy one through
`kGpioConvert` must produce the modern one. **It does for 26. It disagrees for 38** **[V]**.
Two examples:

```
gosund_SL1
  legacy, converted:  GPIO0=KEY1#1                GPIO12=PWM1#1 GPIO13=PWM1#3 GPIO14=PWM1#2  ADC0=ADC_LIGHT
  template9        :  GPIO4=KEY1#1  GPIO5=PWM1#2  GPIO12=PWM1#1 GPIO13=PWM1#3                ADC0=ADC_LIGHT

freecube_AWS01F
  legacy, converted:  GPIO3=KEY1#1  GPIO4=REL1#1  GPIO14=REL1#2
  template9        :  GPIO0=LED1#1  GPIO2=LED1#2  GPIO12=LED1#3  GPIO13=KEY1#1  GPIO15=REL1#1
```

Not a refinement. A completely different pinout for the same product name. One of the two is
wrong, or they are different hardware revisions sold under one name, and **nothing in the data
says which**. `freecube_AWS01F` and `3stone_EBE-QPW36` are the same story.

That is the empirical answer to "can we just import this". The corpus contradicts itself on 59%
of the cases where it gives us a second opinion. Treat every import as a hypothesis.

---

## 3. The conversion

### 3.1 Target schema

Our profile is the "role, index, polarity per pin" shape recommended in
[`flasher-and-registry.md` §3.3](./flasher-and-registry.md), copied from OpenBeken's
`pinsState_t` rather than from Tasmota's packed integers. The registry record is JSON (CC0); the
flasher compiles it to the raw versioned blob described in §3.2 of that document. Sketch, with
only the fields this document needs:

```json
{
  "schema": 1,
  "id": "gosund-sp1-v23",
  "variant": "onoff",
  "pins": [
    { "pad": 2, "esp8285_gpio": 12, "gpio": 14,
      "role": "relay",  "index": 0, "active_level": "high", "boot_state": "off" },
    { "pad": 3, "esp8285_gpio": 13, "gpio": 12,
      "role": "button", "index": 0, "active_level": "low",  "pull": "up" },
    { "pad": 5, "esp8285_gpio": 4,  "gpio": 11,
      "role": "led",    "index": 0, "active_level": "low",  "follows": "relay:0" }
  ],
  "endpoints": [
    { "id": 1, "device_type": "on_off_plug_in_unit", "binds": { "on_off": "relay:0" } }
  ]
}
```

`gpio` is our **ESP32-H2** number under the v2 mapping, already translated. The converter does that
translation from the §1.2 table; the firmware never sees an ESP8285 pin number. `pad` and
`esp8285_gpio` are carried for the reviewer and for debugging, and they are what makes a later table
correction replayable.

The example above is worth reading slowly, because it is the shape that catches people. ESP8285
GPIO12 becomes H2 GPIO**14**, and ESP8285 GPIO13 becomes H2 GPIO**12**. The numbers cross. Anyone
who eyeballs `"esp8285_gpio": 12` and expects `"gpio": 12` on the same line has misread the mapping,
and a profile that agrees with that misreading will drive the wrong pin on a mains appliance.

### 3.2 Code-by-code conversion table

Outcome column: **A** = auto-convert, no human input. **H** = convert, but a human must answer a
question before the profile can be published. **X** = cannot support on this platform, reject.

**All ordinals below are the ESP8266 arm of the enum**, per section 1.3. Everything at ordinal 153
or below is identical on ESP32; above that the two diverge, so do not reuse this table for an
ESP32 template.

#### Digital outputs and inputs

| Tasmota code | Ordinal | Packed value (#1) | Legacy 8-bit | Our role | Matter mapping | Outcome |
| --- | --- | --- | --- | --- | --- | --- |
| `GPIO_NONE` | 0 | 0 | 0 | pin unused | | A |
| (`GPIO_USER` on the wire) | 2047 | `1` | 255 | pin unused | | A |
| `GPIO_REL1` | 7 | 224 | 21 | `relay`, `active_level: high` | On/Off server on the endpoint bound to that relay index | **A** |
| `GPIO_REL1_INV` | 8 | 256 | 29 | `relay`, `active_level: low` | as above | **A** |
| `GPIO_KEY1` | 1 | 32 | 17 | `button`, `active_level: low`, `pull: up` | local toggle of the bound relay; optionally a Generic Switch endpoint | **A** |
| `GPIO_KEY1_NP` | 2 | 64 | 90 | `button`, `pull: none` | as above. `_NP` = no internal pull-up | **A** |
| `GPIO_KEY1_INV` | 3 | 96 | 122 | `button`, `active_level: high` | as above | **A** |
| `GPIO_KEY1_INV_NP` | 4 | 128 | 126 | `button`, `active_level: high`, `pull: none` | as above | **A** |
| `GPIO_KEY1_PD` | ESP32 only | | | not reachable from an ESP8266 template | | **X** |
| `GPIO_KEY1_INV_PD` | ESP32 only | | | not reachable from an ESP8266 template | | **X** |
| `GPIO_SWT1` | 5 | 160 | 9 | `switch`, `active_level: low`, `pull: up` | see 3.3, the semantics need a decision | **H** |
| `GPIO_SWT1_NP` | 6 | 192 | 82 | `switch`, `pull: none` | as above | **H** |
| `GPIO_SWT1_PD` | ESP32 only | | | not reachable from an ESP8266 template | | **X** |
| `GPIO_LED1` | 9 | 288 | 52 | `led`, `active_level: high`, `follows: relay:n` | none. Firmware behaviour, not a cluster | **A** |
| `GPIO_LED1_INV` | 10 | 320 | 56 | `led`, `active_level: low` | as above | **A** |
| `GPIO_LEDLNK` | 17 | 544 | 157 | `status_led`, `active_level: high` | none. Bind to Thread/commissioning state | **A** |
| `GPIO_LEDLNK_INV` | 18 | 576 | 158 | `status_led`, `active_level: low` | as above | **A** |
| `GPIO_BUZZER` | 15 | 480 | 160 | `buzzer`, `active_level: high` | none in our cluster set. Drive it from Identify | **H** |
| `GPIO_BUZZER_INV` | 16 | 512 | 161 | `buzzer`, `active_level: low` | as above | **H** |
| `GPIO_OUTPUT_HI` | 120 | 3840 | not in legacy | `fixed_output`, high | none. Set at boot and leave | **A** |
| `GPIO_OUTPUT_LO` | 121 | 3872 | not in legacy | `fixed_output`, low | none | **A** |
| `GPIO_INPUT` | 222 | 7104 | not in legacy | `input` | Boolean State cluster, or drop | **H** |

The three `_PD` rows are the concrete cost of getting section 1.3 wrong. They exist, they are
plausible things for a Tuya device to want, and on ESP8266 they are simply not in the enum. A
converter using the ESP32 table will decode packed values 7680, 7712 and 7744 as pull-down buttons
and switches when on ESP8266 those values mean `GPIO_SHIFT595_*` and neighbours instead.

Note that `GPIO_LED1` is not a Matter concept. In Tasmota a `LEDn` mirrors `Powern`; in Matter the
state lives in the On/Off attribute and the LED is a local presentation detail. It belongs in our
profile because the firmware must drive it, and it belongs nowhere in the data model.

#### Dimming and light

| Tasmota code | Ordinal | Packed (#1) | Legacy | Our role | Matter mapping | Outcome |
| --- | --- | --- | --- | --- | --- | --- |
| `GPIO_PWM1` | 13 | 416 | 37 | `pwm`, `active_level: high` | Level Control, plus Color Control at 2+ channels | **H** |
| `GPIO_PWM1_INV` | 14 | 448 | 46 | `pwm`, `active_level: low` | as above | **H** |

Always **H**, never **A**, and section 4.3 explains why: the template says *how many* PWM channels
and *which pins*, and says nothing at all about which channel is which colour, what the PWM
frequency should be, what the dimming curve is, or where the minimum-brightness cutoff sits.
Channel counts among the pin-fitting corpus **[V]**: 1 channel 70, 2 channels 131, 3 channels 22,
4 channels 61, **5 channels 193**. The five-channel case is an RGB+CW+WW bulb and is the single
most common shape in the fitting set.

#### Sensors that realistically appear

| Tasmota code | Ordinal | Packed (#1) | Legacy | Matter mapping | Outcome |
| --- | --- | --- | --- | --- | --- |
| `GPIO_DSB` (DS18x20) | 41 | 1312 | 4 | Temperature Measurement | **H** (needs a 1-Wire driver and a firmware variant that links the cluster) |
| `GPIO_DHT11` | 37 | 1184 | 1 | Temperature + Relative Humidity Measurement | **H** |
| `GPIO_DHT22` / AM2301 | 38 | 1216 | 2 | as above | **H** |
| `GPIO_SI7021` | 39 | 1248 | 3 | as above | **H** |
| `GPIO_CNTR1` | 11 | 352 | 42 | pulse counter. No natural Matter cluster | **H** |
| `GPIO_CNTR1_NP` | 12 | 384 | 94 | as above | **H** |
| `GPIO_I2C_SCL` | 19 | 608 | 5 | depends entirely on what is on the bus, which the template does not say | **H** |
| `GPIO_I2C_SDA` | 20 | 640 | 6 | as above | **H** |
| `GPIO_ADC_INPUT` | 147 | 4704 | via FLAG | analogue input | **X** for us: TOUT is not brought out |
| `GPIO_ADC_TEMP` | 148 | 4736 | via FLAG | NTC temperature | **X**, same reason |
| `GPIO_ADC_LIGHT` | 149 | 4768 | via FLAG | LDR | **X**, same reason |
| `GPIO_ADC_BUTTON` | 150 | 4800 | via FLAG | resistor-ladder buttons | **X**, same reason |
| `GPIO_ADC_RANGE` | 152 | 4864 | via FLAG | scaled analogue | **X**, same reason |
| `GPIO_ADC_CT_POWER` | 153 | 4896 | via FLAG | current transformer | **X**, same reason |

The ADC family is marked **X**, and the reason is the TYWE2L footprint rather than anything about
our replacement board. A template that asks for ADC0 is asking for the ESP8285's TOUT pin, and the
TYWE2L does not break it out. TOUT is the ESP8266EX's *only* analogue input, it is a dedicated
input-only pin rather than a GPIO, and the module datasheet's feature list says plainly "Peripherals:
five GPIOs". So **no TYWE2L device has ever presented an analogue signal on a leg**, and there is
nothing here for a converter to lose.

Do not expect the replacement board to rescue these. Under the v2 mapping the five host lines are H2
GPIO13, GPIO14, GPIO12, GPIO10 and GPIO11, **none of which has an ADC channel**. ADC1 is the only
unit on the part and its five channels are GPIO1 to GPIO5, which are now spare pads. Putting an ADC
on a host-facing leg was the mistake v1 made: an ADC input whose net is driven by a host output stage
is not an ADC input, in either direction. Full argument in
[`docs/pin-mapping-v2.md`](./pin-mapping-v2.md). If a future board surfaces an analogue input on a
*spare* pad, these codes still do not become **H**, because the host has no way to feed it.

#### Energy metering

| Tasmota code | Ordinal | Packed (#1) | Legacy | Notes | Outcome |
| --- | --- | --- | --- | --- | --- |
| `GPIO_NRG_SEL` (HLW8012 SEL) | 81 | 2592 | 130 | needs a driver plus per-device calibration constants that live in Tasmota settings, not the template | **X** phase 1 |
| `GPIO_NRG_SEL_INV` | 82 | 2624 | 131 | as above | **X** phase 1 |
| `GPIO_NRG_CF1` | 83 | 2656 | 132 | as above | **X** phase 1 |
| `GPIO_HJL_CF` (BL0937) | 85 | 2720 | 134 | as above | **X** phase 1 |
| `GPIO_CSE7766_TX` / `_RX` | 96 / 97 | 3072 / 3104 | 145 / 146 | serial, and on GPIO1/GPIO3 in practice | **X** |

Only **one** pin-fitting template uses `GPIO_HJL_CF` **[V]**, so this costs us almost nothing at
our pin budget. It matters much more if the board ever grows.

#### Cannot support, and the reason is architectural

| Tasmota code | Ordinal | Packed (#1) | Legacy | Why not | Outcome |
| --- | --- | --- | --- | --- | --- |
| `GPIO_TUYA_TX` | 71 | 2272 | 107 | TuyaMCU. Different architecture entirely, section 4.1 | **X** |
| `GPIO_TUYA_RX` | 72 | 2304 | 108 | as above | **X** |
| `GPIO_SBR_TX` | 56 | 1792 | 71 | Serial bridge. We are not a serial bridge | **X** |
| `GPIO_SBR_RX` | 57 | 1824 | 72 | as above | **X** |
| `GPIO_TXD` / `GPIO_RXD` | 100 / 101 | 3200 / 3232 | 148 / 149 | raw serial | **X** |
| `GPIO_SM16716_CLK` / `_DAT` / `_SEL` | 91 / 92 / 93 | 2912 / 2944 / 2976 | 140 / 141 / 142 | bit-banged LED driver protocol; 42 pin-fitting templates | **X** |
| `GPIO_SM2135_CLK` / `_DAT` | 126 / 127 | 4032 / 4064 | 180 / 181 | as above; 38 templates | **X** |
| `GPIO_DI` / `GPIO_DCKI` (MY92x1) | 94 / 95 | 3008 / 3040 | 143 / 144 | as above; 5 templates | **X** |
| `GPIO_SHIFT595_*` | 239-242 | 7648-7744 | not in legacy | 74HC595 shift register outputs; 1 template | **X** |
| `GPIO_WS2812` | 43 | 1376 | 7 | addressable strip; different light model and strict timing | **X** phase 1 |
| `GPIO_IRSEND` | 33 | 1056 | 8 | IR blaster; a hub, not an endpoint; 30 templates | **X** |
| `GPIO_IRRECV` | 34 | 1088 | 51 | as above; 34 templates | **X** |
| `GPIO_RFSEND` / `GPIO_RFRECV` | 35 / 36 | 1120 / 1152 | 105 / 106 | 433 MHz bridge; same argument | **X** |
| `GPIO_BL6523_TX` / `_RX` | 250 / 251 | 8000 / 8032 | not in legacy | serial energy meter; 1 template | **X** |
| `GPIO_BOILER_OT_RX` / `_TX` | 154 / 155 | 4928 / 4960 | 204 / 205 | OpenTherm boiler; 1 template | **X** |
| `GPIO_SPI_*`, `GPIO_SSPI_*` | 21-30 | 672-960 | various | displays and peripherals we do not model | **X** |
| everything else in the 358 | | | | not seen on five pads, and not in scope | **X** |

The three serial LED-driver families are worth calling out together, because they are **85 of the
124 "cannot support" pin-fitting templates** **[V]**. They are all cheap Tuya bulbs where the ESP
does not drive the LEDs directly at all, it clocks a two-wire protocol into a constant-current
driver chip. Supporting them is a real firmware project (three protocols, per-chip current
registers, gamma) and buys us bulbs, which are the least compelling retrofit target.

### 3.3 The Button versus Switch distinction, and why one is A and the other is H

Tasmota `KEY` is a momentary pushbutton; `SWT` is a maintained switch, typically a wall toggle or
rocker. Both are digital inputs, and our profile records them identically apart from the role name.

The difference is behavioural and it is **not in the template**. Tasmota's `SwitchMode` command
selects among more than a dozen behaviours for a `SWT`: toggle on any change, follow the switch
state, follow inverted, push-button emulation, push-button with hold, and so on. The default is
`SwitchMode 0` (toggle). A retrofit where the installer wired a two-way wall switch behaves
completely differently under "toggle" and under "follow".

Buttons are safe to auto-convert because Tasmota's default button behaviour is nearly universal
(short press toggles the bound relay, long press does the device reset) and because a wrong guess
is annoying rather than dangerous. Switches are not, because "follow" versus "toggle" changes
whether the load is on after a power cut.

**Rule: `KEY` codes auto-convert; `SWT` codes require the importer to pick a switch mode, with
"toggle" offered as the default.**

### 3.4 Outcome, counted

Applying section 3.2 to the 902 pin-fitting templates **[V]**:

| Outcome | Templates | Distinct pin maps |
| --- | --- | --- |
| **Clean auto-convert** | **264** | **134** |
| Needs a human decision | 481 | 78 |
| Cannot support | 124 | 41 |
| Empty (no pins assigned at all) | 33 | 1 |
| **Total fitting** | **902** | 254 |

Adding the `BASE == 18` rule from section 1.5, which we should **[V]**:

| Outcome | Templates | Distinct pin maps |
| --- | --- | --- |
| **Clean auto-convert** | **235** | **124** |
| Needs a human decision | 440 | 74 |
| Cannot support | 90 | 34 |
| Empty | 9 | 1 |
| **Total fitting and `BASE` 18** | **774** | |

The clean set splits 171 legacy-encoded and 93 modern-encoded **[V]**, so **the old corpus supplies
nearly two thirds of the value.** A converter that only reads `template9` gets a third of the
available profiles.

By blakadder category, for the fitting-and-`BASE`-18 clean set **[V]**:

| Category | Clean auto-convert |
| --- | --- |
| plug | 168 |
| relay | 27 |
| switch | 24 |
| light | 5 |
| misc | 4 |
| unlabelled | 2 |
| diy | 2 |
| cover, bulb, preflashed | 1 each |

And for the full fitting set of 902, by category and outcome **[V]**:

| Category | Fitting | Clean | Human | Cannot | Empty |
| --- | --- | --- | --- | --- | --- |
| bulb | 447 | 1 | 364 | 82 | 0 |
| plug | 186 | 184 | 1 | 1 | 0 |
| light | 128 | 5 | 110 | 8 | 5 |
| misc | 47 | 4 | 2 | 30 | 11 |
| switch | 37 | 33 | 0 | 1 | 3 |
| relay | 31 | 29 | 0 | 1 | 1 |
| diy | 13 | 2 | 1 | 1 | 9 |
| unlabelled | 5 | 3 | 2 | 0 | 0 |
| cover | 3 | 2 | 0 | 0 | 1 |
| sensor, sensors | 3 | 0 | 1 | 0 | 2 |
| other, preflashed | 2 | 1 | 0 | 0 | 1 |

**Read the plug row and the bulb row together.** 184 of 186 pin-fitting plugs convert cleanly, and
1 of 447 pin-fitting bulbs does. The import is a plug-and-switch story, and the bulbs are a
separate project with its own firmware work.

The shapes in the clean set are extremely repetitive **[V]**, which is what makes review cheap.
For the 235 clean, `BASE` 18 records:

| Shape (relays, buttons, switches, LEDs) | Templates |
| --- | --- |
| 1, 1, 0, 1 | 104 |
| 1, 1, 0, 2 | 73 |
| 2, 1, 0, 2 | 11 |
| 2, 1, 0, 1 | 8 |
| 1, 0, 1, 1 | 8 |
| 1, 1, 0, 0 | 6 |
| 3, 1, 0, 1 | 3 |
| 1, 0, 0, 0 | 3 |
| all others | 19 |

And the single most common pin map, appearing 16 times **[V]**:

```
GPIO12 = REL1#1    GPIO13 = KEY1#1    GPIO4 = LED1_INV#1
```

which on our board becomes H2 GPIO14 relay (pad 2), H2 GPIO12 button (pad 3), H2 GPIO11 active-low
LED (pad 5), under the v2 mapping in section 1.2.

---

## 4. What does not survive the trip

This is the part worth arguing with.

### 4.1 TuyaMCU and the serial-bridge devices are a different machine

A TuyaMCU device is not an ESP with peripherals on GPIOs. It is an ESP acting as a Wi-Fi modem for
a separate microcontroller that owns every input and output. Tasmota talks to it over a UART at
9600 baud using Tuya's serial protocol, and the device's functions are **datapoints** discovered
at runtime, mapped to Tasmota relays and dimmers with `TuyaMCU <fn>,<dpid>` commands that live in
settings and, occasionally, in the template's `CMND` field.

Nothing about that survives. Three independent reasons:

1. **The UART is on GPIO1 and GPIO3.** Neither is brought out on a TYWE2L. Even if we wanted to,
   the pads are not there.
2. **The datapoint map is not in the template.** It is in `CMND` when we are lucky and in a forum
   post when we are not. Two devices with an identical template can have entirely different
   datapoint semantics.
3. **The replacement MCU would still be there.** Swapping the radio module does not change the
   fact that the device's brain is a separate chip speaking Tuya's protocol. A Matter-over-Thread
   TuyaMCU bridge is a legitimate product, and it is a different product from this one.

In the corpus: 159 templates declare `BASE: 54`, 124 of them also place `GPIO_TUYA_TX`/`_RX`
explicitly, and 6 more place the Tuya pins without `BASE: 54` **[V]**. `GPIO_SBR_TX`/`_RX` (serial
bridge) appears on 3, `GPIO_CSE7766_RX` on 34 **[V]**.

**The dangerous subset is the 26 pin-fitting `BASE: 54` templates with empty GPIO arrays.** They
look like the *easiest* possible import and are in fact impossible. Any filter that does not
inspect `BASE` will happily emit 26 profiles for devices that will never work.

### 4.2 Devices whose function needs pins we do not have

1,637 of the 2,539 ESP8266 templates need at least one pin outside our five **[V]**. Counted over
the non-fitting templates only, they need GPIO0 (705, usually the button on ESP-12 style modules),
GPIO3 (644) and GPIO1 (431), which together are the UART, GPIO15 (572), GPIO16 (339), GPIO2 (336),
ADC0 (202), GPIO9 (70) and GPIO10 (41) **[V]**.

This is not a conversion failure, it is a hardware fact, and the tool should say so in those
words. "This device uses GPIO0 and GPIO16, which the TYWE2L does not bring out; it is almost
certainly built on a TYWE3S or an ESP-12 module and is out of scope for this board" is a much
better message than "unsupported".

### 4.3 PWM state lives in settings, not in the template

The template gives us pin positions and channel indices. Everything that makes a dimmer usable is
elsewhere:

- **`PWMFrequency`**, a single global setting. Wrong value gives audible whine or visible flicker,
  and on a mains triac dimmer it can give worse than that.
- **Dimmer curve and gamma.** `SetOption37` (colour remap), `SetOption68` (per-channel PWM),
  `SetOption73`, `Fade`, `Speed`. All settings.
- **`DimmerRange <min>,<max>`.** The minimum-brightness cutoff below which the load buzzes or
  drops out. Highly device-specific and completely absent from the template.
- **Channel semantics.** For a 3-channel template we know there are three PWM channels. We do not
  know that channel 1 is red. Tasmota infers it from the *count* plus the light type, and the light
  type is set by `SetOption*` and by the base module, not by the template. For 5 channels the
  RGB-versus-CW/WW split is a convention, not a declaration.
- **Colour temperature range.** A CCT bulb needs the mireds at each end. Not present.

Some templates put some of this in `CMND`. Most do not. **Every PWM import is a draft that a human
has to finish**, which is why 439 of the 748 land in the "needs a decision" bucket.

### 4.4 Multi-channel light types

Related but distinct. Tasmota's light subsystem has its own type model (`LT_BASIC`, `LT_PWM2` to
`LT_PWM5`, `LT_RGB`, `LT_RGBW`, `LT_RGBWC`) chosen from the channel count and options. Matter's
model is different: Level Control for brightness, Color Control with either the
ColorTemperature feature, the HueSaturation feature, or both, and a device type
(Dimmable Light 0x0101, Colour Temperature Light 0x010C, Extended Colour Light 0x010D) that fixes
which is mandatory.

The mapping from "5 PWM channels" to "Extended Colour Light" is a guess, not a translation. It is
also the *expensive* guess in terms of our firmware, because
[`flasher-and-registry.md` §3.1](./flasher-and-registry.md) establishes that the cluster
vocabulary is compile-time. Getting the light type wrong does not mean a wrong colour, it means the
profile asks for a cluster the flashed variant does not contain.

### 4.5 Wi-Fi-era assumptions baked into the whole format

Less concrete than the rest, and worth stating anyway:

- **A link LED means something different.** `GPIO_LEDLNK` shows Wi-Fi association and MQTT
  connection. On Thread the states are different (no network, joining, attached, SED sleeping), and
  a device that sleeps cannot blink an LED without defeating the point. Import it as
  `status_led` and let the firmware decide the vocabulary.
- **`GPIO_LED1` semantics assume "always on and connected".** Tasmota's LED mirrors relay state
  continuously. That is fine for a mains plug and wrong for anything that will be a Matter
  intermittently-connected device.
- **Templates assume commissioning is a captive portal.** Ours is a QR code and a passcode
  generated at flash time. Nothing in the template helps and nothing in it hurts, but the `CMND`
  backlog frequently contains Wi-Fi, MQTT and time-zone commands that we must strip rather than
  attempt to interpret.
- **No power-loss behaviour.** Tasmota's `PowerOnState` is a setting. Matter has `StartUpOnOff` on
  the On/Off cluster. The template says nothing, and the safe default (off) is not always what the
  device shipped with.

### 4.6 A template names a device, not a module

This is the one that decides how much trust an import can carry, and the numbers are worse than
the earlier pass suggested.

blakadder does have a machine-readable module field. It is called `chip:`, and despite the name it
carries the Tuya module part number. It is present on **366 of 2,871 files**, 12.8% **[V]**. The
values **[V]**:

| `chip:` value | Files |
| --- | --- |
| TYWE3S | 61 |
| TYWE2S | 51 (plus 4 lower-case) |
| WB2S | 31 |
| WB3S | 28 |
| PSF-B | 13 |
| CB3S | 12 |
| WBR3, CB2S | 11 each |
| TYWE1S | 6 |
| **TYWE2L** | **4** |
| others | the tail |

**Four.** In a 2,871-entry corpus, four entries positively identify our module: HeyLight
Plafoniera 30W CCT, BrilliantSmart 12W Salisbury CCT, LEDLite CCT 10W Fire Rated, and Nedis RGBCCT
1200lm. A fifth, Mirabella Genio 9W Dimmable, says so in prose **[V]**. All five are CCT or RGBCCT
lights. Not one is a plug or a switch, which is exactly the category our clean-convert set is made
of.

Read that carefully, because it cuts both ways. It does **not** mean only five devices contain a
TYWE2L; it means the corpus almost never records the module, so the field is useless as a filter.
And it means the 235 clean-convert plugs and switches are pin-*compatible* with a TYWE2L retrofit
and are, in most cases, probably not TYWE2L devices at all. They are more likely TYWE3S or
ESP-12 based devices that happen to use only five of their pins.

**What that means for the product.** Our board replaces a TYWE2L footprint. A device that
internally has a TYWE3S has a different footprint and a different pinout, so the imported pin map
is right about *function* and useless about *placement*. The import gives us the archetype (one
relay, one button, one active-low LED) and the confidence that the device is a simple GPIO device
rather than a TuyaMCU one. It cannot tell us where the pads are. That is a photograph, an FCC ID
lookup, or a person with a screwdriver.

**And no other corpus fills the gap, which is the part worth internalising.** Two databases do
carry machine-readable module identity next to real pin data: OpenBeken (869 of 889 entries have a
`board` field, 712 have pins) and tuya-cloudcutter (484 of 777 devices have the module named, 433
have pins, all lifted from the device's own OEM flash) **[V]**. Both are excellent. Both are
**entirely Beken and Realtek**: CBU, WB3S, WB2S, CB2S, WB2L. OpenBeken's 37 ESP-family entries have
**zero** pin maps **[V]**, and cloudcutter's chip list contains no ESP part at all **[V]**.

The pattern is structural rather than accidental. The ESP-era Tuya devices were documented in
2019 to 2021 by people flashing them over the air with tuya-convert, who recorded the pinout
because that was what Tasmota needed and did not record the module because nothing asked them to.
The Beken-era devices were documented later by people extracting the manufacturer's own
`user_param_key` blob, which contains both. **There is no ESP-era equivalent, and one is not going
to appear.** For TYWE2L work, module identification is a photograph.

The tool should therefore emit the pin map **plus an explicit warning that the module is
unidentified**, and the registry should never promote such a profile past `community`.

### 4.7 Things that look like they transfer and do not

- **The `NAME` field.** It is the *template* name, not the product name. `2ch_smart_switch` has
  `"NAME":"Generic"` **[V]**. Use blakadder's `title` and `model` front matter instead.
- **`GPIO_LED1` index versus relay index.** Tasmota's LED-follows-relay pairing is by index, and
  the index is in the low five bits. `LED1#2` follows relay 2. A converter that drops the index
  will wire the wrong LED to the wrong relay on the 9 templates with two relays and two LEDs.
- **`CMND`.** Occasionally carries the one setting that makes the device work
  (`Pixels 25`, `SetOption`, `TuyaMCU 11,1`). Most of the time it carries Wi-Fi credentials-era
  junk. Parse it, surface it to the human reviewer as free text, never execute or auto-interpret
  it.
- **blakadder `category`.** Useful, and inconsistently cased: `switch` and `Switch`, `misc` and
  `Misc`, `sensor` and `sensors`, `bulb` and `Bulb` all appear as distinct values **[V]**.
  Normalise on import.

---

## 5. The safety problem

An imported pin map that is wrong energises a relay in someone's mains appliance. Three layers,
and all three are needed.

### 5.1 What the converter can check automatically

Every one of these is cheap and every one of these has already caught something in the corpus:

| Check | Fails on |
| --- | --- |
| Front matter parses, template field parses as JSON | 24 corpus files **[V]** |
| Encoding detection matches Tasmota's own test exactly (13 elements and all values < 256) | any ambiguity |
| Array length matches the declared platform | ESP32 templates mislabelled as `template9`: 13 in the corpus, with array lengths 22, 36, 38 and 48 **[V]** |
| Every assigned pin is in {4, 5, 12, 13, 14} | 1,637 templates **[V]** |
| Legacy `FLAG & 0x0F` is zero | 178 legacy templates use ADC0 **[V]** |
| `BASE` is 18 | 128 pin-fitting templates **[V]**, of which 32 are TuyaMCU |
| Every code is in the section 3.2 allow-list | 124 pin-fitting templates **[V]** |
| No two pins carry the same function and index | malformed contributions |
| Relay indices are contiguous from 1, likewise buttons and PWM channels | gaps that indicate a mis-transcription |
| Every `LED#n` has a matching `REL#n`, or is demoted to a plain indicator | mis-indexed LEDs |
| At least one output role exists | the 33 empty templates **[V]** |
| The requested cluster set exists in some firmware variant | profiles that cannot be flashed |
| **Cross-check: if the file has both `template` and `template9`, they must agree after conversion** | **38 of 64 files [V]** |
| **Cross-check: if another corpus entry carries the same template `NAME`, the pin maps must agree** | **78 names [V]** |
| Cross-check against OpenBeken's `board` field where an entry exists | module identity |

The last three are the valuable ones, because they turn the corpus's internal contradictions into
review flags rather than silent errors.

### 5.2 What a human must confirm, and it is a short list

Because the shapes are so repetitive (section 3.4), review is not 235 separate investigations. It
is 124 distinct pin maps, most of which are one of eight shapes. For each candidate profile a
reviewer confirms:

1. **The module.** A photograph of the board with the module part number legible, or an FCC ID, or
   a teardown link. Without this the profile stays `community` forever.
2. **Relay polarity.** The one that energises mains if it is wrong. `REL1` versus `REL1_INV`.
3. **Which pad is which.** A TYWE2L pad number for each function, confirmed against the physical
   board. An ESP8285 GPIO number on its own is not enough.
4. **Button versus switch**, and for a switch, the mode (section 3.3).
5. **For PWM devices**, the channel-to-colour assignment, frequency and dimmer range (section 4.3).
6. **The Matter device type**, because it decides which firmware variant the user must flash.

Points 1 to 3 are one photograph and five minutes. Points 4 to 6 need someone who has the device
powered up.

### 5.3 Provenance and confidence, recorded per profile

Extend the registry schema described in
[`flasher-and-registry.md` §4](./flasher-and-registry.md) with two objects. The two trust tiers
there stay as they are; `imported` is a state *within* `community`, not a third tier.

```json
"provenance": {
  "source": "blakadder",
  "source_id": "gosund_SP1_v23",
  "source_url": "https://templates.blakadder.com/gosund_SP1_v23.html",
  "source_commit": "30d9c9b16014817641f83a0cf8f2f266a0dbaef2",
  "source_field": "template9",
  "source_encoding": "tasmota-packed-esp8266",
  "source_template": "{\"NAME\":\"Gosund SP1 v23\",\"GPIO\":[...],\"FLAG\":0,\"BASE\":18}",
  "retrieved": "2026-08-13",
  "converter_version": "0.1.0",
  "decode_tables_from": "arendst/Tasmota@<sha>",
  "source_licence": "EPL-2.0",
  "derivation": "transcribed-gpio-only",
  "prose_copied": false,
  "independently_rederived": false,
  "licence_note": "Pin assignments are factual; see docs/tasmota-import.md section 6"
},
"confidence": {
  "state": "imported",
  "pin_map": "derived",
  "module_identified": false,
  "module_evidence": null,
  "corpus_agreement": "single-source",
  "hardware_verified_by": null,
  "hardware_verified_on": null,
  "open_questions": ["switch mode not specified", "module not identified"]
}
```

`confidence.state` moves `imported` to `reviewed` when a human has answered every open question,
and to `verified` only when someone has flashed a real device
([`flasher-and-registry.md` §4](./flasher-and-registry.md)'s existing tier). `corpus_agreement`
takes `single-source`, `corroborated` or **`conflicting`**, and a `conflicting` profile must never
be published, because we measured that 38 of 64 second opinions disagree.

The whole `source_template` string is kept verbatim. It costs a hundred bytes and it means any
future correction to our decode tables can be replayed over every profile without going back to
the network.

### 5.4 What the firmware must do defensively

Five requirements, in priority order.

1. **Pins stay inputs until the profile is validated.** At reset, configure all five host GPIOs as
   inputs with the internal pull-down enabled, and do not change that until the profile blob's
   magic, schema version, length and CRC32 have all passed
   ([`flasher-and-registry.md` §3.2](./flasher-and-registry.md) defines the blob). A relay driver
   that sees a floating gate for 200 ms is a relay that may chatter.
2. **Never latch an output during boot on an unverified profile.** Matter's `StartUpOnOff`
   defaults to off, and our firmware should force off, not "restore last", until
   `confidence.state` is `verified`. Carry the confidence state into the blob so the firmware can
   see it.
3. **A failed profile boots into recovery, not into a guess.** No profile, bad CRC, unknown schema
   version, or a requested cluster the variant does not contain: all five pins stay inputs, no
   endpoint composition happens beyond a bare Identify, and the status LED (if the *previous* good
   profile named one) blinks a fault code. The device must be reflashable at that point over native
   USB, which it is
   ([`flasher-and-registry.md` §1.5](./flasher-and-registry.md)).
4. **A physical route back.** Hold the first button for 10 seconds to erase the profile partition
   and drop into recovery. This is the escape hatch for "I flashed the wrong profile and now the
   relay is stuck on", and it must not depend on the profile being correct, so bind it to whichever
   pin the profile names as button 0 *and* accept it on any of the five if the profile is invalid.
5. **Do not burn eFuses, ever.** Already stated in
   [`flasher-and-registry.md` §1.7](./flasher-and-registry.md) and it belongs here too: the
   recovery path in point 3 depends on USB download mode surviving.

**Three hardware notes that fall out of this, all of them consequences of the v2 pin mapping.**

Our five pads map to H2 **GPIO13, GPIO14, GPIO12, GPIO10 and GPIO11** (legs 1 to 5, per section
1.2). That mapping is adopted and implemented, and
[`docs/pin-mapping-v2.md`](./pin-mapping-v2.md) is the authority. It replaces the GPIO1 to GPIO5
mapping earlier drafts assumed.

1. **Requirement 1 is achievable because nothing drives a host line before firmware does.** All five
   carry reset code `0` in TRM v1.1 Table 6.13-1: input disabled, no pull. None is a strapping pin,
   so no reset latch reads them. None is a JTAG pad, so even the pad-JTAG failure case cannot drive
   one. And none is GPIO4, whose MTCK after-reset pull-up holds a line towards 3V3 for 200 to 400 ms
   and would defeat requirement 1 outright on a relay driver. That last point is why GPIO4 is vetoed
   rather than merely avoided. Do not burn `EFUSE_STRAP_JTAG_SEL_ENABLE` or `EFUSE_DIS_USB_JTAG`;
   under v2 that is a functional concern rather than a contention path, which is an improvement, not
   a licence.
2. **Nothing in a converted profile can ever be analogue, so the parser must reject the idea
   outright rather than degrade it.** No host line has an ADC channel under v2, and more
   fundamentally no TYWE2L leg can carry an analogue signal at all, because the ESP8266EX's only
   analogue input is TOUT and the module does not break it out (section 3.2). A profile requesting
   an analogue role on any pad is malformed, not merely unsupported.
3. **Watch the crossed numbering in every defensive check you write.** ESP8285 GPIO12 maps to H2
   GPIO14 and ESP8285 GPIO13 maps to H2 GPIO12. A validator that compares the two namespaces by
   value will pass a wrong pin map. Validate on `pad`, which is unambiguous and equals the leg
   number, and derive the H2 number from the single table in section 1.2.

**A fourth note, about what the profiles will actually contain.** The community device corpora show
the TYWE2L used exclusively as a multi-channel PWM light module: `blakadder/templates` files it in
the `dt-light` module group, and every TYWE2L device in both `blakadder/templates` and
`esphome/esphome-devices` is a light, with every leg used being a PWM channel. The pattern is two
channels on ESP8285 GPIO12 and GPIO14 for CCT, or all five for RGBCCT
([`pin-mapping-v2.md`](./pin-mapping-v2.md) section 2.7). That is a lower bound rather than a census,
since the `chip:` field is only populated when a contributor filled it in, so it does not licence
dropping the relay and button paths. What it does say is where the importer's effort belongs:
**`GPIO_PWM1` and its inverted and multi-channel variants are the codes that must be flawless**, and
a PWM channel driving a MOSFET gate is the failure topology to design the defensive checks around.

### 5.5 The review workflow

Copy the structure that
[`flasher-and-registry.md` §4.1](./flasher-and-registry.md) recommends from
`devices.esphome.io`, with the untrusted work in a secret-free `pull_request` context and the
privileged commenting in a separate `workflow_run` job.

1. The converter runs offline and opens **one pull request per batch**, not per device, with the
   full JSON diff and a generated summary table.
2. CI runs the section 5.1 checks and posts the results as a comment. Anything flagged
   `conflicting` blocks the merge.
3. A maintainer reviews the batch. Because the shapes repeat, a batch of 40 single-relay plugs is
   one decision plus 40 spot checks, not 40 decisions.
4. Merged profiles land as `confidence.state: "imported"` and the web installer shows them with a
   plain-language warning: "This pin map was imported from a community database and has not been
   confirmed on real hardware. Check the module part number on your board before flashing."
5. Promotion to `verified` needs a human who flashed one, per the existing tier.

**Never auto-merge.** The measured 59% disagreement rate on second opinions is the argument, and
it is not an argument anyone can talk you out of.

---

## 6. Licence and attribution

**I am not a lawyer and this is not legal advice.** What follows is a lay reading with citations,
and where I am reasoning rather than citing I say so. Get an Australian IP lawyer to sign off
before a large-scale import, and brief them specifically on the EU database question in 6.2, which
is the part most likely to decide it and the part least resolvable from public sources.

### 6.1 Correcting the premise

**Tasmota is GPL-3.0, not AGPL-3.0.** `LICENSE.txt` at HEAD is the GNU General Public License
version 3, the README states "This program is licensed under GPL-3.0-only", and the GitHub API
reports `license.spdx_id` as `GPL-3.0` on both `master` and `development` **[V]**. The only
"Affero" strings in the licence file are the standard GPLv3 section 13 compatibility clause
**[V]**. There is a small internal inconsistency worth knowing: the README says GPL-3.0-**only**
while the file headers say "version 3, or (at your option) any later version" **[V]**. Any document
repeating "Tasmota is AGPL" should be corrected, this project's brief included.

**blakadder's template database is EPL-2.0.** `LICENSE.md` at HEAD is 14,199 bytes and
byte-identical to the canonical Eclipse Public License 2.0 text bar a trailing newline; the GitHub
API agrees **[V]**. Contributions arrive through a Google form with no CLA and no rights assignment
**[V]**.

Two facts about how that licence is actually presented, both of which matter:

- **The licence is invisible on the website.** `_config.yml` lists `LICENSE.md` under `exclude:`,
  so `/LICENSE`, `/LICENSE.md` and `/license.html` all 404 on `templates.blakadder.com` **[V]**.
  There is no footer copyright, no terms of use, and no licence notice on any device page **[V]**.
  A consumer of the website, as opposed to the repository, is given nothing to agree to. That is
  helpful under *Ryanair* (see 6.2) and it is not something to rely on, because it could change.
- **blakadder is based in Croatia**, i.e. in the EU **[V]** (GitHub profile). That is the single
  fact that makes the sui generis database right worth taking seriously rather than waving at.

### 6.2 Is a pin map copyrightable?

Almost certainly not, individually. A string like
`{"NAME":"Sonoff Basic","GPIO":[17,255,255,255,0,0,0,0,21,56,0,0,0],"FLAG":0,"BASE":1}` is a
factual statement about how a physical object is wired, discovered rather than authored, and there
is exactly one way to say "relay on GPIO12" because Tasmota defines the encoding.

**United States.** [*Feist Publications v Rural Telephone*, 499 U.S. 340
(1991)](https://www.law.cornell.edu/supremecourt/text/499/340): "no author may copyright his ideas
or the facts he narrates", and sweat-of-the-brow is expressly rejected. Merger runs through
[*Baker v Selden*, 101 U.S. 99 (1880)](https://www.law.cornell.edu/supremecourt/text/101/99), where
methods and the diagrams necessary to practise them are "necessary incidents to the art, and given
therewith to the public".

**Australia**, which is where this project sits. AustLII was unreachable from this environment, so
the primary source used is the High Court's own
[judgment summary in *IceTV v Nine Network*](https://www.hcourt.gov.au/assets/publications/judgment-summaries/2009/hca14-2009-04-22.pdf)
**[V]**:

> expression of the time at which a program is shown can only practically be done by using words or
> figures based on either a 12 or 24 hour time cycle for a day. Thus there was little originality in
> the expression of time and title information.

That is merger in Australian dress and it transfers cleanly to a GPIO number. **Australia has no
sui generis database right.**

**The sting in the Australian position, and it is worth sitting with.** The reasoning in
*Telstra v Phone Directories* [2010] FCAFC 149, that a substantially computer-generated compilation
lacks an identifiable human author, applies to **our** registry too. We would struggle to claim
Australian copyright in our own compiled corpus. That is perfectly comfortable for a CC0 project,
and it is a good reason not to argue in a way that assumes we hold rights we are simultaneously
saying nobody else holds. **[U]** The *Telstra* characterisation above is from recollection and
should be checked against the primary text before it is relied on.

**European Union: the real exposure, and it is about the collection.**
[Directive 96/9/EC](https://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:31996L0009)
Article 7(1) protects substantial investment in obtaining, verifying **or presenting** the
contents; Article 7(5) catches repeated and systematic extraction of insubstantial parts, so
record-by-record scraping is no escape; Article 7(4) applies irrespective of copyright in the
contents.

The spin-off doctrine probably defeats it.
[*British Horseracing Board v William Hill*, C-203/02](https://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:62002CJ0203)
holds that "obtaining" means resources used to seek out **existing independent** materials and
collect them, and does not cover resources used to **create** the materials. A contributor who
opens a plug and probes the board is creating the datum, not obtaining it. The counter-argument is
that once a template is submitted, blakadder is obtaining and verifying an existing material, and
Article 7(1) does expressly count presentation. **This is genuinely open. There is no CJEU
authority on a community-submitted technical corpus.**

One warning worth carrying:
[*Ryanair v PR Aviation*, C-30/14](https://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:62014CJ0030)
establishes that contract can bind where intellectual property cannot. blakadder's site currently
has no terms of use (6.1). **Watch for that changing**, and record the date we retrieved the data.
For the UK post-Brexit, [SI 1997/3032 reg 18](https://www.legislation.gov.uk/uksi/1997/3032/regulation/18)
now reads "the United Kingdom" where it read "EEA", so a Croatian maker acquires no new UK database
right.

| | Individual record | The collection |
| --- | --- | --- |
| US copyright | not protected (*Feist*, *Baker*) | thin; selection and arrangement only |
| EU copyright | not protected | only if the arrangement is the author's own intellectual creation |
| EU sui generis | not applicable | **live risk**, probably defeated by spin-off, arguable |
| Australia | not protected (*IceTV*) | very likely not protected; no database right |

**The practical instruction that falls out of the table: never mirror the collection.** Bulk-copying
2,861 records while preserving blakadder's structure and its selection of which devices to include
is the single act most likely to trigger both Article 7(1) and thin compilation copyright. Taking
the `template` field from records we have individually chosen is a materially different act. Our
own taxonomy should be our own.

**[U] Not verified and worth chasing**: *Southco v Kanebridge*, 390 F.3d 276 (3d Cir. 2004) (en
banc), on the copyrightability of **part numbers**, is probably the most on-point US authority
available and could not be retrieved; the operative text of s 32 of the Copyright Act 1968 (Cth);
and *Morrissey v Procter & Gamble*, cited from knowledge only.

### 6.3 Does the GPL reach the template data?

A licence operates only over rights the licensor actually holds. The GPL cannot make a fact
copyrightable. Three acts, which are not equivalent:

1. **Copying the file, or a recognisable slab of it.** A GPL matter, and properly so. The comments
   in `tasmota_template.h` are authored prose: `GPI8_REL1, // GPIO12 Red Led and Relay (0 = Off,
   1 = On)` is somebody's writing, not a fact.
2. **Reading it and recording that a Sonoff Basic has its relay on GPIO12.** Fact extraction. Not a
   GPL matter.
3. **Mechanically transforming the whole `kModules8266` array while preserving Tasmota's ordering
   and its selection of which devices to include.** The risky middle case, and precisely what
   *Feist* says a subsequent compiler may not take. **Avoid this one.**

This is mostly moot for blakadder-sourced data, which is EPL rather than GPL. The GPL only bites on
direct harvesting from `tasmota_template.h`, and then only for expression.

**Recommendation: license the converter tool GPL-3.0-only**, matching Tasmota, keep it in
`tools/tasmota-import/` with a clear notice, and keep the **registry profiles CC0**. That is the
clean split: GPL for the code that derives from GPL code, CC0 for the factual data. Better still,
**generate the tables at build time by parsing a pinned Tasmota checkout rather than checking a
transcribed copy into our tree**. It stays current, and no GPL source ever lands in our repository.

### 6.4 EPL-2.0 and CC0

**Formally, EPL-2.0 does reach a data corpus, awkwardly.** Its "Source Code" definition covers "the
form of a Program preferred for making modifications, including but not limited to software source
code, **documentation source, and configuration files**", which is broad enough to catch the `.md`
template files. Section 3.1(a) requires source availability plus a statement of it, 3.2(b) requires
a copy of the Agreement with each copy, and **3.3 forbids removing attribution notices**, which is
the sharpest live obligation.

**The copyleft largely does not reach derived data, for two independent reasons.** It is file-level:
the trigger is a "Modified Work", meaning a new file containing *contents* of the Program. And
section 2 grants rights *under copyright*, so where there is no copyright in the datum, section 3
has nothing to attach to. A licence cannot reach past the right underneath it.

There is a narrower practical constraint that matters more than the licence text. Each blakadder
file bundles one factual line with substantial original prose and a photograph. **The prose and the
photograph are the parts that are definitely protected.** Take the `template` field. Leave
everything else.

So, can we CC0 a record derived from an EPL-2.0 corpus? Two answers, and both are honest.

1. **If the individual pin map is not copyrightable, there is nothing for EPL-2.0 to attach to.**
   We may dedicate **our own fresh compilation** to the public domain, because our record is our own
   expression of a fact. We are not relicensing blakadder's compilation, and we should say so in
   those words.
2. **We should attribute anyway.** That conclusion chains several individually-probable
   propositions, and chained probabilities get uncomfortable. Attribution costs one line of JSON
   per profile and converts a legal fight into a social one we win.

CC0 is the right instrument for the job, and specifically so: the
[CC0 1.0 legal code](https://creativecommons.org/publicdomain/zero/1.0/legalcode) section 1
expressly waives "database rights (such as those arising under Directive 96/9/EC ...)", with a
Public License Fallback in section 3 where the waiver fails.

There is also a plain-decency argument that outranks the legal one. Thousands of people opened
their appliances and wrote down what they found. Naming them is the right thing to do whether or
not anyone can make us.

### 6.5 What we should actually carry

**Per profile**, the `provenance` object in section 5.3, extended with the fields the legal reading
argues for: `source_licence`, `derivation` (one of `transcribed-gpio-only`,
`independently-derived`, `vendor-datasheet`), `prose_copied` (always `false`), and
`independently_rederived`.

That last field deserves emphasis. **Make re-derivation from hardware a first-class contribution
type.** Every record whose `independently_rederived` flips to `true` defeats every theory in 6.2 at
once, and it is the same act as promoting the profile to `verified`. The legal hygiene and the
safety workflow want exactly the same thing, which is a nice place to be.

**Per-profile attribution text**, where the record was seeded from blakadder:

> Pin assignment data for this device was cross-checked against the Tasmota Device Templates
> Repository (templates.blakadder.com), maintained by blakadder and contributed by the Tasmota
> community. Only factual GPIO assignments were used. No descriptive text or images were copied.
> This record is published under CC0-1.0.

**Repository-wide `NOTICE.md`**, which is the cheapest available insurance against an EPL section
3.3 argument. It should state that individual pin assignments are statements of fact about physical
hardware; that our CC0 dedication covers **this compilation and its structure** and makes no claim
over any upstream collection; and it should name
[Tasmota](https://github.com/arendst/Tasmota) (GPL-3.0) and
[blakadder/templates](https://github.com/blakadder/templates) (EPL-2.0), noting that no source code,
descriptive text or images from either are included.

**In the converter's `--help` and at the top of every report it writes**, the same statement, so it
travels with the data.

**If either project objects.** Respond quickly and in good faith. Ask which records and on what
basis: compilation copyright, sui generis, or EPL notices, because the three have different
answers. Do not argue the merits in public first. Offer stronger attribution and removal pending
review. Prioritise re-deriving the contested records from hardware, which ends the argument
permanently. Escalate before conceding any principle, because conceding that pin maps are
copyrightable would be bad for the whole ecosystem and not only for us.

### 6.6 What the community itself does, and what that is worth

**These projects import from each other, verifiably, and it runs both ways [V].**

- Tasmota's GPL-3.0 tree contains `tools/templates/templates.py`, which fetches
  `templates.blakadder.com/list.json` and writes `TEMPLATES.md`: 2,809 templates, 2,616 distinct
  names, **100% present in blakadder and none Tasmota-only**, 1,174 byte-identical. Tasmota carries
  **no EPL notice**. It does credit blakadder in the README and the file header.
- Tasmota's docs route users to blakadder's submission form, and blakadder's README describes the
  devices as "submitted by the awesome community built around Tasmota and Tuya-Convert".
- OpenBeken is the counter-example, and instructive: an independent corpus built from elektroda.com
  teardowns (802 of its wiki links) plus machine extraction of Tuya `user_param_key` blobs, with
  exactly **one** blakadder URL in the entire repository.

**That is strong evidence of a community norm of free reuse. It is not a legal defence.** Widespread
practice creates no licence and will not estop a rights-holder who never acquiesced. Its real value
is different: it tells us the probability of anyone objecting is low, and that if someone does, the
resolution is far more likely to be social than judicial. Budget caution accordingly, and do not
mistake the norm for permission.

---

## 7. Plan

Deliberately staged so that the first stage produces value even if the later ones are never built.

### Phase 0: the decode tables (half a day)

Write the table extractor: parse `enum UserSelectablePins`, `enum LegacyUserSelectablePins`,
`kGpioConvert`, `enum SupportedModulesESP8266` and `kModuleNames` from a **pinned Tasmota commit**,
and emit JSON. Check the JSON into our repo, check the extractor in, and record the commit SHA.
This is the whole reason the project is cheap; do it first and do it properly.

The one thing to get right, per section 1.3: **evaluate the ESP8266 arm of the enum
specifically**, and assert that none of the 27 ESP32-only names ends up in the ESP8266 table. That
single assertion is worth more than the rest of the tool's validation put together, because it is
the only error in the whole pipeline that produces confident, plausible, wrong output.

Acceptance: the extracted `kGpioConvert` round-trips the three "Old:"/"New:" example pairs in
Tasmota's own `JsonTemplate()` comment, and the ESP8266 table has 358 entries against the ESP32
table's 385.

### Phase 1: the converter, and a report (two to three days)

Build `tools/tasmota-import` to the specification in
[`tools/tasmota-import/README.md`](../tools/tasmota-import/README.md). Run it over a pinned
blakadder commit. Ship **nothing** to the registry yet. The deliverable is the report: which
devices land in which bucket, which conflict, which are TuyaMCU.

Acceptance: the numbers in section 3.4 reproduce.

### Phase 2: seed the clean set (three to four days, mostly review)

Convert the 235 clean-and-`BASE`-18 templates into draft profiles, deduplicated to the 124 distinct
pin maps, opened as batched pull requests grouped by shape. Every profile lands as
`confidence.state: "imported"` with full provenance. The installer shows the warning from
section 5.5.

Do the 168 plugs first. They are the largest, most uniform and most useful group.

### Phase 3: the human-decision set, driven by demand (ongoing)

Do not bulk-convert the 439. Build the converter's interactive mode so that when a user asks for a
specific device, a maintainer can answer the open questions in a few minutes and publish one
profile. Demand will order the queue better than we can.

### Phase 4: revisit the exclusions (only if warranted)

Reasons to reopen, in rough order of value:

- **Energy metering** (HLW8012, BL0937). Unlocks the metering plugs, which are the most-wanted
  category we currently reject, and pairs with Matter's Electrical Power Measurement cluster.
- **Serial LED drivers** (SM16716, SM2135, MY92x1). Unlocks 85 pin-fitting bulbs. Substantial
  firmware work.
- **A wider board.** If a future board surfaces more pads, the addressable corpus roughly triples,
  and every table in this document is written to be re-run rather than rewritten.

### What not to do

- Do not import all 902. 128 of them lie about being simple, and 124 cannot work.
- Do not auto-merge, ever. Section 5.5.
- Do not depend on `templates.blakadder.com/templates.json` (26 defects, does not parse) or on
  `list.json` (Markdown served as `application/json`) **[V]**.
- Do not mirror the corpus wholesale. Section 6.2 explains why the *collection* is the part with
  legal exposure and the individual records are not.
- Do not hand-maintain a decode table. Tasmota's is authoritative and it is one file.
- Do not treat a pin-compatible template as evidence of module identity. It is not. Section 4.6.

---

## Appendix: reproducing the numbers

Everything in this document came from these, on 13 August 2026.

```bash
# Corpus. Sparse clone: the full tarball is ~170 MB, nearly all images.
git clone --depth 1 --filter=blob:none --sparse https://github.com/blakadder/templates
cd templates && git sparse-checkout set _templates _unsupported _data
# HEAD at time of writing: 30d9c9b16014817641f83a0cf8f2f266a0dbaef2
ls _templates | wc -l        # 2871
ls _unsupported | wc -l      # 180

# Front-matter template keys
grep -ho "^template[a-z0-9]*:" _templates/* | sort | uniq -c | sort -rn

# Decode tables, from a pinned Tasmota commit
curl -O https://raw.githubusercontent.com/arendst/Tasmota/master/tasmota/include/tasmota_template.h
curl -O https://raw.githubusercontent.com/arendst/Tasmota/master/tasmota/include/tasmota_template_legacy.h
curl -O https://raw.githubusercontent.com/arendst/Tasmota/master/tasmota/tasmota_support/support.ino

# Legacy enum stability check, v6.6.0 through v8.5.1
for t in v6.6.0 v7.2.0 v8.1.0 v8.3.1 v8.5.1; do
  curl -sO https://raw.githubusercontent.com/arendst/Tasmota/$t/tasmota/tasmota_template.h  # v6.6.0: sonoff/sonoff_template.h
done

# The platform-conditional blocks that make the enum two enums (section 1.3)
sed -n '26,246p' tasmota_template.h | grep -n '^\s*#'

# The published index does not parse
curl -s https://templates.blakadder.com/templates.json | python3 -c "import json,sys; json.load(sys.stdin)"
# json.decoder.JSONDecodeError: Expecting value: line 770 column 21 (char 31582)

# ...and list.json, which Tasmota itself consumes, is Markdown
curl -sI https://templates.blakadder.com/list.json | grep -i content-type   # application/json
curl -s  https://templates.blakadder.com/list.json | head -2                # "# Templates"
```

The classification itself was done with a throwaway Python script in a scratch directory, not
checked in, because the real one belongs in the tool specified at
[`tools/tasmota-import/README.md`](../tools/tasmota-import/README.md) and reproducing these
numbers is that tool's acceptance criterion. Anyone rerunning it against a newer corpus commit
should expect small drift and no change to the conclusions.

**One warning for whoever writes that script.** Parse the enum with a conditional stack, not by
stripping `#` lines. I did the latter first and it produced an internally consistent, confidently
wrong set of function names, because it silently yields the ESP32 numbering (section 1.3). Every
number in this document was recomputed after that was found. If your counts differ from these,
check that first.
