# Upstream architecture target

This document defines the intended MAME-facing architecture. It is a source
ownership contract, not a claim that every device below is complete.

## Machine composition

`nokia_dct3_state` owns handset composition and board wiring. Product
differences belong in typed product contracts and machine configurations. The
driver must not select behavior from firmware PCs, filenames, driver names or
environment variables.

The board source is divided by responsibility:

- `driver/nokia_dct3.cpp`: runtime board wiring, ordinary handset machine
  configurations, and the single ROM/system catalogue;
- `driver/nokia_dct3_config.h`: typed product contracts, immutable product
  data and shared register constants;
- `driver/nokia_dct3_state.h`: the shared board-state declaration;
- `driver/nokia_dct3_research.cpp` and `.h`: explicit compatibility and
  staged-backend configurations, research input compositions, laboratory
  PCM clocks and instruction observers.

Research machines remain registered in the main catalogue because MAME's ROM
tables have translation-unit-local linkage. This keeps ROM aliases, BIOS
selection and storage namespaces authoritative in one place rather than
duplicating product declarations. Their separate implementation is not a
capability promotion: donor-mask compatibility, laboratory clocks and
unvalidated bus dialects retain their explicit research status. Device models
and ordinary board callbacks are shared, not forked for these machines.

| Layer | Intended owner | Current implementation |
| --- | --- | --- |
| ARM execution | generic MAME ARM7 core | present |
| MAD2 system logic | Nokia MAD2 devices: CTSI, PUP, KBGPIO, UIF, GENSIO, MBUS, SIMI and DSPIF | extracted partial hardware |
| power management | `nokia_ccont_device` | extracted partial hardware |
| audio/RF codec | `nokia_cobba_device` | serial-control capture seam and partial PCM/audio model |
| persistent storage | native flash and I2C EEPROM devices; ST owns command/protection/deferred-operation and persistent OTP engines, literal CFI and explicit identity/electrical limits in `flash_storage.md` | present, partially validated |
| display | geometry-configured PCD8544-family device | present |
| SIM | SIMI controller plus replaceable card-side protocol device | present |
| DSP execution | clean-room TMS320C54x core and product-correct internal ROMs | native 5110 plus explicitly scoped cross-product ROM4 compatibility profiles; fitted-mask identity remains product-specific, with HLE/silent alternatives retained |
| DSP backend contract | `nokia_dsp_backend_interface` | present; transport-facing substitution seam |
| DSP compatibility backend | `nokia_dsp_hle_device` | present and explicitly provisional implementation of that seam |
| GSM-FR codec | `util::gsm_fr_codec` and bundled libgsm | generic utility, independently built and linked by MAME; explicit predictor snapshots |
| laboratory cellular network | radio/link/session/network peer devices beyond the DSP boundary | present; deterministic test infrastructure rather than RF hardware |

Device boundaries follow independently stateful hardware interfaces, not
package count. MAD2 is one physical ASIC, but retaining its internal peripheral
blocks as devices keeps their contracts independently testable and reusable.

The native/staged/HLE DSP backends, MAD2 PCM and MBUS terminal expose attachment
setters rather than assuming parent sibling names. Board configuration binds
their transport, codec, power, peer and ROM dependencies. Their own internal
CPU child remains local. The native BSP fixture deliberately renames all five
external dependencies to exercise this boundary. Laboratory network-peer
internal attachments retain their existing composition; this is not a mandate
to replace every finder with a callback or build a generic dependency framework.

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

The native DSP backend implements that endpoint. Full product coverage still
requires the following contracts to be validated or completed:

1. TMS320C54x program, data and I/O address spaces;
2. MAD2 reset, release and interrupt wiring;
3. host-port/shared-memory visibility and arbitration;
4. MCU-to-DSP doorbells and DSP-to-MCU FIQ/IRQ signaling;
5. firmware-uploaded code/data overlays through ordinary MCU writes;
6. DSP serial control of COBBA;
7. the parallel MFI/control plane used for radio and codec control; and
8. bidirectional PCM clocks, framing and sample data.

Stock MAME has a `tms320c5x` core but no TMS320C54x core. This repository's
MAME overlay adds a clean-room C54x core. The C54x is not a variant of C5x: its
instruction encoding, 40-bit accumulator behavior, repeat-block machinery,
memory overlay rules and on-chip peripherals differ materially. The real
backend therefore uses the new MAME CPU device based on TI's public
architecture manuals. The related C5x core is useful as a MAME CPU
device-API reference only, not as an execution base whose results can be
treated as C54x behavior.

The HLE and real core are alternatives. A real core must not run concurrently
with HLE code that writes the same shared words, advances the same ring cursors
or drives the same COBBA interface.

The clean-room core lives under `cpu/tms320c54x`. It implements the ROM4
instruction surface needed for the 5110's observed startup and menu path, while
still stopping visibly on unsupported opcodes. Nokia mailbox, COBBA and product
profile behavior remain outside the CPU directory. The 5110 machine removes
`dsp_hle` and instantiates `NOKIA_DSP_C54X` with its own ROM4 images; other
handsets do not inherit that ROM or execution contract.

### CPU validation ownership

The CPU supports independently configured atomic/staged execution and explicit
extended-program capability. Product wiring, internal-ROM provenance and bus
profiles remain backend responsibilities; enabling a CPU feature does not
qualify a new handset.

[C54x execution contracts](c54x_execution_contracts.md) owns ISA admission,
arithmetic, pipeline/dependency semantics, synthetic fixtures and the qualified
default-suite checkpoint. [ROM4 DSP loader](rom4_dsp_loader.md) owns upload,
bootstrap, cold-entry masks and original-ROM routine/bus evidence.
[5510 bring-up](5510_bringup.md) owns the separate MU4 application boundary.

Keep these acceptance levels distinct:

| Acceptance | Scope |
| --- | --- |
| `check-c54x-core` and isolated CPU profiles | Executed ISA/composition contracts, not handset or physical timing proof |
| `check-c54x-rom4-coherent` | Organic 5110 DSP service completion |
| `verify-5110-menu` and `verify-5110-save-state` | Unassisted graphical/input and restoration behavior |
| Staged/codec laboratory profiles | Only their separately documented clocks, media and replay contracts |

The default 5110 frontend remains atomic; explicitly selected staged profiles
do not silently promote fitted bus timing, native RF acquisition or speech.
Research mask substitutions and opcode reachability counts are not hardware
reset specifications or CPU correctness oracles.

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
simulation of the analogue RF board. The optional standalone
[SIP bridge](external_call_bridge.md#sip-call-bridge) attaches beyond the host
call adapter; physical 3210 incoming/outgoing HLE gates verify its signaling
and media transport, not native DSP speech. SIP policy and wall-clock media
remain outside MAD2, DSPIF and COBBA. An external GSM backend must preserve
the same boundary.

## Upstream series

The generic ARM7 patch `mame-arm7-pc-export.patch` refreshes the named
`ARM7_PC` debugger entry through the same export path as `STATE_GENPC`.
Without it, Lua can observe stale cached PC data after load despite restored
architectural registers. `check-c54x-rom4-pcm-restore` checks PC/GENPC agreement
and active native audio replay; this correction contains no Nokia logic.

The current overlay contains an ordered MAME patch stack plus local CPU, utility and Nokia
sources. `make check-mame-patches` replays the patches in Makefile order against
the pinned MAME commit in a temporary Git index. It does not inspect the dirty
overlay worktree or prove that the resulting source builds. Run it before
reordering patches or updating the MAME pin, followed by the relevant handset
and CPU gates after any behavior change.

The likely reviewable sequence is:

1. generic flash, PCD8544-family and host microphone-input corrections, plus GSM-FR utility integration;
2. independently useful Nokia hardware devices;
3. the Nokia DCT3 family driver with explicit HLE status;
4. the clean-room, MAME-compatible TMS320C54x CPU core now exercised by 5110;
5. additional real-DSP machine configurations as ROM provenance permits; and
6. further handset profiles promoted by product-specific gates.

Research traces, firmware-address symbol maps, raw captures and exploratory
checkers remain in this repository. Upstream source keeps concise hardware
comments, citations, ROM metadata, save-state support and user-visible
limitations.

The board firmware-trace quarantine has an explicit build boundary. The
standalone source header defaults `DCT3_RESEARCH_TRACES` to zero; the repository
overlay enables it by default for research acceptance. `make build-tracefree`
and `make check-tracefree` select zero. The latter retains the existing exact
v6.00/v5.01 menu oracles and checks three known probe strings are absent from
the linked executable. RAM writes, native flash routing and hardware logs
remain outside the guard; there are no production stubs that emulate firmware
trace results. The positive research build must contain the same canaries.

Both modes share one overlay/object tree: never build or run them concurrently.
The generated configuration is rewritten only when its content changes, so
mode changes rebuild the board through ordinary header dependencies. A normal
`make build` restores the repository research mode. Trace-free is not a claim
that the research machine catalogue, private benches or all upstream packaging
have been removed; those still belong to the separate transplant series.

Before submitting that series:

- Retire `nokia_dct3_trace.inc` from the proposed distributable source series.
  The trace-free build gate proves it can be excluded, not that the export is
  already packaged. Its current
  firmware-PC probes are observation-only but are research instrumentation,
  not a MAME hardware contract; preserve their accepted conclusions in the
  subsystem documents and retain focused checks here.
- Review the proposed bundled libgsm dependency and generic GSM-FR utility as
  their own change. MAME now builds and links the library through its ordinary
  project rules; handset sources contain no private C structure declaration.
  The utility's snapshot adapter includes the same bundled release's private
  header, so library updates require explicit state and codec regression checks.
  The dependency's acceptance remains a maintainer review decision; the local
  integration no longer needs a network download or custom linker flags.
- Review the generic flash, EEPROM, LCD, PulseAudio and C54x build/test patches
  independently. Clean patch application is necessary, but each patch still
  needs its own behavior tests and a scope appropriate to its MAME owner.
- Run MAME's source-format and build checks on the proposed source series, then
  the product gates for every affected handset. Keep private ROMs, generated
  EEPROM identities and research captures out of distributable patches.
- Follow the current [MAME contribution rules](https://docs.mamedev.org/contributing/),
  including disclosure of AI assistance and model versions in the initial PR
  description. Attribution and licensing must be reviewed for the final patch
  series, not inferred from the local source headers alone.

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

The 5110 already selects the real core and passes bootstrap, idle/menu and
save/load gates. RF acquisition and bidirectional speech PCM remain incomplete,
so its real-DSP path is experimental for full phone operation. HLE remains the
compatibility backend for the other handsets until their own ROM and boundary
gates justify promotion.

Before promotion, require the current qualified CPU checkpoint and the affected
handset gates, not a historical list of initially missing instruction families.
The loader contract retains the rejected cross-silicon IMR substitution and
the evidence for the product-specific cold-entry bit. Do not restore an imported
mask to increase opcode reachability or treat reachability as silicon evidence.
