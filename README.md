# Astera EQUINOX-REVC: Aries SDK Files

Unofficial notes and public source code for working with the Aries PT5161L retimer on the EQUINOX-REVC card. This is not an Astera package, and nothing here has been tested on a card.

## Start here

1. Read `docs/QUICK-REFERENCE.md` for connection notes, a call flow and eye measurement.
2. Use `tool/` for a ready-to-build command-line tool. See `tool/README.md`.
3. Use `docs/aries-sdk-api-reference.md` to look up individual functions.
4. See `sources/meta-openbmc-retimer-v2.16.2/files/aries_common.c` for the SDK's example code.

## Contents

| Path | What it is |
| --- | --- |
| `docs/` | Quick reference and full API reference |
| `tool/` | `equinox_diag` command-line tool, tested only against a simulated chip |
| `sources/meta-openbmc-retimer-v2.16.2/` | Astera's C SDK, from Meta's OpenBMC repo |
| `sources/kernel/pt5161l.c` | Linux driver, a short example of the I2C protocol |
| `sources/brief/` | Astera's product brief, if included |

## Not included

- Equinox User's Guide
- PT5161L datasheet
- Debug header pinout
- Python SDK
- Firmware images

The guide and Python SDK are available by request from info@AsteraLabs.com. The header is 1.8 V, so do not connect a 3.3 V adapter directly.

## Licenses

The SDK source files are Apache-2.0, as stated in their headers and `LICENSE`. Meta's build recipe lists GPL-2.0-or-later. The kernel driver is GPL-2.0-or-later. The product brief belongs to Astera Labs.
