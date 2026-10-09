# Nokia 6150 NSM-1 bring-up

## Current boundary

The acquired v5.23 PPM C package normalizes to an exact 2 MiB CPU-order
image. `make verify-6150-static` regenerates it from a hash-pinned ZIP
wrapper without executing the installer, validates every Wintesla record,
and checks its reset and sparse DSP verification contract. This is static
coverage only; no executable profile or graphical boot is promoted.

The next prerequisite is establishing matching resident DSP behavior and
product-local EEPROM contents, plus the physical SRAM/flash part and map.
A 6110 profile is not a safe substitute: this image uses a larger flash
verification stream and stack addresses outside NSE-3's 64 KiB SRAM.

## Own-firmware contract

Input identity and member extents are authoritative in `roms/README.md`.
The checker pins 440 instruction bytes, decodes fourteen Thumb anchors
and seven pool references, and independently resolves three ARM stack loads.
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

The available system text ends at the start of the memory section. Acquire
the complete manual or equivalent primary part evidence before selecting
SRAM/EEPROM capacities or fitted flash attributes. The collaborator's
documented 6150 lock-screen result and NokiX EEPROM lead justify further
software investigation, but are not a matching hardware/provisioning proof.
