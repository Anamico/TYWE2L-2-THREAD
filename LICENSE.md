# Licensing

This project contains four kinds of work, and each is licensed appropriately for
its kind. Every file falls under exactly one of the licences below. Where it is
not obvious from the directory, files carry an `SPDX-License-Identifier` header.

| What | Where | Licence | SPDX |
|---|---|---|---|
| Hardware design (schematics, PCB, footprints, symbols, fabrication data) | `hardware/` | CERN Open Hardware Licence Version 2 - Strongly Reciprocal | `CERN-OHL-S-2.0` |
| Software (firmware, flashing tool, registry tooling, scripts) | `firmware/`, `tools/`, `web/` | Apache License 2.0 | `Apache-2.0` |
| Documentation | `docs/`, `README.md` | Creative Commons Attribution 4.0 International | `CC-BY-4.0` |
| Device profiles in the registry | `registry/` | CC0 1.0 Universal (public domain dedication) | `CC0-1.0` |

Full licence texts are in [`LICENSES/`](LICENSES/).

## Why these licences

**Hardware under CERN-OHL-S-2.0.** Strong reciprocity means anyone who
manufactures and distributes a board based on this design has to publish their
changes to the design. Commercial manufacture is explicitly welcome. The point is
that improvements come back, not that nobody sells these. If it turns out this
deters people we actually want manufacturing the boards, CERN-OHL-W-2.0 (weakly
reciprocal) is the obvious fallback and the switch is easy while the contributor
base is small.

**Software under Apache-2.0.** This matches the licence of everything we build on
(ESP-IDF, esp-matter, the Matter SDK, esptool-js, esp-web-tools), so there is no
compatibility friction in either direction. Apache-2.0 also carries an express
patent grant, which matters more than usual in a project touching Matter and
Thread, both of which sit on substantial patent pools.

**Device profiles under CC0.** A profile is a pin map. It is a statement of fact
about how a device is wired, and facts are not meaningfully copyrightable in most
jurisdictions anyway. Dedicating them to the public domain removes any doubt, and
lets the data flow freely to and from other projects solving the same problem.

## Contributing

Contributions are accepted under the licence covering the directory you are
contributing to. There is no CLA. By opening a pull request you confirm you have
the right to contribute the work under that licence.

If you are contributing a device profile derived from another project's data,
please say so in the pull request so provenance is recorded.

## Trademarks

No trademark rights are granted by any licence in this repository.

- **Tuya** and **TYWE2L** are trademarks of Hangzhou Tuya Information Technology
  Co., Ltd. This project is not affiliated with, endorsed by, or connected to
  Tuya in any way. The names are used only to identify the hardware this module
  is designed to replace.
- **Matter** and **Thread** are trademarks of the Connectivity Standards Alliance
  and the Thread Group respectively. This project is **not certified** under
  either programme. Firmware built from this repository uses the Matter test
  Vendor ID `0xFFF1` and SDK test attestation certificates. It must not be
  described as a certified Matter product, and it will not be listed in the
  Distributed Compliance Ledger.
- **Espressif**, **ESP32** and **ESP-IDF** are trademarks of Espressif Systems.

## Warranty and safety

The licences above disclaim warranty in the usual terms. Read that as written,
because this project has a sharper edge than most.

This module is designed to be fitted **inside existing mains-powered
appliances**. Doing so involves working on equipment that can kill you, and can
cause a fire long after you have walked away from it. Device profiles in the
registry are pin maps supplied by strangers, and an incorrect one can drive a
relay or an output in a way the host hardware was never designed for.

You are responsible for your own safety and for the consequences of anything you
build or fit. If you are not competent to work on mains equipment, do not.

## Regulatory notice

Replacing the radio module in a device replaces the transmitter that device was
certified with. The original TYWE2L carries FCC ID `2ANDL-TYWE2L`, and the host
appliance's compliance was established with that module fitted. Fitting this
board voids that, and the resulting device is no longer covered by the host
manufacturer's FCC, RED, UKCA or RCM approvals, nor by its safety certification
or warranty.

This is published as an open hardware design for people to build and modify
equipment they own. It is not a product, it is not offered for sale, and nothing
here should be read as a claim of regulatory compliance in any jurisdiction.
Anyone manufacturing or distributing boards based on this design is responsible
for their own compliance obligations.
