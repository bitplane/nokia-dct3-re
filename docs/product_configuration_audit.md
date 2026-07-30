# Product configuration debt audit

This audit follows the radio, DSP service-control and DSP bootstrap contract
refactors. Its purpose is to identify invalid product-config combinations and
handset-family branching without turning independent evidence into a single
guessed profile.

## Findings

- Radio configuration is atomic. One `protocol_contract` owns wire values,
  acquisition strategy and lifecycle policy; disabled products cannot combine
  fragments from enabled products.
- DSP bootstrap configuration is atomic. BIOS variants replace the complete
  contract, so a pre-upload token cannot be combined accidentally with another
  exchange or completion policy.
- DSP service-control framing is atomic, but whether a product composes the
  service backend remains independent. This separation is intentional: NSE-3
  proves a receive-side frame contract while its unresolved bootstrap keeps the
  backend disabled.
- External-service transport enablement and its application contract must also
  remain independent for now. NHM-6 proves the lower transport without proving
  NHM-5 application data, while NSE-3 has only bounded dormant receive-side
  evidence.
- SIMI controller presence and the synthetic laboratory card are distinct
  hardware choices. They currently travel together in executable profiles, but
  combining them would make controller-without-card and removal/error testing
  unrepresentable.
- MAD2/COBBA PCM timing and word format already form one `bus_profile`.
  Analogue HLE routing is separately typed because physical connectivity and
  firmware-controlled mux/gain evidence have different confidence.

No product-name or driver-name dispatch remains in reusable protocol layers.
The only runtime firmware-variant selection is a generic, product-owned BIOS
override that replaces a complete bootstrap contract.

## Trace quarantine

Every retained firmware-address diagnostic in `nokia_dct3_trace.inc` has a
named focused consumer. The quarantine remains observational, bounded by its
structural test, and contains no firmware result writes or injected messages.
There is therefore no dead trace family to retire in this pass.

## Completed bounded refactor

DSP speech-control policy now forms one typed contract containing:

- parameter command;
- request mask; and
- request value.

The predicate is explicitly optional. NSE-8 and NHM-5 select independently
named contracts backed by their separate organic call lifecycles. NSE-3 selects
its evidenced command decoder with no speech predicate, so decoding cannot
silently enable media. PCM, radio, service-control and analogue routing remain
separate hardware and protocol boundaries.

## Completed display-geometry refactor

Display controller and viewport dimensions now form one typed geometry
contract. The LCD controller and MAME screen consume the same contract, and
zero-sized or controller-exceeding viewports are rejected centrally. The 3410
retains its evidenced 102-by-72 controller RAM and 96-by-65 viewport. The
unsupported 7110 and 6210 no longer resize only the screen while silently
leaving the controller at 84-by-48; they retain conservative defaults until
their different controller is modeled.

## Completed MAD2 DSP-reset wiring refactor

MAD2 DSP running-status readback and its release mask now form one typed wiring
contract. The default contract preserves ordinary latch behavior; the 3410
selects its evidenced `0x53`/`0x04` pair atomically, and half-configured pairs
are rejected.

## Completed KBGPIO board-wiring refactor

KBGPIO row topology and the power-on column mask now form one device-owned
wiring contract. Each evidenced product selects a separately named complete
contract, even where NHM-5 and NHM-6 currently share values. Invalid row counts,
multi-column power masks and masks outside the five physical columns are
rejected by the device.

## Acceptance status discovered by the audit

The audit's two divergences were caused by one omitted typed DSP contract.
Both NHM-6 and NHM-2 organically send type `0x70` payload `0d00`; leaving their
compact `0x74/0d00` completion disabled caused the observed CONTACT SERVICE and
non-idle outputs. The original oracles were not replaced. Fresh-PMM frontier,
save-state and physical navigation gates now reproduce them, and NHM-2 declares
its product fields explicitly rather than inheriting the NHM-6 builder.

The negative-composition controls also exposed a separate input-ownership
mistake: `HWCFG` and `DIAGCFG` had been nested in the 3210 keypad ports, while
other products silently received the optional-port fallback. They now belong
to the shared DCT3 input contract, which the standalone 3410 and 6110 matrices
include explicitly. `make verify-model-frontier-negative` removes only the DSP
service at that boundary. The 3330 then organically returns to CONTACT SERVICE
without FIQ0, while the 3410 remains at its deterministic non-idle frontier
without FIQ0; neither negative result is a promoted oracle.

## Completed registration/paging cleanup

The NHM-6 registration and paging passes introduced no product dispatch in the
network, session or LAPDm devices. Their post-acquisition behavior remains the
same shared state machine used by NSE-8 and NHM-5. The only retained radio
differences are the pre-existing atomic `protocol_contract` fields for
acquisition grammar, assigned-channel confirmation, repeated assigned-uplink
cadence, MM settle time and the still-opaque traffic-release parameter.

The paging boundary now names the handset-monitored and network-transmitted DRX
schedules explicitly. Whether a transmitted page is monitored is derived by
comparing their standards-defined paging groups, rather than by inspecting a
negative-fixture selector in the radio peer. Laboratory cell and paging
selectors are mapped through typed tables at machine reset; they do not create
phone-model branches.

Trace acceptance no longer embeds the laboratory IMSI or its known `(phase,
frame-offset)` result. It extracts the identity from the organic Location
Updating Request, derives the TS 45.002 paging group, and requires the page and
Paging Response to carry that same identity. Negative gates derive their
matched/unmatched assertions from the same registration evidence. Registration
and paging save-state gates share one replay comparator.

The candidate-window acquisition terminal was also renamed from the historical
`nhm5_terminal_control` to the operation-based
`candidate_terminal_control`. NHM-5 first exposed that state, but the state
belongs to the candidate-window strategy selected independently by NHM-5 and
NHM-6.

The later NHM-2 registration pass preserves the same boundary. Its typed
product fields select an autonomous band-scan entrance, assigned-channel
confirmation value one and independently observed external-service sequences.
Once firmware publishes its candidate window, decoding, SCH/SI delivery,
assignment, LAPDm and MM use the generic components. The network's negative
Immediate Assignment profile alters only the standards-defined request
reference and contains no handset-family dispatch.
NHM-2 paging adds no product field at all: page construction, DRX group
selection, monitored/transmitted scheduling, session state and LAPDm release
are the same generic components already exercised by NHM-5 and NHM-6.

## Multi-cell mobility cleanup

Idle reselection adds no handset-model outcome selector.  The network's typed
cell table owns ARFCN/band, BSIC, PLMN/LAC/identity, SI content, restrictions
and RF availability.  The radio peer owns per-cell measurements, decoded-BCCH
validity, receiver context and saved downlink-loss state.  Firmware owns
ranking, receiver publications, stable selection and the decision to start
Location Updating.

The existing atomic radio contracts continue to select only independently
evidenced Nokia neighbour-list and acquisition grammar.  NSE-8's bitmap and
the NHM product publications are not merged merely because all four ROMs reach
the same standards-level result.  Paging groups, RR/MM, LAPDm, EF_LOCI
handling and same-/different-LAC rules remain generic.

The configuration ports used by mobility fixtures select network topology,
RF/loss scenario and standards-level neighbour faults.  They are test inputs,
not product defaults, and cannot name a desired serving cell in firmware.
Named positive, negative, preserved-NVRAM and save-state targets are documented
in `network_scouting.md`; dedicated-mode handover remains out of scope.

## Frozen post-NHM-2 frontier

This cleanup starts from commit `6b30852` (`speech control`). Its named
acceptance surface is:

- `make verify-radio-incoming-call-lifecycle`;
- `make verify-3310-radio-incoming-call-lifecycle`;
- `make verify-3330-radio-media-resilience`;
- `make verify-3410-radio-registration-preserved
  verify-3410-radio-registration-state verify-3410-radio-unsuitable-cells`;
- `make verify-3410-radio-paging-preserved verify-3410-radio-paging-state
  verify-3410-radio-paging-negatives`; and
- `make verify-3410-radio-incoming-call-lifecycle`;
- `make verify-radio-outgoing-call-lifecycle
  verify-radio-outgoing-call-state`;
- `make verify-radio-outgoing-call-busy
  verify-radio-outgoing-call-no-answer
  verify-radio-outgoing-call-no-answer-state
  verify-radio-outgoing-call-service-reject
  verify-radio-outgoing-call-delayed-decision-state`;
- `make verify-radio-outgoing-call-host-adapter`;
- `make verify-radio-outgoing-call-host-termination
  verify-radio-outgoing-call-host-alerting-termination
  verify-radio-outgoing-call-host-media
  verify-radio-outgoing-call-host-physical-media`;
- `make verify-radio-outgoing-call-host-reconnect
  verify-radio-outgoing-call-host-alerting-reconnect
  verify-radio-outgoing-call-host-media-restore
  verify-radio-outgoing-call-host-release-restore`;
- `make verify-radio-outgoing-call-host-hostile
  verify-radio-outgoing-call-host-local-end
  verify-radio-outgoing-call-host-two-calls`;
- `make verify-3310-radio-outgoing-call-lifecycle`;
- `make verify-3330-radio-outgoing-call-lifecycle`; and
- `make verify-3410-radio-outgoing-call-lifecycle`;
- `make verify-3310-radio-outgoing-call-host-termination
  verify-3330-radio-outgoing-call-host-termination`; and
- `make verify-3410-radio-outgoing-call-host-termination
  verify-3410-radio-outgoing-call-host-media`.

Shared Make variables and standards-level trace vocabulary do not replace
product acceptance. Packet grammar, correlation values, ordering and release
transactions remain independently checked.

NHM-2's bootstrap contract also owns one initial DSP code-block selector.
Firmware analysis at `0x348a96..0x348b78` proves a finite transfer: the handset
copies bounded chunks, decrements its remaining count, then clears selector
`0x0e2` and publishes terminal state four at `0x0e4`.  The HLE therefore saves
a one-shot publication flag rather than reasserting selector one on every
generic IRQ4 completion.  This is product-owned bootstrap grammar, not a
different DSPIF transport or a timing exception.

The generic host-call/session/voice/radio sources contain no handset or product
identifier dispatch. Direct WebSocket targets now enter the same fresh or
explicitly preserved NVRAM preparation boundary as ordinary runs, so fixture
reuse is not a hidden product input. A structural test protects both
properties.

No further wrapper is justified by the current evidence. DSP service delay and
peer polling describe different mechanisms; SIM controller presence and card
presence must remain independently testable; flash, EEPROM, boot and analogue
settings belong to separate boundaries. NHM-6 and NHM-2 own separately
evidenced command-`0x08` speech-control contracts: each organic Answer/End
lifecycle publishes `0x060b` and `0x040a`. Their separately named product
contracts use one constexpr initializer for the equal wire predicate; this
removes mechanical duplication without asserting shared firmware semantics.
Nokia's combined NHM-2/5/6 repair material
identifies the common COBBA-GJP N100 audio boundary, while the COBBA-GJP system
documentation fixes its codec SIO at 1 MHz/8 kHz with 125 clocks per frame and
a sign-extended 13-in-16 word. Both products own independently declared
profiles even though their values match NHM-5. Neither inherits NHM-5's
analogue routes or gains. Physical duplex remains pending identification of
each fitted microphone input and receiver topology.
