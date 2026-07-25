# GSM TCH/F Layer 1 completion audit

This audit maps the requested faithful, generic GSM full-rate traffic-channel
milestone to current implementation and reproducible evidence. It does not
claim that the still-unrecovered Nokia DSP program or COBBA control registers
are emulated.

| Requirement | Implementation | Evidence |
|---|---|---|
| TS 46.010 260-bit speech ordering | `unpack_speech`, `pack_speech`, table-2 importance permutation | `make verify-gsm-tch-f-l1`: exhaustive one-hot permutation and fixed zero-frame vector |
| Class 1a parity, class 1 convolutional protection and class 2 bits | TS 45.003 parity polynomial, K=5 rate-1/2 encoder and hard-decision Viterbi decoder | Clean round trip, isolated hard errors, parity/tail checks and unprotected class-2 corruption tests |
| 456-bit blocks and eight-burst diagonal interleaving | Independent stateful diagonal transmitter and receiver | Continuous speech/FACCH/erasure stream tests |
| Normal bursts, training and stealing flags | 148-bit GMSK burst packing, set-1 TSC table, explicit `hl`/`hu` | Field-position, tail, TSC 2 and FACCH stealing tests |
| Ciphering boundary | `burst_payload::data` contains exactly the 114 cipherable data bits | Structural boundary test; training, tails and stealing flags remain outside |
| TDMA timing | Exact 60/13 ms timer and 26-frame timeslot-1 schedule | Schedule tests plus organic calls; 24 traffic, one idle and one SACCH position per multiframe |
| FACCH substitution | Organic `0xb0` LAPDm blocks enter FIRE/convolutional coding and replace queued speech | Runtime gate requires independently decoded downlink FACCH; clean calls retain teardown with one displaced interval |
| SACCH coexistence | Four-burst rectangular coding plus stateful empty-safe endpoints over 104 frames | All-timeslot phase/wrap tests and live reserved positions; no invented measurement payload |
| Bad/erased frames | Both decoders report protected-frame validity; bad and FACCH-stolen intervals cross explicit BFI boundaries into isolated GSM 06.11 substitution and 320 ms muting; queue absence remains a distinct no-delivery state | `make verify-radio-degraded-speech`: 20 impaired bursts/direction, handset/network concealment of 12/5 intervals, 145 non-silent blocks and clean recovery |
| Independent network decoding | Uplink and downlink cross separate transmitter/receiver and transcoder state; uplink BFI/FACCH cannot pause remote downlink encoding | Clean, bidirectionally degraded, save/load and physical-uplink runtime gates |
| Organic non-silent downlink | Network-side 1 kHz PCM is GSM-FR encoded and crosses timed Layer 1 to COBBA EAR | v6.00 and v5.01: 145 non-silent blocks each |
| Organic non-silent physical uplink | External host audio enters only MAME's microphone, COBBA MIC2, PCM, DSP codec and timed Layer 1 | `make verify-radio-physical-uplink`; audited runs: v6.00 850/850, peak 2264; v5.01 1200/1200, peak 2032 |
| Organic teardown | Firmware drives CC and DSP control back to idle after media/FACCH | v6.00 and v5.01 lifecycle gates |
| Layer separation and generic configuration | Codec, Layer 1, radio peer, DSP PCM and COBBA remain separate devices/components; impairment is a network profile | 325 repository tests and `test_speech_media_boundaries.py` |

Meaningful SACCH measurement reports and downlink SACCH system information are
not fabricated. Their Nokia DSP-side ownership remains an evidence question,
but the standards-defined Layer 1 transport and scheduling contract is ready
for those 184 information bits.
