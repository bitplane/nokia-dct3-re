# Tooling and external references

## In-repo tools

- `tools/*.py` — small Thumb-disassembly / cross-reference helpers built on
  [capstone](https://www.capstone-engine.org/). They operate on the **swap16**
  firmware image (see `roms/README.md`). Set the image path via the `NOKI_BIN`
  environment variable (default points at `roms/3210f600a_swap16.bin`).
  - `fwdis.py ADDR [LEN]` — disassemble
  - `findcalls.py ADDR...` — find `bl`/branch callers of an address
  - `findptr.py VALUE...` — find pointer literals (raw LE and halfword-swapped)
  - `dump.py ADDR [LEN]` — dump words / halfword-swapped pointers
- `ghidra/scripts/*.java` — headless Ghidra scripts (run via `analyzeHeadless`)
  that name functions and export analysis. The naming list is also exported as a
  portable symbol map at `ghidra/symbols/3210.csv` (address, kind, name) so you
  get the names without running Ghidra.
- `tools/mame_noki3210_input_exerciser.lua` — MAME Lua harness used by the run
  targets to drive keypad input.

## NokTool 1.8 (external — EEPROM/NV layout reference)

[NokTool 1.8](https://nokia-tuning.net/download/noktool18.zip) is a third-party
Nokia service utility (Borland Delphi 7, Win32). It was a useful cross-check for
the **EEPROM block layout and the 16-bit additive checksum algorithm** — see
`docs/eeprom_analysis.md`, where the firmware's own checksum (`0x234588`) is
cross-validated against NokTool's `sub_0046AAA8` and the tune/security block
boundaries from `TForm1.e2prom1Click`.

The tool and any reconstruction of it are **proprietary and not included** in this
repo. Only the *findings* (block layout, checksum algorithm — facts) are recorded
in the docs.

**How it was analysed:** the unpacked binary was reconstructed with
[IDR — Interactive Delphi Reconstructor](https://github.com/crypto2011/IDR),
which correctly recovers Delphi forms/units and names. IDR's output (the `.pas`
reconstruction, `.idc`, `.lst`) is a derivative of NokTool's copyrighted binary
and is likewise **not redistributed** here.

## MAME

The driver `driver/nokia_3310.cpp` is overlaid onto an upstream
[MAME](https://github.com/mamedev/mame) checkout (pinned commit in the Makefile);
MAME is not vendored. See `LICENSE` for the licensing split.
