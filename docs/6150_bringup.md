# Nokia 6150 NSM-1 bring-up

## Current boundary

The acquired v5.23 PPM C package normalizes to an exact 2 MiB CPU-order
image. `make verify-6150-static` regenerates it from a hash-pinned ZIP
wrapper without executing the installer, validates every Wintesla record,
and checks its reset and sparse DSP verification contract. Normal handset
support and graphical boot are not promoted. An explicitly separate
`nsm1r4t` executable compatibility instrument now exists.

The next prerequisite is establishing matching resident DSP behavior and
runtime validity of the explicitly derived product-local EEPROM fixture,
plus fitted flash attributes and address aliases.
A 6110 profile is not a safe substitute: this image uses a larger flash
verification stream and stack addresses outside NSE-3's 64 KiB SRAM.

## Executable compatibility boundary

`nsm1r4t` uses the own normalized flash and checksum-consistent EEPROM,
128 KiB SRAM and the acquired NSE-1 ROM4 native DSP program/data. That mask
is an unproved compatibility input, not an identified NSM-1 fitted mask.
The MCU reset exit remains declared boot HLE. The base 16-Mbit flash part,
ROM4 keypad GPIO layout and other conservative peripheral defaults are
research assumptions; fitted flash identity, aliases and physical input
are not validated. DSP/service/radio HLE publications are disabled.

Own CCONT read routine `0x2bf082` selects control `0x28=0x22`, writes its
command through `0x2a`, polls status `0x29` bit 2, and receives at `0x2d`.
The static checker pins this span and register grammar. Selecting this
GENSIO leg alone is insufficient: the later default keypad also owns
`0x28/0x2a` and intercepts the transaction. The explicit compatibility
instrument selects the non-overlapping ROM4 GPIO layout; this is not yet
an independently decoded NSM-1 keypad contract.

Read-only observer `tools/nsm1_native_observe.lua` records actual firmware
shared-memory writes, CPU snapshots and verifier fields; it never writes
firmware/MMIO state. The nine-second experiment progresses past the sparse
verifier into later firmware execution, captures result words `0x0000` and
`0x0004` at `0x111a9e/0x111aa0`, and records native mailbox/completion
activity. The captured 84 x 48 frame is blank. Leaving the verifier is not
proof that its result passes the product's later self-test policy. No
SIM, graphical boot, input, radio or speech acceptance is claimed.

Reproduce using an empty run directory and a ROM directory named
`nsm1r4t` containing the own flash/EEPROM and the hash-pinned NSE-1 program
and data files declared in the driver:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir run_6150_native_cold -- nsm1r4t \
  -rompath /absolute/path/to/roms -video none -sound none -nothrottle \
  -seconds_to_run 9 -log -autoboot_delay 0 \
  -autoboot_script ../tools/nsm1_native_observe.lua
```

Next: decode the own-firmware policy consuming those verifier words and
the live post-verifier wait, then establish the remaining GPIO/LCD
register contracts. Do not replace the result with a passing HLE value.

## Own-firmware contract

Input identity and member extents are authoritative in `roms/README.md`.
The checker pins the reset, loader, identity formatter, serial reader and
security-check instruction spans and emits exact anchor/pool coverage counts.
It independently resolves three ARM stack loads.
It does not claim a whole-image control-flow or producer census.

| Boundary | NSM-1 v5.23 evidence |
| --- | --- |
| Reset | ARM entry `0x200040`, header reorder and MCUIF write at `0x20005c`, vector copy and Thumb switch at `0x2000e8`. |
| Stack roots | Three reset pool loads resolve to `0x1160f8`; exception setup additionally adds offsets. This proves the 64 KiB NSE-3 RAM map cannot cover the selected stacks, not the fitted SRAM capacity. |
| DSP verifier | Loader `0x2a479e` initializes `0x10000=0`, `0x10002=0xffff`; stages source halfwords at 32-byte intervals from `0x200040` through `0x3fffe0`. |
| Transfer count | 127 full 512-word blocks, then 510 source words and two `0xffff` terminators; 131,072 bytes total. |
| Handshake | Alternating acknowledgements use shared `0x100fe` and `0x10100`; payload starts at `0x10200`. |
| Completion | Waits for shared `0x10002` to leave `0xffff`; captures `0x10000` and `0x10002` into state `0x111a94` at offsets `0x0a` and `0x0c`. |

Stream SHA-1 is `0fac5ba57a3504f8cb4490a97ab961f468218121`.
This is a sparse external-flash verification candidate, not a contiguous
DSP program. The DSP-side verdict algorithm and compatible resident mask
are not established. No invented completion value or donor EEPROM may
be used to promote this product.

## Primary hardware leads

Nokia's NSM-1 service manual identifies the product as dual-band GSM
900/1800. Its UE4 chapter specifies a serial GD40 display with 84 x 48
one-bit RAM and a five-by-five key matrix. Its system chapter describes
MAD2's ARM/TI Lead blocks, CCONT, COBBA-GJ and the internal MIC2/EAR route;
the PCM link is 1 MHz with an 8 kHz frame clock, with sign extension in a
16-bit frame. These are hardware facts, not proof of a working firmware
peer or native speech.

Primary references:
- [NSM-1 system module, Original 10/98](https://www.eserviceinfo.com/preview_html.php?fileid=9648&previewid=5152).
- [UE4 UI module, Original 10/98](https://www.eserviceinfo.com/preview_html.php?fileid=9648&previewid=5146).

The [indexed original system manual, page 3-45](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6150/03sys%20%5B92%5D.pdf)
establishes 16 Mbit flash, 1 Mbit SRAM (128 KiB) and 128 Kbit serial EEPROM
(16 KiB). Complete PDF retrieval remains unavailable; fitted flash attributes
and address aliases are not inferred from those capacities.

## Product-local EEPROM input

The original NokiX 2011.07.24 archive contains a 16 KiB `nsm-1.bin` repair
template. Source/member hashes and acquisition URL are in `roms/README.md`.
The static gate audits its original bytes without applying repair scripts
or changing checksums. It is a historical product-local template, not a
verified factory handset dump or proof of boot compatibility.

The own-image directory `0x2da10c..0x2da48b` contains 112 distinct group-7
descriptors; all fit the documented EEPROM, with the highest end `0x3fa8`.
Records `0x0701` and `0x0702` select `0x03cc`/8 bytes and `0x03d4`/44 bytes;
nested record `0x070b` selects the checksum at `0x03d2`/2 bytes.
Settings load `0x2c3a70` places record `0x0702` at SRAM `0x11fc16`.
Validator `0x2bccfe` reads setting state `0x11fc35`, establishing index
`0x1f` and physical EEPROM byte `0x03f3` without a value sweep.
It sums a sixteen-byte identity buffer via `0x2b1e2c`, adds the setting and
compares the truncated result with checksum state `0x112826`.

### Identity and checksum closure

Own reader `0x2b8b90` resolves record descriptors and calls the raw serial
reader `0x2bcace`. The latter deposits bytes from receiver `0x2bdece`
unchanged; no record decoder intervenes. The security record is loaded into
`0x112820`, putting the compared checksum at offset `+6`, independently
agreeing with nested record `0x070b` at physical EEPROM `0x03d2`.

Receiver `0x2bdece` independently establishes the serial wiring: it builds
PUP GenIO data address `0x20020` and direction address `0x20024`, releases
SDA by clearing direction bit 0, raises SCL with data bit 2, samples data
bit 0 while SCL is high, then lowers SCL. Its shifting `0x80` receive mask
establishes MSB-first byte reception. The static gate pins these instructions;
this is not yet runtime verification of the complete serial transaction.
An executable NSM-1 profile must select SCL bit 2, not the NSE-8 default bit 3.

Identity wrapper `0x2ab73a` invokes `0x27e254` with selector 3. This reads
eight EEPROM bytes at `0x000c`, renders the first seven bytes as high/low
packed-BCD decimal digits, computes the fifteenth decimal check digit and
null-terminates the buffer. The wrapper also sets byte 15 to zero. Assuming
successful serial reads, the original template produces
`493006102132132` followed by zero. Raw BCD bytes must not be substituted
for that sixteen-byte ASCII buffer.

The original setting remains `0x58`. Its required checksum is `0x034d`,
not the stored `0x3124`; the unchanged historical template cannot satisfy
this initial security-check relationship. `verify-6150-static` generates
ignored `roms/research/nsm1-v523/nsm1-security-checksum-valid.bin`, changing
only EEPROM bytes `0x03d2..0x03d3`. SHA-1 is
`07335e492f72ba9da9c3436890c9eb4af67fd9a9`. Identity, lock/settings byte,
calibration and all other content remain unchanged. The canonical acquired
template is not rewritten.

This is an evidenced nonvolatile-input consistency fixture, not a factory
dump or boot promotion. Remaining checksums, runtime boot, native DSP and
radio acceptance are unproved. Do not alter a lock level to seek a passing
screen, transplant NSE-3 records or synthesize a DSP completion.
