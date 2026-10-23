# Nokia 5110i / NSE-2 bring-up

## Current boundary

The acquired v5.53 MCU/PPM-A pair is normalized and statically qualified.
The archive labels it `5110i`; both firmware headers identify `NSE-2`,
version `05.53`, dated `11-01-01`. There is no machine configuration or
runtime acceptance yet. Boot, display, physical input, SIM, registration,
calls, SMS and speech are unestablished.

`make verify-5110i-static` independently qualifies the acquired ZIP and
Windows SFX, verifies their exact two member names/extents/hashes and
compares their decoded payloads byte-for-byte. Neither installer executes.
The resulting `roms/noki5110i/5110i_5.53A.fls` is an unchanged concatenation
of its own MCU and PPM, not a donor composition. The generated manifest
retains source/member hashes and the decoded contracts. Input provenance
is indexed in [the ROM collection](../roms/README.md#nokia-5110i-nse-2-v553).

## Own static contracts

| Surface | Evidence | Limit |
| --- | --- | --- |
| Flash input | MCU is `0x190000` bytes; PPM A is `0x70000`; combined CPU-order image is 2 MiB. | No fitted flash-part identification. |
| Reset | ARM entry `200040`; own PC-relative stack literal `10fb98`; ARM/Thumb interworking enters `2000ec`. | Stack placement is not a physical SRAM-size measurement. |
| EEPROM descriptor | Header byte `f4` decodes to two address bytes, 32-byte pages and 8 KiB capacity. | Descriptor interpretation, not fitted silicon or a supplied EEPROM dump. |
| DSP catalogue | Table `29bba0`, 112-byte initializer to `10ba14`, 28 slots, only slot 27 null. | Slots 11 and 26 are populated; an older product's null-slot assumptions do not apply. |
| Verifier | Slot 25 at `2a0f6c`, header `0f00 0000 00ba 0700 00b4 0000`, 186 program words. | Descriptor decode, not execution or a completed ownership exchange. |
| Resident ABI | Verifier stores `4` to DSP data `0803`; three pinned CALLs target `8023`, `8023`, `802b`. | ROM4-style ABI evidence, not identification of the fitted mask or authority to supply a verdict. |

The packages contain no separate external EEPROM or resident DSP image.
Do not attach the NSE-1 provisioning blob or infer the 5110's 2 KiB EEPROM
geometry from the similar product name. The acquired NSE-1 mask remains a
separately sourced compatibility hypothesis, not an implicit NSE-2 ROM.

## Next technical steps

1. Decode own GENSIO/CCONT, EEPROM GPIO, LCD and keypad register consumers,
   retaining instruction-site and byte-lane evidence before composing devices.
2. Establish the reset memory map and flash command/identity requirements;
   preserve the own executable and leave absent storage erased.
3. Add a fail-closed product configuration and capture the own DSP upload
   and first ownership wait. Compare actual transfers against the catalogue.
4. Decide any explicitly separate native-mask compatibility experiment from
   the observed upload/ABI evidence. Do not add completion constants, donor
   provisioning, firmware state writes or radio responses to obtain a frame.

Static parser tests cover member identity/extents, catalogue bounds/null
slots, verifier identity and resident CALL corruption. They do not establish
emulated hardware behavior or replace the future runtime gates.
