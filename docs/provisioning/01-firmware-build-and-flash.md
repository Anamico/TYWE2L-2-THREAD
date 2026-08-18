# Stage 1: firmware build and flash, with per-device Matter identities

Covers: installing the toolchain, building `software/matter-onoff-poc`, generating a batch of
unique per-device Matter commissioning identities with `esp-matter-mfg-tool`, and flashing
firmware plus one identity per physical unit. Written from an actual session getting this exact
project from a fresh checkout to a commissioned, working light — every gotcha below was hit for
real, not anticipated in the abstract.

**Scope note.** This stage builds `software/matter-onoff-poc`, the DevKitM-1 feasibility PoC —
not the TYWE2L-leg-mapped carrier board firmware, which does not exist yet. The build/flash/
mfg-tool mechanics are identical either way; only the firmware source and the LED-vs-PWM-leg
wiring differ. This doc will get a note pointing at the real device firmware once that exists.

---

## 1. Toolchain: ESP-IDF via EIM

Install ESP-IDF using **EIM** (ESP-IDF Installation Manager), Espressif's current installer —
not a manual `git clone` of `esp-idf`. Get it from
<https://docs.espressif.com/projects/idf-im-cli/en/latest/installation.html>.

### 1.1 Python version gotcha on macOS (pyenv + Apple Silicon)

EIM requires Python 3.10–3.14 on the interpreter that's on `PATH` when you run it — it has no
flag to point at a specific interpreter, it just uses whatever `python3` resolves to
(`idf-im-ui` CLI docs, checked directly, only Python-related flag is
`--python-env-folder-name`, nothing for interpreter path).

If you're on a **pyenv-built Python 3.11 on Apple Silicon macOS**, you may hit:

```
ERROR:root:code for hash blake2b was not found.
ValueError: unsupported hash type blake2b
```

This is a known pyenv/Homebrew interaction (`pyenv/pyenv#2573`): a Homebrew `libb2` library gets
detected and linked incorrectly during the pyenv build, breaking just blake2b/blake2s while every
other hash algorithm works. It's often **non-fatal noise** — things can still work despite the
traceback printing on every Python invocation — but if it does block something (some ESP-IDF
build steps use `blake2s` for app image versioning), fix the root cause rather than work around
it:

```bash
brew uninstall libb2
pyenv install 3.11.10 --force   # or whatever patch version you're on
```

Don't run `eim install` from your shell's default Python if it's a different, incompatible
version — use `pyenv local <version>` in the directory you're working from (needs pyenv's shims
on `PATH`; see `pyenv init` in pyenv's own docs), or prefix the one command:
`PATH="$(pyenv root)/versions/<version>/bin:$PATH" eim install`.

### 1.2 Activate ESP-IDF in every new shell

```bash
source "$HOME/.espressif/tools/activate_idf_v6.0.2.sh"   # or whichever version EIM installed
```

Confirms itself with lines like `Activated virtual environment at ...` and
`You are now using IDF version 6.0.2.` — if you don't see that, nothing downstream will work.

---

## 2. Clone `esp-matter` separately

`esp-matter` is a **separate checkout from ESP-IDF**, cloned once, not managed by EIM:

```bash
cd ~   # anywhere outside this git repo — it's a large separate SDK, not project content
git clone --depth 1 https://github.com/espressif/esp-matter.git
cd esp-matter
git submodule update --init --depth 1
cd connectedhomeip/connectedhomeip
./scripts/checkout_submodules.py --platform esp32 linux --shallow
cd ../..
./install.sh
```

### 2.1 The selective submodule checkout can miss things — expect to rerun it

`checkout_submodules.py --platform esp32 linux --shallow` is connectedhomeip's own tool for
pulling in only the `third_party/` submodules a given platform needs, rather than the entire
tree (which is many GB across every supported platform). In practice, across one session
building this exact project, it **silently missed individual submodules on its first run** —
`uriparser`, then later `jsoncpp` and `nlio`, discovered one at a time as the build got further
each time:

```
CMake Error ... Include directory '.../third_party/uriparser/repo/include' is not a directory.
```
```
ERROR at //third_party/connectedhomeip/third_party/jsoncpp/BUILD.gn:26:1: Source file not found.
```

**If you see this, don't patch submodules one at a time.** Rerun the whole selective script — it's
idempotent, only fetches what's actually missing:

```bash
cd ~/esp-matter/connectedhomeip/connectedhomeip
./scripts/checkout_submodules.py --platform esp32 linux --shallow
```

If a specific single submodule is still missing after that, you can target it directly:
```bash
git submodule update --init --depth 1 third_party/<name>/repo
```

### 2.2 Activate `esp-matter` too, after ESP-IDF, every new shell

```bash
source "$HOME/.espressif/tools/activate_idf_v6.0.2.sh"   # ESP-IDF first
cd ~/esp-matter && source ./export.sh && cd -             # esp-matter second
```

Order matters — `esp-matter`'s `export.sh` expects `IDF_PATH` already set.

---

## 3. Build the firmware

```bash
cd software/matter-onoff-poc
idf.py set-target esp32h2
idf.py build
```

`set-target` only needs to run once (it regenerates `sdkconfig` from `sdkconfig.defaults` +
`sdkconfig.defaults.esp32h2`). **Skipping straight to `idf.py build` on a fresh checkout fails**:
this project's `CMakeLists.txt` deliberately refuses to guess a default target.

If a previous build attempt left a `build/` directory that never got a valid `CMakeCache.txt`
(e.g. it failed during the cmake configure step), `set-target`'s automatic clean step will refuse
to touch it — `"doesn't seem to be a CMake build directory. Refusing to automatically delete
files"`. Fix: `rm -rf build sdkconfig sdkconfig.old`, then `set-target` again.

### 3.1 Two config bugs already found and fixed in this project

Both are already applied in `software/matter-onoff-poc/sdkconfig.defaults` and
`sdkconfig.defaults.esp32h2` — read those files' inline comments for the full derivation. Noted
here so the reasoning isn't only discoverable by diffing config files:

- **`esp_matter_bridge` compiles even though this project never uses bridging**, and its
  internal endpoint-count arithmetic goes negative at a very small
  `CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT`. There is no `EXCLUDE_COMPONENTS` mechanism in
  ESP-IDF's build system to drop the unused component (checked directly against the current
  CMake build-system docs — a first attempt to use one silently did nothing and wasted a build
  cycle). Fix: raise `CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT` enough to keep that unused
  component's arithmetic non-negative (currently `3`; upstream's own default is 16).
- **OpenThread's default message buffer count (65) is too few** for BLE commissioning + Thread +
  SRP service registration all running close together — this manifested as
  `chip[DIS]: Failed to advertise ...: 3` (OpenThread's `OT_ERROR_NO_BUFS`, confirmed against
  OpenThread's own `error.h`), which stalled commissioning until the fail-safe timer expired and
  rolled the whole pairing back. Fix: `CONFIG_OPENTHREAD_NUM_MESSAGE_BUFFERS=128` (each buffer is
  a small, fixed OpenThread constant, so this costs only a few KB against the H2's ~157KB heap).

If `idf.py build` fails on something else, check `main/idf_component.yml`'s pinned
`espressif/led_strip` version against whatever ESP-IDF version you're actually on — that's the
most likely remaining version-skew point.

---

## 4. Generate per-device Matter identities with `esp-matter-mfg-tool`

**This is the step that makes 130 units possible without every one of them sharing the same
BLE-advertised discriminator and passcode** — see `docs/matter-identity-and-certification.md` and
`docs/flasher-and-registry.md` section 6.1 for why a shared identity across units is a real
problem, not a cosmetic one. The bench PoC uses one fixed shared test identity
(`software/matter-onoff-poc/README.md`, "Commissioning codes for this build") specifically
because it's a single unit; that approach does not scale past one board.

### 4.1 Install

```bash
python3 -m pip install esp-matter-mfg-tool
```

### 4.2 The VID/PID choice for batch provisioning, and why it differs from the PoC firmware default

The firmware as currently built uses the CHIP SDK's absolute defaults, **VID `0xFFF1` / PID
`0x8000`**, deliberately left un-overridden (see the PoC README's "Matter identity" section).
For **mfg-tool-generated batches specifically, use VID `0xFFF2` / PID `0x8001` instead** — this
is not an arbitrary substitution. `esp-matter-mfg-tool`'s own documented example uses exactly
this pairing, because connectedhomeip's `credentials/test/` tree ships a **self-consistent** set
of test files under this exact name:
`Chip-Test-PAI-FFF2-8001-{Key,Cert}.pem` and `Chip-Test-CD-FFF2-8001.der`.

**This project's own current VID/PID (`0xFFF1`/`0x8000`) does not have an equivalent
self-named PAI.** Checked directly against connectedhomeip's `credentials/test/attestation/`
listing: a `Chip-Test-DAC-FFF1-8000-...` file exists, but its issuer PAI is named
`...-Issuer-PAI-FFF2-8004-Cert.pem` — a *different* VID/PID pairing, not FFF1/8000's own. Using
mfg-tool's documented FFF2/8001 pairing avoids that mismatch entirely rather than requiring you
to hunt down and verify a correct but differently-named PAI/CD combination for FFF1/8000.

**[U] Not verified in this session, and worth confirming before trusting for all 130 units:**
whether the factory (`fctry`) NVS partition's VID/PID values take precedence over the firmware's
compiled-in `CONFIG_DEVICE_VENDOR_ID`/`CONFIG_DEVICE_PRODUCT_ID` at runtime once a factory
partition is present. Espressif's stated architecture (one firmware image, per-device factory
data) implies yes — that's the entire point of the split — but this project's firmware has not
yet been built and flashed against a real mfg-tool-generated factory partition to confirm it
directly. **If it turns out the firmware's compiled VID/PID always wins**, the fix is simple:
rebuild the firmware with `CONFIG_DEVICE_VENDOR_ID=0xFFF2` / `CONFIG_DEVICE_PRODUCT_ID=0x8001` set
explicitly in `sdkconfig.defaults`, so firmware and factory-partition identity always agree. Test
one unit end-to-end before committing to a batch of 130.

### 4.3 Generate a batch

```bash
export MATTER_SDK_PATH=~/esp-matter/connectedhomeip/connectedhomeip

esp-matter-mfg-tool \
    -v 0xFFF2 -p 0x8001 \
    --vendor-name "your-vendor-name" --product-name "your-product-name" \
    --hw-ver 1 --hw-ver-str "1.0" \
    --target esp32h2 \
    --pai \
    -k "$MATTER_SDK_PATH/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Key.pem" \
    -c "$MATTER_SDK_PATH/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Cert.pem" \
    -cd "$MATTER_SDK_PATH/credentials/test/certification-declaration/Chip-Test-CD-FFF2-8001.der" \
    -n 130
```

`-n 130` generates 130 independent identities in one run. Each gets its own discriminator,
passcode, SPAKE2+ verifier, and DAC (all sharing the same PAI/CD, since they're all the same
"product" for Matter's purposes). Output, per device:

```
out/FFF2_8001/<uuid>/
  <uuid>-partition.bin     # the NVS factory partition — flash this
  <uuid>-onb_codes.csv     # QR payload string, manual pairing code, passcode, discriminator
  <uuid>-qrcode.png        # ready-to-print QR image
  internal/                # DAC and PAI, DER and PEM
```

**Not verified this session**: whether `-n` combined with `--dac-in-secure-cert` and
`--commissionable-data-in-secure-cert` (which move data into this project's `esp_secure_cert`
partition rather than plain NVS `fctry`) changes the output layout above — check the tool's
`--help` output for your installed version before a real batch run, since flag behaviour has
changed across mfg-tool releases.

### 4.4 Keep track of which `<uuid>` folder goes on which physical unit

Nothing in this tool associates a generated identity with a specific physical board — that's on
you. Recommended: flash units in the same order the tool generated them (`out/FFF2_8001/`'s
directory listing order), and stick each `<uuid>-qrcode.png`'s printed label on its unit
immediately after flashing, before moving to the next one. See Stage 2 (not yet written) for the
full labelling workflow — the short version, from `docs/flasher-and-registry.md` section 6.3: an
unrecorded passcode after the case is closed means reopening it to reflash.

---

## 5. Flash firmware and identity to a unit

Two separate writes per physical board — same firmware binary every time, a different NVS image
each time.

```bash
# Firmware (same for every unit)
idf.py -p /dev/cu.usbserial-XXXX flash

# This unit's factory identity, at the fctry partition offset from partitions.csv (0x3E0000)
esptool.py -p /dev/cu.usbserial-XXXX write-flash 0x3E0000 out/FFF2_8001/<uuid>/<uuid>-partition.bin
```

Confirm the offset against `software/matter-onoff-poc/partitions.csv` rather than trusting the
figure above blindly if the partition table ever changes — it's `fctry,data,nvs,0x3E0000,0x6000`
as of this project's current layout.

### 5.1 macOS serial port gotchas hit in this session

- **Use the `UART` port (J2, the CP2102N)**, not the native `USB` port (J4), for normal flashing
  — see `docs/devkit-bringup.md` section 3. The native port has no auto-download circuit and
  needs a manual BOOT+RESET dance every time.
- **If both `/dev/cu.SLAB_USBtoUART` and `/dev/cu.usbserial-XXXX` appear for the same physical
  port simultaneously**, two drivers are bound to the same chip: Apple's in-box CP210x-class
  driver, and a separately-installed Silicon Labs kernel extension. Prefer `usbserial-XXXX` — it
  resolved a `termios.error: (22, 'Invalid argument')` port-open failure in this session that
  persisted across every baud rate tried. For a permanent fix, disable the Silicon Labs driver
  extension entirely (System Settings → General → Login Items & Extensions → Driver Extensions),
  since Apple's own driver already covers this chip.
- **The same `termios.error: (22, 'Invalid argument')` can also be an Apple Silicon Mac + USB hub
  interaction**, independent of drivers or baud rate — esptool maintainers' own documented fix
  for an identical report: plug directly into the Mac's own USB-C port, no hub in between.
- `idf.py monitor` exits with **Ctrl+]**, or the chord **Ctrl+T then Ctrl+X** if that doesn't
  register in your terminal.

---

## 6. Verify: watch the console for the pairing code

```bash
idf.py -p /dev/cu.usbserial-XXXX monitor
```

Right as the device opens its commissioning window, it now prints a highlighted block with
*this specific unit's* pairing code, computed live from whatever identity was just flashed to it
(`main/app_main.cpp`'s `print_commissioning_codes()`, added this session — reads the discriminator
and passcode from `GetCommissionableDataProvider()` at runtime via connectedhomeip's own
`GetQRCode()`/`GetManualPairingCode()`, not a hardcoded string). On the bench PoC's default
identity that's `3497-011-2332`; on an mfg-tool-provisioned unit it'll be that unit's own code,
matching its `<uuid>-onb_codes.csv`.

**Prerequisite, easy to forget and indistinguishable from a firmware bug if missed:** a Thread
Border Router (HomePod mini, HomePod, or Apple TV 4K) must already exist on the network before
commissioning is attempted — a phone alone cannot commission a Thread device. The DevKitM-1 also
has no NFC hardware, so Apple Home's "Hold iPhone near accessory" option can never work with it;
use manual code entry or QR scan instead.

The commissioning window is Matter's standard 15 minutes. Since this firmware has no BOOT-button
factory-reset handler (deliberately cut, see the PoC README's "what was cut" section), pressing
**RESET** reopens the window on next boot, as long as no fabric has actually been committed yet.

---

## Sources

- `docs/idf-im-ui` CLI docs (`docs.espressif.com/projects/idf-im-ui`) — EIM Python requirements
  and CLI flags.
- `pyenv/pyenv#2573` — the blake2b/blake2s libb2 conflict on Apple Silicon.
- `espressif/esp-matter` — clone/submodule instructions, `sdkconfig.defaults`/
  `sdkconfig.defaults.esp32h2` (read directly from this project's own copies, adapted from
  upstream's `examples/light`).
- `project-chip/connectedhomeip`, `scripts/checkout_submodules.py` and `credentials/test/` —
  submodule checkout behaviour and test attestation file naming, checked directly against the
  actual repository during this session.
- `espressif/esp-matter-tools`, `mfg_tool/README.md` — CLI flags and the FFF2/8001 example
  invocation, checked directly.
- `docs.espressif.com/projects/esp-matter/en/latest/esp32h2/production.html` — the
  one-firmware/per-device-factory-data architecture this whole stage relies on.
- OpenThread `include/openthread/error.h` — confirming error code `3` is `OT_ERROR_NO_BUFS`.
- esptool maintainers, `espressif/esptool#920` — the Apple Silicon + USB hub `termios` failure
  mode and its fix.
- This repo: `docs/matter-identity-and-certification.md`, `docs/flasher-and-registry.md`,
  `docs/devkit-bringup.md`, `software/matter-onoff-poc/README.md`.

### What I could not verify

- Whether factory-partition VID/PID overrides compiled firmware VID/PID at runtime (section 4.2)
  — architecturally implied, not directly confirmed against a real flash.
- Exact current `-n`/`--dac-in-secure-cert`/`--commissionable-data-in-secure-cert` interaction and
  output layout for the installed mfg-tool version at the time you read this — check `--help`.
- None of this stage's commands have been run in the environment that wrote this document (no
  ESP-IDF toolchain available there); the build/flash mechanics were previously exercised
  interactively by the project owner in a real session, and the mfg-tool step specifically has
  not yet been run by anyone on this project as of this writing.
