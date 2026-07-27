# Driver structure convention

Goal: the Nokia driver should describe machine composition while reusable hardware behavior lives
in explicit MAME devices. Firmware-research execution tracing remains quarantined so it can be
deleted incrementally as observed contracts become components.

## Entry points vs. quarantine

The memory-map-registered handlers are thin. Diagnostic execution observations
are isolated in `driver/nokia_dct3_trace.inc`; no memory read path overrides a
firmware result:

| hardware entry point | quarantined research helper |
|---|---|
| `flash_r/w` | thin routing through `nokia_b3_flash_device`; selected read-only contract probes run before the physical read |
| `ram_w` | backing-store write plus calls to observation-only helpers for the focused audio/authentication gates |
| `ram_r`   (≈2 lines)  | none; display provisioning now arrives through EEPROM/NV |
| `mad2_io_r/w` | functional register routing and board-output helpers, followed by observation-only MAD2 trace helpers |

MAME's native `pcd8544_device` and `I2C_24C128` model the display and external
EEPROM. One validated product geometry contract configures controller RAM and
the visible MAME viewport together; unsupported products cannot resize only
the screen behind the controller. Headless LCD capture and scripted keypad input belong to the Lua
acceptance harness; the production driver contains neither a second LCD parser
nor synthetic key state. CCONT is an explicit local `nokia_ccont_device` owning its serial
registers, ADC results, RTC, interrupt state and watchdog. Task 7 remains the
firmware adapter to the external service/test peer; the request-driven
DSP behavior is split at its evidenced boundaries: `nokia_dspif_device` owns
shared RAM, DSPIF, packet rings and FIQ0/IRQ4 signaling;
`nokia_dsp_hle_device` owns all bootstrap mailbox policy, peer-published shared
state and service timing through one atomic product bootstrap contract with
private exchange helpers. Its DSP parameter-command decoder and optional
speech-request predicate are likewise one atomic product contract; DSPIF
carries no DSP-behavior configuration, making
the HLE a replaceable backend seam;
`nokia_external_service_peer_device` owns the separate class-`0x40` service
session; `nokia_radio_peer_device` owns Nokia L1 transaction correlation
behind one typed protocol contract and private acquisition strategies; and
`nokia_lapdm_link_device` owns decoded LAPDm establishment state, contention
identity, sequence numbers, stop-and-wait downlink segmentation and pending
downlink acknowledgements;
`nokia_gsm_session_device` owns the per-handset Layer-3 request and
acknowledgement-gated registration, paging, bounded call and SMS transactions; and
`nokia_gsm_network_device` owns
immutable standards-shaped cell, RR, MM, call-control and SMS data. The
phone state only composes those devices and wires DSPIF callbacks to MAD2.
`nokia_simi_device` owns the MAD2 register/FIFO/IIR/FIQ-facing controller and
connects by reset/byte callbacks to `nokia_sim_card_device`, which owns T=0,
declared file metadata, persistent mutable ADN/SMS/SMSP records and
the synthetic GSM 11.11 contents. `nokia_mad2_device` owns the CTSI registers
within offsets `0x00..0x16`: reset/clock/watchdog latches, timer state, interrupt
pending/masks, and ARM IRQ/FIQ routing. Attached devices signal it through
callbacks. `nokia_kbgpio_device` owns the sparse keyboard register families,
matrix scan, cold-boot latch and IRQ edge state; product input ports and MAD2
IRQ0 routing remain callbacks. `nokia_pup_device` owns output control, buzzer,
vibrator and GenIO latches and drives the external EEPROM/audio/output devices
through callbacks. `nokia_mbus_device` owns PUP offsets `0x18..0x1a`, RX/TX holding
state, byte callbacks and FIQ2/FIQ3 outputs without supplying a peer. The phone
state retains board wiring and less-established
peripheral windows. Its MAD2 memory-map handlers delegate register ownership,
interrupt routing and diagnostics to separate helpers so traces do not obscure the
functional dispatch. `nokia_b3_flash_device` is a transitional wrapper over
MAME's generic Intel-compatible flash core; it owns the 3410's independently
observable B3 lock, erase-suspend, partition-status timer and save-state until
those generic read-while-write semantics move into `intelfsh`. The phone state
only maps accesses and selects the product capability. `nokia_gensio_device`
owns its sparse serial/status/SELECT
registers and connects CCONT and the PCD8544 through callbacks. A separate
MAME patch extends the otherwise-unused native PCD8544 implementation with
default-preserving configurable controller/viewport geometry and internal
rendering. Its standard six-bank configuration retains the native three-bit Y
command mask; only controllers configured above eight banks decode bit 3. The
Nokia-local duplicate has been removed. Product differences use explicit typed
configurations rather than driver-name parsing or process-environment selection.
The neutral base compositions select only flash width and shared devices; every
machine entry then applies exactly one named product profile. Unvalidated 32-Mbit
products therefore retain conservative defaults instead of inheriting the
3330's SIM, DSP-service and external-service composition accidentally.
The production driver and devices do not read environment variables: passive
diagnostics use MAME logging, while negative composition and conformance
selection use standard MAME configuration ports populated by named external
fixtures.

Digital-baseband reset explicitly resets the stateful radio-correlation,
LAPDm-link and GSM-session peers alongside MAD2, DSPIF, DSP HLE, service peer,
SIMI and the LCD. The GSM network device contains immutable laboratory-cell
data; it has no transaction state to reset. CCONT, flash and EEPROM remain on
their documented surviving domains.

## Rules

- New *hardware* behaviour goes in a device model or the owning MAD2 register block.
- New firmware-address diagnostics require a named focused consumer and belong
  in `nokia_dct3_trace.inc` only while that regression cannot observe a device
  boundary. Do not add result forcing or task-message injection.
- The research helpers should **shrink over time**. Delete a trace after its
  conclusion is normalized; replace each RAM-read shortcut with its real
  hardware or nonvolatile-data owner.
- Driver and component files use MAME attribution headers. Device headers use
  include guards, rely on the including translation unit for `emu.h`, and must
  not introduce a second umbrella include.
- All firmware-result forces, registration-message injections and firmware RAM
  read overrides are removed. The synthetic EEPROM provisions the fields from
  the paired-ROM descriptor-`0x0749` reset constructor; firmware loads and
  copies them through its ordinary NV path. See `evidence_regime.md`.
- The direct firmware allocation trampoline,
  service-status RAM completion, and synthetic startup-report feed are removed.
  The current external-service peer consumes organic DSP-ring requests and returns
  request-derived transport/contact responses without writing firmware state.

## Component completion gate

A subsystem leaves this driver only when its external interface is explicit,
save-state fields are registered, configuration contains no firmware addresses,
and the applicable 3210 paired-ROM, 3310 independent-product and current
cross-ROM gates pass. Phone-specific constants remain in typed machine
configuration.

The local 3330 PPM E Wintesla records are normalized reproducibly by
`make normalize-3330` and exercised by `make smoke-3330e`. This is a labelled
cross-ROM execution baseline, not the canonical PPM C audit declared by
upstream MAME. Component changes must preserve the default and coherent 3210
oracles and avoid
regressing this smoke run.

## Regression oracle

Changes must preserve the exact LCD frame and the semantic structural
predicates exercised by `make verify` and `make verify-frontier`. Raw counters,
timestamps, scheduler order, and `error.log` text are diagnostic observations,
not acceptance criteria.
