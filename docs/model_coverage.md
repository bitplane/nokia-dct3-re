# DCT3 model coverage

This matrix records demonstrated product coverage, not family resemblance. A
cell is promoted only by a named reproducible gate or reviewed hardware or
firmware evidence. `Partial` means that the preceding acceptance level works
but a material hardware contract remains calibrated, opaque, or unverified.

| Product / tested firmware | Booting | Interactive | Registered | Call control | Internal media | Physical duplex | Hardware-faithful | Principal evidence or next boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nokia 3210 NSE-8 v6.00 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | The structural, navigation, authentication, call-lifecycle, paired GSM-FR/FACCH/degraded-media and isolated physical-audio gates pass. Real COBBA DSP-controlled mux/gain semantics remain opaque. |
| Nokia 3210 NSE-8 v5.01 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Independent ROM gates cover boot/UI, the call lifecycle, save-state replay and isolated physical duplex. The same COBBA/DSP limitation applies. |
| Nokia 3310 NHM-5 v6.39 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | The 3310 frontier/navigation, authentication, registration, paging, incoming-call lifecycle, media-resilience and physical-duplex gates pass using its product-specific radio, DSP-control and 1 MHz/125-clock PCM contracts. Product-specific analogue gain programming remains unproved. |
| Nokia 3330 NHM-6 v4.50E | Yes | Yes | Yes | No | No | No | Partial | Fresh-PMM frontier, save-state and physical navigation gates pass. `verify-3330-radio-boundary` proves standards-shaped DCS SI1--SI4 publication and automatic-access selection; the registration gates prove organic Location Updating, EF_LOCI persistence, clean RR release and deterministic camp. The fresh, preserved-NVRAM and save-state paging gates prove correctly phased PCH fill, an IMSI-addressed page, automatic access, Immediate Assignment, LAPDm Paging Response, clean release and return to camp. Wrong-group, unmatched-identity, malformed-page and unsuitable-cell gates remain below RR access. Ringing, physical Answer/End, media and product analogue wiring are not yet established, so Call control remains unpromoted. |
| Nokia 3410 NHM-2 v5.46E | Yes | Yes | No | No | No | No | Partial | Fresh-PMM compaction, save-state, idle, physical Menu/End and return-to-idle gates pass with an independently declared NHM-2 compact DSP completion contract. Radio, media and analogue-board contracts remain unestablished. |
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
