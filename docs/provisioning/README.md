# Provisioning: from source checkout to a labelled, pairable light

This is the index for the multi-stage process that turns this repository's firmware source into
a physical unit someone can add to Apple Home (or another Matter controller) — scaled from "one
bench PoC" up to "130 lights, each with its own commissioning identity."

**Why this exists as its own doc set, separate from `docs/devkit-bringup.md` and
`docs/flasher-and-registry.md`.** Those two are research and bring-up documents: one is bench
prototyping guidance for discovering what a TYWE2L host does with its legs, the other is
decision-support research for a *future* browser-based web installer that does not exist yet.
This doc set is the operational runbook for the tooling that exists **today** —
`esp-matter-mfg-tool` and `idf.py flash` — written from what was actually learned getting the
`software/matter-onoff-poc` project from a fresh checkout to a commissioned, working light in a
real session, not from theory.

## Stages

| Stage | Document | Covers |
| --- | --- | --- |
| 1 | [`01-firmware-build-and-flash.md`](./01-firmware-build-and-flash.md) | Toolchain setup, building `software/matter-onoff-poc`, generating a batch of unique per-device Matter identities with `esp-matter-mfg-tool`, and flashing firmware plus one identity per unit |
| 2 | *(not yet written)* | Commissioning walkthrough at volume: labelling, the Home app flow, what to do when a unit fails to pair, and the fail-safe/rollback pattern seen in bench testing |
| 3 | *(not yet written)* | The browser-based web installer described in `docs/flasher-and-registry.md`, once it exists — this will eventually replace most of Stage 1's manual CLI steps for end users, but Stage 1 remains the reference for what the installer has to reproduce |

Stage 1 is written first because it is the immediate, practical need: getting real per-device
identities onto real hardware now, with tools that already exist, rather than waiting on the web
installer.

## What "provisioning" means here, precisely

Two separate things get written onto each board, and they are independent of each other:

1. **The firmware image** (`matter_onoff_poc.bin`) — identical across every unit built from the
   same source. Flashed to the `ota_0` partition.
2. **A factory identity** (an NVS partition, plus optionally an `esp_secure_cert` partition) —
   unique per unit: discriminator, passcode, SPAKE2+ verifier, and the Matter attestation chain
   (DAC, DAC private key, PAI certificate, Certification Declaration). Flashed to the `fctry`
   partition (and `esp_secure_cert` if used).

This split is deliberate and is Espressif's own documented architecture, not something invented
for this project: it is what lets one firmware binary serve every unit, per
`docs/matter-identity-and-certification.md` and Espressif's ESP32-H2 production guidance
(`docs.espressif.com/projects/esp-matter/en/latest/esp32h2/production.html`), quoted there:
manufacturing-partition data exists "so that the rest of the components like the bootloader,
firmware image are common across all your devices."

## Sources

- `docs/matter-identity-and-certification.md` — the identity/certification decision-support
  research this project already did, including why a test VID is the only realistic option for
  an open-source retrofit project.
- `docs/flasher-and-registry.md` — the future web-installer architecture, and section 6
  specifically ("Recommended stack, Matter credentials, and build order"), which this doc set's
  Stage 1 operationalises using tools that exist today rather than waiting for that installer.
- `software/matter-onoff-poc/README.md` — the firmware project this stage builds and flashes.
