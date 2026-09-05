# Upstream architecture target

This document defines the intended MAME-facing architecture. It is a source
ownership contract, not a claim that every device below is complete.

## Machine composition

`nokia_dct3_state` owns handset composition and board wiring. Product
differences belong in typed product contracts and machine configurations. The
driver must not select behavior from firmware PCs, filenames, driver names or
environment variables.

| Layer | Intended owner | Current implementation |
| --- | --- | --- |
| ARM execution | generic MAME ARM7 core | present |
| MAD2 system logic | Nokia MAD2 devices: CTSI, PUP, KBGPIO, UIF, GENSIO, MBUS, SIMI and DSPIF | extracted partial hardware |
| power management | `nokia_ccont_device` | extracted partial hardware |
| audio/RF codec | `nokia_cobba_device` | serial-control capture seam and partial PCM/audio model |
| persistent storage | generic flash and I2C EEPROM devices, with a temporary B3 adapter where the generic flash core lacks partition behavior | present, partly transitional |
| display | geometry-configured PCD8544-family device | present |
| SIM | SIMI controller plus replaceable card-side protocol device | present |
| DSP execution | a future generic TMS320C54x core and product-correct internal ROMs | absent |
| DSP backend contract | `nokia_dsp_backend_interface` | present; transport-facing substitution seam |
| DSP compatibility backend | `nokia_dsp_hle_device` | present and explicitly provisional implementation of that seam |
| laboratory cellular network | radio/link/session/network peer devices beyond the DSP boundary | present; deterministic test infrastructure rather than RF hardware |

Device boundaries follow independently stateful hardware interfaces, not
package count. MAD2 is one physical ASIC, but retaining its internal peripheral
blocks as devices keeps their contracts independently testable and reusable.

## DSP substitution seam

`nokia_dspif_device` is the MCU-visible transport and is the fixed boundary for
both DSP implementations. It owns only:

- the shared-RAM backing store;
- DSPIF command/doorbell registers;
- transmit and receive ring storage and cursor mechanics;
- MCU-facing FIQ0 and service-interrupt delivery; and
- callbacks that report MCU accesses to an attached backend.

It must not manufacture bootstrap values, interpret GSM messages, decode audio
commands or choose product behavior. `nokia_dsp_backend_interface` is the narrow
abstract endpoint for DSP-originated semantics: DSPIF notifications and shared
cell accesses enter through it, while tone state is exposed to the handset's
audio sinks. It contains no bootstrap, service, speech, radio or product
contract. As a MAME `device_interface`, it can be implemented by an ordinary
device or a future `cpu_device` without duplicate `device_t` inheritance.
`nokia_dsp_hle_device` implements that endpoint today; the driver
retains a separately typed optional finder only while applying HLE-specific
product contracts. Runtime transport, reset and tone paths use the abstract
backend.

The real backend must implement that same endpoint and supply:

1. TMS320C54x program, data and I/O address spaces;
2. MAD2 reset, release and interrupt wiring;
3. host-port/shared-memory visibility and arbitration;
4. MCU-to-DSP doorbells and DSP-to-MCU FIQ/IRQ signaling;
5. firmware-uploaded code/data overlays through ordinary MCU writes;
6. DSP serial control of COBBA;
7. the parallel MFI/control plane used for radio and codec control; and
8. bidirectional PCM clocks, framing and sample data.

The pinned MAME tree contains a `tms320c5x` core but no TMS320C54x core. The
C54x is not a device variant that can be enabled by adding a type alias: its
instruction encoding, 40-bit accumulator behavior, repeat-block machinery,
memory overlay rules and on-chip peripherals differ materially. The real
backend therefore requires a new clean-room MAME CPU device based on TI's
public architecture manuals. The related C5x core is useful as a MAME CPU
device-API reference only, not as an execution base whose results can be
treated as C54x behavior.

The HLE and real core are alternatives. A real core must not run concurrently
with HLE code that writes the same shared words, advances the same ring cursors
or drives the same COBBA interface.

The clean-room core has now started under `cpu/tms320c54x`. Its first checkpoint
is intentionally an execution scaffold, not a claimed emulator: it registers
the three word-addressed spaces, 40-bit accumulators, auxiliary/status/repeat
state, debugger state and save-state data, but stops on every undecoded opcode.
This makes unsupported semantics visible while the independently captured ROM4
transform fixture grows the generic instruction implementation. Nokia mailbox,
COBBA and product-profile behavior remain outside this CPU directory.

The first semantic tranche covers the generic encodings exercised at the
transform boundary for CALL/RET, RPT/RPTB, immediate auxiliary-register loads,
auxiliary-register moves, ordinary indirect pre/post addressing, accumulator
loads/logical operations and low-word stores. Stack growth and block-repeat
termination follow the TI CPU guide. This is still below the 628-opcode ROM4
boot surface and is not yet selected by any handset configuration.

`make check-c54x-core` executes distributable synthetic programs on the MAME
CPU device itself. It currently checks the downward-growing CALL/RET stack,
`RPT`'s `k+1` count, immediate MMR decoding, and an `RPTB` whose final
instruction is a two-word CALL. The first run rejected an incorrect compact
AR-index interpretation of MMR code `0x12`; the corrected decoder treats
`0x10..0x17` as AR0..AR7. This executable gate, not source-token inspection,
is the acceptance boundary for subsequent opcode families.

The gate also executes the final six-iteration ROM4 challenge-transform loop
from independently observed workspace operands. Its six output words reproduce
`1cee 7cb6 d2a3 b986 4c57 e65e` exactly. This validates SXM-controlled
16-to-40-bit operand extension, NOT/OR/XOR behavior, descending auxiliary
pointers, low-word stores and the RPTB/CALL interaction in the MAME core; it
does not yet claim that the preceding transform routines execute.

`make check-c54x-rom4-execute` is the private-input companion gate. It copies
the ignored transform-entry program/data snapshots into MAME's ROM path,
verifies their declared hashes through ordinary ROM loading, restores the
captured architectural registers, and executes from `0x4b73`. The current core
constructs challenge header `3532 0000` organically, proceeds through the
transform that subsequently mutates that workspace, and stops after fetching
unsupported conditional branch `f820` at `0x3810` (`PC=0x3811`). Reaching it executes
the complete `0x4b82` block-repeat copy, its `0x3900` status helper, and the
`0x7f2d` transform helper through its indirect/direct transfers, mixing stages,
and nested CRC-style carry loops. It also executes the delayed `0x37ce` call
through its measurement setup and first DSP port write.
That address is now
the measured implementation frontier; expanding the core must move it forward
while preserving the synthetic and transform-tail gates.

## DSP ROM policy

Keep the following storage domains distinct in MAME ROM declarations:

- MAD2 ARM mask ROM;
- DSP program ROM;
- DSP data ROM;
- DSP peripheral/data ROM, where the silicon exposes a separate domain; and
- overlays that the MCU uploads from handset flash at runtime.

An unidentified or unavailable internal ROM is `NO_DUMP`. A compatibility HLE
does not turn an undumped ROM into a dumped one and must not be represented by a
placeholder ROM file. Images are promoted only with provenance, exact size,
byte order and cryptographic hashes. A ROM from another MAD2/DSP generation is
not a substitute merely because its bootstrap words look similar.

The repository may keep private, ignored research copies under `roms/research`
as described by `roms/README.md`; distributable MAME sources contain only ROM
metadata.

## COBBA boundary

COBBA remains a separate mixed-signal device. Its eventual contract has three
independent planes:

- serial register selection and 12-bit data transfer;
- parallel MFI/RF control frames; and
- PCM audio input/output with product-specific board routes.

Unknown register meanings remain opaque stored state until documentation,
paired firmware behavior or a capture establishes their effects. The present
HLE voice profile is a declared fallback and must disappear from real-DSP
machine compositions.

## Network boundary

The handset model ends at an explicit DSP/radio attachment. The deterministic
GSM network is useful for regression and interactive emulation, but it is not a
simulation of the analogue RF board. A future external GSM or SIP bridge should
attach beyond the network/call adapter and must not add telephony policy to
MAD2, DSPIF or COBBA.

## Upstream series

The likely reviewable sequence is:

1. generic flash and PCD8544-family corrections;
2. independently useful Nokia hardware devices;
3. the Nokia DCT3 family driver with explicit HLE status;
4. a clean-room, MAME-compatible TMS320C54x CPU core;
5. real-DSP machine configurations as ROM provenance permits; and
6. further handset profiles promoted by product-specific gates.

Research traces, firmware-address symbol maps, raw captures and exploratory
checkers remain in this repository. Upstream source keeps concise hardware
comments, citations, ROM metadata, save-state support and user-visible
limitations.

## DSP promotion gates

A real DSP backend is not promoted because it executes instructions. At
minimum it must reproduce, without HLE writes on the same boundary:

- reset and bootstrap exchange;
- MCU-uploaded memory blocks and completion signaling;
- shared-ring traffic and interrupt delivery;
- COBBA control traffic;
- stable idle operation;
- call setup and teardown; and
- bidirectional speech PCM under save/load.

Until those gates pass, the HLE remains the supported compatibility backend and
the real core remains experimental.

Before full-phone promotion, the new CPU core must pass smaller deterministic
ROM4 gates for the instructions already known to be load-bearing: OVLY-aware
MVDP, RPT/RPTB over multiword CALL, F0B0 status operations, repeat-mode
immediate-address updates, 40-bit shifts/rotates and shifted cross-accumulator
logical operations. These tests should be derived from TI semantics and small
instruction fixtures; the external no-assist phone run supplies expected
boundary results but no implementation source.

An observation-only 30-million-MCU-instruction run through interactive ROM4
idle executed 628 distinct 16-bit opcode words spanning 112 high-byte groups;
the sorted opcode-set SHA-256 is
`63ae45c872ccea39112898bfde7f5926acd675701675a860d765a66a35815d34`.
This is broad functional coverage, not evidence that a transform-only decoder
can boot the phone. `tools/c54x_opcode_coverage.py` preserves that baseline and
will measure the clean-room core's implemented/executed surface. Development
should still begin with the deterministic loader and challenge fixtures, then
expand against the 628-word boot set before claiming full ROM4 execution.

The clean MAME NSE-1 composition currently reaches 437 distinct opcode words
across 80 high-byte groups in twelve seconds (sorted-set SHA-256
`523cd6019736d1fb7bb09cae8a0f96124a97a768f00e40df7c3933be6ba7c7bd`).
The 191-word count difference is a reachability delta, not by itself a CPU
correctness failure: the earlier 628-word reference exercised DSP paths that
the current unassisted receiver never activates. In particular, its first-PC
records include the COBBA receive loop at `0x3238`, while the clean MAME run
performs no RF-port reads. Keep the reference fingerprint distinct from the
clean-run fingerprint until the external stimulus behind that extra coverage
is classified.
