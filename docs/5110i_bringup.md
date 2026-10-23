# Nokia 5110i / NSE-2 bring-up

## Current boundary

The acquired v5.53 MCU/PPM-A pair is normalized and qualified. The separate
`noki5110i` research profile renders the own firmware's readable 84x48
`CONTACT SERVICE` frame with initially erased external EEPROM and no DSP
completion. Fresh isolated runs execute EEPROM GPIO, CCONT and LCD traffic
through eight seconds without a repeated reset. This is graphical failure
presentation, not usable boot, physical key delivery, SIM or phone-service
acceptance. The archive labels the pair `5110i`; both firmware headers
identify `NSE-2`, version `05.53`, dated `11-01-01`.

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
| EEPROM GPIO | Own byte reader `295746`, START/STOP `29588c/29581a`: signal `20020`, direction `20024`, SDA bit 0, SCL bit 2. | No factory contents or measured write latency. |
| GENSIO | Own CCONT reader `296704`: select `22` at `28`, TX `2a`, RX `2d`, status `29` bit 2. LCD accessors `296e46/296eac`: command/data `2c/2b`, ready bit 0. | Shared device grammar is supported by these own consumers; fitted IC/serial timing is unproved. |
| Keypad | Scanner `29806c`, decoder `299010`, maps `2b382c/2b3848`, selector `10c6b6`. Five rows, row signal/direction `31/2f`, columns `30`, IRQ mask `33`; special column 4 maps Power. | Own layout matches the existing NSE-1 input layout, not proof of delivered physical input. |
| SIMI | Own initialization `286520` uses IIR `38` and the standard FIFO controls. | Empty socket; no SIM conversation accepted. |

The packages contain no separate external EEPROM or resident DSP image.
Do not attach the NSE-1 provisioning blob or infer the 5110's 2 KiB EEPROM
geometry from the similar product name. The acquired NSE-1 mask remains a
separately sourced compatibility hypothesis, not an implicit NSE-2 ROM.

## Graphical failure acceptance

```sh
make verify-5110i-boundary RUN_DIR=tmp/nse2-fresh-boundary
```

The runner refuses an existing directory, independently qualifies both
wrappers and supplies only unchanged own flash. Absolute run-local ROM,
NVRAM and configuration paths avoid retained state from other runs. Its
input manifest records the binary hash and the declared minimal boot-ROM
flash-entry HLE. The base TE28F160 is a capacity-compatible trial, not fitted
flash identification. The 64 KiB SRAM window is a minimum research map,
not physical-size/alias evidence. ADC inputs and shared clocks remain
research defaults; the 84x48 controller presentation is compatible with the
observed frame, not an identified fitted LCD part.

Read-only taps record eight full-halfword MCU shared-memory initialization writes,
continued CCONT/LCD/EEPROM activity and live own-flash execution. The gate
requires the reviewed failure-frame pixels, rejects repeated reset and DSP
RX/startup publications, and explicitly leaves usable boot/native DSP/key
delivery false. No EEPROM is seeded, no card inserted and no DSP service,
radio or external application responder enabled. The shared writes are not
claimed to be completed ownership exchanges or execution of the verifier.
The failure frame alone does not identify whether NV, DSP or other self-test
conditions selected it.

## Next technical steps

1. Recover the own EEPROM record catalogue and NV validators selecting the
   failure presentation. Establish any own-product defaults or independently
   sourced storage before proposing a provisioning fixture.
2. Trace actual DSP uploads and ownership when reached organically; the
   current graphical failure result does not establish those transfers.
3. Recover fitted flash identity, physical memory/clock and analogue inputs
   without changing returned IDs or samples merely to satisfy self-tests.
4. Decide any explicitly separate native-mask compatibility experiment from
   the observed upload/ABI evidence. Do not add completion constants, donor
   provisioning, firmware state writes or radio responses to obtain a frame.

Static parser tests cover member identity/extents, catalogue bounds/null
slots, verifier identity and resident CALL corruption. They do not establish
emulated hardware behavior or replace the future runtime gates.
