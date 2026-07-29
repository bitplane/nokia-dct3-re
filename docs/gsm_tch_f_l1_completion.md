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
| Ciphering boundary | Generic A5/0 and A5/1 operate on `burst_payload::data` after coding/interleaving and before normal-burst packing; COUNT comes from the scheduled TDMA frame and each direction is independent | Published A5/1 known-answer vector, wrong-key/frame/direction negatives, organic encrypted TCH/F/FACCH/SACCH and exact save-state replay; training, tails and stealing flags remain outside |
| TDMA timing | Exact 60/13 ms timer and 26-frame timeslot-1 schedule | Schedule tests plus organic calls; 24 traffic, one idle and one SACCH position per multiframe |
| FACCH substitution | Organic `0xb0` LAPDm blocks enter FIRE/convolutional coding, replace queued speech and cross explicit BFI boundaries | Paired-ROM runtime oracle observes v6.00/v5.01 uplink/downlink FACCH counts 4/4 and 4/3, concealment increments on both independent receivers, more than 100 recovered speech frames and clean teardown |
| SACCH coexistence | Four-burst rectangular coding plus stateful empty-safe endpoints over 104 frames | All-timeslot phase/wrap tests and live reserved positions; no invented measurement payload |
| Bad/erased frames | Both decoders report protected-frame validity; bad and FACCH-stolen intervals cross explicit BFI boundaries into isolated GSM 06.11 substitution and 320 ms muting; queue absence remains a distinct no-delivery state | Paired-ROM `make verify-radio-degraded-speech`: exact 34-record replay while degraded; canonical v6.00/v5.01 runs retain 295/595 non-silent blocks, handset/network concealment 20/5 and 35/5, clean recovery and teardown |
| Independent network decoding | Uplink and downlink cross separate transmitter/receiver and transcoder state; uplink BFI/FACCH cannot pause remote downlink encoding | Clean, bidirectionally degraded, save/load and physical-uplink runtime gates |
| Organic non-silent downlink | Network-side 1 kHz PCM is GSM-FR encoded and crosses timed Layer 1 to COBBA EAR | v6.00 and v5.01: 145 non-silent blocks each |
| Organic non-silent physical uplink | External host audio enters only MAME's microphone, COBBA MIC2, PCM, DSP codec and timed Layer 1 | Paired real-time `make verify-radio-physical-uplink`: both revisions 250/250 non-silent blocks; isolated microphone/network peaks v6.00 1280/1296 and v5.01 1264/1432; FACCH recovery and physical End-to-idle included |
| Organic physical downlink playback | Network 1 kHz speech crosses Layer 1, DSP decoding, MAD2 PCM, COBBA EAR and the product speaker route into an isolated host playback sink | Isolated gates record mono 8 kHz PCM independently of the microphone: 3210 v6.00 peak/RMS 10461/648.2 and 5.32-second 1 kHz run; 3210 v5.01 9372/614.8 and 5.34 seconds; 3310 v6.39 9737/889.0 and 5.36 seconds |
| Organic teardown | Firmware drives CC and DSP control back to idle after media/FACCH | v6.00 and v5.01 lifecycle gates |
| Layer separation and generic configuration | Codec, Layer 1, radio peer, DSP PCM and COBBA remain separate devices/components; impairment is a network profile | Repository tests and `test_speech_media_boundaries.py` |

Meaningful SACCH measurement reports and downlink SACCH system information are
not fabricated. Their Nokia DSP-side ownership remains an evidence question,
but the standards-defined Layer 1 transport and scheduling contract is ready
for those 184 information bits.

The parallel SDCCH boundary is documented in `gsm_a5_ciphering.md`. LAPDm
blocks now cross the same 184-bit xCCH encoder/decoder and four ciphered normal
bursts rather than applying A5 to decoded Layer 3.
