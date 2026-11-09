# ROM6 native DSP bring-up

## Boundary

A captured 8210 ROM6 reference is available in the private collection; hashes,
source snapshot and reconstruction policy are in [the ROM inventory](../roms/README.md#nokia-8210-dsp-rom6-reference).
No ordinary machine or acceptance result is promoted by acquisition. The
explicit `nsm3r6` research composition selects the audited candidate; existing
HLE and staged configurations retain their separate contracts. The separate
`npe3r6` composition also qualifies bounded native 6210 initialization,
physical Menu interaction and persistent SIM-contact save/cold readback using
its own MCU uploads and provisioning; see [6210 bring-up](6210_bringup.md).
Its fitted mask and DSP multiplier remain unknown. At the provisional 13 MHz
rate it stalls in a counter retry and requests reset near 32 seconds. The
separately labelled `npe3ref` 45.5 MHz reference composition, after removing an
unsupported CCONT RTC-to-CTSI coupling, completes 65 seconds and late physical
SIM save/cold readback through 70/67 seconds. This is sustained compatibility
evidence, not the 6210's established physical rate, normal-machine promotion
or successful native RF acquisition/speech. The [own board clock evidence](6210_bringup.md#native-control-readback-and-timing-boundary)
establishes 13 MHz at the MAD2 package input but leaves its internal DSP
multiplier unresolved.

## Current native boundary

`nsm3r6` selects the C54x backend without `dsp_hle` or `dsp_staged`, separate
program/data regions, the `0080..1fff` executable overlay and the extended
program instruction grammar. Only page zero has an acquired program map.
The 6210 raw traffic experiment now executes FCALLD to `1:a50d`, outside
that input; see the [page/mapping boundary](rom6_rf_interface.md#native-traffic-program-page-boundary).
Do not confuse page-0 completion with acquisition of the full extended ROM.
The independent SDCCH path supports native Alerting, physical rejection,
SACCH SI5/SI6 reception, complete rejected-call clearing, and physical
outgoing dial/CM service admission/SETUP. These do not enter or qualify the
missing traffic bank.
Its initial IMR is zero, not the backend's ROM4-specific bit-0 seed; fitted
reset state remains unmeasured. The product's Nokia System Module manual,
issue 1, 12/1999, printed page 25, specifies a 13 MHz reference multiplied by
3.5: this composition selects **45.5 MHz**. That documentary clock does not
establish every ROM6 product's clock or physical cycle accuracy. The retained
source and hash are in [the hardware atlas](hardware_atlas.md#cobba-part-identity-does-not-select-pcm-timing).

Native initialization enters reset `ff80` and upload loader `0f00` twice,
then resident initialization `2c75` once. The MCU consumes verifier result
`0000/0006` and self-test command `0d`, zero faults, flag `c4`, without an HLE
completion. Passive exports match the handset's own MCU bytes exactly:

| Installed content | Words | Own MCU byte offset | Observation |
| --- | --- | --- | --- |
| Second loader | 623 | `11ac40` | Data `0a00` before its first program fetch |
| Resident upload | 422 | `115128` | Program `0590..0735` at resident entry |

The acquisition's zero-filled `0080..1fff` is not measured ROM. Acceptance
requires zero reads of that region while the executable RAM overlay is off.
Separately initialized data outside the captured alias is a declared policy,
not captured cold RAM.

The selected board inputs reuse the recovered NSM-3 keypad scanner and the
declared nominal BLB-2 ADC profile. These are not measured electrical units.
The storage fixture retains the acquired base record but omits the later PMM
journal. The original journal gives computed/stored checksum `77e2/7b26`,
self-test flag `84` and a blank display; it is not repaired with invented
identity or no-lock fields. The base-record fixture reaches the Security code
editor, where physical `12345`/OK unlocks the phone.

### Partial port-36 control model

Resident code `97a4..97c2` writes request `809c` to DSP I/O port `36`, raises
port 0 bit 4, polls port 36 bit 14, then clears request bit 15 and lowers the
strobe. Plain register storage cannot complete that transaction: the MCU
eventually initiates another DSP reset at `243874`, around 31.965 seconds.
The research composition enables a stateful request/strobe/DONE/ack model:

- Bits 1..4 selector `0e`, request bit 15 and a rising port 0 bit 4 start work.
- DONE bit 14 is hardware-owned, published by a deferred completion, never by
  reading the register or writing a software DONE bit.
- Clearing/changing the request cancels pending work; reset clears state and
  its timer. Unknown selectors do not acquire fabricated completion.
- One backend clock is an **unmeasured latency policy**, not a recovered
  peripheral frequency. Physical ownership and command meaning remain open.

This supplies no RF/sample/calibration data and no MCU or DSP service verdict.
It is disabled in other compositions. Independent literal transition tests
cover request/strobe ordering, completion, acknowledgment, cancellation,
unknown commands and reset. Do not rename it as a proven FIR, MFI or PLL block.

The absolute CMPM at uploaded address `0b38` reads BRC against `ffff`; its
admission is covered by eight independent literal/retention/save-restore cases
derived from TI SPRU172C 4-33, not a ROM-specific compare result. A table-copy
ordering defect was independently reproduced and corrected: READA's older
Read-stage destination update must precede a younger Access-stage use of the
same AR. Otherwise initialization loses its table position and attempts mapped
register copies. The initializer now terminates organically; additional mapped
destinations were not admitted to mask that error. CPU contracts live in
[C54x execution contracts](c54x_execution_contracts.md#harness-contract).

Reproduce the input preparation with:

```sh
python3 tools/dsp_rom6_candidate_check.py roms/research/nsm3-rom6 --prepare roms/nsm3r6
```

The complete acceptance command is `make verify-8210-native-rom6`. It creates
fresh isolated fixtures and checks the actual backend/clock configuration,
own uploads, organic initialization, legal native execution and acquisition
coverage in every process. Its three independent scenarios are:

1. Physical unlock and Menu reach Messages; a physical Down contact at 54.5
   seconds reaches Call register. The 60-second run admits no additional
   reset/upload/resident entries. Reviewed title/softkey pixels exclude only
   the animated icon; input/decode evidence remains mandatory.
2. A 24-second checkpoint restores full ARM/DSP registers and RAM/program
   fingerprints, then repeats physical Menu. Both 26.7-second endpoints and
   full frames match. Every DSP I/O write in the 2.7-second replay window is
   streamed with its timestamp and compared exactly, not just a capped prefix
   (20,066 writes in the qualified run).
3. Physical A/123 Save reaches the SIM through the real controller/card path;
   a separate cold process reopens the retained card and physically reads the
   saved contact. Exact APDU/storage and reviewed contact pixels are required.

`tools/rom6_native_observe.lua` is the read-only observer used by these scripts;
running it alone is not UI/storage acceptance. The canonical runner is
`tools/run_rom6_native.py`, with optional `--scenario menu|state|phonebook`.
This qualification does not promote the normal `noki8210` machine or establish
authentic factory-default storage, native network registration or speech.

The next radio-facing boundary is mapped in [the ROM6 I/O census](rom6_rf_interface.md):
the native DSP issues serial words and reads samples, but the backend has no
attached RF tuning/receive model. This does not reopen the completed boot gate.

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
| Actual MCU upload/copy provenance and resident init at `2c75`, `rom6-faithful-boot-plan.md` | Own 623/422-word uploads, resident entry and organic verification/self-test locally qualified | Keep upload provenance independent of the acquired mask. No transplanted routine, synthetic return, verdict or HLE handoff. |
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

1. **Input gate (qualified).** Strict private-input auditing covers hashes, sizes, accepted
   row statuses, exact coverage, agreeing overlaps, word/byte order and exact
   reconstruction. Distinguish program, data alias and padding. Reject missing
   or contradictory words; retain acquisition originals. `make audit-dsp-rom6`
   now implements this gate. `dsp_rom6_candidate_check.py --prepare DIR` writes
   the verified program and a separately labelled data-alias region with zero
   initialization elsewhere, not a captured cold-RAM state. Neither is selected
   by an ordinary handset configuration.
2. **8210 native composition (qualified research profile).** Use the existing C54x/backend seam in an explicit
   research machine, not a second interpreter or a replacement for `nsm3hle`.
   Bind the candidate program view, evidence-backed data aliases, overlay extent,
   shared window and reset/release wiring. Do not seed ROM4 runtime RAM or copy
   ROM4 power-on interrupt/port policy without evidence. Observe the first
   unsupported access/instruction rather than answering it speculatively.
3. **Organic initialization and bounded UI/storage (qualified).** Require exact own uploads, resident `2c75` entry,
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
