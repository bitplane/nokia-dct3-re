# Nokia 8210 bring-up

## Current boundary

NSM-3 v5.31 PPM C reaches the DSP-owned final sparse-flash verification
publication at `0x2cadce`. No working-phone promotion is claimed. The next
question is whether the externally staged C54x verifier can derive the
publications in shared cells `0x10000` and `0x10002` without the mask ROM or
a read override.

## Inputs and hardware

The stock MCU/PPM-C composition exactly matches the canonical MAME flash.
The canonical product-local PMM tail is also acquired; provenance and hashes
are in `roms/README.md`. No sibling provisioning image is enabled.

Nokia's [NSM-3 System Module, issue 1 12/1999](https://www.eserviceinfo.com/preview_html.php?fileid=5456&previewid=3011)
identifies MAD2WD1, CCONT, CHAPS and COBBA-GJP, a 13 MHz system clock and
flash-backed product data. The stock service package assigns the same MCU
to ROM5 and ROM6; [service bulletin SB-050](https://www.eserviceinfo.com/downloadsm/225117/NOKIA_NSM3-050.html)
documents the later ROM6 board revision. The current profile selects ROM6.
Internal DSP ROM images remain absent. Uniform-fill audit members are not
executable DSP evidence.

## Established contracts

- GENSIO control `0x22` selects CCONT. The byte write at `0x302d32` is followed
  by receive-ready polling at `0x302d36`; control bit 2 is clear. NSM-3 therefore
  uses byte-write-triggered ready, independently of other products.
- `0x2cac80` parks shared offsets 2/4/6 at `0xffff`; `0x2cad46` accepts silicon
  identity 6 or 5 from offset 4. The HLE peer publishes identity 6 after that
  physical sentinel write. It does not overlay reads.
- `0x2cad60..0x2cad9a` transfers 115 blocks of 512 halfwords; the final block
  has 510 halfwords and two `0xffff` terminators. Source starts at `0x200040`
  and advances by `0x20`, so this is sparse ARM-flash input, not DSP program
  code. The trace independently observes 58 alternating ownership pairs.
- `0x2cadce` waits for shared offset 2 to leave `0xffff`, then copies offsets
  2/0 into product-local bootstrap state at `0x135774 + 0x0e/0x0c`. The HLE
  does not yet publish these unvalidated final results.

The boot descriptor pointer at `0x135808` resolves to flash `0x31bcf0`.
Its first program words are independently observed at shared offset `0xe00`
before the sparse transfer. This is executable C54x material, unlike the
subsequent ARM-flash samples, and is the next software-accessible evidence
source. Execution outside that staged image must not be filled with guessed
mask-ROM instructions.

The existing independent emulator clears the final sentinel at read time.
That is a compatibility behavior, not a captured NSM-3 verification result,
and is not imported. Packet/service, SIM, keypad and radio contracts remain
unpromoted behind this boundary.

## Acceptance

`make normalize-8210` reconstructs the stock flash and checks PMM identity.
`make verify-8210-bootstrap` validates the ROM6 publication, exact upload
handoff order and fail-closed final wait. It is a frontier gate, not boot/UI
acceptance. Existing product profiles are unchanged.
