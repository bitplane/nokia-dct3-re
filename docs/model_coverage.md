# DCT3 model coverage

This matrix records demonstrated product coverage, not family resemblance. A
cell is promoted only by a named reproducible gate or reviewed hardware or
firmware evidence. `Partial` means that the preceding acceptance level works
but a material hardware contract remains calibrated, opaque, or unverified.

| Product / tested firmware | Booting | Interactive | Registered | Call control | Internal media | Physical duplex | Hardware-faithful | Principal evidence or next boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nokia 3210 NSE-8 v6.00 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Structural, navigation, authentication, mobile-terminated and physical-dial mobile-originated call lifecycles, paired GSM-FR/FACCH/degraded media and isolated physical audio pass. Organic A5/1 gates cover ciphered SDCCH, SACCH, TCH/F/FACCH, incoming/outgoing calls, degradation and exact active-call save/load; A5/0 remains the default regression composition. Idle mobility proves same-/different-LAC reselection, replacement-cell paging, loss/recovery, persistent loss, preserved EF_LOCI and save-state boundaries. Ordinary MT SMS proves exact notification/storage, preserved cold-boot listing, physical read/delete/cancel, save-state continuation, pre-page malformed rejection, correlated duplicate suppression, organic ten-record capacity and two independently paged messages with selective deletion. Smart Messaging additionally proves RAM-owned concatenation, port dispatch, RTPL playback, physical promptless Save/Discard, product-local 24C128 ownership and ordinary named listing with payload-correlated playback after preserved-NVRAM cold boot. The outgoing host suite additionally proves asynchronous outcomes, termination, overload/reconnect and sequential calls. Physical long-power shutdown reaches CCONT rail-off after save/load. Real COBBA DSP-controlled mux/gain semantics remain opaque. |
| Nokia 3210 NSE-8 v5.01 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Independent ROM gates cover boot/UI, the call lifecycle, save-state replay, isolated physical duplex, physical mobile-originated SMS through CP/RP closure and the same independently executed CCONT rail-off power lifecycle. The same COBBA/DSP limitation applies. |
| Nokia 3310 NHM-5 v6.39 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Frontier/navigation, authentication, registration, paging, incoming and physical-dial outgoing calls, media resilience and physical duplex pass using its product-specific radio, DSP-control and 1 MHz/125-clock PCM contracts. Focused A5/1 promotion proves organic cipher control, ciphered SDCCH and bidirectional traffic media without inheriting its DSP packet grammar. Focused idle-mobility gates independently prove same-/different-LAC reselection, paging, preserved location and replay. Ordinary MT SMS independently proves localized notification, exact text, SIM read status, physical erase confirmation, preserved-NVRAM Menu 2-2 reading and durable deletion. Its localized composer also proves physical UCS-2 SMS-SUBMIT, success UI and a requested delivery report through a second complete paging/CP/RP/RR and SIM-storage lifecycle. Sequential two-part Smart Message transport closes organically through CP/RP and clean RR release; its localized application path proves physical Save, product-local PMM-backed flash ownership, ordinary cold-boot listing and sustained saved-tone DSP output. The MCU-visible ROM4 mailbox publishes a 900-Hz carrier even for saved custom tones, so payload pitch remains DSP-owned and cannot be promoted without a real DSP backend or an equivalent trace. Product-specific analogue gain programming remains unproved. |
| Nokia 3330 NHM-6 v4.50E | Yes | Yes | Yes | Yes | Yes | No | Partial | Fresh-PMM frontier, navigation, registration and paging gates pass. Preserved-PMM incoming and physical-dial outgoing calls complete through physical Navi Answer/End and carry bidirectional GSM-FR over NHM-6's independently evidenced DSP/PCM contracts. Cold preserved-PMM A5/1 promotion organically reauthenticates before ciphered SDCCH and traffic media rather than inheriting a process-local key. Same-/different-LAC reselection, replacement-cell paging, preserved location and save-state pass through NHM-6's own radio grammar. Ordinary MT SMS proves exact SIM storage, CP/RP closure and RR release after physical PMM provisioning, but preserved cold boot re-enters the security/time editor; physical inbox/read/delete remain explicitly unpromoted. Preserved-PMM sequential two-part Smart Message transport closes organically through CP/RP and clean RR release. Its Smart Message completion tone is reproducible, but the same setup editor prevents a reproducible physical Save/list/play gate; that UI is not bypassed. FACCH, degradation, SACCH and active-call replay pass in the established A5/0 composition. The fitted microphone input remains unknown. |
| Nokia 3410 NHM-2 v5.46E | Yes | Yes | Yes | Yes | Yes | No | Partial | Fresh/preserved-PMM registration, paging and assigned-SDCCH replay pass. Incoming and outgoing calls prove physical Send/End, assignment and media lifecycles. Independent A5/1 promotion observes NHM-2's distinct 10-byte nonzero type-`0x14` publications, ciphered xCCH/traffic bursts and non-silent media; secret payloads are redacted. Idle mobility proves same-/different-LAC reselection, replacement-cell paging, loss/recovery, persistent loss, preserved location and replay without inheriting Nokia packet grammar. Ordinary MT SMS independently proves preserved-PMM notification, ordinary Inbox listing, exact text, SIM read status, physical erase confirmation and durable deletion. Sequential two-part Smart Message transport independently closes through SAPI-3 CP/RP and RR release; its application additionally proves a three-slot/Replace Save flow, product-local PMM-backed flash ownership, ordinary named listing and payload-correlated PUP replay after preserved-NVRAM cold boot. Its typed 1 MHz/8 kHz 13-in-16 PCM profile sustains bidirectional GSM-FR. Fitted analogue routes remain unestablished. |
| Nokia 5110 NSE-1 v5.30 | Yes | Yes | No | No | No | No | Partial | A clean-room C54x core executes the recovered ROM4 program/DROM and firmware uploads without DSP-output assists. Generated 24C16 provisioning passes the organic COBBA measurement validator; exact menu and save-state gates exercise the physical serial keypad. A 30-second offline census records 6,497 scheduled CTSI edges but zero RF sample reads and zero synthesizer pairs: DSP INT0 remains masked because the MAD2-specific pre-operational value of reset-uninitialised IMR is not captured. A diagnostic-only mask substitution executes the complete reached receiver/FIR instruction surface and 26,944 sample reads without an illegal opcode, but produces no synthesizer pair. The candidate `0x52fd` is absent from the aligned recovered Nokia program images and comes from a post-bootloader Osmocom/Calypso snapshot, so it is not retained. Operational power-off ownership is also unresolved: live special column 1 scans as raw `0x81`, but the active firmware table maps it to no-key. |
| Nokia 6110 NSE-3 v4.06 PPM B (ROM3 candidate) | No | No | No | No | No | No | No | `verify-6110-static` proves the declared hardware boundary, SIM surface, DSPIF rings, sparse-flash verification transport and product-local radio/service envelopes. Matching F711604 internal ROMs, EEPROM and the DSP-owned final verification publication remain absent. |
| Nokia 6110 NSE-3 v5.48 PPM B (ROM3) | No | No | No | No | No | No | No | `verify-6110-v548-static` proves the distinct ROM3 bootstrap, `3/3` pre-upload identity, 64 transfers, final `0x0b06` first result and ROM3 EEPROM record locations. The final DSP-owned publication, internal ROMs and matching EEPROM remain absent. |
| Nokia 6110 NSE-3 v05.48 PPM B (ROM4) | No | No | No | No | No | No | No | The same gate proves a separately relocated ROM4 loader, different staged stream and different EEPROM record locations. Its pre-upload identity, final DSP publication, internal ROMs and matching EEPROM remain absent; ROM3 values are not inherited. |
| Other declared DCT3 products | No | No | No | No | No | No | No | Driver ROM declarations are not executable evidence. Required images and product-specific contracts are absent. |

## Promotion rules

The levels are cumulative:

- **Booting**: the handset completes its evidenced flash, MAD2, CCONT, SIM and
  DSP bootstrap path without firmware hooks.
- **Interactive**: physical matrix inputs reproducibly drive normal firmware UI.
- **Registered**: the product's own radio packet grammar camps and completes
  Location Updating against the independent network peer.
- **Call control**: paging, ringing, physical Answer/End and clean teardown pass.
- **Internal media**: bidirectional GSM-FR crosses the firmware/DSP boundary,
  including FACCH substitution, degraded frames, SACCH coexistence and
  save-state replay.
- **Physical duplex**: isolated host microphone and playback paths pass without
  loopback, feedback, or UI-level injection.
- **Hardware-faithful**: all material product flash, DSP, PCM, COBBA and analogue
  topology claims are backed by hardware documentation, firmware analysis or
  reproducible traces. A working calibrated compatibility response is
  insufficient.

`Partial` in the Booting column means a deterministic execution boundary is
reached but the complete product boot acceptance gate does not currently pass.

## Product-boundary cautions

- Packet length similarity is not semantic evidence. NHM-5 type `0x20` is not
  NSE-8 type `0x1a`; each enabled radio profile uses its independently recovered
  command/report lifecycle.
- A numeric task id, channel-confirmation value, DSP-ready value or EEPROM
  offset is not portable across products or ROM families.
- The Nokia 6110 remains fail-closed. A physical capture accepted by
  `make verify-6110-bootstrap-capture`, or matching internal ROMs reproducing
  it, is required before executable promotion.

The current radio and media contracts live in `docs/network_scouting.md`,
`docs/dsp_interface.md` and `docs/structural_regression.md`. Detailed NSE-3
hardware, firmware addresses, cautions and the single resumption question
live in `docs/6110_bringup.md`; the capture contract lives in
`docs/6110_bootstrap_capture.md`. This matrix is the sole authority for model
promotion state.
