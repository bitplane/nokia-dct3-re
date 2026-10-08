# DCT3 model coverage

This matrix records demonstrated product coverage, not family resemblance. A
cell is promoted only by a named reproducible gate or reviewed hardware or
firmware evidence. `Partial` means that the preceding acceptance level works
but a material hardware contract remains calibrated, opaque, or unverified.

| Product / tested firmware | Booting | Interactive | Registered | Call control | Internal media | Physical duplex | Hardware-faithful | Principal evidence or next boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nokia 3210 NSE-8 v6.00 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Structural, navigation, authentication, mobile-terminated and physical-dial mobile-originated call lifecycles, paired GSM-FR/FACCH/degraded media and isolated physical audio pass. Organic A5/1 gates cover ciphered SDCCH, SACCH, TCH/F/FACCH, incoming/outgoing calls, degradation and exact active-call save/load; A5/0 remains the default regression composition. Idle mobility proves same-/different-LAC reselection, replacement-cell paging, loss/recovery, persistent loss, preserved EF_LOCI and save-state boundaries. Ordinary MT SMS proves exact notification/storage, preserved cold-boot listing, physical read/delete/cancel, save-state continuation, pre-page malformed rejection, correlated duplicate suppression, organic ten-record capacity and two independently paged messages with selective deletion. Physical mobile-originated SMS additionally proves EF_SMSP editing, SMS-SUBMIT reassembly, success/rejection/timeout and retry outcomes, requested status-report persistence, and active CP-wait save/load. Smart Messaging additionally proves RAM-owned concatenation, port dispatch, RTPL playback, physical promptless Save/Discard, product-local 24C128 ownership and ordinary named listing with payload-correlated playback after preserved-NVRAM cold boot. The outgoing host suite additionally proves asynchronous outcomes, termination, overload/reconnect and sequential calls. Physical long-power shutdown reaches CCONT rail-off after save/load. Real COBBA DSP-controlled mux/gain semantics remain opaque. |
| Nokia 3210 NSE-8 v5.01 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Independent ROM gates cover boot/UI, the call lifecycle, save-state replay, isolated physical duplex, physical mobile-originated SMS through CP/RP closure and the same independently executed CCONT rail-off power lifecycle. The same COBBA/DSP limitation applies. |
| Nokia 3310 NHM-5 v6.39 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Frontier/navigation, authentication, registration, paging, incoming and physical-dial outgoing calls, media resilience and physical duplex pass using its product-specific radio, DSP-control and 1 MHz/125-clock PCM contracts. Focused A5/1 promotion proves organic cipher control, ciphered SDCCH and bidirectional traffic media without inheriting its DSP packet grammar. Focused idle-mobility gates independently prove same-/different-LAC reselection, paging, preserved location and replay. Ordinary MT SMS independently proves localized notification, exact text, SIM read status, physical erase confirmation, preserved-NVRAM Menu 2-2 reading and durable deletion. Its localized composer also proves physical UCS-2 SMS-SUBMIT, success UI and a requested delivery report through a second complete paging/CP/RP/RR and SIM-storage lifecycle. Sequential two-part Smart Message transport closes organically through CP/RP and clean RR release; its localized application path proves physical Save, product-local PMM-backed flash ownership, ordinary cold-boot listing and sustained saved-tone DSP output. The MCU-visible ROM4 mailbox publishes a 900-Hz carrier even for saved custom tones, so payload pitch remains DSP-owned and cannot be promoted without a real DSP backend or an equivalent trace. Product-specific analogue gain programming remains unproved. |
| Nokia 3330 NHM-6 v4.50E | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Fresh-PMM frontier, navigation, registration and paging gates pass. Preserved-PMM incoming and physical-dial outgoing calls complete through physical Navi Answer/End and carry bidirectional GSM-FR over NHM-6's independently evidenced DSP/PCM contracts. Cold preserved-PMM A5/1 promotion organically reauthenticates before ciphered SDCCH and traffic media rather than inheriting a process-local key. Same-/different-LAC reselection, replacement-cell paging, preserved location and save-state pass through NHM-6's own radio grammar. Ordinary MT SMS proves exact SIM storage, CP/RP closure and RR release after physical PMM provisioning; a cold physical route through the still-required security/time editors now renders the exact Inbox text, marks EF_SMS read and durably erases it. Its physical predictive-text composer independently completes a GSM 7-bit SMS-SUBMIT and localized success UI. Preserved-PMM sequential two-part Smart Message transport closes organically through CP/RP and clean RR release; physical Save writes only product-local PMM flash, a cold process lists `Test for Dhiram` at Tones 5-4-1, and Options -> Play emits the reconstructed note stream through PUP. The acquired virgin PMM's stale four-byte phone-code verifier is mapped exactly; a fail-closed derived fixture changes only that record and proves physical `12345` at first boot and in Settings, while the canonical ROM remains unmodified. FACCH, degradation, SACCH and active-call replay pass in the established A5/0 composition. Nokia's combined NHM-2/5/6 repair path and matching COBBA schematic establish MIC2/EAR connectivity; isolated incoming- and outgoing-call gates validate both host-audio directions with neutral gains. |
| Nokia 3410 NHM-2 v5.46E | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Fresh/preserved-PMM registration, paging and assigned-SDCCH replay pass. Incoming and outgoing calls prove physical Send/End, assignment and media lifecycles. Independent A5/1 promotion observes NHM-2's distinct 10-byte nonzero type-`0x14` publications, ciphered xCCH/traffic bursts and non-silent media; secret payloads are redacted. Idle mobility proves same-/different-LAC reselection, replacement-cell paging, loss/recovery, persistent loss, preserved location and replay without inheriting Nokia packet grammar. Ordinary MT SMS independently proves preserved-PMM notification, ordinary Inbox listing, exact text, SIM read status, physical erase confirmation and durable deletion. Its physical composer additionally proves Profile 1 EF_SMSP ownership, GSM 7-bit SMS-SUBMIT, success UI and the shared CP/RP/RR lifecycle. Sequential two-part Smart Message transport independently closes through SAPI-3 CP/RP and RR release; its application additionally proves a three-slot/Replace Save flow, product-local PMM-backed flash ownership, ordinary named listing and payload-correlated PUP replay after preserved-NVRAM cold boot. Its typed 1 MHz/8 kHz 13-in-16 PCM profile sustains bidirectional GSM-FR. Nokia's combined NHM-2/5/6 repair path and the matching COBBA schematic establish fitted MIC2/EAR topology; isolated incoming- and outgoing-call gates prove both host-audio directions with neutral gains. |
| Nokia 5110 NSE-1 v5.30 | Yes | Yes | No | No | No | No | Partial | A clean-room C54x core executes the recovered ROM4 program/DROM and firmware uploads without DSP-output assists. Generated 24C16 provisioning passes the organic COBBA measurement validator; exact menu and save-state gates exercise the physical serial keypad. A ROM-consistent special-column-4 power input now distinguishes short press from sustained CCONT rail-off. The evidenced ROM4 cold-entry IMR bit activates INT0 organically: a 30-second gate records 6,497 CTSI frames and 207,232 DSP port-`0x27` sample reads, but no port-`0x32` write or RF acquisition. An MCU type-`0x1a` search-list packet is observed; the seven later type-`0x51` packets are DSP memory uploads, not radio configuration. Port-`0x27` sample ownership/encoding and MAD2 SCU tuning remain open. |
| Nokia 2100 NAM-2 v5.84 PPM E | Partial | Yes (bounded donor fixture) | No | No | No | No | No | `verify-2100-frontier` proves reset, 16-Mbit flash execution, RAM/MAD2 access, product-local GENSIO/CCONT and display contracts, a 64-exchange DSP bootstrap, service discovery and application registration. `verify-2100-mbus` separately proves the physical single-wire request, sampled-line occupancy, one-shot 423.1 Hz terminal event, request-derived transport ACKs and complete organic terminal `D0/04` -> phone `D0/05` exchange. The sibling v5.21 image's distinct 992-pair DSP handoff and verdict publication are separately protected. `verify-2100-interactive` preserves the v5.84 executable, attaches only the v5.21 PMM tail, cleans stale snapshots, enters physical `12345` and pins the organic `Wrong code!` result. Both cipher calls use the same digits but invalid identity/FAID strings change during the boot; a matching PMM/full-flash capture is required before acceptance, idle or registration can be promoted. |
| Nokia 3610 NAM-1 v5.11 PPM E | Partial | Yes (service application + terminal) | No | No | No | No | No | Runtime evidence independently establishes NAM-1's byte-write-driven CCONT receive-ready contract, BLB-2 inputs, later-MAD2 SIMI composition and nine-bank 96x65 reversed-segment display. Focused gates protect the complete `CONTACT SERVICE` frame, DSP bootstrap/service control, request-derived discovery, application registration, channel-`0x5f` use and the complete physical M2BUS terminal startup exchange. The supplied MCU/PPM package omits the PMM at `0x5f0000`; two product-calibration checks clear service-status bit 6 and exclude the second application-task batch, leaving SIMI, keypad and radio dormant. A foreign NHM-6 PMM proves the logical loader and first checksum but fails the product-specific second check, so a matching NAM-1 PMM is required. |
| Nokia 5210 NSM-5 v5.40 PPM E | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Product-owned GENSIO, compact DSP/service completion, BLB-2 CCONT inputs, reversed-segment display, keypad and ROM6 candidate-window radio contracts reach registered standby. Ordinary and authenticated Location Updating update EF_LOCI, release SDCCH and render `DCT3 LAB`; exact gates cover menu, Messages, return navigation, idle save/load and active-call save/load followed by physical teardown. IMSI paging and ordinary MT SMS complete through persistent storage and physical reading; wrong-group, unmatched and malformed pages are rejected without RR access. The physical predictive-text composer independently completes SMS-SUBMIT, CP/RP closure and `Message sent`. The product-specific Names route also writes `Ada`/`123` to SIM `EF_ADN` and renders it after a cold reload. Physical `*123#` and `*#21#` independently prove USSD and call-divert interrogation over GSM 04.80. A Phase-2+ card independently proves the product's nine-byte TERMINAL PROFILE, proactive DISPLAY TEXT and successful physical dismissal. Incoming and physical-dial outgoing calls additionally prove caller/called-number presentation, Answer/End, CC/RR traffic assignment and release, product-owned speech-control decoding and registered idle. A focused A5/1 gate proves organic cipher control plus encrypted SDCCH and traffic bursts through the incoming-call lifecycle. Same/different-LAC reselection, serving/all-cell loss, RF recovery and paging after reselection are also validated on the NSM-5 86/87 carrier pair. Product-specific short/long power handling and charger-originated CCONT restart are validated at physical/UI/device boundaries. Its evidenced 1 MHz/8 kHz 13-in-16 PCM bus and MIC2/EAR routes pass isolated incoming- and outgoing-call physical-duplex gates; analogue gains remain neutral rather than guessed. Handset-local application writes beyond the separately validated SIM phonebook remain unvalidated. |
| Nokia 6110 NSE-3 v4.06 PPM B (ROM3 candidate) | No | No | No | No | No | No | No | `verify-6110-static` proves the declared hardware boundary, SIM surface, DSPIF rings, sparse-flash verification transport and product-local radio/service envelopes. Matching F711604 internal ROMs, EEPROM and the DSP-owned final verification publication remain absent. |
| Nokia 6110 NSE-3 v5.48 PPM B (ROM3) | No | No | No | No | No | No | No | `verify-6110-v548-static` proves the distinct ROM3 bootstrap, `3/3` pre-upload identity, 64 transfers, final `0x0b06` first result and ROM3 EEPROM record locations. The final DSP-owned publication, internal ROMs and matching EEPROM remain absent. |
| Nokia 6110 NSE-3 v05.48 PPM B (ROM4) | No | No | No | No | No | No | No | The same gate proves a separately relocated ROM4 loader, different staged stream and different EEPROM record locations. Its pre-upload identity, final DSP publication, internal ROMs and matching EEPROM remain absent; ROM3 values are not inherited. |
| Nokia 8210 NSM-3 v5.31 PPM C (ROM6) | Research HLE | Yes | Yes | Yes | No | No | No | Normal machine retains its native wait; own uploads reach missing resident `2c75` before explicit research HLE. The acquired PMM journal fails validation; only the declared unchanged base-record comparison reaches the laboratory SIM. Isolated acceptance proves physical security/menu, calculator, persistent SIM contacts, own-ROM registration/EF_LOCI, incoming/outgoing CC/RR signaling and SMS. Idle, active-call and delivered-SMS restoration pass exact architecture, ordered protocol replay, pixels and physical continuation. Host incoming calls and bidirectional SMS, including rejection/timeout recovery, are independently verified. Speech, other bands, native DSP runtime and factory provisioning remain unproved. Contracts and runners: `8210_bringup.md`. |
| Nokia 6210 NPE-3 v5.56 PPM C | Partial | Yes (research HLE) | Yes (laboratory) | Yes (signaling) | No | No | No | Normal execution retains its fail-closed native DSP boundary; own verifier/loaders reach missing `2c75` before explicit `npe3hle` substitution. Unchanged acquired PMM supports physical Menu/SIM PIN, calculator, persistent SIM contacts, registration/EF_LOCI, incoming/outgoing calls and SMS. Idle/call/SMS restoration and product-local unattached-accessory input are verified. Host incoming calls independently prove caller presentation, physical Answer/End, own CC/RR grammar and registered idle recovery. Named host-SMS gates additionally prove received text and SIM storage, physical submission, RP-error rejection and full firmware-timeout recovery with physical continuation. Native DSP, measured verdicts, speech and electrical analogue units remain incomplete. See `6210_bringup.md`. |
| Nokia 7110 NSE-5 v5.01 PPM C | Partial | No | No | No | No | No | No | Product-local ownership acknowledgements complete 228 sparse-flash handoffs; the final DSP publication remains fail-closed. Its distinct 210-word verifier calls routines matching the recovered ROM4 ABI. See `7110_bringup.md`. |
| Nokia 8250 v5.02 PPM K | Partial | No | No | No | No | No | No | Normal execution reaches 58 alternating DSP ownership pairs and the final-result wait. Native staged execution verifies own uploads before missing resident routine `2c75`. The declared runtime HLE comparison receives the exact acquired PMM record but its candidate decode fails the own-ROM structural validator. Selector ownership and codec-direction audits do not resolve original chip pairing or ROM6 preprocessing. Graphical and downstream phone acceptance remain blocked on matched evidence; no donor provisioning or success verdict is installed. See `8xxx_bringup.md`. |
| Nokia 8850 v5.31 C | Partial | Yes (research) | Yes (research) | Yes (research signaling) | No | No | No | Own native uploads reach absent mask routine `2c75`; separate `nsm2hle` verifies physical security/menu, calculator, persistent SIM contacts, own radio registration/EF_LOCI, incoming/outgoing CC/RR signaling and SMS read/submission. Idle, active-call and delivered-SMS restoration pass exact architecture, protocol replay, pixels and post-load physical input. Host incoming calls and bidirectional SMS, including distinct rejection/timeout screens and physical recovery, pass independently. The normal machine remains at the DSP-result poll; media and native DSP runtime are unverified. Contracts and acceptance recipes: `8xxx_bringup.md`. |
| Nokia 8890 v12.20 C | Research HLE | Yes | Yes | Yes | No | No | Partial | Normal machine remains conservative. Separate `nsb6hle` executes own native verifier/loaders before explicitly handing off at absent resident routine `2c75`; fragment-derived ROM6 input is not measured silicon. Own-consumer self-test, discovery, BLB-2 inputs, SIM initialization and physical security/menu interaction pass with own PMM. Physical A/123 save and cold Search/Detail pass; calculator computes 12+3=15. Own radio contract completes laboratory GSM900 and strict two-cell PCS1900 registration, EF_LOCI and paging/BCCH; PCS acceptance requires organic carrier-600 selection and SI1 band indication. Physical clock/date entry and recovery from invalid time reach registered idle; cold RTC persistence is unproved. Incoming physical Answer/End and intended 1234567 outgoing CC/RR lifecycle pass on both bands, including host incoming signaling from settled idle. Incoming hello SMS is physically read and stored; outgoing A/5551234 SMS and CP/RP closure pass on both bands. Settled GSM900 idle and active-call/delivered-SMS restoration on both bands pass exact CPU/RAM/time, ordered protocol replay and post-load physical continuation checks. Identity/record replies remain unanswered; speech and native DSP runtime are unproved. See `8xxx_bringup.md`. |
| Nokia 6250 v5.03 PPM C | Partial | Yes (research PMM comparison) | Yes (research) | Yes (research) | No | No | No | Normal machine remains fail-closed at `429842`. Own uploads plus declared runtime HLE and an explicit initial-record PMM comparison validate physical calculator, persistent SIM contacts, fresh/preserved registration, call signaling and SMS. Idle, active-call and delivered-SMS restoration pass exact architecture/protocol/pixel replay and physical continuation. The unattached accessory-input polarity is schematic-backed and runtime-verified. Host incoming calls and bidirectional SMS include independently checked rejection/timeout recovery. The acquired journal is inconsistent; native mask execution and PCM/audio remain unresolved. These gates do not promote the normal machine. See `6250_bringup.md`. |
| Nokia 5510 NPM-5 v3.53 PPM C | Partial | Diagnostic only | No | No | No | No | No | `nmp5stage` executes its own verifier and loaders to missing mask `2c75`, retaining ownership without runtime HLE. Separate `nmp5hle` reaches `CONTACT SERVICE` through request-derived discovery and own compact self-test consumption, not idle or phone-service parity. No MU4 keys are mapped and no donor PMM is supplied. See `5510_bringup.md`. |
| Other declared DCT3 products | No | No | No | No | No | No | No | Driver declarations and acquired service packages are not executable evidence. Extraction, matching storage inputs and product-specific contracts must be established before promotion. |

## Promotion rules

`verify-8850-sim-pin-registration` independently proves physical SIM PIN and
phone-code entry, own staged/HLE boundary, reviewed navigation pixels and
persistent registration. Its existing type-55 scan suffices; the delayed
type-57 response used by other products is not inherited.
The `verify-8850-pin-host-*`, `verify-8850-pin-phonebook` and
`verify-8850-pin-state-*` gates separately compose authentication with
bidirectional host signaling/SMS, cold persistent contacts and exact
idle/call/SMS restoration. These remain research-HLE contracts.

`verify-8890-sim-pin-registration` independently proves delayed physical
PIN acceptance and configured GSM900 registration through its own
`57:01140000` measurement request and correlated type-8b consumer.
`verify-8890-pin-host-*`, `verify-8890-pin-phonebook` and
`verify-8890-pin-state-*` separately cover authenticated GSM900 host calls/SMS,
cold persistent contacts and exact idle/call/SMS restoration.
`verify-8890-pin-pcs-registration` and four `verify-8890-pin-pcs-host-*` gates
independently add authenticated PCS acquisition and both host call/SMS
directions. Three `verify-8890-pin-pcs-state-*` gates independently add exact
authenticated PCS idle/call/SMS restoration and physical continuation.
Native speech remains unproved.

`verify-8210-slow-pin-registration` independently covers physical SIM PIN and
phone-code entry, own task-12 background-measurement completion and persistent
registration on the coherent ARFCN 4/5 laboratory network. It retains the
declared base-record PMM comparison and research HLE boundary.
Its `verify-8210-pin-host-incoming-call` and `-sms` gates independently add
physical Answer/End and caller/idle presentation, or physical Read and durable
message storage, after that authenticated registration.
The paired `verify-8210-pin-host-outgoing-call` and `-sms` gates additionally
verify physical dialing/composition, correlated host decisions and firmware
CC/RR or CP/RP completion after authentication.
`verify-8210-pin-phonebook` separately proves save, new-process authentication,
retained-location registration and exact contact readback with unchanged
persisted card bytes.
The three `verify-8210-pin-state-*` gates independently verify authenticated
idle/call/SMS restoration, exact architecture/protocol/pixels and post-load
physical Menu, call release or SMS reading.

The 6210's `verify-6210-slow-pin-registration` additionally verifies physical
late PIN acceptance followed by registration and persisted EF_LOCI on the
explicit ARFCN 35/36 laboratory topology. The ordinary PIN/menu gate alone
does not establish this lifecycle. These are runtime-HLE signaling tests,
not native DSP or speech evidence.
`verify-6210-pin-host-incoming-call` additionally composes this authentication
and registration lifecycle with host paging, physical Answer/End, exact
NPE-3 call signaling and reviewed caller/registered-idle pixels.
`verify-6210-pin-host-incoming-sms` also proves host delivery, physical Read,
exact message-body pixels and persistent SIM content after authentication.
The paired `verify-6210-pin-host-outgoing-call` and
`verify-6210-pin-host-outgoing-sms` gates independently verify physical dialing
and composition, correlated host decisions and firmware CC/RR or CP/RP
completion after PIN-enabled registration; speech remains unproved.
`verify-6210-pin-phonebook` proves physical save, independent-process PIN
reauthentication, retained-location registration and exact contact readback
without changing the saved card bytes.
The three `verify-6210-pin-state-*` gates separately cover authenticated
idle/call/SMS architecture and protocol restoration, physical call release
and SMS reading after load; they do not establish native DSP restoration.

The 6250's PIN-enabled gates independently cover delayed registration,
incoming/outgoing host call signaling and SMS, and phonebook save followed by
new-process PIN reauthentication and readback. They retain the declared PMM
comparison and do not promote the normal machine or native speech.
The three `verify-6250-pin-state-*` gates establish authenticated idle,
connected-call and delivered-SMS restoration on coherent lab carrier 19,
including architectural/protocol replay and physical menu, hang-up or read
continuation. `verify-6250-coherent-call-state` and
`verify-6250-coherent-sms-state` cover the corresponding no-PIN lifecycles.
Registration and, where enabled, PIN acceptance must precede the save
boundary; events after reload cannot satisfy those prerequisites.
These local-peer fixtures disable the host adapter, while still requiring
handset Location Updating and persistent SIM location; host-service gates
retain their separate host/carrier agreement requirement.

The 6250's `verify-6250-coherent-registration` independently verifies its
declared initial-record HLE composition on external lab carriers 19/20:
own native uploads, compact self-test, SCH/MCU/host carrier agreement,
Location Updating/release/paging and persistent SIM location. Its older
default-cell call/SMS tests remain bounded signaling comparisons because
MCU carrier 19 and host carrier 1 do not agree. This new registration proof
does not automatically promote those downstream lifecycles or native speech.
Separate `verify-6250-coherent-host-incoming-call` and
`verify-6250-coherent-host-outgoing-call` now establish both physical call
directions, exact configured traffic/release words, host identity correlation
and reviewed caller/digits/connected/idle pixels on carrier 19. Four separate
`verify-6250-coherent-host-*-sms` gates now validate incoming persistent Read,
physical outgoing success, RP rejection and unshortened silence-timeout
recovery on the same coherent cell. `verify-6250-sip-cancel` separately proves
a real local unanswered INVITE/CANCEL/487 lifecycle, own CC/RR release,
zero media and decoded physical missed-call dismissal to reviewed idle.
Answered SIP, native speech and SIP restoration do not inherit that promotion.

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

## External SIP evidence

These independently executed profiles connect to a real local PJSIP endpoint
through the standalone host bridge. They do not promote native DSP execution,
real RF service or the other firmware versions of the same handset.

| Tested profile | Incoming/outgoing SIP with HLE media | Incoming connected/ringing restore and caller CANCEL | Outgoing connected/pending restore | SIP physical waveform proof |
| --- | --- | --- | --- | --- |
| 3210 v6.00 | Yes | Yes | Yes | Yes, both directions |
| 3310 v6.39 | Yes | Yes | Yes | Yes, both directions |
| 3330 v4.50E | Yes, own physical PMM setup | Yes | Yes | Yes, incoming and outgoing |
| 3410 v5.46E | Yes, physical Send | Yes | Yes | Yes, incoming and outgoing |
| 5210 v5.40E | Yes, physical Send | Yes | Yes | Yes, incoming and outgoing |

Media transport requires sustained ordered handset acceptance, not merely
bridge transmission counts; bounded queue drops remain reported. Restore gates
clear the external dialog and the restored GSM call under a new epoch, rather
than claiming to restore SIP. Unanswered scenarios reject Answer/CONNECT and
accepted media. Named gates, protocol details and waveform limits are maintained
in `external_call_bridge.md`. Signaling-only research products have not inherited
this media promotion.

SIP failure coverage is separate: busy (486) and unavailable (480) are verified
for all five profiles in the table. The 3310 fixtures cover
two organic attempts; the 3410 and 5210 fixtures observe one attempt and no
redial in the 45-second run. The 3330 fixtures independently observe one ended
attempt and no redial in the 40-second call run after fresh physical PMM setup. Checkers
require every observed host request to end and reject unhandled retry attempts
or accepted media.

Fresh incoming SIP after an idle save/load is independently verified for
all five profiles in the table. The new epoch admits exactly one new call, physical
Answer, sustained ordered media and normal release. This is a separate gate
from clearing an active restored call; other profiles have not inherited it.

The 6210 v5.56 `npe3hle` profile separately passes `verify-6210-sip-cancel`:
real caller CANCEL/487 during alerting, complete CC/RR release, the missed-call
screen and physical Exit back to registered idle. It rejects Answer and media
and does not establish answered SIP speech.
`verify-6210-sip-idle-restore` separately repeats this fresh-call lifecycle after
exact idle CPU/RAM/time restoration and host epoch invalidation; it does not
restore a SIP dialog.

The 8890 v12.20 `nsb6hle` profile separately passes
`verify-8890-sip-cancel` after own physical security/clock/date setup:
real caller CANCEL/487 while alerting, full CC/RR release, localized missed-call
presentation and decoded physical Exit back to registered idle. Its configured
ARFCN60, own SETUP and persisted SIM location are checked independently.
No Answer, accepted media or native DSP speech is allowed. It does not inherit
the media-capable profiles' answered-call promotion.
`verify-8890-sip-idle-restore` additionally proves exact idle CPU/RAM/time and
protocol replay before a fresh epoch-2 incoming call, followed by the same
real cancellation and physical UI cleanup. No external dialog is saved or
restored and no speech is established.

## Product-boundary cautions

The 8210 additionally passes `verify-8210-sip-cancel`: real local
INVITE/CANCEL/487, coherent ARFCN4 SCH/tuning/host registration, zero media,
complete CC/RR release and physical missed-call dismissal to reviewed idle.
It retains the labelled acquired base-record comparison; factory PMM and
native speech are not promoted.
`verify-8210-sip-idle-restore` additionally proves exact idle CPU/RAM/time,
pixels and ordered protocol replay before a fresh epoch-2 unanswered SIP
call with the same release and physical-cleanup requirements. No external
dialog is saved or restored.
The four `verify-8210-host-*-sms` gates share that explicitly configured
ARFCN4 cell: incoming persistent physical Read, outgoing success text,
RP rejection and silence-timeout recovery each require coherent registration
and their independent host/handset protocol checks. The invalid acquired
PMM journal remains excluded only by the labelled base-record comparison.
Both `verify-8210-host-incoming-call` and `verify-8210-host-outgoing-call`
also require coherent ARFCN4 registration and exact own traffic/release
words, physical Answer/End or dial/Send/End, correlated host identities and
reviewed presentation. These establish bidirectional host call signaling,
not media or native speech.

The 6210 passes `verify-6210-power-cycle` with unchanged own PMM: physical
shutdown, blank display, at least eight seconds of retained RTC ticks and
DSP/radio silence, PWRONX restart, both own native-upload/HLE stages,
retained-LAI registration, exact idle recovery and decoded physical Menu.
Its warm registration rewrites LAI but not the valid status byte, unlike
the 8890's no-rewrite lifecycle. Firmware resets RTC seconds after wake;
cold-process persistence and native speech remain unproved.

The 8210 separately passes `verify-8210-power-cycle` in its explicit
base-record HLE comparison on coherent carrier 4: physical shutdown,
blank LCD, at least eight seconds of continuous RTC and DSP/radio silence,
PWRONX wake, independently verified own uploads/HLE/self-test/registration,
retained-LAI Location Updating and decoded security entry back to idle.
Like the 6210 it rewrites LAI but not valid location status. Firmware resets
RTC seconds after wake; this does not establish native speech, factory PMM
provisioning, cold RTC persistence or complete rail clock-gating.

The 8890 additionally passes `verify-8890-power-cycle`: physical shutdown,
blank LCD, six seconds of continuous CCONT RTC ticks with DSP/radio
transport silence while CPU power is off, separate PWRONX restart,
both own native-upload/HLE handoffs and decoded security input back to idle.
Its second registration carries retained LAI and closes without redundant
EF_LOCI writes, checked against a distinct strict grammar. Firmware itself
resets seconds after wake. This does not prove cold-process RTC persistence,
physical wake timing, all-block rail clock-gating or native speech.
`verify-8890-power-off-restore` additionally proves exact CPU/RAM/time
restoration while off, identical RTC replay and blank frames, sustained
DSP/radio silence, and physical restart to registered idle after load.

The 8850 v5.31 `nsm2hle` profile additionally passes
`verify-8850-sip-cancel` with own acquired MCU/PMM: settled physical startup,
own registered ARFCN1, caller `5551234`, real CANCEL/487 while alerting,
CC/RR release, persistent SIM location and decoded physical Exit from the
missed-call screen. Initial idle presents `DCT3 LAB`; final idle presents
numeric PLMN `00101`. Both frames are checked separately, not treated as
proof of retained operator text. Answered SIP, media and native speech
remain unproved.

`verify-8850-sip-idle-restore` separately verifies exact idle architectural
state, byte-identical replayed LCD pixels and transport/SIM replay before
a new epoch-2 incoming SIP call and the same cancellation/physical cleanup.
It does not restore an external dialog or establish media.

The 6210 research-HLE profile independently passes `verify-6210-sim-toolkit`:
its own nine-byte TERMINAL PROFILE, card-owned STATUS/FETCH DISPLAY TEXT,
physical successful clearance and exact registered-idle recovery. Other
proactive commands and native DSP are not promoted by this gate.

The 8210 research-HLE profile additionally passes `verify-8210-ussd`: physical
`*123#`/Send, exact GSM supplementary-service request/response, RR release,
reviewed result presentation and physical Back to registered idle. Its own
decoder table matches the independently checked 8850 and 8890 tables, so
their research profiles share corrected Star/Hash labels; this does not
promote native DSP or the normal machines.
`verify-8210-call-divert` separately proves physical service-`21`
interrogation, the inactive result, RR release and Back-to-idle; forwarding
activation and delivery are not covered by that gate.

The 8850 and 8890 independently pass `verify-8850-ussd`,
`verify-8890-ussd`, `verify-8850-call-divert` and
`verify-8890-call-divert` through fresh isolated own-MCU/PMM runs.
Each requires physical input, exact supplementary signaling, its own
reviewed result and registered-idle frames, and persisted EF_LOCI.
The 8890 includes physical clock/date settlement; its localized frames
are not inherited from the 8850. Divert coverage is inactive interrogation
only, and these gates do not establish native resident DSP or speech.

The 8890 additionally passes `verify-8890-host-incoming-sms` and
`verify-8890-host-outgoing-sms` with unchanged own MCU/PMM and a configured
ARFCN60 laboratory cell. Acceptance requires carrier-correlated SCH and
host network state, external SMS request/decision ownership, own physical
composition/read and firmware CP/RP/RR closure; received hello persists
with read status and reviewed body pixels. The earlier default ARFCN1
topology beside the firmware's ARFCN60 candidate is not this promotion.
Native DSP, speech and hardware RF remain unproved.

`verify-8890-host-rejected-sms` and `verify-8890-host-silent-sms` additionally
prove distinct localized failure/timeout presentation and decoded physical
menu recovery. Host decisions remain correlated and single-use; silence
contains no RP result and closes through handset DISC/UA/deconfiguration.
These do not promote native DSP, speech or other products' failure behavior.

- Packet length similarity is not semantic evidence. NHM-5 type `0x20` is not
  NSE-8 type `0x1a`; each enabled radio profile uses its independently recovered
  command/report lifecycle.
- A numeric task id, channel-confirmation value, DSP-ready value or EEPROM
  offset is not portable across products or ROM families.
- The Nokia 6110 remains fail-closed. A physical capture accepted by
  `make verify-6110-bootstrap-capture`, or matching internal ROMs reproducing
  it, is required before executable promotion.

The current radio and media contracts live in `docs/network_scouting.md`,
`docs/dsp_interface.md` and `docs/structural_regression.md`. The bounded NSM-5
frontier is recorded in `docs/5210_bringup.md`. Detailed NSE-3
hardware, firmware addresses, cautions and the single resumption question
live in `docs/6110_bringup.md`; the capture contract lives in
`docs/6110_bootstrap_capture.md`. This matrix is the sole authority for model
promotion state.
