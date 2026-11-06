# ROM6 native DSP bring-up

## Boundary

A captured 8210 ROM6 reference is available in the private collection; hashes,
source snapshot and reconstruction policy are in [the ROM inventory](../roms/README.md#nokia-8210-dsp-rom6-reference).
No ordinary machine or acceptance result is promoted by acquisition. The
explicit `nsm3r6` research composition selects the audited candidate; existing
HLE and staged configurations retain their separate contracts.

## Current native boundary

`nsm3r6` selects the C54x backend without `dsp_hle` or `dsp_staged`, separate
program/data regions, the `0080..1fff` executable overlay and the extended
program instruction grammar. Only page zero has an acquired program map.
Its initial IMR is zero, not the backend's ROM4-specific bit-0 seed. The
52 MHz backend clock and other inherited peripheral policy remain research
assumptions, not measured ROM6 hardware parameters.

A fresh three-second run enters reset `ff80` and the upload loader `0f00`
twice, then reaches resident initialization `2c75` organically. The native
backend publishes one completion strobe; complete upload equivalence and MCU
self-test success are not yet qualified. The next CPU exclusion is MVDD at
uploaded address `0b48`: opcode `e51a`, source `0061`, destination SP (`0018`).
The preceding absolute CMPM at `0b38` reads BRC against `ffff`; its admission
is covered by eight independent literal/retention/save-restore cases derived
from TI SPRU172C 4-33, not a ROM-specific compare result. Direct/indirect BRC
CMPM and other control-register admissions remain excluded.

Reproduce the input preparation with:

```sh
python3 tools/dsp_rom6_candidate_check.py roms/research/nsm3-rom6 --prepare roms/nsm3r6
```

Run `mame nsm3r6` in an isolated run directory with `-debug -debugger none
-autoboot_delay 0 -autoboot_script /absolute/path/tools/rom6_native_observe.lua
-seconds_to_run 3 -log` and the collection's absolute `-rompath`. The observer
records bounded reset/upload/resident entry and CPU state without firmware
writes. This is a bring-up observation, not a passing initialization gate.

The first target is **8210 NSM-3 v5.31**, which shares the reported capture
product and already has an own-upload native fixture. 6210 NPE-3 is the second
portability target; 3350 NHM-9 supplies a distinct missing-code call at `861f`.
3310/3330/3410/5210 and other ROM6 candidates follow their own version/upload
checks. Do not infer fitted-mask identity from a handset name or MCU version.
The 5510 baseband is separate from its MU4 music DSP.

## Reviewed external leads

Reference snapshot: `djr-747/nokia-dct3-emu` commit
`dd356ff7c5fc62529ab5843ce737593ebff51f73`. Findings below come from its capture
artifacts and documentation, not imported implementation. Hardware traces cited
only under the author's private paths are not locally reproduced evidence.

| Lead and source document | Status here | Consequence |
| --- | --- | --- |
| Program/data-view separation, `rom6-boot-handoff.md` | Locally verified byte difference; capture method reported externally | Map program and data inputs separately. Program `b4f8` is code; the old data view has coefficients there. Do not diagnose frame/ISA bugs from a disassembly of the wrong view. |
| `0080..1fff` overlay boundary on the captured 8210, same handoff | Product-specific external observation | Existing native backend has a configurable overlay end and defaults to `2800`. Validate a `2000` ROM6 profile against own uploads; do not change ROM4 globally or confuse allocated API storage with executable overlay extent. |
| Actual MCU upload/copy provenance and resident init at `2c75`, `rom6-faithful-boot-plan.md` | Resident entry locally reproduced; complete upload equivalence still unqualified | Run the product's own verifier and loaders into the acquired resident image. No transplanted routine, synthetic return, verdict or HLE handoff. |
| Successful 8210 MCU/UI boot with corrected program view, `rom6-boot-handoff.md` | External bounded observation, not native network/speech acceptance | Use as feasibility evidence. Locally require DSP completion, organic UI input and continued execution separately. |
| ROM6 P30/P31/P32/P04 timed RF tuples, `rom6-rf-acquisition-current.md`, `rom6-pll-unmatched-o3x.md` | Reported runtime observations plus provisional physical interpretations | Our native backend retains P31/P32 but lacks this complete ROM6 timed-output model. Keep raw tuple, scheduled delivery, PLL route and receiver state distinct. Unknown tuning must not fall back to the laboratory carrier. |
| PA09 block-ready versus frame notification, `rom6-frame-int0-ownership.md` | Experimental notification-ownership hypothesis | Avoid duplicate IRQs for one sample block. Validate samples/counts, underflow, cancellation and restore before adopting a routing rule. |
| Zero-control output entry, `rom6-zero-control-output.md` | Explicitly provisional no-shift interpretation | Keep as a test candidate, not a universally ignored command. Account for one scheduled entry without inventing RF completion. |
| Native SCH and byte-exact SI1/2/3/4 BCCH under an explicit amplitude setting, `rom6-bcch-air-origin.md` | External experimental decode evidence, not local reproduction or complete registration | Establish waveform origin and sample provenance independently; compare decoded L2 bytes to generated SI. Do not import the `-28` amplitude or a status-zero result as a hardware constant. |

Earlier relocation/trap shims and ROM4 routine analogies in the reference are
not needed as executable substitutes now that captured program bytes exist.
Likewise, earlier ROM-family tables conflict with later dual-upload findings;
select and validate the product's actual upload branch rather than inheriting
a broad model-to-ROM classification. The local CPU retains its independent
TI-derived contracts; external interpreter fixes and rate ratios are leads only.

## Milestones

1. **Input gate.** Add a strict private-input auditor: hashes, sizes, accepted
   row statuses, exact coverage, agreeing overlaps, word/byte order and exact
   reconstruction. Distinguish program, data alias and padding. Reject missing
   or contradictory words; retain acquisition originals. `make audit-dsp-rom6`
   now implements this gate. `dsp_rom6_candidate_check.py --prepare DIR` writes
   the verified program and a separately labelled data-alias region with zero
   initialization elsewhere, not a captured cold-RAM state. Neither is selected
   by an ordinary handset configuration.
2. **8210 native composition.** Use the existing C54x/backend seam in an explicit
   research machine, not a second interpreter or a replacement for `nsm3hle`.
   Bind the candidate program view, evidence-backed data aliases, overlay extent,
   shared window and reset/release wiring. Do not seed ROM4 runtime RAM or copy
   ROM4 power-on interrupt/port policy without evidence. Observe the first
   unsupported access/instruction rather than answering it speculatively.
3. **Organic initialization.** Require exact own uploads, resident `2c75` entry,
   real verification/self-test publication and no HLE completion in the same
   boot. Then qualify physical menus, SIM, storage, sustained operation and
   save/load. A painted standby frame is insufficient. Fix new CPU exclusions
   only with an independent instruction/composition test and primary evidence.
4. **Cross-product proof.** Repeat on 6210's own image/PMM/uploads, then check
   3350's `861f` call and one 3310-family product. Differences become typed board,
   DSP-memory or peripheral contracts, not firmware-PC branches. Preserve all
   existing HLE and native ROM4 gates; migrate defaults only after qualification.
5. **Native receive and registration.** Model the required RF/codec interfaces
   outside the CPU: timed output, oscillator programming, sample FIFO/cadence,
   IRQ ownership and an independent laboratory air source. Require own native
   FCCH/SCH/BCCH results, actual decoded SI, camp, Location Updating and registered
   UI. A copied report or forced carrier is not a receiver implementation.
6. **Native calls and media.** Reuse existing host telephony and phone-service
   acceptance while replacing only the DSP path. Qualify paging, assignment,
   bidirectional speech and release with real buffer/clock provenance and restore.
   A call-signaling success does not establish native speech.

Stop each milestone at a named, evidenced boundary if the necessary input or
contract is unavailable. The ROM removes a missing-code dependency; it does not
complete RF silicon, calibration or product provisioning. Hardware acquisition
remains useful for independent mask checks, reset state and peripheral traces,
but it is not required to begin milestones 1--3.
