# ROM4 DSP loader contract

## Current result

The NSE-1 v5.30 MCU drives a real staged DSP boot. A cold transfer runs through
loader1 at DSP address `0x0f00`, the MCU asserts DSP reset, and a second release
preserves DARAM. Loader1 then requests block `0x12`; the matching descriptor
installs loader2 at `0x0d80`. Loader2 enters the run-mode dispatcher, which
requests further blocks by catalogue index.

This result no longer requires the compatibility path that copied words from a
flat DSP image into `0x2000..0x27ff`. Its old explanation was wrong: loader2
does not clear that range. Loader1 itself populates the resident branch table
with a repeated MVDP data-to-program transfer.

## Descriptor fields

The six halfwords in each recovered catalogue record are:

| Word | Current meaning | Evidence |
|---|---|---|
| 0 | DSP destination | Matches loader2 home `0x0d80` and later installed block homes. |
| 1 | transform/decoder selector | Stable within block families; values such as `0x1e80` and `0x0b39` are callable DSP addresses. Exact algorithm names remain open. |
| 2 | remaining output words | Block 2 falls `0x011c -> 0x00a4 -> 0x002c` as its destination advances by `0x78` words per chunk. |
| 3 | staging address | Stable during a block and points into the loader's low-DARAM work area. |
| 4 | input chunk length | Matches the upload-header values observed at each transfer. It need not equal output length. |
| 5 | flags | Zero in the recovered 27-record catalogue; no semantics assigned. |

The multi-chunk observation is important: catalogue records describe a
transform from staged input to an output range, not a direct flat-memory copy.
Tools must therefore not reconstruct DARAM by copying bytes following a
descriptor to word 0.

## Observed lifecycle

The normalized ordered trace is:

1. loader1 receives the cold double-buffer stream and the first DSP release;
2. reset is asserted after the cold transfer;
3. descriptor 0 (`fd00 ff80 0244 0500 0078 0000`) is installed and acknowledged;
4. the warm release resets CPU state while preserving DARAM;
5. DSP request `0x12` selects descriptor 18 and installs loader2 at `0x0d80`;
6. run mode requests block 1, then block families with input lengths `0x0078`,
   `0x0118`, and `0x04ec`.

`make check-c54x-rom4-cold-execute` independently starts the local clean-room
core at the mask-ROM reset vector with only the recovered program image and
complete `0xb000..0xefff` DROM populated. Reset reaches PC `0x0f00`, where the
program word is still zero, and stops at PC `0x0f01`. This is the expected MCU
upload boundary: loader1 is not resident in the mask image and isolated DSP
execution cannot proceed by pre-seeding it.

`tools/dsp_rom4_upload_trace_check.py` verifies this ordering and rejects an
observed input length absent from the recovered catalogue.

The local MAME tree contains a selectable `nokia_dsp_c54x_device` backend. It
maps DSP data addresses `0x0800..0x0fff`
directly onto DSPIF's existing MCU-visible shared store and resolves program
accesses in `0x0080..0x27ff` through the same data store while `PMST.OVLY` is
set. The host doorbell latches C54x `HPINT` (maskable source 9, vector 25).
The `noki5110` research configuration selects it instead of the HLE. NSE-1
firmware now uploads loader1 through ordinary HPI writes, drives the recovered
hold/release sequence, and executes the uploaded code. Runtime interleave is
boosted only at DSP reset release and the two mailbox ownership writes; no DSP
reply or shared word is synthesized.

The cold exchange, block-`0x12` request, and loader2 install now complete in
MAME. Loader1 publishes matching header words `0x0802=0x0803=0x0004`, the MCU
enters its 64-block streaming loop, and the core executes the uploaded loader
and mask-ROM transform helper. After the warm release it transforms descriptor
0, writes request selector `0x0012` to DSP word `0x0871`, and raises the
DSP-to-MCU service interrupt. The MCU's ordinary IRQ4 path consumes that
selector, streams descriptor 18 (`0x01f4` input words), and installs loader2 at
`0x0d80`; no shared word or acknowledgement is synthesized.

Loader1 also fills the otherwise undeclared resident range beginning at
`0x23c4`. Its `RPT`/`MVDP *AR2+, pmad` sequence increments both the indirect
data source and the encoded program destination on each repetition. Modeling
that architectural repeat behavior removes the last flat-image/`SEEDDARAM`
dependency from the MAME backend. The installed program subsequently requests
and receives another block (`0x044c` input words) and executes into the
operational ROM. The DSP's INT2 input is the level comparison between the
port-1 command latch plus live MDISND ring state and the port-2 accept mask.
The operational ISR now consumes the complete MCU ring (`0x0033/0x0033`) and
returns to its ordinary idle at `0x31a5` without an illegal instruction.
DSP port-1 completion strobes also ring the MCU doorbell. The ordinary FIQ0
handler drains the DSP receive ring to `0x0083/0x0083`; this is transport
delivery, not a synthesized MCU message.

The coherent run also executes the challenge transform on the MCU-supplied
security records. With the generated factory profile and a fresh NVRAM root,
the input halves are `d6fb 4394 e437 da16 9668 964f` and
`5cd4 32fe 5be2 dba6 9643 82d7`. The C54x decodes them to the wildcard profile
and publishes its response through FIQ0. At the validator boundary the MCU
object contains `3532 0000 ffff ffff ff0f 0000 0078 54c2 0000 0000 0000 0000
087c 0000`, and validation continues with `r6=1`. No reason-4
warm reset occurs. The input acquisition is also organic: DSP reader `0x4610` selects
COBBA serial registers 5 and 6, reads the modeled nominal values `0x160` and
`0x010`, and the caller stores `0x0016`/`0x0010` in its input object. The
coherent gate checks these values after the transaction. The old rejection
conclusion used stale ROM addresses and mistook the
DSPIF backing array for the mapped HPI view. It is retired.

`make verify-5110-menu` extends that result through the handset UI. It creates
a fresh NSE-1 EEPROM profile, boots v5.30 with the C54x backend, and applies a
physical Menu-key transition before the firmware's unresolved idle clock-stop
request. The key reaches MAD2 IRQ0 and the ROM4 five-by-five matrix scanner;
firmware renders the Phone book menu with deterministic frame SHA-256
`d82cc6891fcf4efb0bd11ded583508f40826f58aa69463708a46897b76fdffb5`.
The gate rejects baseband resets and illegal DSP instructions. It uses no HLE
DSP reply, firmware-state write, or injected firmware input event.

NSE-1 uses KBGPIO data/command ports `0x2b`/`0x2c` and a five-row matrix.
Firmware temporarily masks all columns while changing row drive and consumes
the cold-start power indication as a one-shot. MAD2 IRQ0 acknowledgement must
therefore clear that latch and adopt the released matrix as its new baseline;
retaining it as a held key makes column 1 permanently low and hides subsequent
physical input transitions.

Cold-start and operational power-button ownership are not interchangeable on
this ROM. The NSE-1 board contract places the power indication on special
column mask `0x02`; both the reset latch and live input sampling use that mask.
A physical post-boot press consequently reaches IRQ0 and the scanner at
`0x290c2c`, which records raw special value `0x81` at `0x10b6c8`. The active
firmware key-map variant remains zero, however, and its table at `0x2ab518`
maps that value to `0x3e` (no key), not semantic key `0x0d`. A one- and
four-second hold therefore produces no shutdown transaction. The neighboring
variant does contain `0x0d`, but no observed or statically direct writer selects
it; forcing that selector would not establish the operational power circuit.
NSE-1 shutdown input ownership remains unresolved rather than being assigned
to KBGPIO from the cold-start evidence alone.

At about 8.54 seconds this ROM writes clock-control value `0x0e` from
`0x292868`. The ROM4 wake protocol after that request remains unresolved, so
the later-ROM ARM-suspension rule is not projected onto NSE-1. The menu gate
deliberately presses at 8 seconds and is keypad/UI evidence, not ROM4 idle-wake
evidence.

The backend also terminates the distinct C54x memory-mapped `0x22`/`0x32`
parallel control path at COBBA. Frames retain their recovered opaque form:
bits 15--12 select one of 16 registers and bits 11--0 carry data. The coherent
ROM organically emits register-C transitions `0x008 -> 0x0c8` during codec
bring-up and writes codec serial port `0x21`. A corrected twelve-second
interface census records zero reads from RF sample port `0x27` and zero
synthesizer pairs on ports `0x31`/`0x32`. An earlier gate appeared to require
an RF read but lacked shell fail-fast behavior, so that observation is retired.
These results establish audio-interface initialization only; receiver
activation, tuning, decoded analog register meanings and a completed speech
call remain outside the demonstrated lifecycle.

The absence persists across a 30-second unassisted run: the backend delivers
6,497 enabled GSM frame interrupts while recording `rf_reads=0` and
`rf_tune_pairs=0`. Thus the immediate missing contract is the command or DSP
state transition that enables the ROM4 receiver, not an I/Q waveform. Ports
`0x31` and `0x32` are retained as a saved, passive low/high-pair census and
port `0x27` remains connected to deterministic unattached RF input. Supplying
FCCH/SCH/BCCH samples before the DSP reads that port would be unobservable and
is not an admissible registration fix.

The SIM transaction ending near 8.51 seconds is not a stalled initialization
sequence: the firmware has read all ten configured ADN records. At 31.002
seconds it organically issues `A0 F2` STATUS as its periodic card-presence
monitor while the UI also refreshes the LCD. The long quiet interval is a
healthy maintenance cadence and does not explain the absent receiver
activation. A 40-second run still records zero RF reads and zero completed
synthesizer pairs.

`make check-c54x-rom4-coherent` now treats that absence as a quantified
boundary. Its recipe is fail-fast and enables the trace category required by
its validator assertions; a failed intermediate assertion cannot be hidden by
a later PASS line.

`make verify-5110-save-state` saves the running real-DSP composition at seven
seconds, restores it, verifies MAD2 and C54x idle state, and then opens the same
Phone book menu through a physical key transition. This guards the C54x core,
uploaded program/data overlays, DSPIF, COBBA, timers, and keypad composition
against state-registration regressions.

The historical harness's headline `74 acknowledgements` counter is not a count
of DSP port-1 completion strobes. A four-second local run observes 72 MCU
writes across shared mailbox words `0x0fe`/`0x100` and 29 port-1 strobes while
still reaching the accepted validator result and interactive UI. The external
counter was attached to intercepted MCU mailbox writes with different
boot-phase lifetime rules. Keep these as separately named measurements; do
not tune the local transport merely to make the integers equal.

An older persisted donor EEPROM produces the distinct `c9f4 cd44 ... 6075`
challenge and the structurally valid but rejected `3532 0000 312b ... 88b2
0000` response. That run repeatedly requests a reason-4 reset and is a negative
control, not a coherent-boot result. The coherent gate therefore regenerates
the external 24C16 profile and uses a fresh NVRAM directory before requiring
both the C54x completion and the MCU validator's `r6=1` continuation. Resident
slot `0x250b` is still an organically uploaded no-op; the recovered acquisition
path is the later COBBA serial-register read rather than a missing overlay at
that slot. No response field is synthesized.

The NSE-1 external EEPROM participates in this transaction. Firmware organically reads
its board-level 24C16 through PUP GenIO (SDA bit 0, SCL bit 2). A virgin repair
image generates a different challenge; a provisioned image generates the exact
known-good challenge. Runs must use a fresh NVRAM directory when changing the
ROM seed because MAME correctly persists the device contents.

The ROM4 data map also contains read-only dispatcher entries at offsets ending
in `0x07` across `0x9000..0xdfff`. A live run proved code at `0x37fc` otherwise
attempts to overwrite `0xb707` immediately before the challenge. The backend now treats
those mask-ROM writes as no-ops. This prevents later dispatcher corruption but
does not by itself correct the first challenge's task selection.
COBBA control ports `0x2c`/`0x2d` and codec serial port `0x21` now terminate in
the COBBA device. PCM sample timing remains open.

The core corrections required to reach this point are generic C54x semantics:
absolute `STM` extension order, `MVDM`, `DLD`/`DST`, `CMPL`, immediate `XOR`,
compound `XC`, all `IDLE` and ST0/ST1 bit-set/reset variants, `RPTZ`, and
memory-counted repeat of multiword instructions, repeated `MVDP` and `MVDM`
destination update, sign-extended shifted loads, carry rotations, delayed and
conditional control flow, interrupt return,
extended absolute arithmetic/load/store, and accumulator-indirect branch.
Operational ROM execution has additionally established signed/unsigned
accumulator arithmetic, accumulator-shift-mode loads, memory compare,
accumulator-addressed program writes, stack data pushes, conditional
branch/call/return families, signed `FRAME`, immediate cross-accumulator ALU,
and dual-memory moves. Each family has focused core conformance coverage;
none recognizes a Nokia address or loader byte pattern.

The retained NSE-1 trace fixes reset polarity and edge behavior without an
inference: MCU writes to MAD2 byte `0x20002` are `1` (cold release), `0`
(hold), `0`, then `1` (warm release). A later value `3` leaves the DSP running.
MAD2 now exports this product-mask-derived level to the backend; the C54x
implementation drives the CPU reset input so CPU state restarts while the
external DSPIF/on-chip DARAM stores survive the warm transition.

## Resolved flat-image dependency

Before the decoder correction, sampled words at
`0x2000`, `0x216a`, `0x2286`, `0x23c3`, `0x2470`, `0x25b4`, and `0x27ff` are all
zero. Subsequent demand loads populate catalogue-owned ranges, but the gap
`0x23c3..0x25b3` is outside every declared destination. Without the assist the
co-sim completes 70 rather than 74 DSP acknowledgements and does not reach a
stable UI.

The gap's bytes occur in MCU flash, but location alone is not a destination
proof. In particular, the bytes matching `0x23c3..0x25b3` immediately follow
descriptor 0, whose declared output destination is `0xfd00`. They may be
encoded input, shared source material, or an artefact of how the recovered flat
image combined address spaces.

The producer is loader1 at `0x0f1f..0x0f28`. It computes a count from the
`0x23c4..0x252a` bounds, selects source `0x0d00`, and executes repeated MVDP to
program destination `0x23c4`. With `PMST.OVLY=1`, those program stores must
resolve to the same DARAM cells used by program fetches.

The external experimental core originally wrote MVDP directly to an immutable
program store, bypassing its overlay helper. MAME's backend already routes
program writes through the DSPIF overlay, but initially repeated the encoded
program destination without the C54x's repeat-mode address update. Advancing
that destination per repetition populates the resident range organically. The
external unassisted run's 74 acknowledgements and stable UI remain the complete
ROM4 reference; MAME has now independently crossed the same loader boundary.
This is an instruction-semantics correction, not a reconstructed image or
timing adjustment.
