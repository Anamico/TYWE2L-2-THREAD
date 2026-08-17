# Flasher and device-profile registry: research and recommendations

Decision-support document for the TYWE2L-to-ESP32-H2 retrofit project.
Research only. No application code is proposed here.

Date: 13 August 2026.
Scope: how an end user selects their device from a shared registry and flashes an
ESP32-H2-MINI-1-H4S so it behaves correctly in that device.

Every claim below is tagged. **[V]** means I verified it against the actual source,
repository, registry API or datasheet during this research, and the citation points at
the thing I read. **[U]** means unverified or partly verified, and I say why.

---

## Bottom line

Build a **pure web installer**, hosted on GitHub Pages, using **esptool-js over Web Serial**,
with the device-profile registry as a **separate GitHub repository of JSON files validated
in CI** and published as a **prebuilt search index**. Do not build a native desktop app in
phase one. Do not build one in phase two either, unless a specific measured failure forces it.

The recommended stack:

| Layer | Choice | Why |
| --- | --- | --- |
| Transport | Web Serial, over the H2's native USB. Chrome, Edge, Opera, and **Firefox 151+** **[V]** | esptool-js 0.6.1 has a working ESP32-H2 target and stub flasher, and Windows binds `usbser.sys` automatically with no Zadig step **[V]** |
| Flash library | `esptool-js` **pinned `>= 0.6.1`**, directly, not `esp-web-tools` | We must write a generated partition per flash, which the manifest model cannot express. Below 0.5.6, current H2 silicon is not even detected **[V]** |
| Fallback | Espressif's **already-signed and notarised** `esptool` binaries plus a printed command | Covers Safari, locked-down machines and CI, and inherits Espressif's signing spend rather than taking on our own **[V]** |
| Partition generation | `partitions-tool-esp` (TypeScript, browser-capable), vendored. Its NVS output was tested byte-for-byte identical to `esp-idf-nvs-partition-gen` **[V]** | Lets the browser build both the profile image and the Matter factory NVS image with no server |
| Matter credentials | Generated in-browser: discriminator, passcode, SPAKE2+ verifier via WebCrypto PBKDF2 plus P-256 scalar multiply **[V]** | The derivation is fully specified and reimplementable; no server, no secrets to hold |
| Firmware model | Small set of variant binaries, split by **cluster set** rather than device category, each reading a device profile from a dedicated partition at boot | esp-matter builds its data model at runtime, but the linked cluster set stays compile-time **[V]** |
| Profile storage | A raw versioned blob (magic, schema version, length, CRC32) in a **custom-type partition**, not NVS. NVS only for the Matter factory data | We define our own format; custom partition types 0x40-0xFE are reserved for exactly this **[V]** |
| Registry | Own repo, one JSON file per device, JSON Schema 2020-12, validated by GitHub Actions, index built in CI, served via GitHub Pages | Direct copy of the `devices.esphome.io` model, which works **[V]** |
| Search | MiniSearch 7.2.0, prebuilt index loaded with `loadJSON` **[V]** | Zero dependencies, MIT, handles a few thousand records client-side |
| Trust | Two tiers: `community` (schema-valid only) and `verified` (a maintainer confirmed it on real hardware), enforced by CODEOWNERS and shown in the UI | Modelled on the "Made for ESPHome" review workflow **[V]** |

The two decisions that matter most, and why:

**Web, not native.** The native path costs money and calendar time before it flashes a single
device. Windows code signing now requires the private key on certified hardware, and macOS
requires an Apple Developer Program membership plus notarisation on every build. A web installer
skips both. The old objection to web, that it is Chromium-only, is now out of date: **Firefox 151
shipped Web Serial on 19 May 2026** **[V]**, leaving Safari as the only desktop browser that will
never support it. Section 2 gives the numbers.

**Profile as data, not as a firmware build.** ESPHome's model, where each device becomes its own
compiled binary, means running a build farm forever, and ESPHome has **zero Matter support** as of
August 2026 in any case **[V]**. Tasmota's model, where one binary reads a template, is the right
shape. esp-matter supports it properly: it no longer uses the ember and ZAP static tables at all,
and implements CHIP's `DataModel::Provider` over a runtime linked list **[V]**. Section 3 has the
source.

### The five things to check before committing

Two questions that were open in my first pass are now closed, and both came back positive:
**Windows binds `usbser.sys` to the H2 automatically with no Zadig step** (section 1.5), and
**4 MB flash does fit Matter with dual-slot OTA**, at about 80% of a slot (section 3.1).

What remains:

1. **esptool-js must be pinned `>= 0.6.1`.** Below 0.5.6 the library does not recognise current
   ESP32-H2 silicon, because a second chip-detect magic was added only in June 2025. This is the
   highest-risk single line in the dependency list. See section 1.1.
2. **The device re-enumerates during flashing and esptool-js has no reconnect logic.** The web app
   must reacquire the serial port itself, possibly needing a fresh user gesture. Design the state
   machine around this now, because retro-fitting it means rewriting it. See section 1.6.
3. **GPIO8 floats and needs an external pull-up.** The automatic USB reset path bypasses the
   straps, so this only bites on manual recovery, which is exactly when you cannot afford it. A
   BOOT button also needs a strong 10k pull-down. See section 1.5.
4. **Test vendor IDs make Apple and Google show an "uncertified accessory" prompt**, permanently,
   and Google additionally expect Developer Console registration. A real vendor ID starts at
   USD 7,500 per year. See section 6.2.
5. **Changing a device's profile after commissioning changes its Matter data model.** Plan on
   telling users to re-pair, and treat "no re-commissioning needed" as unproven until tested
   against all three ecosystems. See section 5.4.

---

## 1. Flashing transport and tooling

### 1.1 esptool-js and ESP32-H2

**ESP32-H2 is supported, and the minimum usable version is higher than you would guess.** I
downloaded the `espressif/esptool-js` repository and checked the source, and agent research
confirmed the version history.

- Package version **0.6.1** (6 Aug 2026), licence Apache-2.0, runtime dependencies `atob-lite`,
  `pako`, `tslib` **[V]** (`package.json`).
- `src/targets/esp32h2.ts` defines `ESP32H2ROM`, extending `ESP32C6ROM`, with
  `CHIP_NAME = "ESP32-H2"` and `IMAGE_CHIP_ID = 16` **[V]**.
- A compiled stub flasher ships: `src/targets/stub_flasher/stub_flasher_32h2.json` **[V]**.
- H2 support landed in **v0.3.0, 30 June 2023** (PR #102) **[V]**.

**The version trap, and it is the highest-risk pin in the whole stack.** Version **0.5.6, 16 June
2025** added a second chip-detect magic value `0x97e30068` alongside `0xd7b73e80`
([PR #203](https://github.com/espressif/esptool-js/pull/203/files)) **[V]**. **Below 0.5.6,
current ESP32-H2 silicon is not detected at all.** Pin `>= 0.6.1`. Note also that v0.6.0 changed
`writeFlash` to take a `Uint8Array` rather than a string **[V]**.

Newer H2 silicon (revision v1.2) also trips ESP-IDF bootloader minimum-revision checks, not just
esptool ([esp-zigbee-sdk#695](https://github.com/espressif/esp-zigbee-sdk/issues/695)) **[V]**.
Worth knowing when someone reports a board that will not boot.

**Feature notes for our own integration [V]:** compression via pako; `flashMd5sum()` exists but
there is **no built-in MD5**, so verification only happens if you inject a `calculateMD5Hash`
function, and is silently absent otherwise; reset strategies Classic, UsbJtagSerial, Hard and
Custom; `chip.readMac(loader)`. There is no `UnixTightReset`.

**We should inject an MD5 implementation and verify every write.** esp-web-tools does not (see
1.3), and for a mains-powered device a silent partial flash is not an acceptable failure mode.

### 1.2 esptool (Python) and espflash (Rust)

| Tool | H2 support since | Latest | Notes |
| --- | --- | --- | --- |
| `esptool` (PyPI) | **v4.5, 13 Feb 2023**, "full esptool and flasher stub support"; v4.5.1 enabled USB-JTAG/Serial in the H2 stub **[V]** | **5.3.1** (29 Jun 2026), Python >= 3.10; the v4 branch is still maintained at 4.12.0 **[V]** | See the breaking changes below |
| `espflash` (crates.io) | PR #371 merged 24 Mar 2023, released **2.0.0-rc.4, 8 June 2023** **[V]** | **4.5.0** (9 Jul 2026), MSRV 1.95.0 **[V]** | Prebuilt binaries and a library crate, both usable |

**esptool 5.0 is a breaking release** ([migration guide](https://docs.espressif.com/projects/esptool/en/latest/esp32/migration-guide.html))
**[V]**: the console script dropped the `.py` suffix (`esptool.py` became `esptool`), every option
moved from underscores to dashes, argparse was replaced by Click, the Python API takes explicit
parameters rather than an args object, `--verify` was removed because verification is now
automatic, Python 3.7 to 3.9 were dropped, and the Linux binaries require glibc 2.35.

**This is a real documentation tax on the fallback path.** Any command line we print is wrong for
the other major version. Detect and print the right one, or pin a version in the instructions.

**espflash is more capable than expected**, which matters if a native path is ever revisited
**[V]**:

- **Prebuilt binaries**: 14 archives per release covering aarch64 and x86_64 macOS, aarch64,
  x86_64 and armv7 Linux (gnu), x86_64 Linux (musl), and x86_64 Windows MSVC, for both `espflash`
  and `cargo-espflash`. `cargo binstall espflash` works.
- **Library crate**, Tauri-usable, as `espflash = { version = "4.5", default-features = false }`,
  exposing flasher, connection, target, image_format and command modules. The `cli` module carries
  no SemVer guarantee; the rest does.
- No `libudev-dev` requirement any more.
- **It can read and erase an NVS partition but cannot generate or parse NVS contents**, so it does
  not remove our need for a partition generator.
- **WSL1 is unsupported, and WSL2 cannot use USB Serial/JTAG at all**, UART only.

### 1.3 Espressif's own binaries are signed, and it does not save them from antivirus

A finding worth recording because it changes how we should think about a Python fallback.
Espressif's CI ([`build_esptool.yml`](https://raw.githubusercontent.com/espressif/esptool/master/.github/workflows/build_esptool.yml))
uses `espressif/python-binary-action` wired to Azure Key Vault for Windows Authenticode, plus a
macOS signing identity and full Apple notarisation **[V]**. Their prebuilt binaries are properly
signed and notarised.

They still get flagged. [esptool#944](https://github.com/espressif/esptool/issues/944) reports
"detected as Trojan:AndroidOS/ZkarletFlash by Windows Defender" and
[esptool#849](https://github.com/espressif/esptool/issues/849) reports quarantine by Avira, and
Espressif's own [install docs](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html)
still warn that the binaries "may trigger antivirus alerts" **[V]**.

**Read that plainly: code signing reduces the PyInstaller antivirus problem, it does not eliminate
it.** Any cost model that treats a certificate as buying its way out of false positives is wrong.

The practical consequence is a nice one. If we need an offline fallback, **point users at
Espressif's already-signed, already-notarised binaries plus a thin script**, rather than building
and signing our own bundle. We inherit their signing spend and their antivirus firefighting
instead of taking on our own.

### 1.4 esp-web-tools: useful reference, wrong abstraction for us

esp-web-tools is the Nabu Casa and ESPHome web installer component. It supports our chip.

From `src/const.ts` **[V]**:

```ts
export interface Build {
  chipFamily:
    | "ESP32" | "ESP32-C2" | "ESP32-C3" | "ESP32-C5" | "ESP32-C6"
    | "ESP32-C61" | "ESP32-H2" | "ESP32-P4" | "ESP32-S2" | "ESP32-S3" | "ESP8266";
  parts: { path: string; offset: number }[];
  serialType?: "cdc" | "uart";
}
```

`"ESP32-H2"` is a first-class member of the chip family union, added in esp-web-tools **10.0.0
(February 2024)** alongside ESP32-C6 **[V]**. The package depends on `esptool-js ^0.6.0` and is
Apache-2.0 licensed **[V]** (`package.json`). The comparison is an exact string match against
esptool-js's `CHIP_NAME` with no fallback, so a typo in a manifest produces a flat
`NOT_SUPPORTED` error **[V]**.

The `serialType` field is directly relevant to our native-USB question. From `src/flash.ts`
**[V]**, esp-web-tools decides whether it is talking to a native USB CDC device by inspecting
the USB descriptors:

```ts
const isCdcUsbPort =
  portInfo.usbVendorId === 0x303a &&
  [0x1001, 0x1002, 0x1003, 0x0002, 0x0003].includes(portInfo.usbProductId);
```

`0x303A` is Espressif's USB vendor ID. A build can then declare `serialType: "cdc"` and the
installer will prefer it when the device enumerated as native USB rather than through a
USB-to-UART bridge. That confirms the native-USB flashing path is real and in production use.

**Why we should not use esp-web-tools as our installer.** Its manifest is a static list of
`{path, offset}` parts fetched from URLs. Our flash operation is not static: we must generate a
per-device NVS profile image and a per-device Matter factory image in the browser, at flash
time, then write them at computed offsets. There is no manifest field for "run this generator
first". Forking it or bolting generation on top would fight the design. Using `esptool-js`
directly and writing our own UI is less code, not more.

We should still copy its behaviours: the CDC detection above, the `setRTS(true)` then
`esploader.after()` hard-reset sequence, and the error message it shows on init failure
("Try resetting your device or holding the BOOT button while clicking INSTALL"), which is the
single most useful string in the whole project **[V]**.

**Three further reasons not to adopt it, all verified [V]:**

- **It does no MD5 verification.** `src/flash.ts` passes no `calculateMD5Hash` to esptool-js, so
  the opt-in verification described in 1.1 never runs. It also hard-codes 115200 baud and leaves
  flash size, mode and frequency at `"keep"`.
- **Improv Wi-Fi is meaningless on the H2**, which has BLE 5 and 802.15.4 and no Wi-Fi radio.
  A large part of what esp-web-tools does for us is dead weight. Anyone writing a manifest for an
  H2 must set `new_install_improv_wait_time: 0`.
- **Neither flagship deployment is the reference integration people assume.**
  `web.esphome.io` does not use esp-web-tools at all: it drives esptool-js and
  `improv-wifi-serial-sdk` directly. Tasmota uses a **fork** (`Jason2866/esp-web-tools`) with a
  diverged schema, using `usbInterface` where upstream uses `serialType`. And
  `firmware.esphome.io` ships no ESP32-H2 build at all
  ([esphome/esp-web-tools#603](https://github.com/esphome/esp-web-tools/issues/603)).

The two most successful users of this component both went around it. That is the clearest possible
signal about which way to build.

**One practical gotcha for any H2 flashing code [V]:** the H2's valid flash frequency values are
**24m, 16m, 12m and 48m**, not the 40m and 80m familiar from the ESP32 and S3. Copying an ESP32
configuration will be wrong. If size detection fails, esptool falls back to 4 MB, which happens to
be correct for our part.

### 1.5 ESP32-H2 native USB and the strapping problem

Verified directly against the module datasheet held in this repository
(`hardware/datasheets/esp32-h2-mini-1_mini-1u_datasheet_en.pdf`, v1.6).

**Pin facts [V]**, from Table 3, Pin Definitions:

| Module pin | Signal | Alternate functions | Role on our board |
| --- | --- | --- | --- |
| 12 | IO13 | GPIO13, XTAL_32K_P | **host leg 1** (ESP8285 GPIO14) |
| 13 | IO14 | GPIO14, XTAL_32K_N | **host leg 2** (ESP8285 GPIO12) |
| 16 | IO12 | GPIO12 | **host leg 3** (ESP8285 GPIO13) |
| 20 | IO10 | GPIO10, ZCD0 | **host leg 4** (ESP8285 GPIO5) |
| 21 | IO11 | GPIO11, ZCD1 | **host leg 5** (ESP8285 GPIO4) |
| 9 | IO0 | GPIO0, FSPIQ | spare, test pad |
| 10 | IO1 | GPIO1, FSPICS0, **ADC1_CH0** | spare, test pad |
| 5 | IO2 | GPIO2, FSPIWP, **ADC1_CH1**, MTMS | spare, **strapping pin** |
| 6 | IO3 | GPIO3, FSPIHD, **ADC1_CH2**, MTDO | spare, **strapping pin** |
| 18 | IO4 | GPIO4, FSPICLK, **ADC1_CH3**, MTCK | spare, **WPU after reset** |
| 19 | IO5 | GPIO5, FSPID, **ADC1_CH4**, MTDI | spare, test pad |
| 30 | RXD0 | GPIO23, U0RXD | UART0 console pad |
| 31 | TXD0 | GPIO24, U0TXD | UART0 console pad |
| 26 | IO26 | GPIO26, **USB_D-** | native USB |
| 27 | IO27 | GPIO27, **USB_D+** | native USB |
| 8 | EN | chip enable, "Do not leave the EN pin floating" | reset |

UART0 is GPIO23/24 and native USB is on GPIO26/27, as the brief said.

**Two corrections to the brief on the GPIO1 to GPIO5 block, and they matter.**

1. **Those five are no longer the host lines.** The host mapping is v2: legs 1 to 5 go to H2 GPIO13,
   GPIO14, GPIO12, GPIO10 and GPIO11. GPIO0 to GPIO5 all became spares.
   [`docs/pin-mapping-v2.md`](./pin-mapping-v2.md) is the authority and this document does not
   re-derive it.
2. **GPIO2 and GPIO3 are strapping pins**, contrary to the module datasheet's Table 4, which lists
   only three. TRM v1.1 Register 6.7 enumerates the entire strapping latch as "bit0: GPIO2, bit1:
   GPIO3, bit2: GPIO8, bit3: GPIO9, bit4: GPIO25", and section 8.2.2 says their reset values help
   select the boot mode. That is exactly what invalidated the old GPIO1 to GPIO5 host mapping.
   Working in [`docs/h2-strapping-and-reset-states.md`](./h2-strapping-and-reset-states.md).

The ADC1 column above is correct as a statement about the silicon: ADC1 is the only unit, its five
channels are GPIO1 to GPIO5 and nothing else, and **no host line has one under v2**. That costs
nothing, because **no TYWE2L leg can ever carry an analogue signal in the first place**. The
ESP8266EX has exactly one analogue input, TOUT, which is a dedicated input-only pin rather than a
GPIO, and the TYWE2L does not break it out. All five ADC channels now sit in the spare field, where
the carrier could actually use one, which is strictly better than where v1 put them.

**Native USB flashing works with no bridge.** The H2 has a USB Serial/JTAG controller on those
two pins. Joint Download Boot explicitly "supports the following download methods: USB Download
Boot: USB-Serial-JTAG Download Boot; UART Download Boot" **[V]** (Table 6 footnote). So a single
USB connector on the replacement board is sufficient for flashing. GPIO27 (D+) idles high out of
reset because the pull-up is enabled by default, and both USB pins default to 40 mA drive **[V]**.
On the MINI-1 module their only alternate functions are spare SPI chip selects, so there is no
conflict to design around, and the in-package flash uses pins that are not brought out **[V]**.

**Windows: resolved, and the answer is good.** This was my biggest open question and it is now
closed. **Windows 10 and 11 bind the in-box `usbser.sys` CDC-ACM driver automatically. No INF, no
Zadig, no user action.** Espressif state it directly: "For Windows 10 and above, drivers will be
automatically installed when connected to the internet. For Windows 7/8 systems, manual driver
installation is necessary" **[V]**
([ESP-IoT-Solution](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_overview/usb_serial_jtag.html)).
Microsoft's own documentation confirms the mechanism: "If your device belongs to the communications
and CDC control device class, Usbser.sys loads automatically. You don't need to write your own INF
file" **[V]**
([MS Learn](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-driver-installation-based-on-compatible-ids)).

The H2 enumerates as a composite IAD device, class `0xEF/0x02/0x01`, **VID:PID 303A:1001**,
described as "USB JTAG/serial debug unit". Interfaces 0 and 1 are the CDC pair that `usbser.sys`
claims automatically. **Interface 2 is the vendor-specific JTAG function, and it is the only part
that ever needs WinUSB or Zadig, purely for OpenOCD debugging** **[V]**.

**That last point dissolves a piece of folklore.** The widespread advice that ESP32 native USB
"needs Zadig on Windows" comes entirely from interface 2. Flashing and serial monitoring never
touch it. If we do not ship a debugger, no user ever runs Zadig.

This is also the strongest single argument for Web Serial over WebUSB on Windows: WebUSB cannot
claim an interface that a class driver has already bound without WinUSB or Zadig, whereas Web
Serial simply opens the COM port that `usbser.sys` already created **[V]**.

The only Windows nuisance found is cosmetic: each chip has a unique serial number, so every new
board gets a new COM port number **[V]**. For Windows 7 and 8, an INF pack exists at
`https://dl.espressif.com/dl/idf-driver/idf-driver-esp32-usb-jtag-2021-07-15.zip` **[V]**, and we
should simply not support those.

**The strapping picture, with an important nuance.** From Table 4 and Table 6 **[V]**:

| Strapping pin | Default | Meaning |
| --- | --- | --- |
| GPIO8 | **Floating** | no defined level without external circuit |
| GPIO9 | Weak pull-up, bit value 1 | 1 selects SPI Boot |
| GPIO25 | **Floating**, no internal pull resistors | JTAG signal source strap |

Table 6, Chip Boot Mode Control: SPI Boot needs GPIO9 = 1 with GPIO8 any value. **Joint Download
Boot needs GPIO8 = 1 and GPIO9 = 0.**

The brief describes "GPIO9 as the download-mode strap". That is correct as far as it goes, and
GPIO9's internal 45k pull-up gives a defined idle state **[V]**. But GPIO8 also has to be high to
enter download mode, and it floats by default. The combination GPIO8 = 0 with GPIO9 = 0 is
invalid **[V]**
([boot mode selection](https://docs.espressif.com/projects/esptool/en/latest/esp32h2/advanced-topics/boot-mode-selection.html)).

**The nuance that changes how much this matters.** On the *automatic* USB path, the straps are
never consulted. The reset sequence over USB Serial/JTAG sets a hardware download-mode flag
directly, so GPIO8 and GPIO9 play no part **[V]**. Strapping only matters on the *manual* path:
holding a BOOT button, or entering download mode when the automatic sequence has failed, which is
exactly the recovery case that has to work.

**Recommendation stands, with the reason sharpened: put an external pull-up on GPIO8, and if there
is a BOOT button give it a strong 10k pull-down** **[V]**. Espressif's own documentation says
GPIO8 has no internal pull and that manual BOOT entry is unreliable without one. Get this wrong
and everything works until the day a user needs to recover a device, which is the worst possible
time to discover it.

Two more datasheet notes worth carrying into the hardware review:

- Table 9, JTAG Signal Source Control **[V]**: with the default eFuse state
  (`EFUSE_STRAP_JTAG_SEL_ENABLE` = 0), the JTAG signals come from the USB Serial/JTAG controller
  and GPIO25 is ignored. GPIO2 to GPIO5 carry the MTMS, MTDO, MTCK and MTDI pad functions, and
  MTDO is an output, so under pad JTAG the chip could drive GPIO3 before our firmware ran.
  **Under v2 this can no longer reach a host line**, because all four JTAG pads are spares. That
  turns a hardware-contention path into an inconvenience, and it is one of the reasons v2 leaves
  GPIO5 unused even though it screens as safe. Still do not burn `EFUSE_STRAP_JTAG_SEL_ENABLE`,
  and note `BACKLOG.md`'s rule that the two JTAG eFuses must never be burnt singly. The datasheet
  also warns that GPIO25 "must be controlled by the external circuit that cannot be in a high
  impedance state", so terminate it even though it is ignored by default.
- Tables 7 and 8 **[V]**: ROM boot messages print to UART0 *and* the USB Serial/JTAG controller
  by default. GPIO24 (U0TXD) will emit boot chatter on every reset. Harmless on a test header,
  worth knowing if UART0 ever ends up wired to anything in the host device.

### 1.6 Reset, re-enumeration, and the one real design problem

**Reset over native USB does still toggle DTR and RTS**, contrary to what I assumed, just with a
different eight-step sequence taken from the H2 technical reference manual, Table 33.4-1 **[V]**.
The manual explains why the sequence is odd: "most operating systems only allow setting or
clearing DTR and RTS separately, but not in tandem... some drivers (e.g. the standard CDC-ACM
driver on Windows) do not set DTR until RTS is set". It is implemented as
`usb_jtag_bootloader_reset()`.

Both esptool and esptool-js select this path **by USB product ID rather than by chip**: esptool
tests `self.get_usb_vid_pid()[1] == self.USB_JTAG_SERIAL_PID` (0x1001), and esptool-js does the
identical check via `transport.getPid()` **[V]**.

**The design problem, and it is the one thing that will make or break the web installer's feel.**
Reset destroys the USB device and it re-enumerates, so the file descriptor or COM handle dies
mid-operation. esptool mitigates this with `ESPTOOL_OPEN_PORT_ATTEMPTS` and auto-reconnect since
v4.9.0 **[V]**. **esptool-js has no reconnect logic at all.** It detects the loss (a `NetworkError`
reading "device has been lost" routed to `onDeviceLostCallback`) and stops **[V]**.

So our application has to reacquire the port itself, and in a browser that may require a fresh
`requestPort()`, which requires a fresh user gesture. **Design the flashing flow around this from
the start**: expect one or more re-enumerations, keep a visible "reconnect" affordance, and never
assume a single `SerialPort` object survives the whole operation. Retro-fitting this later means
rewriting the state machine.

Related, and it should shape the UI copy: **if download mode was entered *manually*, esptool
cannot get the chip out of it.** Espressif document this directly: "The USB-Serial/JTAG peripheral
can only trigger a core reset, which does not re-sample the state of the boot strapping pin... the
chip stays in download mode" **[V]**. The fix is `--after watchdog-reset`, or pressing EN. Our UI
must say so rather than leaving the user staring at a device that appears bricked.

**[U]** Whether a "hold BOOT while clicking install" fallback is *required* on H2 through a
browser is not documented, but the evidence points that way:
[esptool-js#96](https://github.com/espressif/esptool-js/issues/96) (unreliable flashing over CDC)
was closed as not planned, and #227 (a Chrome 143 timing regression) is still open. **Budget a
manual BOOT and EN fallback in the UI.**

One more implementation trap: signal polarity differs between pyserial (active-low on the wire)
and Web Serial's `setSignals` (assertions), so `reset.py` and `reset.ts` are not line-for-line
ports of each other **[V]**. Do not port reset code between them by eye.

### 1.7 When firmware kills the USB peripheral

Three cases, with different severities **[V]**
([ESP-IDF USB Serial/JTAG console guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-guides/usb-serial-jtag-console.html)):

1. **The application reconfigures the pins or disables the controller.** The device disappears from
   the system. Fully recoverable: pull GPIO9 low with GPIO8 high, reset, then `erase-flash`. This
   is exactly why the GPIO8 pull-up in 1.4 matters.
2. **Sleep.** Deep sleep disconnects the device; light sleep leaves it enumerated but unresponsive
   and may need a physical replug. Not fixable in software. Relevant to any sensor variant that
   sleeps.
3. **eFuses, and these are permanent.** A correction to the usual claim: **there is no plain
   `DIS_USB_SERIAL_JTAG` fuse on the H2**. The real ones are `DIS_USB_JTAG` (bit 41, JTAG only, the
   CDC function survives), `DIS_USB_SERIAL_JTAG_DOWNLOAD_MODE` (bit 132, kills USB flashing),
   `DIS_USB_SERIAL_JTAG_ROM_PRINT` (bit 130), `DIS_DOWNLOAD_MODE` (bit 128, disables all flashing),
   `DIS_FORCE_DOWNLOAD` (bit 44), `USB_EXCHG_PINS` (bit 57) and `JTAG_SEL_ENABLE` (bit 47). All
   default to 0 and none can be reverted.

**Our firmware and our flasher must never burn any of these.** There is no legitimate reason for a
DIY retrofit to disable its own recovery path.

A documentation bug worth knowing about: the ESP-IDF ESP32-H2 page renders the recovery pin as the
literal string "Not Updated!" because a substitution has no `esp32h2` entry. The correct value is
**GPIO9** **[V]**.

### 1.8 Web Serial browser support: better than the folklore

**Web Serial is no longer Chromium-only.** This is the single most out-of-date assumption in the
field, and it materially strengthens the web-installer case.

| Browser | Status | Source |
| --- | --- | --- |
| Chrome, Edge desktop | 89+ **[V]** | [MDN BCD](https://bcd.developer.mozilla.org/bcd/api/v0/current/api.Serial.json) |
| Opera desktop | 75-76 **[V]** | MDN BCD, caniuse |
| **Firefox desktop** | **151, shipped 19 May 2026**, behind a synthetically generated site-permission add-on, on the WebMIDI model **[V]** | [Firefox 151 release notes](https://www.firefox.com/en-US/firefox/151.0/releasenotes/) |
| Safari, iOS Safari | **Never.** WebKit position is `oppose`, closed 5 May 2026 **[V]** | [WebKit standards-positions #199](https://github.com/WebKit/standards-positions/issues/199) |
| Chrome Android | **Partial and not useful to us.** Serial ports only via Bluetooth RFCOMM emulation; wired USB is gated behind an Android platform API and a Finch flag **[V]** | [blink-dev PSA](https://groups.google.com/a/chromium.org/g/blink-dev/c/yGhvQ6mEmcY) |
| Firefox Android, Samsung Internet, Android WebView | No **[V]** | [caniuse](https://caniuse.com/web-serial), global usage 74.5% |

Mozilla's standards position moved from negative in 2020 to **`neutral`** when
[PR #959](https://github.com/mozilla/standards-positions/pull/959) merged on 10 January 2024, and
the issue closed on 4 March 2026 **[V]**.

**Two consequences for us.** First, **do not gate on the user agent, feature-detect
`'serial' in navigator`.** Tasmota's installer still hard-codes "Chrome or Edge" and is now wrong
**[V]**. Second, the "Chromium-only" objection to a web installer is now roughly a Safari-only
objection, and Safari is never going to implement it. That is a much smaller hole to document
around.

**Do not plan on flashing from a phone.** Android over USB is effectively unmaintained: Google's
`web-serial-polyfill` was **archived on 23 January 2025**, yet Chrome's own Web Serial
documentation still recommends it for Android and esptool-js's README still links it **[V]**. It
also cannot work against a device already claimed by `cdc_acm` or `usbser.sys`, which ours always
will be. ESP Web Tools states its own position plainly: "Android support should be possible but has
not been implemented yet" **[V]** ([docs](https://esphome.github.io/esp-web-tools/)).

**Requirements to design around [V]** ([spec](https://wicg.github.io/serial/)): a secure context
(localhost is exempt under the usual trustworthy-origin rule); transient user activation for
`requestPort()` but not for `getPorts()`; and Permissions Policy token `serial` with a default
allowlist of `'self'`, so a cross-origin iframe needs `allow="serial"` or both calls reject with
`SecurityError`. That last one matters if the installer is ever embedded in a documentation site.

### 1.9 Linux serial permissions

**Group membership is required, and Chrome does not work around it.** Verified at source:
Chromium's `services/device/serial/serial_io_handler.cc` calls
`chromeos::PermissionBrokerClient::OpenPath` on ChromeOS, and on every other platform does a plain
`base::File file(port_, flags)`, an ordinary `open(2)` as the browser's own uid **[V]**
([source](https://chromium.googlesource.com/chromium/src/+/main/services/device/serial/serial_io_handler.cc)).
There is no setuid helper, no polkit and no broker on desktop Linux.

**The failure mode is nasty and worth pre-empting in the UI**: sysfs is world-readable, so the port
**appears in the picker**, and then `open()` fails with a generic `NetworkError` **[V]**.
Diagnosable at `about://device-log`. Without a specific error message from us, a user will conclude
the device is broken.

Group names by distribution **[V]** unless noted: Debian and Ubuntu use **`dialout`**; Arch uses
**`uucp`**; Fedora uses `dialout` or `plugdev` **[U** at the Fedora source**]**; openSUSE **[U]**.
systemd's upstream default rule is
`KERNEL=="tty[A-Z]*[0-9]|...|rfcomm[0-9]*", GROUP="dialout"` in `50-udev-default.rules.in`,
covering `ttyUSB*`, `ttyACM*` and `ttyS*` **[V]**.

**A correction worth carrying**: Debian's `uucp` group is *not* a serial-access group. It owns the
UUCP spool and permits running `uucico` **[V]** ([Debian wiki](https://wiki.debian.org/SystemGroups)).
Arch's reuse of the name for serial access is what trips up people moving between the two.

Group changes need a new login session, and **the browser must be fully restarted**, which in
practice usually means a full desktop logout **[V]**.

**Recommended udev rule, using `uaccess` rather than a group or mode 0666:**

```
SUBSYSTEM=="tty", SUBSYSTEMS=="usb", ATTRS{idVendor}=="303a", TAG+="uaccess"
```

udev tags the device and systemd-logind attaches a POSIX ACL granting read and write to the user
on the active local seat **[V]**
([70-uaccess.rules.in](https://github.com/systemd/systemd/blob/main/rules.d/70-uaccess.rules.in)).
Dynamic, revoked at logout, no permanent group membership, and no world-writable device node. Then
`udevadm control --reload-rules && udevadm trigger`, and **replug**.

Its limitation is that it is seat-only, so SSH sessions and CI still need the group **[V]**. The
underlying limitation is well established; **[U]** the specific systemd issue number usually cited
for it should be read before being quoted.

**ModemManager interference, with the exact fix.** ModemManager probes newly appeared ACM devices
and can hold the port during flashing. The rule, in a file named `78-mm-*` or `79-mm-*` **[V]**
([ModemManager docs](https://modemmanager.org/docs/modemmanager/port-and-device-detection/)):

```
ACTION!="add|change|move|bind", GOTO="mm_my_rules_end"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="303a", ENV{ID_MM_DEVICE_IGNORE}="1"
LABEL="mm_my_rules_end"
```

`ENV{ID_MM_PORT_IGNORE}="1"` scopes it to a single port instead. That this is a real and not a
folklore problem is established by
[Red Hat bug 1261040](https://bugzilla.redhat.com/show_bug.cgi?id=1261040), fixed in
ModemManager 1.6.4, using exactly this pattern **[V]**. Note that `ENV{MTP_NO_PROBE}="1"` targets a
different daemon and is not a ModemManager fix **[V]**.

**The H2 is the worst case for this**, because a CDC-ACM device at `/dev/ttyACM*` looks like a
cellular modem, and it re-enumerates mid-flash (see 1.6), creating a fresh probe window at the
worst possible moment. **[Partly U**, this last inference is synthesis rather than a cited claim**]**

**PlatformIO's udev rules, with a URL correction.** The commonly cited path 404s. The live one is
`https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules`
**[V]**. It uses `MODE:="0666"` plus both ModemManager ignore variables, and covers Espressif
`303a` product IDs `1001` and `4001`.

**ChromeOS is the one platform with no setup at all**, thanks to `permission_broker` **[V]**. Worth
recommending explicitly for classroom, workshop or kiosk deployments.

**[U]** Snap and Flatpak confinement behaviour, and whether Firefox 151 on Linux behaves
identically to Chrome here, were not verified.

---

## 2. Application architecture options

### 2.1 The four candidates

| | Web installer | Tauri v2 | Electron | Python CLI/GUI |
| --- | --- | --- | --- | --- |
| User install step | None. Open a URL. | Download, warn-through, install | Download, warn-through, install | Install Python, then pip |
| Browser or OS coverage | Chrome, Edge, Opera, **Firefox 151+**. Not Safari **[V]** | All three desktops | All three desktops | All three, plus servers |
| Update path | Instant, server-side | Needs updater plugin plus signing | Needs updater plus signing | `pip install -U` |
| Code signing needed | **No** | Yes, Windows and macOS | Yes, Windows and macOS | Only if bundled to an exe |
| Bundle size | Nothing to download | Small, roughly 5-15 MB **[U]** | Large, roughly 100-150 MB **[U]** | Small, or 30 MB+ bundled |
| Serial access | Web Serial | **`espflash` 4.5 as a library crate**, `default-features = false`, no `libudev-dev` needed **[V]** | `node-serialport`, native module rebuilds | `pyserial`, mature |
| Offline use | Poor. Needs a service worker and cached firmware | Good | Good | Good |
| CI to release | One static build | Three-OS matrix, cannot cross-build macOS | Three-OS matrix | One build, or three if bundling |
| Ongoing maintenance | Low | Medium | High. Frequent Chromium security updates | Low |

**[U]** Bundle sizes for Tauri and Electron were not verified. The Tauri point that is not in
doubt: you cannot build and sign a macOS app on Linux, so any native path forces a three-runner CI
matrix, and a macOS runner in particular.

If a native path is ever revisited, **Tauri with the `espflash` library crate is the least-bad
option**, because espflash already ships prebuilt binaries for 14 target triples and exposes a
semver-stable library API **[V]** (section 1.2). Note one limitation that affects a surprising
number of developers: **WSL1 is unsupported and WSL2 cannot use USB Serial/JTAG at all** **[V]**.

### 2.2 Code signing: the hidden cost

This is the part that decides the question, so it deserves precision. One finding here is
concrete and important for an Australian project.

**Windows.**

Since mid-2023 the CA/Browser Forum baseline requirements have mandated that code signing private
keys live on hardware meeting FIPS 140-2 Level 2 or Common Criteria equivalent. In practice that
means a shipped USB token, a cloud HSM, or a managed signing service. Cheap file-based
certificates no longer exist. **[U]** on the exact date and clause; the effect is not in dispute
and is visible in every CA's current product line.

The cheapest credible route is Microsoft's managed service, which has just been renamed. It was
"Azure Code Signing", then "Azure Trusted Signing", and as of the current documentation it is
**Azure Artifact Signing** **[V]**
([overview](https://learn.microsoft.com/en-us/azure/artifact-signing/overview)). Keys live in
FIPS 140-3 Level 3 HSMs, and the service integrates with normal build pipelines.

The eligibility rule is the thing to read carefully. From the quickstart **[V]**
([source](https://learn.microsoft.com/en-us/azure/artifact-signing/quickstart)):

> Public Trust certificates are available to organizations in the United States, Canada, the
> European Union, the United Kingdom, **Australia**, New Zealand, Japan, South Korea, Singapore,
> Switzerland, Norway, and Israel. **Individual developers must be located in the United States
> or Canada.**

So for this project: an Australian **registered legal entity** can use Azure Artifact Signing. An
Australian **individual developer** cannot. If the project is going to sign Windows binaries
cheaply, it needs a company behind it. Identity validation takes "from 1 to 20 business days"
and requires government-issued ID plus business registration documents **[V]**.

Pricing is Basic and Premium SKUs at 5,000 and 100,000 signatures per month **[V]**
([pricing page](https://azure.microsoft.com/en-us/pricing/details/trusted-signing/)), but the
page currently renders the dollar figures as `$-` and directs you to a quote. **[U]** The widely
reported Basic price has been around USD 10 per month; treat that as unconfirmed.

**[U] Not verified in this session**, and needed before any native decision is final:

- Current OV and EV code signing certificate prices from SSL.com, Sectigo, DigiCert and Certum,
  including Certum's discounted open-source developer offer.
- Whether SmartScreen still treats EV certificates as instant reputation, and how long an OV
  certificate takes to build reputation. Microsoft's public position on this has shifted and is
  vague. Assume that a brand-new certificate of any type will show a warning on early downloads.

If you do not sign at all: SmartScreen shows a full-screen "Windows protected your PC" interstitial
with the "Run anyway" control hidden behind a "More info" link. For an audience that is about to
open a mains-powered appliance, this is the wrong first impression.

**macOS.**

**[U] for the figures, high confidence on the shape.** Distributing a native macOS app outside
the App Store requires an Apple Developer Program membership (widely known to be USD 99 per year),
a Developer ID Application certificate, the hardened runtime enabled, notarisation of every build
via `xcrun notarytool`, and stapling of the ticket to the artefact. `altool` was retired in favour
of `notarytool`. Recent macOS releases have tightened the manual override path for unsigned
applications, so "right-click and Open" is no longer the reliable escape hatch it once was.
**Verify the current Gatekeeper behaviour on the macOS version you intend to support before
promising anything.**

There is no free path that gives a good user experience. Ad-hoc signing and
`xattr -d com.apple.quarantine` both work and both require the user to paste a terminal command,
which for this audience is equivalent to failing.

**Linux.** No gatekeeper. The cost is packaging breadth: AppImage is the least effort, Flatpak
and `.deb` multiply the work.

### 2.3 Realistic annual cost

| | Web installer | Signed native app |
| --- | --- | --- |
| Windows signing | 0 | Azure Artifact Signing, company entity required, roughly USD 120/yr **[U]**, or USD 300-600/yr for a token-based EV certificate **[U]** |
| macOS | 0 | Apple Developer Program, USD 99/yr **[U]** |
| CI | One static build | Three-OS matrix including a macOS runner |
| Setup effort | Hours | Days, plus 1-20 business days of identity validation waiting **[V]** |
| Per-release effort | `git push` | Build, sign, notarise, staple, publish, per platform |
| Entity requirement | None | An Australian individual **cannot** use the cheap Windows route **[V]** |

The web installer's cost is not zero. It is paid in support load from users on browsers that
cannot run it. As of Firefox 151 that is Safari and mobile, a much smaller group than it was a year
ago. The cost is real but bounded, and it is paid in documentation rather than in dollars,
calendar time and a company structure.

**[U] Comparable projects.** ESPHome, Tasmota, WLED and Meshtastic all ship browser-based flashers
as the primary route for end users. Whether any of them built and abandoned a native flasher was
not established.

### 2.4 Recommendation

**Web installer, with a printed command as the fallback.**

The fallback deserves design attention rather than being an afterthought. When Web Serial is
unavailable, the app should still do everything except the final write: let the user pick their
device, generate the profile image and the Matter credentials in the browser, then hand them a zip
of the binaries plus the exact command line with the offsets already filled in. That converts an
unsupported browser from a dead end into a copy-and-paste, and it gives us the CI and automation
path for free.

**Point that fallback at Espressif's own prebuilt `esptool` binaries** rather than shipping our
own bundle. They are Authenticode-signed via Azure Key Vault and Apple-notarised at Espressif's
expense **[V]** (section 1.3). Two details to get right: print the correct syntax for the version
you link, since esptool 5.0 renamed the entry point and changed every option separator; and warn
that antivirus software sometimes quarantines these binaries anyway, because it does, even signed.

---

## 3. Configuration model

The plan is a small number of firmware variants, each reading a device profile from flash at boot.
The research question is whether esp-matter can actually do that. It can.

### 3.1 Runtime endpoint composition in esp-matter

**Yes, genuinely. esp-matter no longer uses the ember and ZAP static endpoint tables at all.**
Verified by reading the SDK source at commit `fd297702` (12 August 2026)
([repo](https://github.com/espressif/esp-matter)).

esp-matter implements CHIP's `chip::app::DataModel::Provider` interface directly.
`components/esp_matter/data_model_provider/esp_matter_data_model_provider.h` declares
`class provider : public chip::app::DataModel::Provider`, overriding `Endpoints()`,
`ServerClusters()`, `Attributes()`, `AcceptedCommands()`, `DeviceTypes()`, `ReadAttribute()`,
`WriteAttribute()` and `InvokeCommand()` **[V]**. The companion file
`data_model_provider/esp_matter_ember_stubs.cpp` says it outright in its header comment **[V]**:

> Since we will not use ember data model anymore, but the upstream code still uses ember APIs so
> we define these stub functions to make upstream code access our esp_matter data model instead
> of ember data model.

So there is no `zap-generated/endpoint_config.h` and no `emberAfEndpointConfig`. The model is a
heap-allocated linked list that the provider walks on every read. From
`data_model/esp_matter_data_model.cpp` **[V]**:

```c
endpoint_t *create(node_t *node, uint8_t flags, void *priv_data)
{
    VerifyOrReturnValue(
        get_count(node) < CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT, NULL, ...);
    _endpoint_t *endpoint = (_endpoint_t *)esp_matter_mem_calloc(1, sizeof(_endpoint_t));
    endpoint->endpoint_id = current_node->min_unused_endpoint_id++;
    SinglyLinkedList<_endpoint_t>::append(&current_node->endpoint_list, endpoint);
```

`cluster::create(endpoint_t*, uint32_t cluster_id, uint8_t flags)` takes a plain integer cluster
id, so cluster composition is fully data-addressable at runtime **[V]**. Same for attributes and
commands. The `light` example's `main/` directory contains no `.zap` file and no `zap-generated`
directory **[V]**.

**There is already an example that does almost exactly what we want.**
`examples/all_device_types_app` stores a single device-type index in NVS and reads it at boot
**[V]** (`main/nvs_helpers.cpp`, `main/app_main.cpp` around lines 247 to 262):

```c
node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
uint8_t device_type_index;
if (esp_matter::nvs_helpers::get_device_type_from_nvs(&device_type_index) != ESP_OK) {
    /* fall back to a console command */
} else {
    esp_matter::data_model::create(device_type_index);
}
esp_matter::start(app_event_cb);
```

The SDK release notes of 9 June 2026 add a `change <device_type>` console command that "saves the
selection and reboots the device with stable endpoint IDs" **[V]**. That is the reboot-to-reconfigure
pattern our flasher and firmware should follow.

Note the shape of that example carefully: it selects among **compiled-in compositions**, rather
than interpreting an arbitrary composition description. Arbitrary composition is possible with the
raw `endpoint::create` and `cluster::create(id)` calls, but you then hand-roll everything the
generated device-type helper does for you: mandatory clusters, feature maps, delegate wiring and
attribute bounds. For our purposes the middle path is right: **compiled-in endpoint archetypes,
selected and parameterised by the profile.**

**The real limits [V]:**

| Limit | Value | Source |
| --- | --- | --- |
| Max dynamic endpoints | `CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT`, default **16**, range 1-255 | `components/esp_matter/Kconfig` |
| Max device types per endpoint | `CONFIG_ESP_MATTER_MAX_DEVICE_TYPE_COUNT`, default 16 | `Kconfig` |
| DRAM cost | roughly 550 bytes per endpoint or device-type slot | `docs/en/optimizations.rst` |

**The one thing that stays compile-time, and it is the seam to design around.** Which cluster
*server implementations* get linked into the binary is a build choice.
`components/esp_matter/utils/cluster_select/Kconfig.in` defines **139**
`CONFIG_SUPPORT_<X>_CLUSTER` booleans, all defaulting to `y`, and
`utils/cluster_select/cluster_dir.cmake` conditionally adds
`${MATTER_SDK_PATH}/src/app/clusters/<name>-server` to the source list per flag **[V]**.

Stated plainly: **endpoint composition is data, cluster vocabulary is firmware.** A variant that
links On/Off, Level Control and Color Control can express any switch, dimmer, plug or light profile
from NVS. A profile that wants Window Covering will not work on that variant. That is exactly the
right axis on which to split the firmware variants, and it should drive the variant list rather
than device categories doing so.

**The 4 MB question is resolved, and the answer is yes with room to spare.** Real measurements
from `docs/en/optimizations.rst`, for the `light` example on ESP32-H2 **[V]**:

| Configuration | Flash | D/IRAM | Free heap at boot | Free heap after commissioning |
| --- | --- | --- | --- | --- |
| Default | **1,576,436** | 179,487 | 44,256 | 77,976 |
| `CONFIG_ENABLE_CHIP_SHELL=n` | 1,521,816 | 178,695 | 54,136 | 87,592 |
| `CONFIG_BT_CTRL_RUN_IN_FLASH_ONLY=y` | 1,589,720 | **159,553** | 64,044 | 97,608 |

The standard `examples/light/partitions.csv`, shared by `light_switch`, `generic_switch`,
`zap_light` and `icd_app`, is close to our target layout **[V]**:

```
esp_secure_cert, 0x3F, ,          0xd000,   0x2000,  encrypted
nvs,      data, nvs,              0x10000,  0xC000
nvs_keys, data, nvs_keys,         ,         0x1000,  encrypted
otadata,  data, ota,              ,         0x2000
phy_init, data, phy,              ,         0x1000
ota_0,    app,  ota_0,            0x20000,  0x1E0000
ota_1,    app,  ota_1,            0x200000, 0x1E0000
fctry,    data, nvs,              0x3E0000, 0x6000
```

Each OTA slot is 0x1E0000, or 1,966,080 bytes. The light example at 1,576,436 uses **80% of a
slot**, leaving roughly 390 KB of headroom. Dual-slot OTA fits.

The cautionary data point sits right beside it: `all_device_types_app/partitions.h2.csv` uses a
**single 3.9 MB application partition with no OTA slots**, because linking every cluster does not
fit twice in 4 MB **[V]**. Trimming `CONFIG_SUPPORT_*_CLUSTER` per variant is what keeps us
OTA-capable. That reinforces the variant-split argument above: variants exist to bound the cluster
set, not to bound the pin map.

**Versions to target [V].** esp-matter publishes releases as **branches, not tags**:
`release/v1.3`, `release/v1.4`, `release/v1.4.2`, `release/v1.5`, `release/v1.6`, with `main`
tracking specification v1.7. connectedhomeip is pinned at commit `efefc94fee`, and the recommended
ESP-IDF is **v5.5.4**. ESP32-H2 is a first-class Thread target: every example ships a
`sdkconfig.defaults.esp32h2` with `CONFIG_OPENTHREAD_ENABLED=y`, `CONFIG_OPENTHREAD_SRP_CLIENT=y`
and `CONFIG_ENABLE_WIFI_STATION=n`.

**Persistence across reboot.** esp-matter persists only the endpoint-id allocator, storing
`min_uu_ep_id` as a `u16` in NVS **[V]**. `endpoint::resume(node, flags, endpoint_id, priv_data)`
re-attaches a *known* endpoint id and refuses if the id is at or above the minimum unused id
**[V]**. Rebuilding the composition itself is the application's job, which is why the
all-device-types example stores its own index. **Our boot path must reconstruct the same endpoint
ids from the profile every time**, or bindings from a controller will point at the wrong thing.

### 3.2 Generating an NVS image on the client

**Espressif's Python tool** now lives in its own repository,
[espressif/esp-idf-nvs-partition-gen](https://github.com/espressif/esp-idf-nvs-partition-gen), and
is published to PyPI as `esp-idf-nvs-partition-gen` **v0.3.0, 21 July 2026**, Apache-2.0 **[V]**.
It takes a CSV of `key,type,encoding,value` rows with namespace rows and emits a partition binary
of a given size. Minimum partition size 0x3000, all sizes multiples of 4096. Encodings are
`u8 i8 u16 i16 u32 i32 u64 i64 string hex2bin base64 binary` **[V]**
([docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_partition_gen.html)).

One trap worth recording: `binary` encoding stores the CSV cell's raw bytes, so
`blob,data,binary,deadbeef` stores eight ASCII characters. Use `hex2bin` for actual hex **[V]**.

**A pure-JavaScript equivalent exists, and its output was verified byte-identical.** This is the
finding that makes the browser-only approach viable rather than merely plausible. Both candidates
were installed and executed, and their output diffed against a reference partition generated by
the Python tool **[V]**:

| Package | Version | Verdict |
| --- | --- | --- |
| [`partitions-tool-esp`](https://github.com/laride/partitions-tool-esp) 0.2.0, Apache-2.0 | 7 Jul 2026 | **Byte-identical to `esp-idf-nvs-partition-gen`** over a 24,576-byte (0x6000) partition **[V]** |
| [`@m1kad0/esp-nvs-utils`](https://www.npmjs.com/package/@m1kad0/esp-nvs-utils) 0.2.4, Apache-2.0 | 1 Aug 2026 | Byte-identical page 0, but auto-sizes output to the minimum page count instead of honouring a requested partition size **[V]** |

`partitions-tool-esp` is the clear choice. It parses `nvs_partition_gen.py`-style CSV verbatim,
covers plaintext NVS v1 and v2, multipage blobs and AES-256-XTS encrypted NVS with `nvs_keys`
serialisation, and separately builds partition tables, FAT, SPIFFS and LittleFS images. Its own
test suite (`tests/nvs.spec.ts`) leads with a case named "matches esp_idf_nvs_partition_gen output
byte-for-byte" against checked-in golden fixtures **[V]**. There is a
[live browser demo](https://laride.github.io/partitions-tool-esp/).

`@m1kad0/esp-nvs-utils` also has a practical problem beyond the sizing behaviour: its git remote
points at a private host (`dev.kernstock.net`), so it cannot be audited or forked easily **[V]**.

**The residual risk is maturity, not correctness.** Both libraries first appeared in June 2026.
Mitigation, and this should be non-negotiable:

- Vendor `partitions-tool-esp` into our repository rather than tracking a floating npm version.
- Keep the golden-file comparison in our own CI, not just theirs, over our own profile corpus.
- Have the firmware validate what it reads regardless (see section 5).

For reference, `esptool-js` does not do NVS at all. It is flash transport only **[V]**.

**A reimplementation is tractable if it ever becomes necessary.** The NVS format is a 4096-byte
page of a 32-byte header, a 32-byte entry-state bitmap at 2 bits per entry, and 126 32-byte
entries, with CRC32 over the header and each entry, keys 16 bytes NUL-padded, and namespace names
in namespace index 0 **[V]**. `partitions-tool-esp`'s writer is roughly 1,100 lines of TypeScript
in total. Worth knowing; not worth doing now.

**The alternative worth keeping in your pocket, and my recommendation for our own data.** We do
not have to use NVS for the device profile. Partition table types **0x40 to 0xFE are free for
custom use** **[V]**
([partition table docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html)),
with any subtype from 0x00 to 0xFE. There is precedent right in the esp-matter partition table:
`esp_secure_cert` uses type `0x3F`. Firmware reads it with `esp_partition_find_first` followed by
`esp_partition_read`.

The trade-off: NVS gives us standard tooling, the `nvs_flash` API, key-level updates, wear
levelling and a power-fail-safe write path. A raw blob gives us total control, a format emittable
from any language in twenty lines, and no third-party format dependency. For a **write-once
factory profile that firmware only ever reads**, none of NVS's advantages actually pay for
themselves.

**Recommendation: NVS for the Matter factory partition, because `esp-matter-mfg-tool` defines that
format and we must match it. A raw versioned blob in a custom-type partition for our own device
profile, carrying magic, schema version, length and CRC32, because we define that format and
simplicity is worth more than tooling.** That also splits the risk usefully: if
`partitions-tool-esp` ever has a bug, it can only affect the Matter partition, where Espressif's
generator gives us a cross-check.

Two options to rule out explicitly. **Do not append the profile to the firmware image, and do not
write it at a fixed offset inside an application slot.** Either couples the profile to the app
image, so an OTA update either wipes the profile or has to carry it, which defeats the point of
one binary per variant. A separate partition survives OTA cleanly, which is why the esp-matter
layout has `fctry` in the first place.

**Fit with the existing layout.** The esp-matter table already carries `nvs` (0xC000, application
and CHIP config) and `fctry` (0x6000, Matter factory data). Adding a third small partition for the
device profile keeps factory provisioning and device personality separable, which is the right
boundary because they have different lifecycles and different trust stories. It costs nothing in
the 4 MB budget, since it comes out of the unallocated space rather than the application slots.

### 3.3 How others describe a pin map as data

**ESPHome: compile-time, and no Matter at all.** Each device is a YAML file compiled through
Python codegen into its own binary. There is no runtime pin reconfiguration path. The cost is a
build farm, forever, which is the thing to avoid at our size.

Two verified points, one stronger than expected **[V]** (source tree at HEAD, 12 August 2026):

- `git ls-tree -r --name-only HEAD | grep -i matter` **returns nothing**. ESPHome has zero Matter
  support as of August 2026. Not limited: absent. This is a good part of the reason this project
  exists.
- It does have `esphome/components/openthread/` for raw Thread networking, gated on the C5, C6,
  **H2**, H4, H21 and S31 variants, and `esphome/components/zigbee/`. ESP32-H2 is a supported
  variant with its own `gpio_esp32_h2.py`.

**[U]** Whether `web.esphome.io` compiles or only flashes prebuilt binaries was not confirmed.

**Tasmota: runtime template, one binary.** This is our model, and the format is now fully pinned
down. A real entry from the community database **[V]**:

```yaml
template9: '{"NAME":"Generic","GPIO":[32,1,1,1,1,225,33,1,224,288,1,1,1,1],"FLAG":0,"BASE":18}'
```

**Array length.** From `tasmota/include/tasmota_template.h` **[V]**:

```c
#define MAX_GPIO_PIN       18   // Number of supported GPIO (0..16 + ADC0)
#define MIN_FLASH_PINS     4    // GPIO6, 7, 8 and 11 unusable
#define MAX_USER_PINS      14   // MAX_GPIO_PIN - MIN_FLASH_PINS
```

**Slot to physical GPIO mapping**, from `tasmota/tasmota_support/support.ino` (around lines 1700
to 1721) **[V]**:

```c
if (6 == i) { j = 9; }
if (8 == i) { j = 12; }
dest[j] = src[i];
```

giving slot order `[GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO9, GPIO10, GPIO12, GPIO13,
GPIO14, GPIO15, GPIO16, ADC0]`. ESP32 uses 36 or 38 slots, C3 uses 22.

**Value encoding**, from `tasmota/include/tasmota_globals.h` **[V]**:

```c
#define AGPIO(x) ((x)<<5)
#define BGPIO(x) ((x)>>5)
```

A value is `(function_enum << 5) | instance_index`. Five bits of index means at most 32 instances
of a function. The function enum is `UserSelectablePins` in `tasmota_template.h`, currently **389
entries**, guarded by `static_assert(GPIO_SENSOR_END < 2000)` **[V]**. Decoded against the live
corpus:

| Value | Meaning |
| --- | --- |
| 0 | `GPIO_NONE`, unused |
| 1 | free, user-configurable |
| 32, 33 | `GPIO_KEY1` #1, #2 |
| 160-167 | `GPIO_SWT1` #1-#8 |
| **224, 225, 226** | **`GPIO_REL1` #1, #2, #3** |
| 256-259 | `GPIO_REL1_INV` #1-#4 |
| 288 / 320 | `GPIO_LED1` / `GPIO_LED1_INV` |
| **416-419** | **`GPIO_PWM1` #1-#4** |
| 544 / 576 | `GPIO_LEDLNK` / `GPIO_LEDLNK_INV` |
| 2272 / 2304 | `GPIO_TUYA_TX` / `GPIO_TUYA_RX` |

`FLAG` is the legacy ADC0 flag, `BASE` is the base module number (18 generic ESP8266, 54 Tuya MCU,
1 ESP32), and an optional `CMND` field carries a `Backlog` of setup commands **[V]**
([Tasmota template docs](https://tasmota.github.io/docs/Templates/)).

**OpenBeken: the best-shaped schema of the three, and a correction.** Repository
[openshwprojects/OpenBK7231T_App](https://github.com/openshwprojects/OpenBK7231T_App), HEAD
11 August 2026. Its runtime pin configuration is refreshingly flat, from `src/new_pins.h` **[V]**:

```c
typedef struct pinsState_s {
    byte roles[48];         // indexed by physical pin
    byte channels[48];      // indexed by physical pin
    byte channels2[48];     // second channel, for double-click actions
    byte channelTypes[CHANNEL_MAX];
} pinsState_t;
```

Roles are an `IOR_*` enum: `IOR_None`, `IOR_Relay`, `IOR_Relay_n`, `IOR_Button`, `IOR_Button_n`,
`IOR_PWM`, `IOR_PWM_n`, `IOR_Button_ToggleAll`, `IOR_Button_NextDimmer` and so on. The whole
configuration is one `mainConfig_t` written with a magic-and-size header for forward compatibility,
and edited through a web UI at `/cfg_pins` **[V]**.

Three bytes per pin expresses everything a switch, dimmer or plug needs. **If you want a proven
schema for "pin map as data", copy this shape rather than Tasmota's packed integers.** It is more
readable, it separates role from channel binding, and it does not burn five bits on an instance
index.

**Correction to a common belief: OpenBeken does not import Tasmota templates.** A grep of the
whole tree finds only *protocol* compatibility, meaning Tasmota-style HTTP and MQTT commands,
`drv_tasmotaDeviceGroups.c`, and `Dimmer` and `CT` command aliases. There is no GPIO template
parser or converter anywhere in the repository **[V]**. Any claim otherwise, including my own
earlier assumption, is false.

### 3.4 Tasmota templates as an import source: quantified

This is where the project's specific hardware makes the answer much better than it first looks.

**The TYWE2L exposes exactly five host GPIOs, and we know which ones.** From the TYWE2L datasheet
held in this repository (`hardware/datasheets/TYWE2L Module Datasheet_Tuya Smart_Docs Center.pdf`,
Table 1) **[V]**:

| Module pin | Symbol | ESP8285 function |
| --- | --- | --- |
| 1 | 14 | GPIO_14 |
| 2 | 12 | GPIO_12 |
| 3 | 13 | GPIO_13 |
| 4 | 5 | GPIO_05 |
| 5 | 4 | GPIO_04 |
| 6 | GND | ground |
| 7 | 3V3 | 3.3 V supply |

The datasheet's own feature list says "Peripherals: five GPIOs" **[V]**. So the module's host
footprint is a fixed set: **{GPIO4, GPIO5, GPIO12, GPIO13, GPIO14}**.

The datasheet's own feature list confirms it: "Peripherals: five GPIOs" **[V]**. The only other
pads are test points: IO2 (UART1_TXD), RST and IO0.

**The pin-number half of the translation is completely mechanical.** Tasmota slot index to ESP8285
GPIO is a fixed 14-entry table taken from source. ESP8285 GPIO to TYWE2L pad is a fixed 5-entry
table taken from the datasheet. Composed **[V]**:

```
slot 4  -> GPIO4  -> pad 5
slot 5  -> GPIO5  -> pad 4
slot 8  -> GPIO12 -> pad 2
slot 9  -> GPIO13 -> pad 3
slot 10 -> GPIO14 -> pad 1
slots 0,1,2,3,6,7,11,12,13 -> not broken out on TYWE2L
```

Write that table once and it is exact.

**How much of the corpus is representable?** Measured two ways, independently, with agreeing
results. Parsing the published `templates.json` from
[templates.blakadder.com](https://templates.blakadder.com/) gave 271 of 889. Parsing the repository
files directly gave **273 of 900, or 30%** **[V]**. The remaining 70% need a pin the TYWE2L does
not break out.

Physical GPIO usage frequency across those 900 templates **[V]**:

| GPIO | Uses | On TYWE2L? |
| --- | --- | --- |
| 13 | 629 | yes |
| 12 | 591 | yes |
| 5 | 560 | yes |
| 14 | 534 | yes |
| 4 | 530 | yes |
| 3 (RXD) | 286 | no |
| 0 | 221 | no |
| 15 | 188 | no |
| 1 (TXD) | 171 | no |
| 16 | 144 | no |
| 2 | 124 | no |

**The five TYWE2L pads are the five most-used pins in the entire corpus.** That is not a
coincidence: they are the pins the popular Tuya modules break out. The 70% that fail are mostly
devices built on TYWE3S and similar modules that expose more pins, plus TuyaMCU devices that need
the UART on GPIO1 and GPIO3.

Function distribution across those 900 templates **[V]**: `GPIO_REL1` 928, `GPIO_KEY1` 657,
`GPIO_PWM1` 655, `GPIO_LED1_INV` 310, `GPIO_LED1` 232, energy metering roughly 110 each,
`GPIO_LEDLNK` 105, `GPIO_SWT1` 81, TuyaMCU TX and RX 69 each. Most templates assign between two
and six functions.

**What does not transfer, and this is where human judgement enters [V]:**

- **PWM frequency, dimming curve, gamma and minimum-brightness cutoff.** Tasmota holds these as
  separate `PWMFrequency` and `DimmerRange` settings, sometimes in the optional `CMND` field,
  often nowhere at all.
- **Pull-up and pull-down requirements.** The `KEY` versus `SWT` distinction and the `_NP` and
  `_PD` variants encode some of this, but not the electrical detail.
- **Strapping constraints.** GPIO0, GPIO2 and GPIO15 have strapping roles on ESP8285, and ESP32-H2
  has an unrelated set of five: GPIO2, GPIO3, GPIO8, GPIO9 and GPIO25. This does not bite us for
  TYWE2L devices, because none of the five ESP8285 leg nets is a strapping pin on that part and,
  under the v2 mapping, none of the five H2 host pins is a strapping pin either. It will bite any
  later attempt to support wider modules.
- **Which module the device actually contained.** This is the killer. The corpus does not record
  it in machine-readable form: a `chip` field appears on only 380 of 2,871 entries and a `module`
  field on 3. Module names appear in free prose only, at roughly TYWE3S 89, TYWE2S 82, TYWE1S 8
  and **TYWE2L 5**.

So pin-compatibility is **necessary but not sufficient**. A template that fits within the five
pads is consistent with a TYWE2L, and the device might equally use a different module that
happens to use the same pins. Confirmation needs a photograph, an FCC ID, or a person who has
opened one.

**OpenBeken's device database has the module field that blakadder lacks, and none of the pin data
we need.** [`openbekeniot.github.io/webapp/devices.json`](https://openbekeniot.github.io/webapp/devices.json)
is a real, live, 487 KB JSON file holding **889 devices** **[V]**, backed by
[OpenBekenIOT/webapp](https://github.com/OpenBekenIOT/webapp). Entries look like:

```json
{ "vendor": "Generic", "name": "WiFi DIY Switch", "model": "ZN268131",
  "chip": "BK7231T", "board": "WB2S",
  "keywords": ["switch","relay","AP8506"],
  "pins": { "6": "Rel;1", "7": "WifiLED_n;1", "10": "Btn;1" } }
```

That `board` field carries the actual Tuya module part number, machine-readable, on **869 of 889**
entries **[V]**. Exactly what we need. The catch, and it decides the matter **[V]**:

| Chip | Entries | With a pin map |
| --- | --- | --- |
| BK7231N | 427 | 368 |
| BK7231T | 214 | 190 |
| BL602 | 52 | 48 |
| **ESP8266** | **26** | **0** |
| **ESP8285** | **11** | **0** |

**Zero of the 37 ESP-family entries carry a pin map.** They are catalogued by name and board only.
So for TYWE2L retrofit work, OpenBeken gives module identification and no pin data, and blakadder
gives pin data and no module identification. Neither hands us a finished device library.

Its real value is different and still substantial: 712 devices with pin maps, of which 480 use
five or fewer pins, all expressed in a clean role-and-channel vocabulary. **Use OpenBeken for the
schema shape and blakadder for the ESP pin data.**

**A concrete conclusion for the registry.** Do not import either corpus wholesale. Use them as a
**seeding and cross-checking tool**: a maintainer script that takes a candidate device, pulls the
matching Tasmota template, mechanically translates the pin functions, cross-references OpenBeken
for the module identity where an entry exists, and produces a draft profile marked `community`
with links back to both sources. A human then confirms it against a real device before it is
promoted to `verified`. What transfers cleanly is the **archetype**: device name, vendor, model,
category, how many relays, how many buttons, how many PWM channels, whether there is a link LED,
and the inverted-logic hint. That turns a device confirmation into a minute's work rather than an
hour's, without importing either corpus's quality problems.

The 273 pin-representable templates are the realistic day-one import set, and they are dominated
by exactly the archetypes you would expect: single and double relay with button and LED, and two
to five channel PWM bulbs.

---

## 4. Registry design

### 4.1 What the comparable registries actually look like

I examined two in detail, one that works well and one that does not.

**devices.esphome.io: the model to copy [V].**

Repository: [github.com/esphome/devices.esphome.io](https://github.com/esphome/devices.esphome.io),
GPL-3.0. An Astro plus Starlight site. **780 device directories** under `src/docs/devices/`, each
holding an `index.md` with YAML front matter and prose.

The schema is enforced with Zod, in `src/content.config.ts` **[V]**, with the enumerations
factored into `src/utils/validFrontmatter.ts` **[V]**:

```ts
export const VALID_TYPES = new Set([
  "dimmer", "light", "misc", "plug", "relay", "sensor", "switch",
]);
export const VALID_BOARDS = new Set([
  "bk72xx", "esp32", "esp8266", "ln882x", "rp2040", "rtl87xx",
]);
export const VALID_STANDARDS = new Set([
  "au", "br", "eu", "global", "in", "uk", "us",
]);
```

Seven device types. Six chip families, including the Beken, Lightning and Realtek parts that Tuya
uses. Seven electrical standards, and `au` is one of them. The schema also carries `difficulty`
constrained to 1 to 5, `date-published`, `project-url`, `manufacturer`, `model` and a
`made-for-esphome` flag.

The CI is the interesting part **[V]** (`.github/workflows/`):

- `ci.yaml` runs markdownlint, yamllint, a dedicated front-matter linter on changed files only,
  and a line-endings check.
- `npm run validate-devices` runs `scripts/validate-devices.ts` and
  `scripts/validate-yaml-configs.ts`. The second one **actually validates the embedded device
  configuration**, not just the metadata around it.
- A set of `made-for-esphome-*` workflows implement a review tier. The header comment on
  `made-for-esphome-review.yml` is worth quoting in full, because it is the exact problem we will
  have **[V]**:

  > SECURITY: compiling a fork PR's linked YAML runs the ESPHome/PlatformIO toolchain on
  > contributor-controlled input (external_components can execute code at codegen time). This
  > workflow therefore runs in the secret-free `pull_request` context with `contents: read` and no
  > secrets [...] The review is posted by the separate `workflow_run` workflow, which never checks
  > out or executes PR code.

  That split, untrusted work in a no-secrets context and privileged commenting in a separate
  `workflow_run` job, is the pattern to adopt from day one.

Even this well-run registry shows drift: the Zod schema accepts both `model` and `Model` as
separate optional fields **[V]**. Case-insensitivity is worth building in from the start.

**The Tasmota template database: the failure modes to avoid [V].**

Repository: [github.com/blakadder/templates](https://github.com/blakadder/templates), EPL-2.0,
a Jekyll site. **2,872 template files** under `_templates/`, plus 181 under `_unsupported/` and
2,858 asset files. The full repository tarball is about 170 MB, almost all images.

Contributions arrive through a **Google Form**, per the README **[V]**. There is no schema, no
CI validation, and no record of whether anyone tested the entry on real hardware.

The published machine-readable index is broken. `templates.json` is generated by a Liquid template
that emits most fields through `jsonify` but writes the `template` value raw. The result: **all
2,861 entries have an unquoted `template` value, so the published file is not valid JSON** **[V]**.
Parsing fails at the first entry whose template is free text rather than a JSON object, for
example `"template": Module 1,`. Any consumer has to repair the file with regular expressions
before use, which is exactly what I had to do to produce the numbers in section 3.4.

Other observable symptoms of having no schema **[V]**:

- Category values are inconsistently cased: `switch` and `Switch`, `misc` and `Misc`, `sensor` and
  `sensors`, all present as distinct values.
- The template field name varies by chip and Tasmota version: `template9`, `templates2`,
  `templates3`, `templatec3`, `templatec6` all appear, and the index-generating Liquid has to try
  each in turn.
- Device files have no file extension at all, which is a Jekyll collection detail leaking into the
  data.

None of this means the corpus is worthless. It is the largest body of Tuya device knowledge in
existence and it is EPL-2.0 licensed. It means the corpus is an **input to a curation process**,
not a dependency.

**Zigbee2MQTT [U].** `zigbee-herdsman-converters` is MIT-licensed, holds device definitions as
TypeScript under `src/devices/` grouped by manufacturer, matches hardware on `zigbeeModel` or a
`fingerprint` combining `modelID` and `manufacturerName`, and requires `pnpm run check`,
`pnpm run build` and `pnpm test` to pass before a PR is accepted. Since version 22.0 the external
definition API is `addExternalDefinition`, which lets a user add a device without waiting for a
release. Verified only at README level. The lesson worth taking: **typed and tested definitions in
the repo, plus an escape hatch for users who cannot wait for a release**, is the right combination.

### 4.2 Recommended registry design

**One repository, separate from the firmware.** Firmware releases and device additions have
completely different cadences and completely different reviewers.

```
tywe2l-profiles/
  schema/
    profile-1.0.0.schema.json      JSON Schema 2020-12
    CHANGELOG.md
  profiles/
    <vendor>/<slug>.json           one device per file
  media/
    <vendor>/<slug>/*.webp         board photos, size-limited
  scripts/
    build-index.ts                 emits index.json and search-index.json
    import-tasmota.ts              maintainer seeding tool, section 3.4
    validate.ts                    ajv, plus semantic rules
  .github/
    workflows/validate.yml         schema plus semantic checks on PR
    workflows/publish.yml          builds index, deploys Pages on merge to main
    ISSUE_TEMPLATE/new-device.yml  GitHub issue form
    CODEOWNERS
```

**Format: JSON, not YAML.** The registry is read by the flasher, which is TypeScript in a browser.
JSON parses natively, is unambiguous, and has no significant-whitespace failure mode for
contributors editing in the GitHub web UI. Use JSON Schema **2020-12** and validate with `ajv` in
CI. Contributors who prefer YAML can use the issue form, which the bot converts.

**Profile shape**, sketched to show the required fields rather than as a final design:

```json
{
  "schema_version": "1.0.0",
  "id": "tuya/generic-2ch-switch-tywe2l",
  "revision": 3,
  "name": "Generic 2-gang Wi-Fi switch (TYWE2L)",
  "manufacturer": "unbranded",
  "models": ["TS0002", "QS-WIFI-S02"],
  "standard": ["au", "eu"],
  "original_module": "TYWE2L",
  "firmware_variant": "relay-nga",
  "trust": "verified",
  "verified_by": ["gh:someuser"],
  "verified_at": "2026-07-02",
  "source": { "type": "tasmota", "url": "https://templates.blakadder.com/..." },
  "hardware": {
    "mains": true,
    "pins": [
      { "board": 1, "role": "relay",  "index": 0, "active": "high", "boot": "off" },
      { "board": 2, "role": "relay",  "index": 1, "active": "high", "boot": "off" },
      { "board": 3, "role": "button", "index": 0, "active": "low",  "pull": "up"  },
      { "board": 4, "role": "led",    "index": 0, "active": "low",  "boot": "off" },
      { "board": 5, "role": "unused" }
    ]
  },
  "endpoints": [
    { "type": "on_off_plugin_unit", "binds": { "relay": 0, "button": 0, "led": 0 } },
    { "type": "on_off_plugin_unit", "binds": { "relay": 1 } }
  ]
}
```

Note that pins are addressed by **board pad number**, not by H2 GPIO number. That keeps profiles
valid across a board revision that moves a signal, since the firmware variant owns the
pad-to-GPIO table.

The `role` and `index` pairing is deliberately borrowed from OpenBeken's `roles[]` and `channels[]`
arrays rather than from Tasmota's packed integers (section 3.3). It is more readable in a diff, it
separates the pin's function from what that function is bound to, and it does not spend five bits
on an instance index. OpenBeken's `IOR_*` enum, exercised across 712 devices, is the right starting
vocabulary for the `role` values **[V]**.

**Validation in CI, in two layers.**

1. *Schema*: `ajv` against the JSON Schema. Types, enumerations, required fields, patterns.
2. *Semantic*, which is where the real value is and which a schema cannot express:
   - no two pins claim the same board pad;
   - every `binds` reference points at a pin that exists and has the matching role;
   - the `firmware_variant` exists in the current firmware manifest and supports every role used;
   - a mains device declares a `boot` state for every output pin (see section 5);
   - `models` values are unique across the whole registry, so device identification stays
     unambiguous;
   - the file name matches the `id`.

Run these on PRs in the secret-free `pull_request` context, following the ESPHome pattern above.

**Versioning, three separate things that are easy to conflate.**

| What | Mechanism |
| --- | --- |
| Schema | Semver on `schema_version`. Additive changes bump minor. The flasher supports a range and refuses profiles above it, with a clear "update your browser tab" message. |
| A single profile | Integer `revision`, incremented on any change. Cheap to compare, no ordering ambiguity. |
| The registry as a whole | The index carries a build timestamp and the source commit SHA. The flasher shows it. When a user reports a problem, that SHA identifies exactly what they flashed. |

Do not use git tags as the primary version mechanism. Contributors will not manage them.

**The built index.** A CI job on merge to `main` emits two artefacts to GitHub Pages:

- `index.json`: one compact record per device, holding only what search and the picker need
  (id, name, manufacturer, models, standard, type, firmware variant, trust, revision). At a few
  thousand devices this stays comfortably under a megabyte and can be gzip-served.
- `search-index.json`: a serialised MiniSearch index over the same records.

**Search library: MiniSearch [V].** Version **7.2.0**, MIT, **zero runtime dependencies**,
published 16 September 2025, roughly 86 KB unminified UMD build. It has
`MiniSearch.loadJSON(json, options)` and `loadJSONAsync` for loading an index built elsewhere
([npm](https://www.npmjs.com/package/minisearch)).

For comparison, all verified from npm registry metadata **[V]**:

| Library | Latest | Published | Deps | Licence | Note |
| --- | --- | --- | --- | --- | --- |
| **MiniSearch** | 7.2.0 | 16 Sep 2025 | 0 | MIT | Prebuilt index via `loadJSON`. Recommended. |
| Fuse.js | 7.5.0 | 13 Jul 2026 | 0 | Apache-2.0 | Fuzzy matching, builds its index at load. Good for small sets. |
| FlexSearch | 0.8.212 | 6 Sep 2025 | 0 | Apache-2.0 | Fastest, but the largest package and the most idiosyncratic API. |
| Lunr.js | 2.3.9 | **20 Aug 2020** | 0 | MIT | Effectively unmaintained. Avoid. |

MiniSearch wins on the combination of prebuilt-index support, zero dependencies and a small,
stable API. Fuzzy matching matters here because users will type "sonof" and "smart plug 16a".
Configure prefix and fuzzy search on the name and model fields.

**Distribution. [U] on the specifics, so verify before relying on the limits.**

Serve the index and the profiles from **GitHub Pages**, from the registry repository. Pages sends
permissive CORS headers and is CDN-backed. GitHub's documented Pages limits are soft: roughly 1 GB
of published site and 100 GB of bandwidth per month. Our text payload is trivially inside that;
device photographs are the thing that could grow, so cap image size in CI and consider keeping
media out of the index entirely.

Do **not** fetch profiles from `raw.githubusercontent.com` at runtime. It is rate-limited, its
caching behaviour is unhelpful, and GitHub have discouraged using it as a CDN. If Pages ever
becomes a constraint, jsDelivr's GitHub endpoint is the natural next step.

**Do not plan on fetching GitHub Releases assets from the browser either.** Independent tests in
this research found the download redirect landing on different blob hosts
(`objects.githubusercontent.com` in one run, `release-assets.githubusercontent.com` in another),
with **no `Access-Control-Allow-Origin` header on any hop and an explicit `OPTIONS` preflight
returning 404** **[V** as a test result, **U** as documented behaviour**]**. Releases remain fine
for firmware binaries that a *native* tool or a CI job downloads, and unusable for anything the web
installer must `fetch()`. Publish the firmware binaries the web app needs to Pages alongside the
index.

**Contribution flow, designed to keep the reviewer sane.**

1. A **GitHub issue form** (`.github/ISSUE_TEMPLATE/new-device.yml`) collects the device details in
   structured fields with dropdowns for the enumerated values. Non-technical contributors never see
   JSON.
2. An Action converts a valid submission into a branch and opens a PR, so the schema and semantic
   checks run against real data and the contributor sees a green or red tick. **[U]**
   `peter-evans/create-pull-request` is the usual Action for this; not verified this session.
3. **CODEOWNERS** requires a maintainer review for anything under `profiles/`, and a stricter set
   of owners for `schema/` and the firmware manifest.
4. The `trust` field can only be raised to `verified` by a CODEOWNER, enforced by a CI check that
   compares the diff against the PR's approvers. This is the single most important rule in the
   whole registry and section 5 explains why.

---

## 5. Safety and provenance

A device profile is a pin map for a mains-powered appliance. A wrong entry can hold a relay closed
at boot, drive two outputs into each other, or PWM a pin that is wired to a triac gate. This
section is about making the failure modes boring.

### 5.1 What the hardware already gives us

Three verified facts about the v2 pin mapping work in our favour **[V]**. All three are
consequences of the mapping rather than luck, and the working is in
[`docs/pin-mapping-v2.md`](./pin-mapping-v2.md):

- **No host GPIO is a strapping pin.** ESP32-H2 has five: GPIO2, GPIO3, GPIO8, GPIO9 and GPIO25
  (TRM v1.1 Register 6.7, section 8.2.2). The v2 host lines are GPIO13, GPIO14, GPIO12, GPIO10 and
  GPIO11, and none of them appears in the strapping latch. *This was not true of the old v1
  mapping, which had legs 4 and 3 on GPIO2 and GPIO3, and it is why the mapping changed.*
- **No host GPIO is a JTAG pad.** MTMS, MTDO, MTCK and MTDI are GPIO2 to GPIO5, all spares under
  v2. Even in the pad-JTAG failure case the chip cannot drive a host line.
- **Every host GPIO comes out of reset in the cleanest state the part offers**, reset code `0` in
  TRM Table 6.13-1: input disabled, no pull, nothing driving. In particular none of them carries the
  MTCK after-reset pull-up that vetoed GPIO4, which would otherwise hold a line towards 3V3 for 200
  to 400 ms on every boot.

That leaves one hazard the silicon cannot solve: **between power-on and the first line of our
firmware, the host GPIOs are high-impedance inputs.** Whatever the device does during that window
is decided by the circuit, not by us.

**Recommendation, and it is a hardware one: every output that can energise something must have an
external pull-down (or pull-up, matching the driver's inactive sense) on the replacement board or
in the host device.** Firmware safe-defaults cannot cover the boot window. Do not rely on the
original Tuya board having done this, because for some devices it will not have.

### 5.2 Firmware guardrails

In rough order of value:

1. **Safe state before anything else.** The very first thing after the bootloader hands over,
   before Wi-Fi, before Matter, before reading the profile: drive every host GPIO to its declared
   inactive level as an output. If the profile is unreadable, drive all five to the *variant's*
   conservative default, which for every relay and switch variant means off.
2. **Validate the profile, then act on it.** Magic number, schema version, length, CRC32 over the
   payload. Reject on any mismatch. A profile that fails validation must never be partially
   applied; half a pin map is more dangerous than none.
3. **Reject profiles the variant cannot honour.** If the profile asks for a role the running
   firmware does not implement, refuse the whole profile rather than ignoring that pin. Silent
   partial application is how a "spare" pin ends up floating on a live triac.
4. **Recovery mode.** If the profile is missing or invalid, or if the device reboots N times
   within M seconds, enter a recovery state: all outputs inactive, no Matter commissioning, a
   distinctive LED pattern if a status LED is known-safe, and the USB serial console available so
   the flasher can talk to it. The device must be recoverable without the user opening it again.
5. **Boot-count rollback.** Pair recovery mode with the ESP-IDF app rollback mechanism so a bad
   firmware update cannot brick a device that is screwed into a wall.
6. **Never PWM a pin whose role is not a PWM role.** Enforce in the profile parser, not by
   convention.
7. **Log the profile id and revision at boot**, so a support conversation starts with facts.

### 5.3 Trust and provenance model

The registry needs two tiers and no more. More tiers get ignored.

| Tier | Meaning | Gate |
| --- | --- | --- |
| `community` | Schema-valid and semantically consistent. Nobody has confirmed it on hardware. | Automated CI only |
| `verified` | A named maintainer has flashed this profile into this device and it behaved correctly. | CODEOWNER approval, enforced in CI |

Rules that make the tiers mean something:

- The flasher **shows the tier prominently** and requires an explicit extra confirmation before
  flashing a `community` profile into a mains device. The wording should be plain: nobody has
  tested this, check your wiring, you can break it.
- Promotion to `verified` records **who** and **when** in the profile itself, so provenance travels
  with the data instead of living in a PR thread.
- Any change to `hardware.pins` on a `verified` profile **drops it back to `community`** until
  re-verified. Enforce in CI by diffing against the base branch. This is the rule that prevents the
  most likely real-world accident, which is a well-meaning edit to a trusted entry.

This is a direct adaptation of the "Made for ESPHome" tier **[V]**, which pairs an automated,
deterministic review with a human sign-off and a visible badge.

**On signing [U].** Cryptographic signing of individual profiles is probably over-engineering for
phase one, and it introduces key management that a volunteer project will get wrong. The registry's
integrity comes from the same place every other package registry's does: a reviewed git history on
a hosted platform with branch protection. What we should do instead, and it is cheap:

- The flasher records the registry commit SHA and the profile revision it used, and displays them.
- Firmware binaries are built in CI from a tagged commit and published with checksums. If we ever
  do sign anything, sign the **firmware**, where the blast radius is larger and the key can live in
  CI.

Revisit signing if and when third parties start mirroring the registry.

### 5.4 The Matter consequence of a profile change

Worth stating plainly because it will surprise users. The endpoint composition is part of the
device's Matter data model. Reflashing a device with a different profile can change the number of
endpoints, their device types and their Descriptor cluster PartsList.

The SDK does signal structural changes. `esp_matter_data_model.cpp`'s `enable()` calls
`MatterReportingAttributeChangeCallback(..., EndpointChangeType::kAdded)` and then walks up the
parent chain to mark endpoint 0's `Descriptor::PartsList` dirty **[V]**. Live add and remove on a
commissioned node is demonstrated by the bridge examples, whose READMEs walk through reading
`PartsList` before and after a bridged device appears **[V]**.

But reflashing a different profile is not the same as a bridge adding an endpoint, and the
important parts remain unverified:

- **[U]** Fabrics, ACLs and NOCs live in `chip-config` NVS keyed independently of the data model,
  so a composition change should not by itself invalidate commissioning. This is inferred from the
  storage split, not from a spec citation.
- **[U]** Bindings and ACL entries are scoped to `{endpoint, cluster}`. If a profile change moves
  or removes an endpoint, existing targets dangle, and what each controller does about that is
  unknown.
- The CSA specification text on structural-change semantics is behind a paywall and could not be
  read **[V]** that it is paywalled.
- Relevant context: **Matter 1.4.2 introduced an Endpoint Unique ID** (a Descriptor attribute, up
  to 32 characters) precisely because endpoint ids were not stable across administrators or
  re-commissioning **[V]**
  ([CSA announcement](https://csa-iot.org/newsroom/matter-1-4-2-enhancing-security-and-scalability-for-smart-homes/)).
  esp-matter supports it behind `CHIP_CONFIG_USE_ENDPOINT_UNIQUE_ID` **[V]**. **Use it.** It is the
  mechanism designed for exactly our problem.

**The position to take, until tested.** For a factory or first-flash profile write this is a
non-issue. For re-profiling a device that is already commissioned, **plan on a factory reset and
re-commission**, tell the user so plainly, and design the firmware to clear Matter fabric data when
the profile changes rather than leaving a node that is commissioned but no longer shaped the way
its controller remembers. Treat "no re-commissioning needed" as unproven until it has been tested
against Apple, Google and Alexa.

---

## 6. Recommended stack, Matter credentials, and build order

### 6.1 What the flasher must generate per device

Every Matter device needs its own commissioning credentials. This is not optional and it cannot be
shared across units, so the flasher has to generate it on the user's machine at flash time.

**Discriminator.** 12 bits, 0 to 4095. Random per device.

**Setup passcode.** From `src/setup_payload/SetupPayload.cpp` in connectedhomeip **[V]**:

```c
bool PayloadContents::IsValidSetupPIN(uint32_t setupPIN)
{
    // SHALL be restricted to the values 0x0000001 to 0x5F5E0FE (00000001 to 99999998 in decimal),
    // excluding the invalid Passcode values.
    if (setupPIN == kSetupPINCodeUndefinedValue || setupPIN > kSetupPINCodeMaximumValue ||
        setupPIN == 11111111 || setupPIN == 22222222 || setupPIN == 33333333 ||
        setupPIN == 44444444 || setupPIN == 55555555 || setupPIN == 66666666 ||
        setupPIN == 77777777 || setupPIN == 88888888 || setupPIN == 12345678 ||
        setupPIN == 87654321)
    { return false; }
    return true;
}
```

Range 1 to 99999998, with those ten values excluded. Generate from a CSPRNG
(`crypto.getRandomValues`) and reject-and-retry, exactly as the SDK does.

**SPAKE2+ verifier.** The device stores a verifier, never the passcode. The derivation is fully
specified, and this is the finding that removes the need for any server-side component.

From `src/crypto/CHIPCryptoPAL.h` and `CHIPCryptoPAL.cpp` **[V]**:

```c
inline constexpr size_t   kSpake2p_Min_PBKDF_Salt_Length  = 16;
inline constexpr size_t   kSpake2p_Max_PBKDF_Salt_Length  = 32;
inline constexpr uint32_t kSpake2p_Min_PBKDF_Iterations   = 1000;
inline constexpr uint32_t kSpake2p_Max_PBKDF_Iterations   = 100000;
inline constexpr size_t   kSpake2p_WS_Length              = kP256_FE_Length + 8;   // 40
inline constexpr size_t   kSpake2p_VerifierSerialized_Length
                            = kP256_FE_Length + kP256_Point_Length;                // 32 + 65 = 97
```

and

```c
CHIP_ERROR Spake2pVerifier::ComputeWS(uint32_t pbkdf2IterCount, const ByteSpan & salt,
                                      uint32_t setupPin, uint8_t * ws, uint32_t ws_len)
{
    Encoding::LittleEndian::Put32(littleEndianSetupPINCode.Bytes(), setupPin);
    ...
    return pbkdf2.pbkdf2_sha256(littleEndianSetupPINCode.Bytes(), 4,
                                salt.data(), salt.size(), pbkdf2IterCount, ws_len, ws);
}
```

In plain terms, and every step is available in a browser:

1. PBKDF2-HMAC-SHA256 over the passcode as a **4-byte little-endian integer**, with the salt and
   iteration count, producing **80 bytes**. `crypto.subtle.deriveBits` does this natively.
2. Split into two 40-byte halves. Reduce each modulo the P-256 group order `n` to get `w0` and `w1`.
3. `L = w1 * G`, a single P-256 scalar multiplication of the base point.
4. Serialise as `w0` (32 bytes) followed by `L` (65 bytes, uncompressed point), 97 bytes total,
   base64-encoded for NVS.

`@noble/curves` provides the P-256 arithmetic in pure JavaScript with no WebAssembly and no native
dependency. **A browser can generate a complete, correct set of Matter commissioning credentials
offline.** This is what makes the web installer a serious proposal rather than a compromise.

**Also per device:** a serial number, and optionally a rotating device ID unique identifier.

### 6.2 Attestation, vendor IDs, and the certification wall

**What the reference tooling does.** `esp-matter-mfg-tool`, PyPI version **1.0.24**, released
17 June 2026 **[V]** ([PyPI](https://pypi.org/project/esp-matter-mfg-tool/),
[source](https://github.com/espressif/esp-matter-tools/tree/main/mfg_tool)), generates the
manufacturing partition. Its output per device **[V]**:

```
out/<vid_pid>/<uuid>/
  <uuid>-partition.bin     the NVS factory partition to flash
  <uuid>-onb_codes.csv     QR payload string, manual pairing code, passcode, discriminator
  <uuid>-qrcode.png        the QR image
  internal/                DAC and PAI in DER and PEM
```

Relevant flags: `--target esp32h2`, `--dac-in-secure-cert`, `--commissionable-data-in-secure-cert`,
`--enable-rotating-device-id`, and `-n` for a batch count **[V]**.

**Where the credentials live.** Espressif's ESP32-H2 production guidance **[V]**
([docs](https://docs.espressif.com/projects/esp-matter/en/latest/esp32h2/production.html)) says the
manufacturing partition holds the commissioning data (discriminator, salt, iteration count,
SPAKE2+ verifier), the attestation data (Certification Declaration, PAI, DAC, DAC private key) and
device information, so that "the rest of the components like the bootloader, firmware image are
common across all your devices". That is precisely our architecture. By default the DAC and its
key go in the NVS factory partition; `--dac-in-secure-cert` moves them to a dedicated
`esp_secure_cert` partition. Espressif also offer pre-provisioned modules with the DAC key pair
and certificates pre-flashed at module manufacture **[V]**, which is worth pricing if this ever
goes past hobby scale.

**The vendor ID wall, and it is a wall.**

CSA membership pricing, from csa-iot.org **[V]**
([become a member](https://csa-iot.org/become-member/)):

| Tier | Annual | Per-product certification |
| --- | --- | --- |
| Associate | USD 0 | USD 2,500 per product plus USD 500 per year, white-label only |
| **Adopter** | **USD 7,500** | USD 3,000 new, USD 2,500 derivative |
| Participant | USD 21,500 | USD 2,000 new, USD 1,500 derivative |
| Promoter | USD 112,500 | USD 2,000 new, USD 1,500 derivative |

A Vendor ID requires membership. For an open-source retrofit project, that is not happening.

**So we use a test vendor ID.** 0xFFF1 to 0xFFF4 are reserved for testing, with 0xFFF1 the
conventional choice. The consequences, which the flasher's UI must state up front:

- **Apple Home**: commissioning proceeds, with a prompt that this is an uncertified accessory,
  and the user taps through it **[V]**
  ([matter.js ecosystems doc](https://github.com/matter-js/matter.js/blob/main/docs/ECOSYSTEMS.md),
  [Qorvo QMatter commissioning guide](https://github.com/Qorvo/QMatter/blob/v1.0.1/Documents/Guides/commissioning_with_apple.md)).
- **Google Home**: same uncertified prompt, and Google additionally expect test-VID devices to be
  registered as an integration in the Google Home Developer Console **[V]**
  ([Google Home Matter troubleshooting](https://developers.home.google.com/matter/troubleshooting),
  matter.js ecosystems doc). **[U]** Whether that registration is a hard requirement or only removes
  the warning is not something I could pin down definitively, and Google's own developer forum
  threads are inconsistent on it. **Test this early**, because Google Home is a large share of the
  likely user base.
- **Amazon Alexa**: matter.js report that "no special setup is needed to pair with matter.js as
  development device" **[V]**.
- **Home Assistant**: **[U]** not covered by the sources I read, but as an open ecosystem it is the
  least likely to object, and it is probably where most of our users will land anyway.

**On the DAC.** A certified product must protect the DAC private key. A DIY device on a test VID is
uncertified by definition, so the requirement is moot in the compliance sense. It is not moot in
the security sense: the flasher will be writing a private key into flash on the user's desk. Use
`--dac-in-secure-cert` and the `esp_secure_cert` partition if the H2 build supports it, treat the
test DAC as public knowledge (it is, it ships in the SDK's `credentials/test/attestation/`
directory **[V]**), and do not pretend otherwise in the documentation.

### 6.3 The QR code and pairing code flow

**What the payload is.** The QR payload is an `MT:` prefix followed by a base38-encoded structure
carrying the version, vendor ID, product ID, commissioning flow, a discovery capabilities bitmask,
the discriminator and the passcode. The manual pairing code is an 11-digit decimal string, with a
21-digit variant that also carries VID and PID. **[U]** on the exact bit layout and the Verhoeff
check digit; both are in the Matter specification and in
`connectedhomeip/src/setup_payload/`.

**Generating it in the browser [U], leaning towards yes.** `matter.js` includes
`QrPairingCodeCodec` and `ManualPairingCodeCodec`, and the project is TypeScript with the crypto
abstracted behind an interface, with the caveat that the maintainers say "no official browser
package exists" and non-Node environments must supply their own Network, Crypto and Storage
implementations **[V]** ([matter.js](https://github.com/matter-js/matter.js)). We only need the
codecs, not the stack, so the practical path is to lift or reimplement the base38 encoder, which is
a small, well-specified piece of code. Rendering the QR image itself is a solved problem with any
QR library.

**The discovery capabilities bitmask.** For an H2 that commissions over BLE and then joins a Thread
network, the BLE bit is the one that must be set. **[U]** Verify the exact bit values against the
specification before hard-coding them.

**The user-facing flow, which is where the real design work is.**

The original Tuya QR code on the device is now wrong, and worse than wrong, because it points at an
app that will fail. So:

1. After a successful flash, the app shows the QR code and the manual pairing code on screen,
   large, with the device name from the profile.
2. It offers a **printable label sheet** sized for the device, and a plain-text copy for a password
   manager. Both should carry the profile id and the flash date.
3. It tells the user, before they close the case: **stick the label on the device now, and cover or
   remove the old Tuya QR code.** Once the case is closed and the device is in the ceiling, an
   unrecorded passcode means opening it up and reflashing.
4. It should offer to re-display the credentials for a device flashed earlier in the same session,
   because people close tabs.
5. **[U]** Consider whether the flasher can read the credentials back off an already-flashed device
   over serial. If the factory partition is readable, this turns a lost label from a disaster into
   an inconvenience. Worth checking whether esp-matter exposes a console command for it.

### 6.4 Thread prerequisites, which will generate support tickets

**[U] but well established.** The ESP32-H2 has no Wi-Fi. A Matter-over-Thread device needs a
**Thread Border Router** on the user's network before it can be commissioned into a fabric: an
Apple TV or HomePod, a recent Google Nest Hub, an Amazon Echo with Thread, or Home Assistant with a
Thread radio and OpenThread Border Router. Commissioning itself happens over BLE from the phone,
then the device joins Thread.

If the user has no border router, commissioning will fail in a way that looks like the flash failed.
**The flasher should say this before the user opens the device**, not after. A single up-front
question, "do you have one of these?", with pictures, will save more support time than any other
piece of UI in the project.

Because these devices are mains powered, they should be configured as Thread **Routers** rather than
sleepy end devices. Each retrofitted device then strengthens the mesh, which is a genuinely nice
property of this project and worth saying out loud in the README.

### 6.5 Phased build order

**Phase 0: prove the hardware path.** Before any application code. Two of the original four
questions were answered by desk research (Windows driver binding, and 4 MB fitting dual-slot OTA),
so this phase is now about confirming them on the actual board and closing what desk research
cannot.

- Flash a stock esp-matter example onto the replacement board over native USB, from Chrome on
  Windows, macOS and Linux, on machines with no Espressif tooling installed. Documentation says
  this works with no driver step; confirm it on our board.
- **Characterise the re-enumeration behaviour**, which is the real unknown now. How many times does
  the port disappear during a full flash, and does Chrome return the same `SerialPort` object or
  demand a fresh `requestPort()`? This shapes the entire application state machine (section 1.6).
- Confirm manual download-mode entry, and settle the GPIO8 pull-up and BOOT pull-down on the board
  (section 1.5). Test the recovery path deliberately: flash firmware that disables the USB
  peripheral, then get the board back.
- Build the largest planned variant against `examples/light/partitions.csv` and measure the
  remaining headroom against the 1,966,080-byte slot. Decide the cluster-selection split from real
  numbers (section 3.1).
- Commission into Apple Home, Google Home and Home Assistant with a test VID, and record exactly
  what each shows the user. Establish whether Google Home Developer Console registration is
  actually required (section 6.2).

Do not proceed until all five are answered. Each can invalidate large parts of the design.

**Phase 1: one variant, one device, end to end.** A single firmware variant (the N-gang relay is
the most useful and the least risky), a hand-written profile, and a minimal web page that flashes
it and prints a QR code. No registry, no search, no schema. The goal is one person converting one
real switch and controlling it from their phone.

**Phase 2: the profile mechanism.** Formalise the profile binary format, the CRC and version
checking, the safe-state boot path and recovery mode. Build `partitions-tool-esp` in, with the
golden-file tests against Espressif's generator. Still no registry: profiles are files in the
firmware repository.

**Phase 3: the registry.** Schema, CI validation, the built index, MiniSearch, the issue form and
the PR bot, the two trust tiers. Seed it with the maintainer-run Tasmota import over the 271
pin-compatible candidates from section 3.4, all landing as `community` until someone verifies them.

**Phase 4: the rest of the variants, and the fallback path.** Dimmable, CCT, RGBW, sensor, plug.
The printed-command fallback for Safari and locked-down machines. Matter OTA, if it earns its
place.

**Explicitly not planned:** a native desktop app, CSA membership, and per-profile cryptographic
signing. Each of those should be triggered by a measured problem, not by anticipation.

---

## Appendix A: verification status

Verified against primary sources during this research:

- esptool-js 0.6.1 has an ESP32-H2 target and stub flasher (repository source).
- esp-web-tools accepts `"ESP32-H2"` as a chip family, depends on esptool-js ^0.6.0, and detects
  native USB CDC by Espressif's USB VID 0x303A (repository source).
- esptool 5.3.1 and espflash 4.5.0 are current (PyPI and crates.io APIs).
- ESP32-H2-MINI-1 pin definitions, strapping defaults, boot mode table, JTAG source table and ROM
  message printing tables (module datasheet v1.6, in this repository).
- TYWE2L exposes five host GPIOs: 4, 5, 12, 13, 14, on a 7-pin footprint (TYWE2L datasheet, in this
  repository).
- esp-matter creates endpoints at runtime by heap allocation into a linked list, caps them with
  `CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT` (default 16), provides `endpoint::resume`, and its
  `light` example contains no ZAP files (repository source).
- Tasmota template format, the 14-slot ESP8266 pin index mapping, and the `UserSelectablePins` enum
  ordering (`tasmota_template.h`).
- The blakadder template index: 2,861 entries, invalid JSON as published, inconsistent category
  casing, multiple chip-specific template field names, Google Form contribution flow, EPL-2.0.
- 271 of 889 parsable ESP8266 templates use only pins within the TYWE2L footprint (own analysis of
  the published index).
- devices.esphome.io: 780 device directories, Zod front-matter schema, the valid type, board and
  standard enumerations, the CI workflow set, and the Made-for-ESPHome review split.
- MiniSearch 7.2.0, Fuse.js 7.5.0, FlexSearch 0.8.212, Lunr 2.3.9 metadata (npm registry).
- `partitions-tool-esp` 0.2.0 and `@m1kad0/esp-nvs-utils` 0.2.4 exist, are Apache-2.0 and claim
  browser support (npm registry).
- `esp-idf-nvs-partition-gen` 0.3.0 on PyPI.
- Matter passcode validity rules (connectedhomeip `SetupPayload.cpp`).
- SPAKE2+ verifier derivation, salt and iteration bounds, and the 97-byte serialised form
  (connectedhomeip `CHIPCryptoPAL.h` and `CHIPCryptoPAL.cpp`).
- `esp-matter-mfg-tool` 1.0.24, its CLI flags and its output layout.
- Espressif ESP32-H2 Matter production guidance on factory partitions and pre-provisioned modules.
- CSA membership and certification pricing (csa-iot.org).
- Azure Artifact Signing country eligibility, including that Australian individuals are excluded
  and Australian organisations are not (Microsoft Learn).

Verified in the second research pass, after the first draft of this document:

- Web Serial support matrix, including Firefox 151 shipping it on 19 May 2026, WebKit's `oppose`
  position, Mozilla's move to `neutral`, and Chrome Android's Bluetooth-only limitation.
- Windows binds `usbser.sys` to the H2 automatically; Zadig applies only to the vendor-specific
  JTAG interface, not to flashing.
- esptool v4.5, esptool-js v0.3.0 and espflash 2.0.0-rc.4 as the versions where H2 support landed,
  plus the esptool-js 0.5.6 chip-detect magic that gates current silicon.
- The USB Serial/JTAG reset sequence, PID-based path selection, and esptool-js's lack of reconnect
  logic.
- The H2 eFuse list relevant to disabling USB and download mode, and the recovery procedure.
- Linux group names per distribution, the `uaccess` udev rule, the ModemManager ignore variables,
  and Chromium's plain `open(2)` in `serial_io_handler.cc`.
- esp-matter's `DataModel::Provider` architecture, the 139 `CONFIG_SUPPORT_*_CLUSTER` flags, the
  `all_device_types_app` profile-in-NVS precedent, and real ESP32-H2 flash and RAM figures showing
  dual-slot OTA fits in 4 MB.
- `partitions-tool-esp` producing byte-identical NVS output to `esp-idf-nvs-partition-gen`.
- Tasmota's `AGPIO(x) ((x)<<5)` encoding and the slot-to-GPIO remapping in `support.ino`.
- ESPHome having no Matter support at all, and OpenBeken not importing Tasmota templates.
- OpenBeken's `pinsState_t` schema and its 889-device database, including that no ESP-family entry
  carries a pin map.
- Espressif's own signed binaries still being flagged by antivirus software.

Still not verified, and listed here so nobody mistakes them for research:

- Current OV and EV code signing certificate prices, Azure Artifact Signing dollar pricing, and
  Microsoft's current SmartScreen reputation policy.
- Apple Developer Program pricing and current Gatekeeper behaviour on recent macOS.
- Bundle sizes for Tauri and Electron.
- Whether any comparable project built and abandoned a native flasher.
- Zigbee2MQTT internals beyond README level.
- GitHub Pages, jsDelivr and Releases limits and CORS behaviour, in detail. One partial result:
  GitHub Releases assets appear to send no `Access-Control-Allow-Origin` on any hop and reject an
  `OPTIONS` preflight, which would rule them out as a browser-fetchable artefact host. Confirm
  before relying on it either way.
- The Matter QR payload bit layout and the discovery capabilities bitmask values.
- How ecosystem controllers react to a structural data-model change on a commissioned node.
- Whether Google Home Developer Console registration is mandatory for test-VID devices.
- Whether `web.esphome.io` compiles firmware or only flashes prebuilt binaries.
- Snap and Flatpak serial confinement, and Firefox 151's Linux permission behaviour.
