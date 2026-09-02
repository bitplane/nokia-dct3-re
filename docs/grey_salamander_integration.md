# Grey Salamander knowledge-integration catalogue

This document catalogues reusable protocol knowledge from
[`jmacato/grey-salamander-magpie-kit`](https://github.com/jmacato/grey-salamander-magpie-kit)
and defines how it may inform this project without copying its implementation.

The reviewed upstream baseline is tag `v0.1.1`, commit
`b09ec27ac96f3607442337ed80cde177e8b92ddb` (22 July 2026). The repository was
still at that commit when rechecked on 25 July 2026.

## Use policy

Grey Salamander is a source of:

- protocol coverage hints;
- known-working transaction order;
- useful edge cases and test scenarios;
- host-boundary and state-ownership ideas; and
- pointers to relevant GSM specifications.

It is not a source file dependency and its C# is not to be translated line by
line. New behavior is implemented natively from the cited GSM specifications,
our recovered Nokia boundary, and firmware observations. A feature enters the
supported model only when organic DCT3 traffic reaches the relevant boundary
and the resulting lifecycle passes a named local acceptance gate.

The upstream tests are compatibility clues rather than GSM conformance proof.
Differences accepted by their tested handset must not override behavior already
proved by our firmware.

## Architectural result

The useful common boundary is a complete decoded LAPDm block:

```text
Nokia firmware
    <-> DSPIF shared rings and Nokia L1 packets
        <-> nokia_radio_peer_device
            <-> nokia_lapdm_link_device
                <-> nokia_gsm_session_device
                    <-> nokia_gsm_network_device cell/profile data
                    <-> optional host call/SMS backend
```

Ownership is:

| Component | Owns | Must not own |
| --- | --- | --- |
| `nokia_dspif_device` | shared RAM, packet rings, doorbells and interrupt-facing completion | radio, LAPDm or GSM session policy |
| `nokia_dsp_hle_device` | DSP bootstrap and service timing | decoded radio protocol |
| `nokia_radio_peer_device` | Nokia L1 request correlation, channel configuration, frame scheduling and delivery | LAPDm sequence numbers or call/SMS transactions |
| `nokia_lapdm_link_device` | decoded link establishment, SAPI state, N(S)/N(R), acknowledgements and segmentation; future link expiry | Nokia packet types, RF policy or application routing |
| `nokia_gsm_network_device` | immutable facade-cell identity, broadcast data and network-side message encoding | per-handset transactions, DSPIF transport or firmware state |
| `nokia_gsm_session_device` | per-handset Layer-3 request and acknowledgement-gated bounded MM/CC/SMS transaction state plus queued network actions | LAPDm sequence state, RF scheduling or backend policy |
| future host backend | accept/reject/connect/terminate decisions and external message endpoints | emulated protocol sequencing |

The link and GSM session may grow independently. Call signalling does not imply
traffic-channel audio, and SMS signalling does not imply an external messaging
service.

The upstream `PROTOCOL-COVERAGE.md` explicitly excludes traffic-channel
allocation, speech channel coding, transcoding, and RF audio. It therefore
supplies no MCU/DSP/COBBA contract to copy or reinterpret; the local audio work
must continue from firmware traces and physical MAD2/COBBA documentation.

## Capability catalogue

Status terms:

- **landed**: present and organically regression-tested;
- **next**: the next bounded extension with an existing firmware entrance;
- **queued**: useful, but depends on an earlier boundary;
- **reference**: retain the knowledge but do not implement without new evidence;
- **out of scope**: explicitly not supplied by Grey Salamander or not presently
  justified here.

| ID | Capability learned from upstream | Local destination | Admission work and evidence gate | Status |
| --- | --- | --- | --- | --- |
| GS-00 | Separate decoded LAPDm state from host/DSP scheduling | `nokia_lapdm_link_device` between radio peer and network | Preserve the existing SABM-with-information, echoed UA, Location Updating Accept and Channel Release bytes; retain save-state ownership; pass `make verify-radio-registration` | **landed** |
| GS-01 | Advance Layer 3 only after the handset acknowledges downlink I-frames | LAPDm uplink I/RR parsing and pending-downlink state | The link records the next N(R), the peer requests an uplink block after Location Updating Accept, and only the organic SAPI-0 RR with N(R)=1 permits Channel Release; the full sequence is required by `make verify-radio-registration` | **landed** |
| GS-02 | Complete the exposed registration data-link teardown | `nokia_lapdm_link_device` and radio-peer uplink scheduling | After RR Channel Release the peer requests one uplink and requires the organic SAPI-0 RR with N(R)=2 before accepting physical deconfiguration. An extra probe produced only `01 03 01` UI/fill before firmware-driven deconfiguration, so no DISC/UA exchange is invented | **landed** |
| GS-03 | MM Information can carry network name and NITZ after registration or connection establishment | GSM MM session state plus LAPDm downlink queue | A deterministic time-only MM Information is acknowledgement-gated ahead of incoming SETUP because the v6.00 ROM otherwise acknowledges but ignores SETUP; network-name and idle clock/operator effects remain independent work | **landed for active connection** |
| GS-04 | Keep an idle PCH alive with no-identity fill and calculate the subscriber paging group | network cell scheduler through `nokia_radio_peer_device` | After registration, channel-`0x60` no-identity Paging Request Type 1 blocks are interleaved with channel-`0x50` BCCH and RSSI reports at the paging group derived from the registered IMSI; the ordinary registration gate requires return to PCH fill | **landed** |
| GS-05 | Incoming service begins with paging, RACH correlation, Immediate Assignment and Paging Response | radio peer scheduler -> LAPDm -> GSM MM session | A named network-event fixture sends exactly one IMSI page requesting SDCCH; the phone organically emits RACH, accepts an assignment carrying its exact request reference, establishes LAPDm with Paging Response, receives bounded Channel Release, and returns to PCH fill | **landed** |
| GS-06 | Minimal cipher-mode command/complete exchange can exercise firmware control flow without implementing A5 | GSM RR/MM session | After organic Paging Response, the acknowledgement-gated SC=0 command makes the ROM publish DSP TX type `0x14` and organically emit Cipher Mode Complete without a DSP reply. Calls and both SMS fixtures continue afterwards. A quarantined SC=1 probe carried the SIM Kc in the same packet, but was removed because decoded radio blocks remain clear and no A5 bitstream processing is modeled | **landed for unciphered control flow; A5 out of scope** |
| GS-07 | Mobile-originated call ordering from CM Service Request through Setup, Connect and Release | GSM call-control session and optional backend request | Physical dialing proves the Nokia random-access entrance, CM Service Request/Accept, firmware SETUP and called-party BCD digits, Call Proceeding, one TCH/F assignment, Alerting, Connect/Acknowledge, bidirectional speech, physical clearing and restored PCH cadence across NSE-8, NHM-5, NHM-6 and NHM-2. A valid SETUP now creates a saved monotonic request ID with retained digits. NSE-8 additionally proves network busy before TCH, no-answer with local clearing (including alerting save/load), and CM service rejection before SETUP. | **landed through the saved decision seam; external host adapter pending** |
| GS-08 | Mobile-terminated call ordering from paging through Setup, Alerting, Connect and clearing | incoming-service queue and GSM call-control session | The bounded fixture proves page, SC=0 cipher control/complete, MM Information, SETUP, Call Confirmed/Alerting, TCH/F Assignment, organic Nokia channel configuration, new-link SABM/UA, Assignment Complete and DISC/UA clearing. A separate deterministic physical-input fixture proves PUP ringing, Answer, CC Connect/Connect Ack and a stable TCH/F interval. Its post-answer packet census contains only empty TCH polls and a known external-service poll. A lower changed-write census proves answer-only shared-control command `0x08/0x060b` plus a separate bounded acknowledgement tone. The resulting speech path now crosses GSM-FR, timed TCH/F Layer 1 and the documented product-configured MAD2/COBBA PCM bus in both directions; only DSP-local COBBA mux/register programming remains unrecovered | **landed through organic bidirectional speech and teardown; analogue-control encoding remains open** |
| GS-09 | Mobile-originated SMS uses CP-DATA/ACK and RP-DATA/ACK/error, with GSM-7 and UCS-2 decoding | GSM SMS session and host request event | Cipher-control and SIM SMS parameters are present; recover the mobile-originated SAPI-3/CP entrance organically, then test accept/reject and final SAPI-0 RR release | **queued** |
| GS-10 | Mobile-terminated SMS uses paging, SAPI 3, SMS-DELIVER, timestamps and CP/RP acknowledgements | incoming-service queue and GSM SMS session | The ordinary-text fixture proves independent SAPI-3 establishment and the firmware's exact persistent unread `EF_SMS` record. The port-addressed multipart fixture additionally proves the handset's five-byte CP-DATA/RP-ACK, network CP-ACK, acknowledged RR release and return to PCH. A generic GSM 04.11 parser derives the CP transaction and RP reference from the actual downlink and rejects malformed or mismatched closure | **landed through persistent text delivery and independent CP/RP transport closure** |
| GS-11 | A usable legacy SMS profile needs EF_SMSP and 176-byte linear-fixed EF_SMS records | `nokia_sim_card_device` and NVRAM schema | `EF_SMSP` and ten 176-byte `EF_SMS` records are declared under `DF_TELECOM`, advertised by `EF_SST`, covered by save state/card NVRAM and exercised by organic select/read/update traffic. Existing 32-byte/50-record ADN geometry is unchanged | **landed** |
| GS-12 | Port-addressed 8-bit SMS and concatenation carry Nokia Smart Messaging payloads | SMS TPDU/user-data codec above GS-10 | The bounded 251-byte RTPL tone uses 128-byte multipart capacity, shared reference `7a`, two queued parts and independent RP references. Both parts pass exact stop-and-wait SAPI-3 delivery, handset CP/RP response, network CP-ACK and RR release through separate pages on NSE-8, NHM-5, NHM-6 and NHM-2. NSE-8 then proves RAM-owned reference/count reassembly across RR release, port dispatch, named receipt and physical Options/Play with note-varying PUP output. Commandless RTPL reaches the ringtone UI but cannot play; wrong reference/total/port cannot reach the completion transition. NHM-2 and NHM-5 independently corroborate playback. `EF_SMS` stays free; save/discard and persistence remain open | **multipart transport, reassembly, dispatch and RTPL playback landed** |
| GS-13 | One identity/profile should generate IMSI, PLMN, LAI, paging identity, operator data and SIM contents consistently | typed subscriber/cell configuration shared by SIM and GSM devices | Extract current constants without changing the validated 001-01 profile; add consistency checks before exposing alternate profiles | **queued, independent** |
| GS-14 | Calls and SMS should produce asynchronous request objects and accept later host decisions | backend-neutral session submission seam plus optional WebSocket adapter | MO calls expose a saved monotonic request ID and decoded called digits. MT calls accept one correlated caller identity, defer paging until registration, and retain physical Answer/End ownership. The optional adapter moves bounded decisions, termination and correlated GSM-FR events from the HTTP thread to emulation-owned GSM state. Epoch/cursor snapshots cover reconnect and restore; live gates cover external paging, overload, stale input, local/remote clear and physical non-silent loopback. SMS host requests remain future work. | **bidirectional call host lifecycle landed** |
| GS-15 | Paging and dedicated-channel work require deadlines and ordered queues | radio scheduler and LAPDm timers | Paging groups and the one-page fixture now use emulated frame/scheduler time and save-state fields; add explicit expiry and mid-transaction save/load coverage when an unanswered or queued service is admitted | **queued for expiry coverage** |
| GS-16 | Mutable Layer-3 session state should be distinct from both the decoded link and immutable facade cell | `nokia_gsm_session_device` | The session recognizes the complete Location Updating Request, owns contention-delivery and both acknowledgement-gated MM transitions, queues typed downlink actions, registers save-state fields, and preserves the network device's pure cell/data role; pass the unchanged organic registration gate | **landed** |
| GS-17 | Additional LAPDm SAPIs, DISC/UA, fill handling, segmentation, reassembly and expiry | `nokia_lapdm_link_device` | Independent save-state-backed SAPI-0/SAPI-3 establishment, sequence and acknowledgement state plus two-frame downlink segmentation are exercised by ordinary MT SMS. The TCH/F call exposes and now proves mobile-originated DISC/network UA; uplink reassembly and expiry remain reference work | **partially landed through SAPI-3 segmentation and TCH DISC/UA** |

## Upstream source map

This map makes the provenance of each knowledge item reviewable without making
the upstream repository a build dependency:

| Upstream area | Knowledge represented here |
| --- | --- |
| `GsmCellCodec.cs` and its tests | GS-04/05 paging groups, fill frames, RACH reference preservation and assignment scenarios |
| `LapdmLink.cs` and its tests | GS-00--02, GS-15 and GS-17 link establishment, acknowledgement, SAPI, segmentation, expiry and release-ordering scenarios |
| `GsmNetwork.cs` and its tests | GS-03 and GS-05--10 MM, paging-response, call-control and SMS transaction order |
| `SimCard.cs`, `SimPhonebookCodec.cs` and tests | GS-11/13 SIM file requirements, persistence scenarios and identity consistency |
| `SmartMessageSms.cs` and tests | GS-12 port-addressed and multipart payload scenarios |
| `PORTING.md`, `PROTOCOL-COVERAGE.md` and `SPEC-SOURCES.md` | host-boundary assumptions, stated coverage limits and specification routing |

## Implementation sequence

### Phase A: finish the registration link

1. GS-00--02: decoded link extraction and both acknowledgement-gated
   registration downlinks are landed.
2. Preserve the exact registration and post-release BCCH gates.
3. Keep DISC/UA and other GS-17 mechanics as references until observed.
4. GS-16 is landed: mutable Layer-3 registration state is separate from link
   sequencing and immutable cell data.
5. Add MM Information/NITZ only as a separate GS-03 change.

This phase proves that the extracted link is a real bidirectional protocol
owner rather than only a downlink frame builder.

### Phase B: create a network-originated entrance

1. Idle PCH channel scheduling is landed without changing BCCH acquisition.
2. No-identity PCH fill and steady camp are part of the registration gate.
3. A named fixture queues exactly one incoming IMSI page.
4. The existing correlated Immediate Assignment path preserves its RACH
   octet and request frame.
5. The organic Paging Response reaches LAPDm and the Layer-3 session, then a
   bounded release returns the phone to PCH fill.

This phase supplies the prerequisite entrance for incoming calls and SMS. Nokia
L1 scheduling remains in
`nokia_radio_peer_device`; Grey Salamander deliberately starts above the Nokia
DSP/L1 boundary and cannot supply that mapping.

### Phase C: Layer-3 services

1. Keep the current call entrance explicitly unciphered through SC=0. DSP
   type `0x14` and Cipher Mode Complete are organic checkpoints; actual A5
   bitstream processing remains a separate backend boundary.
2. Add mobile-originated call signalling first if the dormant post-dial Nokia
   path is recovered before paging.
3. Mobile-terminated call signalling is landed through organic TCH/F
   Assignment Complete and bounded clearing. Deterministic answering and
   speech-frame/codec behavior remain separate work.
4. SIM SMS files, bounded ordinary MT text delivery and independently closed
   CP/RP transport are landed. MO SMS remains separate work.
5. The bounded long-ringtone codec and queue are landed after the ordinary
   persisted-delivery oracle. Both parts are proved through separate pages and
   complete stop-and-wait SAPI-3/CP/RP/RR transactions. Do not infer firmware
   reassembly or UI/persistence from transport completion.

Each service gets a small protocol-state fixture and a full-ROM organic
acceptance test. A UI result alone is not sufficient: traces must also prove
the expected Nokia packet, LAPDm and Layer-3 boundaries.

## Knowledge to retain without adopting

The following upstream choices are useful comparison points, not local changes:

- Its host supplies SCH/RSSI and already-decoded control blocks; ours must
  continue reaching those blocks through DSPIF and the recovered Nokia L1
  protocol.
- Its 23-byte LAPDm block API differs from the 24-byte field carried by our
  Nokia `RECEIVED_BLOCK` envelope. The adapter boundary must make that padding
  explicit rather than changing a validated packet layout.
- Its Immediate Assignment selects SDCCH/8 subchannel 0 on timeslot 1. Our
  organically accepted assignment uses timeslot 0; channel description becomes
  configurable only after another validated profile requires it.
- Its SIM geometry and broad default service table must not replace our
  firmware-observed ADN geometry and conservative service advertisement.
- Its combined Layer-3 implementation is a useful behavioral map, but local
  per-handset RR/MM/call-control/SMS state belongs in the session device,
  separate from immutable cell data and organized into reviewable,
  save-state-aware transitions rather than one monolithic method.

## Explicit non-goals

Grey Salamander does not provide, and this catalogue does not infer:

- Ki/RAND/SRES authentication or a real VLR/HLR;
- A5 cipher generation;
- burst coding, interleaving, equalization or RF;
- traffic-channel allocation, speech coding or transcoding;
- handover, hopping, GPRS, USSD or supplementary services; or
- multi-cell/multi-subscriber network simulation.

Those require separate evidence and architecture decisions. Successful
call-control signalling must not be described as working call audio.

## Per-change checklist

For every catalogue item:

1. name the organic Nokia or standards-defined entrance;
2. cite the governing GSM section and the upstream scenario that highlighted
   it;
3. keep DSPIF, Nokia L1, LAPDm and Layer-3 ownership separate;
4. add save-state coverage for new mutable state;
5. add a focused protocol/structure test;
6. pass the existing lower-layer gates;
7. add a full-ROM acceptance check for the new lifecycle; and
8. update this table from **queued** to **landed** only after that check passes.
