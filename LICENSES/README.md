# Licence texts

Complete, unmodified texts of the four licences used in this repository. The
reasoning behind each choice, along with the trademark, safety and regulatory
notices, is in [`../LICENSE.md`](../LICENSE.md).

Each file is named for its SPDX identifier and holds the official text verbatim.
Please do not edit, reflow or tidy them.

## Which licence covers what

| Area | Covers | Text | SPDX header for new files |
|---|---|---|---|
| `hardware/` | Schematics, PCB, footprints, symbols, fabrication data | [`CERN-OHL-S-2.0.txt`](CERN-OHL-S-2.0.txt) | `SPDX-License-Identifier: CERN-OHL-S-2.0` |
| `firmware/`, `tools/`, `web/` | Firmware, flashing tool, registry tooling, scripts | [`Apache-2.0.txt`](Apache-2.0.txt) | `SPDX-License-Identifier: Apache-2.0` |
| `docs/`, `README.md` | Documentation | [`CC-BY-4.0.txt`](CC-BY-4.0.txt) | `SPDX-License-Identifier: CC-BY-4.0` |
| `registry/` | Device profiles | [`CC0-1.0.txt`](CC0-1.0.txt) | `SPDX-License-Identifier: CC0-1.0` |

Every file in the repository falls under exactly one of these. Where the
directory does not make that obvious, the file carries the header.

## Adding the header

Put the SPDX line at the top of your new file, using whatever comment syntax
that file format uses. A copyright line above it is welcome but not required.

```c
// SPDX-License-Identifier: Apache-2.0
```

```python
# SPDX-License-Identifier: Apache-2.0
```

```yaml
# SPDX-License-Identifier: CC0-1.0
```

```markdown
<!-- SPDX-License-Identifier: CC-BY-4.0 -->
```

KiCad schematic and PCB files cannot carry a comment header, so `hardware/`
falls under `CERN-OHL-S-2.0` by directory. Where a hardware file does support
comments, use `SPDX-License-Identifier: CERN-OHL-S-2.0`.

## Where these texts came from

| File | Source |
|---|---|
| `CERN-OHL-S-2.0.txt` | `https://ohwr.org/cern_ohl_s_v2.txt`, which redirects to the CERN OHL project wiki on GitLab |
| `Apache-2.0.txt` | `https://www.apache.org/licenses/LICENSE-2.0.txt` |
| `CC-BY-4.0.txt` | `https://creativecommons.org/licenses/by/4.0/legalcode.txt` |
| `CC0-1.0.txt` | `https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt` |
