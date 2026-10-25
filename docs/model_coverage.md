# DCT3 model coverage

Nokia 6190 / NSB-3 v6.13 has a conservative `noki6190` MCU instrument.
Two initially erased EEPROM runs reproduce exact own initialization and idle
samples (`tools/nsb3_boot_check.py`), not graphical/input or DSP completion.
Its own peripheral and dual-catalogue contracts are in `6190_bringup.md`;
no donor identity or fitted-mask compatibility is assumed.
Separate `nsb3r4t` explicitly uses acquired NSE-1 ROM4 as an unproved
compatibility input. It retains all 811 own uploaded words above `27ff` and
executes legally through the bounded native mailbox/frame test, ending on a
white panel with default full-scale ADC2. That control is not interactive or native
RF/voice support; see `tools/nsb3_native_check.py` and `6190_bringup.md`.
With the explicit nominal ADC2 hypothesis and six decoded integrity bytes
recomputed in its own cold EEPROM, `nsb3r4t` separately reproduces an exact
`Insert SIM card` frame with unchanged final storage. The passing fixture and
calibration-only negative control are checked by `tools/nsb3_selftest_check.py`.
A separate inserted laboratory card/fresh phone-code retry fixture accepts
physical security entry and clean Messages/Names menus. Bounded native save/load
restores observed architecture/RAM checksums, exactly replays 7,767 DSP I/O
writes and reproduces physical Down to Call log; long-run stability is not implied.
Physical A/12345 save
issues one EF_ADN update; an independent cold process retrieves the name and
number without rewriting or changing the complete card. The boundary is checked
by `tools/nsb3_menu_check.py` and `tools/nsb3_phonebook_check.py`.
Physical Options/Erase additionally emits one erased ADN record; independent
cold Search displays No phone numbers without card mutation. Physical Edit
and explicit Save also change both name and number to D/12346; independent
cold Details retrieves both fields without card writes. Own Names/Options
exposes Scrolling view/Memory status and reports the laboratory SIM's 1 used/49 free,
not a separate Phone-memory choice; Nokia's product manual corroborates
SIM-dependent contact capacity. A 6150-style local Phone store is not an
independently supported requirement for this profile.
An explicit FDN-capable lab card additionally proves physical PIN2-authenticated
On/cold-On/Off/cold-Off and exact file-validity/identity rehabilitation, with
unchanged credentials and records. These settings do not establish native registration.
Wrong-PIN2 FDN activation also preserves Off state, consumes only one retry
and cold-recovers through correct PIN2 with the normal validity protocol.
Physical PIN2-protected EF_FDN A/12345 save and independently cold Details
also pass, with unchanged ordinary ADN, credentials and validity. Protected
FDN number edit/cold read to A/12346, authenticated erase cancellation/cold
retention, and confirmed erase/cold empty Search also pass. Local dialing rejects
unlisted54321 while On but not listed12346 or the same54321 after physical Off.
The non-rejected screen has retained pixels; native registration and connected
calls remain unqualified.
Wrong-PIN2 FDN Add preserves the existing record, consumes exactly one retry
and cold-retrieves that record without changing the retry count or card.
Physical offline SMS composition/save, cold Outbox read, confirmed deletion
and a second cold empty-Outbox check also pass through the laboratory SIM.
A Phase-2+ external card separately proves the own five-byte TERMINAL PROFILE,
organic STATUS/FETCH, displayed DCT3 SAT text and successful physical dismissal.
A separate interactive profile returns physical GET INKEY 5 and GET INPUT 42,
with the complete persistent card unchanged. SET UP MENU installs DCT3 menu;
physical Continue/Exit send distinct accepted item-1/item-2 envelopes. Immediate
post-selection Wait is observed for ACK-only selection. A follow-up SELECT ITEM
card displays Alpha/Beta, returns physical Beta and restores the clean installed
list; ACK-only automatic settlement remains unqualified. Other Toolkit commands remain open.
A fetched DISPLAY TEXT awaiting physical input also survives native save/load:
one FETCH yields two successful physical completions with identical I/O and
returned-menu pixels, preserving the complete persistent card.
The separate five-second TIMER MANAGEMENT request receives explicit result31;
this is a qualified refusal, not timer-start/expiry support.
Physical SIM-security gates cover PIN-request Off/On, PIN1/PIN2 changes and
cold restoration, retry persistence, physical PIN exhaustion, wrong-PUK
persistence/recovery and replacement-credential authentication. Exact whole-card
comparisons protect unrelated files and credentials. Permanent PUK1 loss causes
cold SIM-card rejection with responsive dialing input but no authentication.
Permanent PUK2 loss instead retains PIN1/phone login and Messages, while PIN2
selection is refused without another credential APDU. Rejection pixels persist
over a neighboring security-item redraw even at90s after Down alone. A separate
physical Back/End/Menu control clears the notice and restores clean menus with
the same blocked card; automatic notice clearing remains unqualified. These credential fixtures are
bounded to 90–92 seconds: independent extended controls reset near 94 seconds
after the DSP receive-activity window becomes empty and firmware publishes
reason `68`. That trigger is mapped, not suppressed; the missing native
response and fitted-mask/RF compatibility remain unresolved.
The repeated idle glyphs now trace to erased startup-text record0745: own
PPM conversion maps byteff to U+017E before LCD output. A declared one-byte
empty-text input plus physical Field test `00` yields repeatable idle-layout
pixels after an independent cold boot, with unchanged complete card/flash.
Own CCONT selector0 plus MAD2 external-status bit1 now independently account
for Headset, Car and title-free idle branches through three raw-input controls,
with unchanged complete storage and shared physical Messages-menu acceptance.
Electrical open-input levels, fitted-mask identity and radio/call/SMS-delivery
acceptance remain open; native compatibility is unchanged.

Nokia 8855 / NSM-4 v5.18 has a fail-closed `noki8855` MCU research profile.
Two own erased-storage boots produce identical catalogue/storage state and
stop at an unacknowledged DSP ownership wait; `tools/nsm4_boot_check.py`
checks that boundary, not graphical boot. The own register/key/static upload
contracts and exact remaining dependency are in `8855_bringup.md`. No donor
PMM, resident mask, DSP completion or application acceptance is implied.

This matrix records demonstrated product coverage, not family resemblance. A
cell is promoted only by a named reproducible gate or reviewed hardware or
firmware evidence. `Partial` means that the preceding acceptance level works
but a material hardware contract remains calibrated, opaque, or unverified.
Authenticated restoration gates must establish PIN acceptance and laboratory
registration before the save boundary, independently of post-load events.
Whole-run hardware/provisioning checks remain separate from that ordering
requirement.

NSM-5 Phone-memory breadth is separately qualified by
`make verify-5210-pin-phone-memory-multiple`: physical Add preserves the
original Ada/123 alongside A/123, and an independent cold process selects
both distinct names and their Details with complete unchanged SIM contents.
Physical confirmed erasure of A also preserves Ada/123 across a separate
cold process. Capacity, arbitrary record enumeration and failure atomicity
remain open.

The5110i's separate Phase-2+ card control qualifies own terminal profile
`03018000`, organic FETCH after a physical contact-save completion and
automatic DISPLAY TEXT result30 capability refusal, not Toolkit display.
The matched Phase-2 control has no proactive traffic; both save A/123 with
identical complete card/flash endpoints. Own encoder/write-buffer/decoder
evidence bounds18 EEPROM differences to unused empty-field padding;
exact whole-bank hashes remain required, not masked.
See`run_nse2_toolkit.py` and`5110i_bringup.md`; sustained native service
remains unqualified.

The 6210's `npe3mbus` is a clocked-MBUS/sleep research instrument, not an
additional handset-support promotion. Read-only version service passes there;
post-wake call input remains unqualified. Established `npe3hle` call/software
acceptance retains the timer-disabled composition until both contracts compose.

The6150's separate native-compatibility Toolkit coverage additionally includes
physical REFRESH selection, own terminal re-profiling, successful completion
and exact idle return. Its one-byte firmware settings update is protected by
an exact whole-EEPROM output hash; card/flash stay unchanged. This92s gate does
not qualify the later restarted application or sustained native RF operation;
see `nsm1_toolkit_check.py --refresh` and `6150_bringup.md`.
The6150 also qualifies PLAY TONE presentation and physical cancellation with
result11, paired with a no-key control that stays pending through92s. Whole
storage stays unchanged. An independent93.5s no-key run completes organically
after the current Timer1/FIQ5 event, returning success and the installed menu.
Requested-duration accuracy and production audio remain open; this exposes calibrated
clock debt, not a new DSP-reply requirement. A separate explicit8kHz laboratory
composition now qualifies native tone output and selected active-output save/load;
it does not establish the physical codec law or handset clock attachment.
Own physical clock/date setup additionally qualifies an awake RTC alarm,
firmware acknowledgement, Alarm!/Stop/Snooze presentation and PUP buzzer output.
A same-binary no-Stop control distinguishes physical Stop from automatic quieting.
Exact whole storage and retained-clock endpoints are guarded by
`nsm1_clock_alarm_check.py`. A separate cold process retrieves both12:35
editors without reprogramming CCONT, acknowledges the retained counter's next
minute event and redraws12:36 idle. Exact whole endpoints and the parent
Stop/control pair are required by `nsm1_clock_cold_check.py`; calendar year/date
interpretation remains unqualified. A separate physical power-off
alarm gate proves retained-RTC wake, Stop and both activation choices: No
returns to sustained rail-off silence; Yes accepts PIN/phone authentication
again and navigates Messages/Call register. Whole storage endpoints are exact;
this does not prove complete clock gating or native radio/call functionality.
One awake Snooze cycle also passes: physical Snooze silences the first alarm,
firmware programs12:41, RTC re-alarm resumes native PUP output, and physical
Stop restores the quiet idle presentation. Repeated/powered-off snooze and
physical duration accuracy are not qualified.
See `6150_bringup.md` for the separate contracts.
The5190 independently passes the same command family with its own profile,
physical route, exact idle pixels and a different complete settings-bank
transition (`nsb1_toolkit_check.py --refresh`); no storage field or fitted-mask
conclusion is transferred between those products.

NSB-7 media qualification: the `nsb7hle` ordinary incoming/outgoing SIP
fixtures now pass own documented clocked HLE PCM and product-owned exact
UI/storage checks (`nsb7_sip_check.py --media`, add `--incoming` for Answer).
The separate `verify-8290-sip-{incoming,outgoing}-waveform` gates require
sustained microphone/earpiece tones through actual MAME host audio streams.
Connected and unanswered incoming/outgoing restoration pass independent
exact-state/old-dialog-disposal gates on the PCM-enabled composition.
Unanswered external media stays zero; any local outgoing PCM is separately
bounded to restored radio teardown with zero downlink. No native DSP
speech or conservative `noki8290` promotion is implied.

Native NSE-1 opcode coverage is currently 626 asserted forms out of 626
observed in cold idle; Menu and long-power runs add no unasserted forms.
The reproducible gate is `make check-c54x-observed-coverage`; it verifies
the exact opcode fingerprint, instruction-extension decoding and fixture
assertions across all three runs, rather than inferring coverage from decoder
source patterns alone.
The older 594-word closure is historical; the current exact
fingerprint is deliberately rebanked with a negative control rejecting the
old set. See the current audit boundary in
`rom4_dsp_loader.md`. No native RF acquisition or speech is promoted.
The separate `nse1pcm` external-frame laboratory fixture additionally proves
native runtime key-2 DTMF synthesis, with paired unclocked control and complete
sample accounting. `nse1bit` independently exercises native RX/TX bit-clock
inputs at 1 MHz with an assumed sync pulse/fan-out and exact active partial-word
save/load replay. Neither is another supported product, and neither attaches
clocks or promotes call media on the ordinary 5110 machine; see
`cobba_control_boundary.md`.
The original `nse1pcm`/`nse1bit` tone inventory is separately reconciled: all 642 observed forms
have exact-opcode executable assertions across completed default, `mac-smem`
and `xc-signed` CPU fixtures. This extends instruction-evidence coverage, not
the cold-idle fingerprint, physical clock ownership or native speech support.
The separate `nse1pin` laboratory also clocks the native boot loopback without
the untimed word echo. Its 643-form inventory includes the retry-store path,
independently checked by the backend controller fixture. Graphical/key-tone
continuation and active replay pass against paired controls; ordinary handset
clock-enable wiring remains unproved.

| Product / tested firmware | Booting | Interactive | Registered | Call control | Internal media | Physical duplex | Hardware-faithful | Principal evidence or next boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nokia 5110i NSE-2 v5.53 PPM A | Yes (research, short-run) | Physical menus, SIM security and cold storage | No | No | No | No | No | `nse2r4v` executes own uploads on acquired NSE-1 ROM4 with own checksum/retry fixtures and a nominal ADC2 hypothesis. Physical phone authentication, persistent SIM contacts and offline SMS save/read/delete/cancel pass. PIN1/PIN2 changes, PIN-request toggles, retry exhaustion, PUK recovery and cold replacement-credential acceptance pass. Permanent PUK1 rejection is distinguished from permanent PUK2 loss with live unrelated menus. FDN settings, protected record create/read/edit/erase/cancel and local listed/unlisted/Off dialing contrasts pass; call-progress presentation is not network acceptance. Native receive supervision resets an advancing scan at about31.97s. Ports34/35/36 expose a pending word transmission with unproved clock/completion ownership and no established causal link to that reset. Sustained operation, registration, network SMS and speech remain unaccepted. Ordinary silent `noki5110i` and open-pin `nse2r4t` controls are unchanged. Fitted mask/clock and electrical calibration remain unproved; no donor EEPROM or firmware forcing. See `5110i_bringup.md`. |
| Nokia 8290 NSB-7 v5.22 PPM A | Failure frame only | No | No | No | No | No | No | Own package supplies MCU/PPM but no PMM or resident DSP mask. Fail-closed `noki8290` uses independently reviewed byte-write-ready GENSIO wiring and renders exact `CONTACT SERVICE` pixels from erased storage. DSP exchange and service replies are disabled; keypad and board defaults remain provisional. See `8290_bringup.md`. |
| Nokia 8290 NSB-7 v5.22 research-HLE `nsb7hle` | Yes (research) | Physical menus/input/storage | Yes (lab) | Incoming/outgoing SIP | Yes (HLE) | Yes (HLE) | No | Own loaders precede explicit runtime HLE, own checksum/retry fixtures and nominal ADC2 hypothesis. PIN/PUK, protected FDN, phonebook and SMS have physical cold/storage gates. Own documented PCM and speech controls qualify incoming/outgoing SIP and sustained endpoint tones with exact UI/card/journal checks. Four connected/unanswered call-phase restoration gates preserve exact handset state and dispose external dialogs without replay. Unanswered external media stays zero. Toolkit and RTC retain explicit product scope. Native execution stops at missing resident2c75; native speech, external RF and normal-machine promotion remain unproved. See `8290_bringup.md`. |
| Nokia 6130 NSK-3 v5.61 PPM A | Bounded research menus | Physical code/menu/storage | No | No | No | No | No | Conservative `noki6130` retains erased-source failure controls. Separate `nsk3r4t` declares NSE-1 ROM4 compatibility, diagnostic record, nominal ADC2 hypothesis554 and own retry-byte fixture. Physical phone-code acceptance, Send/End decoding, SIM phonebook save/cold read, PIN/PUK/request settings, PIN1/PIN2 changes, physical PIN1/PIN2 exhaustion with PUK recovery/cold new-PIN authentication and offline SMS save/read/delete/cancel are checked. A physical PUK2-exhaustion cold chain proves permanent loss with unchanged credentials, independent PIN1 authentication and responsive security menus. A separate PUK1-exhaustion cold chain from the declared PIN1-blocked lab card proves permanent SIM-card rejection, unchanged credentials and cold refusal of further credential entry. FDN activation/deactivation, cold identity rehabilitation, PIN2-protected save/edit/erase/cancel, cold number/empty-search display and wrong-PIN2 record preservation also pass; local listed/unlisted/Off dial rejection controls pass without connected-call acceptance. Physical Net monitor Test00 additionally changes own selector03a9 fromff to3f; a separate authentication-only cold boot preserves the complete resulting EEPROM and readable idle. Another210 changed bytes remain semantically unmapped and are protected by the whole-bank oracle. Headset remains an accessory/profile title, not an operator. A separate40s idle save/load exactly restores observed ARM/DSP architecture and memory checksums, repeats7764 DSP I/O writes over2.7s and reproduces physical Messages navigation with unchanged storage. This is bounded restoration, not exhaustive device-state coverage. A declared Phase-2+ card additionally proves own four-byte terminal profile03018000, organic FETCH and automatic DISPLAY TEXT result30 capability refusal before physical input; the matched Phase-2 control has no proactive traffic. Both preserve whole storage and physical Messages navigation; this does not qualify successful Toolkit display. A native packet-supervision reason68 reboot at93.975196s prevents sustained-operation acceptance; the90s scan advances without qualifying RF results. Radio/network SMS, fitted DSP/clock, electrical calibration and registered idle remain unproved. See `6130_bringup.md`. |
| Nokia 3210 NSE-8 v6.00 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Structural, navigation, authentication, mobile-terminated and physical-dial mobile-originated call lifecycles, paired GSM-FR/FACCH/degraded media and isolated physical audio pass. Organic A5/1 gates cover ciphered SDCCH, SACCH, TCH/F/FACCH, incoming/outgoing calls, degradation and exact active-call save/load; A5/0 remains the default regression composition. Idle mobility proves same-/different-LAC reselection, replacement-cell paging, loss/recovery, persistent loss, preserved EF_LOCI and save-state boundaries. Ordinary MT SMS proves exact notification/storage, preserved cold-boot listing, physical read/delete/cancel, save-state continuation, pre-page malformed rejection, correlated duplicate suppression, organic ten-record capacity and two independently paged messages with selective deletion. Physical mobile-originated SMS additionally proves EF_SMSP editing, SMS-SUBMIT reassembly, success/rejection/timeout and retry outcomes, requested status-report persistence, and active CP-wait save/load. Smart Messaging additionally proves RAM-owned concatenation, port dispatch, RTPL playback, physical promptless Save/Discard, product-local 24C128 ownership and ordinary named listing with payload-correlated playback after preserved-NVRAM cold boot. The outgoing host suite additionally proves asynchronous outcomes, termination, overload/reconnect and sequential calls. Physical long-power shutdown reaches CCONT rail-off after save/load. Real COBBA DSP-controlled mux/gain semantics remain opaque. |
| Nokia 3210 NSE-8 v5.01 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Independent ROM gates cover boot/UI, the call lifecycle, save-state replay, isolated physical duplex, physical mobile-originated SMS through CP/RP closure and the same independently executed CCONT rail-off power lifecycle. The same COBBA/DSP limitation applies. |
| Nokia 3310 NHM-5 v6.39 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Frontier/navigation, authentication, registration, paging, incoming and physical-dial outgoing calls, media resilience and physical duplex pass using its product-specific radio, DSP-control and 1 MHz/125-clock PCM contracts. Focused A5/1 promotion proves organic cipher control, ciphered SDCCH and bidirectional traffic media without inheriting its DSP packet grammar. Focused idle-mobility gates independently prove same-/different-LAC reselection, paging, preserved location and replay. Ordinary MT SMS independently proves localized notification, exact text, SIM read status, physical erase confirmation, preserved-NVRAM Menu 2-2 reading and durable deletion. Its localized composer also proves physical UCS-2 SMS-SUBMIT, success UI and a requested delivery report through a second complete paging/CP/RP/RR and SIM-storage lifecycle. Sequential two-part Smart Message transport closes organically through CP/RP and clean RR release; its localized application path proves physical Save, product-local PMM-backed flash ownership, ordinary cold-boot listing and sustained saved-tone DSP output. The MCU-visible ROM4 mailbox publishes a 900-Hz carrier even for saved custom tones, so payload pitch remains DSP-owned and cannot be promoted without a real DSP backend or an equivalent trace. Product-specific analogue gain programming remains unproved. |
| Nokia 3330 NHM-6 v4.50E | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Fresh-PMM frontier, navigation, registration and paging gates pass. Preserved-PMM incoming and physical-dial outgoing calls complete through physical Navi Answer/End and carry bidirectional GSM-FR over NHM-6's independently evidenced DSP/PCM contracts. Cold preserved-PMM A5/1 promotion organically reauthenticates before ciphered SDCCH and traffic media rather than inheriting a process-local key. Same-/different-LAC reselection, replacement-cell paging, preserved location and save-state pass through NHM-6's own radio grammar. Ordinary MT SMS proves exact SIM storage, CP/RP closure and RR release after physical PMM provisioning; a cold physical route retains the entered RTC/date, completes phone-code entry and acknowledgement, then renders the exact Inbox text, marks EF_SMS read and durably erases it. Its physical predictive-text composer independently completes a GSM 7-bit SMS-SUBMIT and localized success UI. Preserved-PMM sequential two-part Smart Message transport closes organically through CP/RP and clean RR release; physical Save writes only product-local PMM flash, a cold process lists `Test for Dhiram` at Tones 5-4-1, and Options -> Play emits the reconstructed note stream through PUP. The acquired virgin PMM's stale four-byte phone-code verifier is mapped exactly; a fail-closed derived fixture changes only that record and proves physical `12345` at first boot and in Settings, while the canonical ROM remains unmodified. FACCH, degradation, SACCH and active-call replay pass in the established A5/0 composition. Nokia's combined NHM-2/5/6 repair path and matching COBBA schematic establish MIC2/EAR connectivity; isolated incoming- and outgoing-call gates validate both host-audio directions with neutral gains. |
| Nokia 3410 NHM-2 v5.46E | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Fresh/preserved-PMM registration, paging and assigned-SDCCH replay pass. Incoming and outgoing calls prove physical Send/End, assignment and media lifecycles. Independent A5/1 promotion observes NHM-2's distinct 10-byte nonzero type-`0x14` publications, ciphered xCCH/traffic bursts and non-silent media; secret payloads are redacted. Idle mobility proves same-/different-LAC reselection, replacement-cell paging, loss/recovery, persistent loss, preserved location and replay without inheriting Nokia packet grammar. Ordinary MT SMS independently proves preserved-PMM notification, ordinary Inbox listing, exact text, SIM read status, physical erase confirmation and durable deletion. Its physical composer additionally proves Profile 1 EF_SMSP ownership, GSM 7-bit SMS-SUBMIT, success UI and the shared CP/RP/RR lifecycle. Sequential two-part Smart Message transport independently closes through SAPI-3 CP/RP and RR release; its application additionally proves a three-slot/Replace Save flow, product-local PMM-backed flash ownership, ordinary named listing and payload-correlated PUP replay after preserved-NVRAM cold boot. Its typed 1 MHz/8 kHz 13-in-16 PCM profile sustains bidirectional GSM-FR. Nokia's combined NHM-2/5/6 repair path and the matching COBBA schematic establish fitted MIC2/EAR topology; isolated incoming- and outgoing-call gates prove both host-audio directions with neutral gains. |
| Nokia 5110 NSE-1 v5.30 | Yes | Yes | No | No | No | No | Partial | A clean-room C54x core executes the recovered ROM4 program/DROM and firmware uploads without DSP-output assists. Generated 24C16 provisioning passes the organic COBBA measurement validator; exact menu and save-state gates exercise the physical serial keypad. A ROM-consistent special-column-4 power input distinguishes short press from sustained CCONT rail-off. The cold-entry IMR and absolute CTSI compare model activate INT0 organically; the short boot requires thirteen ordered port-`0x32` writes with queue-producer evidence. The fixed 30-second RF gate checks 20,280 frame expiries, 648,032 reads and 1,537 writes. Scan return, frame-period restoration and FIR queue consumption have separate passive evidence. These observations do not establish RF acquisition or handset speech. All 626 cold-idle opcode words are independently fixture-asserted; Menu and power runs introduce no additional words. An MCU type-`0x1a` search-list packet is observed; the seven later type-`0x51` packets are DSP memory uploads, not radio configuration. Port-`0x27` sample ownership/encoding and MAD2 SCU tuning remain open; see `rom4_dsp_loader.md` for the measured lifecycle and timing scope. |
| Nokia 2100 NAM-2 v5.84 PPM E | Partial | Yes (bounded donor fixture) | No | No | No | No | No | `verify-2100-frontier` proves reset, 16-Mbit flash execution, RAM/MAD2 access, product-local GENSIO/CCONT and display contracts, a 64-exchange DSP bootstrap, service discovery and application registration. `verify-2100-mbus` separately proves the physical single-wire request, sampled-line occupancy, one-shot 423.1 Hz terminal event, request-derived transport ACKs and complete organic terminal `D0/04` -> phone `D0/05` exchange. The sibling v5.21 image's distinct 992-pair DSP handoff and verdict publication are separately protected. `verify-2100-interactive` preserves the v5.84 executable, attaches only the v5.21 PMM tail, cleans stale snapshots, enters physical `12345` and pins the organic `Wrong code!` result. Both cipher calls use the same digits but invalid identity/FAID strings change during the boot; a matching PMM/full-flash capture is required before acceptance, idle or registration can be promoted. |
| Nokia 2100 NAM-2 v5.21 Sharp-labelled full image | Partial | Yes (security editor) | No | No | No | No | No | The source-hash-pinned own-PMM checksum fixture changes two bytes, without a donor mix. The explicit `nam2c3` supported-part trial preserves its EEPROM catalogue across parameter erases, physically enters `12345` and renders Code accepted; a restart from persisted NVRAM reproduces checksum and startup release. Its C3 user-OTP read plane defaults erased, with factory words undumped. The original `noki2100` uniform-block composition instead loses records and rejects the code. Usable idle, late input, fitted-part identity and phone services remain unproved. See `2100_bringup.md`. |
| Nokia 3610 NAM-1 v5.11 PPM E | Partial | Yes (service application + terminal) | No | No | No | No | No | Runtime evidence independently establishes NAM-1's byte-write-driven CCONT receive-ready contract, BLB-2 inputs, later-MAD2 SIMI composition and nine-bank 96x65 reversed-segment display. Focused gates protect the complete `CONTACT SERVICE` frame, DSP bootstrap/service control, request-derived discovery, application registration, channel-`0x5f` use and the complete physical M2BUS terminal startup exchange. The supplied MCU/PPM package omits the PMM at `0x5f0000`; two product-calibration checks clear service-status bit 6 and exclude the second application-task batch, leaving SIMI, keypad and radio dormant. A foreign NHM-6 PMM proves the logical loader and first checksum but fails the product-specific second check, so a matching NAM-1 PMM is required. |
| Nokia 5210 NSM-5 v5.40 PPM E | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Product-owned GENSIO, compact DSP/service completion, BLB-2 CCONT inputs, reversed-segment display, keypad and ROM6 candidate-window radio contracts reach registered standby. Ordinary and authenticated Location Updating update EF_LOCI, release SDCCH and render `DCT3 LAB`; exact gates cover menu, Messages, return navigation, idle save/load and active-call save/load followed by physical teardown. IMSI paging and ordinary MT SMS complete through persistent storage and physical reading; wrong-group, unmatched and malformed pages are rejected without RR access. The physical predictive-text composer independently completes SMS-SUBMIT, CP/RP closure and `Message sent`. The product-specific Names route also writes `Ada`/`123` to SIM `EF_ADN` and renders it after a cold reload. Physical `*123#` and `*#21#` independently prove USSD and call-divert interrogation over GSM 04.80. A Phase-2+ card independently proves the product's nine-byte TERMINAL PROFILE, proactive DISPLAY TEXT and successful physical dismissal. Incoming and physical-dial outgoing calls additionally prove caller/called-number presentation, Answer/End, CC/RR traffic assignment and release, product-owned speech-control decoding and registered idle. A focused A5/1 gate proves organic cipher control plus encrypted SDCCH and traffic bursts through the incoming-call lifecycle. Same/different-LAC reselection, serving/all-cell loss, RF recovery and paging after reselection are also validated on the NSM-5 86/87 carrier pair. Product-specific short/long power handling and charger-originated CCONT restart are validated at physical/UI/device boundaries. Its evidenced 1 MHz/8 kHz 13-in-16 PCM bus and MIC2/EAR routes pass isolated incoming- and outgoing-call physical-duplex gates; analogue gains remain neutral rather than guessed. Phone-local storage additionally passes a physical empty-memory control, Ada/123 Save and independent cold Search/Details with unchanged complete SIM and predecessor storage; a separate four-process gate edits the selected number to124 and cold-reads Ada/124 while preserving the original123 acceptance. Cancellation and confirmed erasure additionally pass cold checks of the retained contact or empty Phone memory, with the storage selector independently inspected. Physical Add and separate cold selections also preserve A/123 alongside Ada/123 with unchanged SIM storage. Confirmed physical erasure of A additionally preserves Ada/123 across a cold boot. Capacity, arbitrary record enumeration and other handset-local writes remain unvalidated. |
| Nokia 6110 NSE-3 v4.06 PPM B (ROM3 candidate) | No | No | No | No | No | No | No | `verify-6110-static` proves the declared hardware boundary, SIM surface, DSPIF rings, sparse-flash verification transport and product-local radio/service envelopes. Matching F711604 internal ROMs, EEPROM and the DSP-owned final verification publication remain absent. |
| Nokia 6110 NSE-3 v5.48 PPM B (ROM3) | No | No | No | No | No | No | No | `verify-6110-v548-static` proves the distinct ROM3 bootstrap, `3/3` pre-upload identity, 64 transfers, final `0x0b06` first result and ROM3 EEPROM record locations. The final DSP-owned publication, internal ROMs and matching EEPROM remain absent. |
| Nokia 6110 NSE-3 v05.48 PPM B (ROM4) | No | No | No | No | No | No | No | The same gate proves a separately relocated ROM4 loader, different staged stream and different EEPROM record locations. Its pre-upload identity, final DSP publication, internal ROMs and matching EEPROM remain absent; ROM3 values are not inherited. |
| Nokia 8210 NSM-3 v5.31 PPM C (ROM6) | Research HLE | Yes | Yes | Yes | Yes (HLE) | Yes (HLE) | No | Native boot stops at missing resident `2c75`; explicit HLE uses only the declared unchanged PMM base-record comparison, not the invalid journal. Physical security/menu, calculator, cold SIM contact persistence, registration/EF_LOCI, call signaling and SMS pass. GSM900 and early-PIN DCS independently verify host incoming/outgoing signaling, all four host SMS outcomes, contact cold readback and idle/call/SMS restoration with exact architecture, replay, pixels and physical continuation. No-PIN DCS has separate service/restore gates. Eight-second late-PIN DCS independently proves request-derived lab timing failure, organic registration, bidirectional host call/SMS signaling, idle/call/SMS restoration, cold phonebook and two-process SMSS reference advancement. A fixed7/8/10/12/20-second physical-PIN matrix independently qualifies registration only, not all input timings or media. Own documented PCM/MIC2/EAR contracts support independently verified incoming/outgoing local SIP media and physical audio waveforms; all four connected/alerting incoming/outgoing SIP restoration scenarios discard external dialogs without replay. Native DSP speech, analogue gains and factory provisioning remain unproved. Contracts and gates: `8210_bringup.md`. |
| Nokia 6210 NPE-3 v5.56 PPM C | Partial | Yes (research HLE) | Yes (laboratory) | Yes (signaling) | No | No | No | Normal execution retains its fail-closed native DSP boundary; own verifier/loaders reach missing `2c75` before explicit `npe3hle` substitution. Unchanged acquired PMM supports physical Menu/SIM PIN, calculator, persistent SIM contacts, registration/EF_LOCI, incoming/outgoing calls and SMS. Idle/call/SMS restoration and product-local unattached-accessory input are verified. Host incoming calls independently prove caller presentation, physical Answer/End, own CC/RR grammar and registered idle recovery. Named host-SMS gates additionally prove received text and SIM storage, physical submission, RP-error rejection and full firmware-timeout recovery with physical continuation. Native DSP, measured verdicts, speech and electrical analogue units remain incomplete. See `6210_bringup.md`. |
| Nokia 7110 NSE-5 v5.01 PPM C | Partial | No | No | No | No | No | No | Product-local ownership acknowledgements complete 228 sparse-flash handoffs; the final DSP publication remains fail-closed. Its distinct 210-word verifier calls routines matching the recovered ROM4 ABI. Separate whole-backup research BIOSes501d-full/500-full preserve each acquired4MiB image and its own storage. The v5.01 MCU is byte-identical to the existing code and advances beyond the old structural identity rejection to an actual0d00 self-test consumer; result-table item12 and a low-record checksum mismatch still exclude the second task batch and render a failure frame. The intact v5.00 trial remains blank at native913e/9140. Its own live low-record checksum matches, and an independently decoded initializer/delegated builder posts0000,010a into the native serial restart. Six-second port/helper/call-edge evidence qualifies that pending boundary, not physical baud, clock, completion or usable UI. Neither trial promotes normal support, fitted-mask identity, UI, RF or speech. See `7110_bringup.md`. |
| Nokia 8250 v5.02 PPM K | Partial | No | No | No | No | No | No | Normal execution reaches 58 alternating DSP ownership pairs and the final-result wait. Native staged execution verifies own uploads before missing resident routine `2c75`. The declared runtime HLE comparison receives the exact acquired PMM record but its candidate decode fails the own-ROM structural validator. Selector ownership and codec-direction audits do not resolve original chip pairing or ROM6 preprocessing. Graphical and downstream phone acceptance remain blocked on matched evidence; no donor provisioning or success verdict is installed. See `8xxx_bringup.md`. |
| Nokia 8850 v5.31 C | Partial | Yes (research) | Yes (research) | Yes (research) | Yes (HLE) | Yes (HLE) | No | Own native uploads reach absent mask routine `2c75`; separate `nsm2hle` verifies physical security/menu, calculator, persistent SIM contacts, own radio registration/EF_LOCI, incoming/outgoing CC/RR signaling and SMS read/submission. Idle, active-call and delivered-SMS restoration pass exact architecture, protocol replay, pixels and post-load physical input. Host incoming calls and bidirectional SMS, including distinct rejection/timeout screens and physical recovery, pass independently. Own PCM and MIC2/EAR documentation plus ROM-pinned speech-control decoding support incoming/outgoing real local SIP HLE media and physical waveforms in both call directions. All four connected/pending incoming/outgoing SIP restoration scenarios clear external dialogs without replay; native DSP speech remains unverified and normal execution remains at the DSP-result poll. Contracts and acceptance recipes: `8xxx_bringup.md`. |
| Nokia 8890 v12.20 C | Research HLE | Yes | Yes | Yes | Yes (HLE) | Yes (HLE) | Partial | Conservative normal machine unchanged; `nsb6hle` executes own loaders before missing resident2c75 and explicit HLE handoff. Own PMM supports physical security/menu, persistent SIM contacts, calculator, clock/calendar and laboratory GSM900/PCS1900 registration, calls and SMS with restoration gates. Own documented512kHz/8kHz,64-clock,13-in-16 PCM, command8/bit0200 and MIC2/EAR support independent incoming/outgoing SIP media and sustained microphone/earpiece waveforms on both bands with neutral gains. All four SIP call-restoration phases independently match37 ARM/banked registers, RAM checksum and time on both bands: connected media precedes save; unanswered external media remains zero; old dialogs clear without replay. Any local unanswered PCM is bounded to post-load teardown, not CONNECT. Native DSP speech, measured analogue gains and factory provisioning remain unproved. Contracts and gates: `8xxx_bringup.md`. |
| Nokia 6250 v5.03 PPM C | Partial | Yes (research PMM comparison) | Yes (research) | Yes (research) | No | No | No | Normal machine remains fail-closed at `429842`. Own uploads plus declared runtime HLE and an explicit initial-record PMM comparison validate physical calculator, persistent SIM contacts, fresh/preserved registration, call signaling and SMS. Idle, active-call and delivered-SMS restoration pass exact architecture/protocol/pixel replay and physical continuation. The unattached accessory-input polarity is schematic-backed and runtime-verified. Host incoming calls and bidirectional SMS include independently checked rejection/timeout recovery. The acquired journal is inconsistent; native mask execution and PCM/audio remain unresolved. These gates do not promote the normal machine. See `6250_bringup.md`. |
| Nokia 5510 NPM-5 v3.53 PPM C | Partial | Diagnostic only | No | No | No | No | No | `nmp5stage` executes its own verifier and loaders to missing mask `2c75`, retaining ownership without runtime HLE. Separate `nmp5hle` reaches `CONTACT SERVICE` through request-derived discovery and own compact self-test consumption, not idle or phone-service parity. No MU4 keys are mapped and no donor PMM is supplied. See `5510_bringup.md`. |
| Nokia 8810 NSE-6 v6.02 PPM A | Partial | Diagnostic only | No | No | No | No | No | Normal `nse6stage` waits for its own DSP-verifier acknowledgement. Separate `nse6r4t` uses own MCU flash with an unproved NSE-1 ROM4 mask and an explicitly synthetic integrity/retry EEPROM fixture. Documented compatibility experiments establish physical security entry, Menu / Names and persistent SIM phonebook save/recall, but do not constitute current full-native handset acceptance. Native control reconfiguration waits on DSP I/O port `0x36` bit `0x4000`. The nominal ADC and keypad cause-register contracts resolve the earlier missing-report/input prerequisites, not the serial completion. Fitted-mask compatibility, stable native radio, registration, calls and SMS remain unproved. See `8810_bringup.md`. |
| Nokia 3350 NHM-9 v5.30 PPM A | Partial | No | No | No | No | No | No | Fail-closed `noki3350` executes its own MCU, creates catalogue headers from erased flash and installs the exact 247-word DSP program. The passive boundary checker validates own release arguments, catalogue relocation and the first unacknowledged transfer through ten seconds. Separate `nhm9stage` executes the own verifier up to its explicit missing resident-code CALL at `861f`. Own key tables and 18 contacts/90 controller scans plus Power pass conformance, not organic UI delivery. Matching internal masks remain `NO_DUMP`; no DSP completion or service peer is supplied. The physical display viewport remains unvalidated. See `3350_bringup.md`. |
| Nokia 5130 NSK-1 v5.61 PPM A | Partial (native compatibility; late reset) | Physical code/menu/contacts and offline SMS | No | No | No | No | No | Explicit `nsk1r4t` uses own firmware and acquired NSE-1 native DSP input, not a proved fitted mask. External record/checksum inputs and the nominal ADC2 hypothesis reach readiness and organic SIM initialization. Physical 12345, contact save/cold-read/delete/cold-empty, wrong-PIN/cold-correct-PIN, PIN exhaustion/PUK/new-PIN and persisted wrong-PUK recovery pass. Ordinary PIN2 change, retry persistence, organic PIN2 exhaustion, cold PUK2 recovery/authentication and permanent PUK2 rejection with responsive cold navigation also pass. An explicit extended card proves physical FDN On/Off, cold state and SIM identity rehabilitation; protected FDN save/edit/erase/cancel, cold number/empty Search and wrong-PIN2 preservation pass; local listed/unlisted/Off dial-permission controls also pass without connected-call acceptance. A separate twelve-process gate physically exhausts all ten PUK retries and verifies exact cold SIM-card rejection with unchanged storage. Offline SMS draft save/cold-read/delete/cold-empty also pass. Physical manual network selection starts Searching and genuine server exchanges, but discovers no operator. The native scan consumes samples without a qualifying result through90s; empty DSP packet-liveness supervision produces the independently observed93.974s reboot. No donor identity/calibration, injected packets or watchdog bypass is used. Registration, network SMS, calls, speech, long-run stability and physical RF/mask compatibility remain unproved. `2915` stops after its first loader exchange. See `5130_bringup.md`. |
| Other declared DCT3 products | No | No | No | No | No | No | No | Driver declarations and acquired service packages are not executable evidence. Extraction, matching storage inputs and product-specific contracts must be established before promotion. |
| Nokia 5190 NSB-1 v6.71 PPM A/B | Partial (graphical comparison) | Physical code/menu/storage | No | No | No | No | No | Normal `noki5190` retains the silent-DSP boundary. Separate `nsb1r4t` uses unproved NSE-1 mask compatibility, own version selection/63 uploads and a bounded90s native/menu gate. Six checksum bytes, nominal ADC2=554 and an own retry-byte fixture permit graphical startup and physical phone-code/menu input, not factory provisioning. Independent cold gates cover SIM phonebook save/read, PIN/PUK and PIN2/PUK2 recovery, request settings, ordinary credential changes, and PIN2/PUK2 retry/exhaustion controls. Explicit FDN On/Off, cold identity rehabilitation, PIN2-protected contact save/edit/erase/cancel/cold view and rejected-write preservation pass; Local listed/unlisted/Off dial permission controls pass without registration or connected-call acceptance. PPM B with a synthetic English preference proves offline SMS draft save/cold read/delete, not network SMS. A Phase-2+ card separately proves the own five-byte terminal profile, organic DISPLAY TEXT FETCH and physical successful dismissal, with a matched Phase-2 no-proactive control and unchanged persistent card. That Toolkit fixture retains Field test01; a separate physical00 selection and independent cold boot qualify readable Headset idle pixels, exact one-byte diagnostic persistence and unchanged original startup text/card. Fitted mask/cadence, ADC wiring/calibration, other SIM security, handset-local storage, registration, calls and speech remain unproved. See `5190_bringup.md`. |
| Nokia 6150 NSM-1 v5.23 PPM C | Partial (graphical diagnostic) | Physical menus/SIM security/storage/local FDN | No | No | No | No | No | `verify-6150-static` pins own inputs; separate `nsm1r4t` declares an unproved NSE-1 native-mask comparison. Own diagnostic record validation, explicit nominal ADC2 and a single own-initializer phone-retry byte permit graphical boot and physical phone-code acceptance, not factory calibration or normal-machine promotion. Independent cold runs prove SIM contact save/read, distinct Phone D/6789 save/read with an unchanged SIM control and code-only boot-write contrast, separate Phone-memory erase, PIN1/PIN2 changes and retry persistence, PIN/PUK and PIN2/PUK2 recovery, PIN-request Off/On, and authenticated offline SMS save/read/cancel/erase. Physical PUK2 exhaustion additionally proves permanent loss across complete predecessor-card cold links, independent PIN1/phone authentication, unchanged card and responsive security menus. Separate physical PIN1/PUK1 exhaustion proves permanent SIM-card rejection across another cold boot and physical input control without credential or unrelated card mutation. An explicit extended card proves physical PIN2-protected FDN activation/cold-state/deactivation, rejected activation with cold retry recovery, record save/edit/erase/cold read and local listed/unlisted/Off dial controls. Exact complete-card, APDU, physical-input and frame gates distinguish these software results from radio support. A separate40s native idle save/load observes all128KiB of backed research ARM RAM, restores37 ARM registers/36 DSP fields and memory checksums, repeats7771 DSP I/O writes over2.7s and reproduces physical Messages navigation with unchanged complete EEPROM/card/flash. Template timeout policy03ca=00 remains explicit; bounded restoration is not exhaustive device-state or sustained native acceptance. Native registration, connected calls, network SMS, speech and fitted-mask/clock/electrical fidelity remain unproved. See `6150_bringup.md`. |

The5130's separate physical Field test00 selection and authentication-only
cold boot now qualify a clean Headset-titled idle. Its own0299 selector
changesff to3d; all127 selection-run EEPROM changes are protected by the
complete bank hash, unchanged on cold boot. Card/flash/input configuration
are unchanged. This does not resolve its native93.974s supervision reset,
accessory polarity, registration or speech.

Its separate40s native save/load also restores37 ARM registers,36 DSP fields
and64KiB research RAM checksums while the DSP is active. The2.7s branches
repeat1170 DSP I/O writes and physical Phone book pixels with unchanged
complete storage. This is bounded restoration, not long-run acceptance.

The5130's separate Phase-2+ Toolkit probe reaches FETCH but automatically
refuses DISPLAY TEXT with result30. Its matched Phase-2 control preserves
EEPROM; the Phase-2+ probe rewrites108 bytes of unused empty-digit-field
padding. Passive input/write observations and the length-driven decoder
explain those bytes, not general storage integrity or Toolkit display support.
The complete transition remains checked. Paired lifecycle traces show both
profiles execute the same accepted-code record updates; only stale padding
differs, not whether firmware writes the records.

The7110 v5.01 checksum-consistency fixture is separate from its intact-input
row: changing only the latest journal checksum byte releases the second task
batch but still leaves a white raster and native913e/9140. Identity-check
branch3af64e runs twice; this is not authentic provisioning or usable boot.

The5190's separate clean-idle save/load qualifies37 ARM registers,36 DSP
fields,64KiB research RAM checksums and7764 identical DSP I/O writes over2.7s.
Physical Menu repeats the exact Phone book pixels with unchanged complete
EEPROM/card/flash. This does not promote fitted-mask, RF or sustained operation.

The5190's own clean-idle bank separately passes an external profile3 sequence:
DISPLAY TEXT, physical GET INKEY5 and physical GET INPUT42, followed by its
reviewed clean idle pixels. Complete EEPROM/card/flash remain unchanged;
the original Field test01 DISPLAY TEXT/control gates remain distinct.
This does not promote native registration or fitted-mask compatibility.
A profile4 menu also returns distinct accepted physical Continue/Exit envelopes
and clean item-list pixels after ACK-only selection, unlike the6150's Wait.
Profile7 separately follows Continue with SELECT ITEM; physical Beta selection
returns the exact accepted response and restores the original installed list.
A separate pending DISPLAY TEXT save/load repeats two physical completions
after one FETCH, restores observed architecture/64KiB research-RAM checksums
and1170 DSP I/O writes, and returns identical idle pixels with unchanged storage.

The6150 also independently passes one card-originated Toolkit DISPLAY TEXT
with own terminal profile0b41ff7700, exact prompt/idle pixels and successful
response after physical dismissal. Its matched Phase-2 control has no proactive
traffic and opens Messages on that same contact. A separate profile3 run
also returns physical GET INKEY5 and GET INPUT42, with reviewed empty/filled
editor pixels and clean idle return. Profile4 independently installs DCT3 menu;
physical Continue/Exit produce distinct accepted item-1/item-2 envelopes.
ACK-only selection leaves Wait, so automatic menu return is not qualified.
Separate profile7 responds with SELECT ITEM; physical Beta selection returns
the exact accepted result and restores the clean installed list.
Pending DISPLAY TEXT also survives native save/load: one FETCH, two physical
successful completions, exact observed architecture/checksum restoration,
1170 identical DSP I/O writes and equal returned-idle pixels.
Whole storage is unchanged across these fixtures; other Toolkit commands
and radio support are not qualified.

The6150 native165s repeated-scan result is explicitly template-policy-dependent:
record0607 reads EEPROM03ca=00 from the unchanged repair template, suppressing
packet-timeout reset selection. It is not evidence of factory policy or RF
acquisition. The6130's separately measured reason68 restart remains a distinct
product/fixture result; neither deadline nor policy is inherited across ROMs.

NSB-7's separate host-USSD notification gate additionally proves settled
PIN/phone-authenticated PCS600 delivery, exact `Host notice` physical
readback, End-to-idle recovery and complete unchanged SIM contents. This is
one-way notification acceptance. A separate physical `*123#` gate verifies a
host-decided `Host network` response, rejection of an unrelated request ID,
RR release before physical End and exact idle/menu recovery with unchanged
card. Pending `*123#` restoration additionally proves exact CPU/RAM/time,
same-request republication, stale-epoch/wrong-ID rejection, retained RTC and
physical response/idle/menu recovery. Its branches intentionally differ in
host input; equal protocol replay is not claimed. One continued laboratory
dialogue additionally proves physical Answer, numeric reply, correlated final
result and idle/menu recovery with unchanged card; it does not promote
host-controlled multi-turn USSD. A separate network-originated laboratory
request proves physical Read/Answer/2/Send, correlated release and exact idle/
menu; pending-editor restoration reproduces architecture, time and protocol.
An explicit WebSocket host request also delivers the unchanged typed reply
after RR release, with matching identity/epoch and unchanged card. That host
request's pending editor additionally restores exact architecture and protocol,
with one host response after each branch's RR release under distinct epochs.
Native radio and speech remain unproved; see
`nsb7_host_ussd_check.py` and `8290_bringup.md`.

## Promotion rules

NSM-2 additionally qualifies a two-process PIN1 retry lifecycle through
`run_noki8850_pin_retry_cold.py`: physical9999 rejection consumes exactly one
retry without registration, complete predecessor NVRAM is inherited by an
independent cold process, and physical1234 restores retries and reaches the
existing registered physical-menu acceptance. Exact whole-card comparisons
and the reviewed Code error frame are required. This is not PIN exhaustion,
PUK/PIN2 acceptance or native DSP/speech.
Its independently executed `--puk` variant additionally qualifies three
physical wrong-PIN attempts, durable blocking, cold PUK1/new-PIN recovery
and another cold authentication with2468. Whole-card equality and own
boot/HLE/UI/registration checks remain mandatory. The separate
`--puk --wrong-puk-first` four-process chain additionally qualifies one
physical wrong-PUK rejection, persistent10-to9 PUK allowance, correct recovery
from that cold state and new-PIN cold authentication. Permanent PUK exhaustion
is independently qualified by `--puk --exhaust-puk`: ten cold physical
wrong-PUK transactions consume all allowances, and a final cold process keeps
the complete card unchanged, emits no credential APDU or registration, and
returns to the reviewed SIM card rejected frame after physical input/End.
PIN2 credential change and cold replacement-credential use are independently
qualified by `run_noki8850_pin2_change.py`: physical5678-to2468 change,
complete-NVRAM cold boot, then2468-to5678 change, exact card/editor/result
checks and responsive Messages menu. The independent `--reject-first` chain
also qualifies one physical wrong-old-PIN2 CHANGE, exactly one persistent
retry consumed without changing the PIN, cold recovery with the unchanged
credential and subsequent cold replacement-credential use. Its separate
five-process `--puk2` chain qualifies three cold rejected changes, durable
PIN2 blocking, cold physical PUK2/new-PIN2 recovery with the firmware's
post-UNBLOCK VERIFY, and cold replacement-PIN2 use. Whole-card/UI/registration
checks remain mandatory. Its independent `--puk2 --wrong-puk2-first` chain
adds one cold wrong-PUK2 rejection, exactly one consumed PUK2 allowance with
the old PIN2 retained, no post-failure VERIFY, then correct cold recovery and
replacement-credential use. The independently executed
`--puk2 --exhaust-puk2` chain qualifies all ten physical PUK2 rejections and
cold permanent PIN2-code refusal while PIN1 and registration remain valid.
The final rejected-entry/digit/End/Menu control preserves the entire card
and sends no PIN2 credential APDU or call/SMS transaction. The independent
four-process `run_noki8850_fdn.py` qualifies physical PIN2-protected On/Off,
cold selected-state persistence, exact ADN-validity exchanges and unchanged
empty FDN records, with complete-card and own boot/UI/registration checks.
Its independently executed `--records` branch additionally qualifies physical
PIN2-protected A/12345 Save to EF_FDN record1 and a complete-card-unchanged
cold Search/Details, with exact own-product UI and wire/storage checks.
Its independent ten-process `--dial-controls` chain qualifies listed12345
connected/released, unlisted12346 refused before call establishment while
FDN is On, and the same unlisted number connected/released after physical
PIN2-protected Off. Identical saved-card branches, post-Send FDN lookup,
complete-card equality, own CC/RR/UI checks and explicit isolated lab peer
configuration are required. The independent twelve-process
`--record-lifecycle` chain adds PIN2-protected12345-to12346 editing/cold
Details, identical erase-dialog cancellation with cold retention, and
confirmed record1 erasure with cold No phone numbers. Complete-card and
own protocol/UI/registration checks remain mandatory. Capacity, arbitrary
prefix matching and speech remain unpromoted.

NPE-3 additionally has an independent handset-bank A/123 save/cold-read
fixture with an empty-bank negative control and unchanged complete SIM files;
see `run_noki6210_phone_memory.py`. Its `--lifecycle` variant adds a physical
numeric edit, cancellation and confirmed erase, each with independent cold
readback and unchanged SIM storage. Capacity and native DSP are not promoted;
the separate one-contact copying acceptance is below. Its five-process
`--rename` fixture cold-reads D / 123
after physically renaming A, retaining the number and unchanged SIM storage.

NHM-3 separately qualifies Phone-bank A/123 save/cold retrieval against an
empty control, with unchanged SIM storage and its own PMM comparison and
upload/HLE checks. Its independent `--product 6250 --lifecycle` chain also
qualifies numeric edit, erase cancellation and confirmed erase through cold
readback, with product-owned frame/timing checks rather than inherited6210
acceptance.

NPE-3 separately qualifies one physical Phone-to-SIM **Keep original** copy
through `run_noki6210_phone_memory_copy.py`. Independent cold branches
explicitly select Phone and SIM using distinct selection-screen oracles,
then read the same A/123 contact. Only the expected EF_ADN record may change
in the complete card, and neither cold readback may write it. Its independent
`--move` variant additionally proves **Erase original**, the distinct Moved
frame, cold empty Phone bank and retained cold SIM contact. The separate
`run_noki6210_phone_memory_roundtrip.py` then copies that firmware-created
SIM source back into the empty Phone bank, preserving the complete SIM and
reading both banks in independent cold branches. Bulk copying, merging and
capacity remain unpromoted; NHM-3 does not inherit this acceptance.
The independently run `--product 6250 --rename` fixture also cold-reads
D / 123 after physical name editing, with its own boot/provisioning checks
and product-qualified save frame. Neither rename gate establishes arbitrary
name encodings, capacity or multi-number contacts.

Both products independently pass the seven-process `--numbers` variant:
physical addition of789 to A / 123, cold navigation of123/789/A, selective
confirmed deletion of789, and cold navigation of123/A/123. Exact frames,
unchanged complete SIM storage and product boot/provisioning checks remain
mandatory. This qualifies one two-number contact, not phonebook capacity or
arbitrary field types.

`verify-6210-pin-retry` additionally proves physical9999 rejection with one
consumed PIN1 retry, physical1234 recovery/reset, delayed coherent laboratory
registration and exact error/idle/Menu frames. Whole-card comparison admits
only the documented location update. `verify-6210-pin-retry-cold` independently
preserves one failed retry through a separate process, then physically
authenticates, resets retries and recovers registered Menu. Full predecessor,
cold input and final cards are checked. `verify-6210-puk-recovery` adds three
physical PIN failures, exact blocked-card cold continuation, physical PUK/new
PIN/confirmation and a third-process VERIFY with the installed PIN, retained
location, exact idle/Menu and unchanged card. `verify-6210-puk-retry-cold`
adds a fourth process: one physical wrong PUK consumes exactly one attempt
without changing credentials, and the retained failed-PUK card cold-recovers
normally. `verify-6210-puk-permanent` adds all ten physically consumed PUK
attempts through separate cold processes, exact permanent-card preservation,
cold blocked presentation and a live input control that cannot reopen the
credential path. `verify-6210-pin2-change` independently proves physical
current/new/confirmation input, CHANGE CHV2 success, exact settings/success/Menu
frames and complete card preservation except PIN2/LOCI.
`verify-6210-pin2-change-cold` then cold-authenticates the installed PIN2 and
physically changes it back, with retained location, independent launch/storage
provenance and unchanged predecessor NVRAM. `verify-6210-pin2-retry-cold`
proves Code error/9804 for wrong current PIN2, exactly one durable PIN2 retry
consumed, unchanged credentials and cold recovery with the original PIN2.
`verify-6210-pin2-exhaustion` independently consumes all three PIN2 retries in
cold-linked processes, checks the PIN2 code blocked screen and preserves PIN1
registration/menu access. `verify-6210-puk2-recovery` adds physical PUK2 recovery,
firmware's automatic replacement-PIN2 VERIFY and cold use of that PIN2 to
change back, with exact card preservation and30 frames across five processes.
`verify-6210-puk2-retry-cold` adds a physical wrong-PUK2 rejection with exactly
one durable attempt consumed, unchanged credentials, cold recovery and cold
replacement-PIN2 use across six processes. `verify-6210-puk2-permanent` proves
all ten physically consumed PUK2 attempts, exact permanent-card preservation
and a responsive cold control that cannot reopen PIN2 recovery, while PIN1
registration/menu access remains usable. `verify-6210-fdn-cycle` adds physical
PIN2-protected On/Off, cold-state persistence and the identity/ADN validity
handshake on an explicit extended card, with exact complete-card preservation
and12 reviewed frames. `verify-6210-fdn-retry-cold` additionally proves wrong
PIN2 activation denial with unchanged FDN validity/credentials, durable retry
loss and cold correct-PIN2 activation through the complete lifecycle.
`verify-6210-fdn-records` adds physical PIN2-protected A/12345 record save and
exact cold Details retrieval, with complete-card preservation and12 reviewed
frames across three processes. `verify-6210-fdn-record-edit` extends this to a
PIN2-protected12345->12346 edit and exact cold Details, preserving the rest of
the card across five processes. `verify-6210-fdn-record-erase` additionally proves
PIN2-protected erase cancellation, confirmed record deletion and cold empty-list
display, with exact card preservation and22 frames across six processes.
`verify-6210-fdn-record-retry` additionally proves wrong-PIN2 Add denial with no
UPDATE, durable retry loss, cold correct-PIN2 save and cold Details, preserving
all unrelated card data. `verify-6210-fdn-dial-controls` proves listed-number
HLE call signaling, local unlisted-number denial and successful signaling for
that same unlisted number after physical Off. Exact card preservation, decoded
inputs and23 frames pass across seven processes. These are exact-number lab
controls, not native speech, physical RF or external end-to-end calls; emergency,
prefix and special-dial-string behavior remains outside their scope.

`verify-6210-host-ussd-notify` independently proves host-originated Host notice
after PIN authentication and settled registration, firmware-owned supplementary
service acknowledgement/RR release, physical Back/Menu recovery and three
reviewed frames. Complete card contents match normal registration.
`verify-6210-host-ussd-request` separately proves network `Choose 2`, physical
Answer/2/Send, exact firmware ReturnResult and correlated packed reply received
by the host after RR release, with six own-product frames and unchanged card.
`verify-6210-host-ussd-restore` additionally proves exact ARM/RAM restoration
of the pending editor and a single physical reply under a new host epoch,
without another admission or REGISTER. `verify-6210-ussd-continued` independently
qualifies physical `*123#`, one correlated network continuation, physical2/Send,
final result/RR release and six own-product frames with unchanged card contents.
Empty replies, arbitrary dialogue length and host-controlled continuation
remain unqualified.

`verify-5210-sim-pin-registration` independently proves early physical
`1234`/OK, successful CHV1 verification before laboratory registration,
reviewed masked-code/menu/idle frames and physical recovery.
`verify-5210-late-pin-registration` additionally proves delayed physical
entry, own-carrier background measurements and full Location Updating on a
coherent ARFCN 86/87 laboratory topology, with registered idle/menu/recovery
frames. `verify-5210-pin-incoming-call` extends that configuration through
laboratory paging, reviewed caller pixels, physical Answer/End, full CC/RR
closure and exact idle recovery. `verify-5210-pin-incoming-sms` independently
adds segmented delivery, CP/RP/RR closure, persistent read-status SIM contents
and physical Read with the existing exact `hello` frame. These gates establish
HLE signaling and durable SIM/UI interaction, not native speech.
`verify-5210-pin-idle-state` adds exact time/PC/SP/RAM restoration, identical
registered-idle pixels and physical post-load Menu/End. `verify-5210-pin-call-state`
adds exact answered-call restoration before physical End and complete release,
with restored reference pixels and pinned registered-idle recovery; native
speech and host media restoration are not established by it.
`verify-5210-pin-sms-state` adds exact delivered-but-unread restoration,
identical notification pixels and physical post-load Read, with the existing
message-body oracle and persistent SIM read status.
`verify-5210-pin-outgoing-call` adds physical dialling, exact called-party
SETUP, connection and physical End release with pinned idle recovery. It is
laboratory HLE signaling. `verify-5210-pin-outgoing-sms` adds physical `a`
composition for `5551234`, exact one-character SMS-SUBMIT, CP/RP/RR closure
and reviewed composer/`Message sent` pixels.
`verify-5210-pin-outgoing-call-state` independently restores an outgoing
connected call before physical End, requiring exact time/PC/SP/RAM and
reference/restored pixels, complete release and the idle oracle.
`verify-5210-pin-host-outgoing-sms` independently adds correlated loopback
host acceptance to the physical composer, exact SMS-SUBMIT and CP/RP/RR
closure. `verify-5210-pin-host-incoming-sms` independently adds correlated
host delivery on ARFCN 86, CP/RP closure, physical Read, exact body pixels and
persistent read-status SIM storage without automatic laboratory SMS.
`verify-5210-pin-host-rejected-sms` independently verifies correlated RP-error,
no success RP-ACK, closure, exact failure/idle/menu pixels and physical recovery.
`verify-5210-pin-host-silent-sms` independently observes CP-ACK without RP
result, firmware-owned timed DISC, UA/release and host termination, with
reviewed timeout and physical idle/menu recovery pixels.
`verify-5210-pin-host-outgoing-call` independently adds correlated host connect,
wrong-id/duplicate rejection, physical End, CC/RR release and exact idle pixels.
`verify-5210-pin-host-incoming-call` independently adds host paging after
registered idle, correlated lifecycle states, reviewed caller pixels,
physical Answer/End, CC/RR closure and exact idle recovery.
`verify-5210-pin-sms-wait-state` independently restores exact architecture
and pixels during a local-peer RP wait, then requires timed DISC, UA/release,
paging and physical idle/menu recovery. Whole-interval reference/replay,
external SMS transaction restoration remain separate.
`verify-5210-pin-host-outgoing-call-state` restores a physically dialed,
PIN-authenticated host-connected call with exact PC/SP/RAM and pixel equality.
The backend remains attached through save/load; the adapter republishes the
connected call in epoch 2 with `clients=1`, rejects a stale epoch-1 decision,
then reaches ended after physical End and firmware CC/RR release. The ordinary
host-connect mock, which disconnects before save/load, cannot prove this
contract. This gate covers restored host signaling, not surviving or cleaned-up
SIP dialogs, host media cursor restoration or native speech.
`verify-5210-pin-host-incoming-call-state` independently adds readiness-gated
host paging, physical Answer, exact active-call architecture/pixel restoration,
epoch-2 connected republication without repeated paging, stale-epoch termination
rejection and physical End/CC/RR closure. The backend stays attached through
the restored ended state. This is host signaling restoration, not media or
external SIP-dialog cleanup.
`verify-5210-pin-host-incoming-media-state` independently adds incoming HLE
frame echo across the same authenticated lifecycle. Its dedicated received
adapter-state JSON pins the epoch-2 downlink cursor; the gate requires stale
epoch-1 media rejection and 80 contiguous accepted frames from that cursor
before physical End. Exact restored architecture/pixels and CC/RR teardown
remain required. External SIP-dialog cleanup and native speech remain separate.
`verify-5210-pin-outgoing-sip-state` independently verifies cold PIN acceptance,
physical dialing, one confirmed real outgoing SIP dialog and exact handset
architecture restoration. Epoch change closes the external dialog with BYE
and clears the restored GSM call once under epoch 2, without redial. Physical
continuation reaches the exact registered recovery frame. This gate proves
external-dialog cleanup, not dialog survival, native speech or identical
active-call pixels after intentional external teardown; the separate local
and mock-host gates retain their active-call pixel-equality checks.
`verify-5210-pin-incoming-sip-state` independently applies the same external
cleanup policy to readiness-gated incoming SIP, physical Answer and connected
save/load. One confirmed dialog closes with BYE on epoch change; the restored
GSM call clears once without repeated SIP paging. PIN acceptance, exact
architecture restoration and physical recovery pixels are checked separately
from the real SIP/CC/RR evidence. Neither direction restores a live SIP dialog
or establishes native speech.
`verify-5210-pin-host-outgoing-media-state` independently adds a continuously
attached host echo backend, restored GSM-FR cursors, rejection of stale
epoch-1 media and at least 80 contiguous accepted epoch-2 downlink frames.
It retains exact architecture/pixel restoration and physical End followed by
CC/RR release, ended state and paging. The fixture allows three additional
seconds before End to exercise sustained restored transport. This is HLE
frame transport, not decoded speech, a microphone/earpiece waveform test or
SIP-dialog restoration.
`verify-5210-pin-outgoing-call-sip` independently combines cold PIN
authentication, physical dialing and End, real loopback SIP confirmation,
bidirectional host media transport, ordered DISCONNECT/RELEASE/RELEASE COMPLETE
and exact idle recovery. This is research-HLE signaling and host media transport,
not microphone/earpiece waveform fidelity or native DSP speech.
`verify-5210-pin-incoming-call-sip` independently combines cold PIN
authentication, registered-idle-gated SIP paging, physical Answer/End,
confirmed dialog and bidirectional host media transport, ordered handset
DISCONNECT/network RELEASE/handset RELEASE COMPLETE, correlated ended state
and exact caller/idle recovery pixels. The checker keeps this physical-End
contract separate from remote-ended fixtures; negative tests reject missing
release steps and incorrect Answer keys. This remains research HLE, not
microphone/earpiece waveform fidelity or native DSP speech.
`verify-5210-pin-phonebook` independently adds physical `Ada/123` Save,
exact ADN ownership, cold reauthentication and Search/Detail with retained
record contents and reviewed number pixels; the cold read cannot rewrite ADN.
`verify-5210-pin-ussd` independently combines an own fresh PIN-protected card,
physical PIN acceptance, coherent NSM-5 registration and physical `*123#`/Send.
It requires the exact GSM 04.80 request, correlated success response and RR
release before dismissal, the existing product-specific USSD result pixels,
and physical End returning to the exact registered-idle frame. This is
laboratory HLE supplementary-service evidence, not native DSP execution or
public-network connectivity.
The isolated 5210 runner writes `invocation.json` before launching firmware:
resolved command/cwd, actual executable and primary fixture/checker hashes,
selected loose BIOS inputs, and initial config/NVRAM hashes. Missing loose
inputs are explicit, not silently treated as hashed ROM evidence. The manifest
does not prove source-to-binary correspondence or capture every Python/Lua
dependency; the runtime checkers remain the behavioral acceptance evidence.
`verify-5210-late-pin-boundary` independently proves an organic background
measurement request, request-correlated reply and its own forty-record
consumer before accepted delayed PIN; no random access follows within 45 s.
This negative control deliberately uses mismatched ARFCN 1/2; it does not
establish a missing firmware continuation or DSP acknowledgement. Correcting
the external topology resolves the delayed-PIN registration boundary.

`verify-sim-toolkit-interactive-5210` independently extends NSM-5's Phase-2+
DISPLAY TEXT coverage with physical GET INKEY `5`/GET INPUT `42`, successful
terminal responses and reviewed entered/registered-idle pixels. Its INKEY
editor requires an additional physical OK after the digit. This gate does
not promote card-menu/network Toolkit services or native DSP execution.

`verify-3410-radio-a5-1-degraded` independently proves NHM-2 encrypted
incoming-call resilience: organic authentication/cipher completion, own
physical Answer/End lifecycle, bidirectional impaired traffic bursts,
bad-frame detection, handset/network concealment and non-silent recovery.
The same gate independently requires organic FACCH stealing in both
directions, BFI concealment and media recovery, plus four-phase SACCH/TF
rotation while bidirectional speech continues.
It uses NHM-2's evidenced 1 MHz/8 kHz, 125-clock, 16-bit PCM contract and
external network fault selectors, not firmware state forcing. This gate
does not by itself establish native DSP speech or physical host duplex under impairment.
`verify-3410-radio-a5-1-degraded-state` separately saves the active encrypted
call at 18.51 seconds, compares two-second reference/restored digital media
records exactly, and requires post-load physical End plus clean release.
It also independently requires cipher, degraded-media, FACCH and SACCH
checks. The ordinary physical input plan finishes before save/load; the
replay coroutine owns the later End key, explicitly selecting NHM-2 `end`
rather than the historical NSE-8 `enter` default. Lua coroutines are not
emulator-saved state, so an outstanding ordinary input wait must not be used
to terminate the comparison interval. This is HLE deterministic replay,
not native DSP execution or proof of every possible active-call save point.

`verify-3410-radio-a5-1-degraded-physical-duplex` independently adds real host
microphone/speaker streams under the same encrypted bidirectional impairment.
Temporary isolated PulseAudio routes feed a non-clipping microphone stimulus
and capture decoded speaker output; the gate requires sustained non-silent
uplink at the network decoder, a sustained recorded 1 kHz downlink, own
physical Answer/End, ciphering, error/concealment/recovery, FACCH and SACCH.
The physical call holds for eleven seconds to include a peer observation
after the final impaired block (peer logs are sampled every 50 exchanges).
This proves the modeled MIC2/EAR host boundary with neutral gains, not native
DSP speech, measured silicon gain or a real GSM network call.

`verify-3410-radio-a5-1-outgoing-degraded` independently covers physical
dialing of `5551234`, organic A5/1, the same external burst faults, FACCH/BFI
recovery, SACCH coexistence and clean mobile-originated release to PCH.
The fresh gate observes 550 microphone and 514 decoded earpiece frames,
68 impaired bursts per direction and 32 downlink impairment-induced bad
blocks. Network concealment also occurs at startup/FACCH boundaries; this
does not establish that the configured uplink corruption defeats decoding.
The recovery checker excludes bad ring blocks after the explicit firmware
speech-stop event: the release tail cannot recover media already stopped by
End. An unrecovered uplink bad frame before speech-stop still fails.
Outgoing impaired host duplex and active-call restoration are not promoted
by this internal-media gate, nor is native DSP speech.

`verify-6250-calendar-cold` independently proves physical NHM-3 Calendar
time/date entry, the reviewed 7 October 2026 Wednesday frame, and the same
frame in a separate process retaining only its own saved NVRAM/configuration.
CCONT retains 13:47 and both processes register; the cold phase uses the
persisted LAI/TMSI contract and supplies no replacement time/date input.
This remains the declared initial-record PMM research comparison, not normal
machine support, offline calendar advance or native DSP/audio acceptance.
`verify-6250-alarm` uses that own clock/storage seed for physical Clock 10-2
arming, natural 13:48 RTC expiry/acknowledgement, buzzer programming and Stop
back to reviewed registered idle. It independently checks its own status
bytes and four product-local frames. `verify-6250-alarm-off-no` and
`verify-6250-alarm-off-yes` additionally prove RTC-only wake from rail-off,
Stop to the activation question, physical No back to off, and physical Yes
to preserved registration, with separate uploaded-stage checks and exact
frames. `verify-6250-alarm-snooze` proves physical awake Snooze, firmware
re-arm to 13:53, natural second expiry and Stop with two additional exact
frames. `verify-6250-alarm-off-restore` and `-restore-yes` additionally
check exact 37-register ARM/RAM/time restoration and off-rail RTC replay,
then natural deadline wake and the respective physical activation choice.
`verify-6250-alarm-off-snooze-no` additionally proves two natural RTC wakes,
physical Snooze/Stop/No, silent intervening rail-off intervals, independent
verifier/uploads on each wake and exact recurrence/final-off frames.
`verify-6250-alarm-off-snooze-yes` independently proves physical activation
and preserved registration after the same two alarm-only wakes.
`verify-6250-alarm-off-snooze-restore` additionally proves exact architecture
and off-rail RTC replay from the initial countdown, followed by the complete
two-wake Snooze/Stop/No continuation.
`verify-6250-alarm-snooze-countdown-restore` separately proves exact state
and RTC replay during the later Snooze-induced rail-off, then natural
recurrence and final Stop/No. `verify-6250-alarm-awake-restore` separately
snapshots the sounding awake alarm at second 65, restores all 37 ARM/banked
registers, SRAM and time exactly, and reproduces the full architecture, RTC
ticks, timed buzzer programming and nonblank 96x60 pixels after a 1.25-second
replay. Physical Stop then silences the buzzer and returns to the reviewed
registered-idle frame. It uses a fresh own-product clock seed and the declared
research PMM comparison, not RTC/firmware injection or audible/native speech.
`verify-6250-alarm-snooze-countdown-restore-yes` independently adds physical
Yes after the restored later countdown, firmware-requested software reset,
separately checked own verifier/uploads and renewed registered idle.
Alarm-editor restoration and audible/native media remain unproved; the
initial off-countdown Snooze/Yes combination is not covered by this gate.
`verify-6250-alarm-cold` additionally proves own physically saved alarm
reconstruction in a new process, natural expiry and Stop, with independent
uploaded-stage/registration checks and exact retained-alarm frames. It does
not establish offline RTC advancement.

`verify-8890-alarm` independently proves physical Settings 4-1 alarm entry
on NSB-6, own `13:48` CCONT programming, natural expiry, alarm-cause `b3`
consumption and `a0` acknowledgement, buzzer control and physical Stop back
to reviewed registered idle. Its German 84x48 frames are separately reviewed;
the seed is a fresh verified physical clock-entry fixture from the same
product. This does not promote Snooze, powered-off alarm wake or audible
output and does not alter the normal/native-DSP boundary.
`verify-8890-alarm-snooze` independently adds physical right-softkey Snooze,
the own observed 13:54 deadline, natural recurrent cause/acknowledgement,
renewed buzzer control and physical Stop with reviewed recurrence/idle
frames. Its deadline is not inherited from the 6210 fixture. Powered-off
wake and audible output remain outside both 8890 alarm gates.
`verify-8890-code-change` separately proves the phone's physical old/new/
confirmation workflow, firmware-owned persistent storage and independent
cold acceptance of the changed code. `verify-8890-alarm-wake-security`
uses only that own saved state to prove physical power-off, RTC-only wake,
security acceptance, alarm presentation and Stop to registered idle.
Both boot phases independently pass upload/HLE and registration checks.
There is no demonstrated 6210-style activation-choice prompt or native/audio
promotion in this path.
`verify-8890-alarm-wake-restore` additionally proves exact powered-off
ARM/banked-register, SRAM and time restoration, matching RTC replay and blank
off frames, followed by the same natural wake/security/Stop acceptance.

`verify-6210-alarm` independently proves physical Menu-4-1 alarm entry,
CCONT programming, naturally timed expiry, firmware alarm presentation and
physical Stop returning to registered idle. Its seed is a fresh physically
entered Calendar/time fixture, not donor storage or register injection.
Reviewed frames and ordered hardware/input checks cover alarm-on, active
alarm and post-Stop idle; buzzer programming is checked but audible output
is not promoted. `verify-6210-alarm-off-no` and `-off-yes` separately prove
autonomous RTC rail wake, physical Stop/activation choice and either return
to rail-off or firmware-owned warm restart to the exact registered idle
frame. Warm SRAM retention and distinct cold/software reset status are
verified at firmware consumers; native DSP and silicon timing are not inferred.
`verify-6210-alarm-off-restore` additionally proves exact 37-register ARM,
banked-state, SRAM and time
restoration during the powered-off countdown, matching RTC replay and blank
frames, then natural deadline wake and physical Stop/No. It injects no alarm
cause or clock value and does not establish audible output.
The paired `verify-6210-alarm-off-restore-yes` proves physical Yes, the
firmware-owned warm restart, independently checked upload/self-test phases
and exact registered idle after the same restored countdown.
`verify-6210-alarm-snooze` additionally proves physical Snooze, firmware's
five-minute reprogramming, natural recurrence, IRQ acknowledgement, renewed
buzzer control and physical Stop. Exact frames additionally cover `Snooze
active` and repeated `Alarm! 13:53` in its independently observed visible
phase; no render override is used. `verify-6210-alarm-cold`
independently proves physical arming followed by retained-register reads,
natural expiry and Stop in a new process with no clock/alarm entry. Its
armed-idle, active-alarm and stopped frames are reviewed and reproducible.
Seconds restart at zero; offline elapsed time is not inferred.

`verify-6210-alarm-off-snooze-no` combines the independently validated
powered-off and Snooze lifecycles: two natural RTC wakes, separately checked
alarm-only uploads, silent endpoint intervals while powered off, physical
Stop, the reviewed activation question, and physical No returning to a blank
powered-off display. Its recurrence input waits for the second alarm boot;
scripted key labels alone are not evidence that the intended UI consumed them.
This remains research HLE, not native DSP or audible-output acceptance.
The paired `verify-6210-alarm-off-snooze-yes` independently requires
firmware-owned software restart after physical Yes, a separately validated
normal upload/self-test phase, registration and exact registered-idle pixels.
`verify-6210-alarm-snooze-countdown-restore` separately saves at second 100
in the Snooze-induced off interval, checks exact CPU/banked state, SRAM,
time and silent RTC/frame replay, then validates recurrence and physical No.

`verify-6250-calendar-midnight` proves own-product physical 23:59/date entry,
natural midnight, firmware day-counter consumption/reset, reviewed 8 October
pixels and a separate cold boot retaining that date without replacement input.
It validates research HLE, not native DSP or offline elapsed time.

`verify-6210-calendar-cold` independently proves physical Menu-8 Calendar
entry, firmware-owned CCONT programming to 13:47, physical date entry and
the reviewed 7 October 2026 Wednesday pixels. A separate process retaining
only that handset's generated NVRAM reproduces the date without replacement
input, retains hour/minute and completes retained-location registration.
This remains research HLE and does not prove offline elapsed time, midnight
rollover, leap-year boundaries or native DSP behavior.
`verify-6210-calendar-midnight` separately enters 23:59 physically and lets
the emulated RTC cross midnight. Physical Back makes firmware consume and
rebase the hardware day increment; reopened Calendar shows 8 October 2026,
Thursday. Another cold process retains that date and laboratory registration.
This covers one ordinary-day transition, not leap/year boundaries or
automatic redraw of an already-open Calendar window.
`verify-6210-calendar-leap-day` and `verify-6210-calendar-year-end` separately
exercise physical 28 February 2024 -> 29 February 2024 and 31 December 2026
-> 1 January 2027. Each checks both date frames, organic midnight/day
consumption and a retained cold Calendar with registration. Non-leap
February is independently covered by `verify-6210-calendar-nonleap`: physical
28 February 2023 -> 1 March 2023, exact reviewed before/after pixels, hardware
day consumption and retained cold Calendar with registration. Century rules
and other untested calendar boundaries remain outside these cases.

The 6210 research-HLE pending-outgoing and alerting-incoming SIP restore
gates independently prove exact architectural restoration, external-dialog
clearing without replay, complete signaling release and registered-idle
recovery. They prohibit CONNECT and accepted media and do not establish
answered SIP calls, connected restoration or native speech.

`verify-8890-calendar-rollover`, `verify-8890-calendar-leap-day` and
`verify-8890-calendar-year-end`
independently prove physical time/date entry, RTC day-increment consumption,
own flash-journal persistence and cold Calendar presentation of 8 October
2026, 29 February 2024 and 1 January 2027, respectively. All require laboratory
registration and exact date pixels. Offline elapsed time and full
calendar-boundary coverage are not inferred from these three cases.
`verify-8890-calendar-non-leap` independently covers physical 28 February
2025 -> 1 March 2025, the observed day-scalar pair and cold registered
Calendar with its own reviewed frame. Leap-day exit and century rules remain
unproved.

`verify-8210-dcs-registration` independently establishes no-PIN research-HLE
DCS1800 acquisition, Location Updating, release, carrier-823 paging and
persistent SIM location. A separate `--pin-start 7` control passes physical
PIN and full DCS registration; the default eight-second entry still fails.
This is a bounded ordering result, not timing-independent authentication
coverage. DCS native speech remains unproved; GSM900 gates do not establish it.
`verify-8210-dcs-idle-state` adds no-PIN registered-idle restoration:
exact CPU/RAM snapshots, transport replay, identical restored pixels and
physical Menu continuation, with DCS registration and EF_LOCI checked
separately. It does not admit the unresolved late-PIN profile.
`verify-8210-dcs-incoming-sms` validates no-PIN DCS paging, SMS transport,
delivery/read-status SIM writes, persisted content and physical reading
with reviewed message-body pixels. This is research-HLE, not native DSP.
`verify-8210-dcs-sms-state` additionally validates delivered-SMS restoration,
exact architectural/transport/pixel replay and physical post-load reading.
`verify-8210-dcs-outgoing-sms` verifies physical text/destination entry,
exact SMS-SUBMIT, laboratory acceptance, CP/RP closure, resumed paging
and reviewed success pixels, with DCS registration checked independently.
`verify-8210-dcs-outgoing-call` proves physical outgoing call signaling and
release using exact observed carrier-823 traffic/release words and reviewed
UI. It does not prove native speech or an external end-to-end call.
`verify-8210-dcs-call-state` adds active-call save/load with exact
architecture, transport and pixel replay, then physical End/release.
`verify-8210-dcs-incoming-call` independently proves paging, physical
Answer/End, exact DCS CC/RR channel/release words, caller text and returned
idle pixels. DCS host/backend and PIN service coverage are not inferred.
`verify-8210-dcs-phonebook` covers physical SIM contact creation, cold
reboot with the own provisioning/DCS topology, persisted record readback
and reviewed contact pixels.

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
| 8210 v5.31 research HLE | Yes | Yes | Yes | Yes, both directions |
| 8850 v5.31 research HLE | Yes | Yes | Yes | Yes, both directions |

Media transport requires sustained ordered handset acceptance, not merely
bridge transmission counts; bounded queue drops remain reported. Restore gates
clear the external dialog and the restored GSM call under a new epoch, rather
than claiming to restore SIP. Unanswered scenarios reject Answer/CONNECT and
accepted media. Named gates, protocol details and waveform limits are maintained
in `external_call_bridge.md`. Signaling-only research products have not inherited
this media promotion.

SIP failure coverage is separate: busy (486) and unavailable (480) are verified
for 3210, 3310, 3330, 3410 and 5210. Separate research-profile gates cover
8210 and 8850; their promotion is not inherited from the first five. The 3310 fixtures cover
two organic attempts; the 3410 and 5210 fixtures observe one attempt and no
redial in the 45-second run. The 3330 fixtures independently observe one ended
attempt and no redial in the 40-second call run after fresh physical PMM setup. Checkers
require every observed host request to end and reject unhandled retry attempts
or accepted media.

Fresh incoming SIP after an idle save/load is independently verified for
3210, 3310, 3330, 3410 and 5210. The new epoch admits exactly one new call, physical
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

The 8890 additionally passes `verify-8890-cold-clock` for physical user-clock
entry and cold retained-clock validation, and `verify-8890-power-cycle`: physical shutdown,
blank LCD, six seconds of continuous CCONT RTC ticks with DSP/radio
transport silence while CPU power is off, separate PWRONX restart,
both own native-upload/HLE handoffs and decoded security input back to idle.
Its second registration carries retained LAI and closes without redundant
EF_LOCI writes, checked against a distinct strict grammar. Firmware itself
resets seconds after wake. The separate cold-clock acceptance uses physical
13:47 entry and a new process preserving own NVRAM: the original firmware
restores its clock record, validates the retained CCONT domain and reaches
reviewed 13:47 idle after security entry without time/date input. See
`tools/noki8890_cold_clock_check.py` and the reproduction in `8xxx_bringup.md`.
Neither test proves offline calendar advance, battery-removal behavior,
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
are not established by this unanswered gate. Separate 8850 incoming/outgoing
media and waveform gates establish HLE speech transport; native DSP speech
remains unproved.

`verify-8850-sip-idle-restore` separately verifies exact idle architectural
state, byte-identical replayed LCD pixels and transport/SIM replay before
a new epoch-2 incoming SIP call and the same cancellation/physical cleanup.
It does not restore an external dialog or establish media by itself. The
separate connected/alerting incoming and connected/pending outgoing restore
gates validate invalidation and one fresh-epoch GSM release, not dialog replay.

The 8210 acquired base-record research comparison independently passes
`verify-8210-sim-toolkit`: own nine-byte profile, STATUS/FETCH DISPLAY TEXT,
physical successful dismissal, exact 84x48 text/numeric-PLMN idle frames,
own staged predicates and persisted laboratory registration. Its interactive
gates also cover GET INKEY and GET INPUT. `verify-8210-sim-toolkit-menu`
adds card-owned SET UP MENU, physical menu selection, the accepted item-1
ENVELOPE and reviewed entry/list pixels. No PIN/DCS Toolkit combination,
other menu items or native DSP is promoted.
`verify-8210-sim-toolkit-menu-state` additionally restores the open menu and
replays physical selection with identical architectural snapshots, card
protocol records and reviewed pixels.
`verify-8210-sim-toolkit-sms` additionally completes the card-owned SEND
SHORT MESSAGE through the handset's CP/RP/RR lifecycle, with one decoded
`SAT` submission, accepted terminal response and physical return to idle.
Delivery outside the laboratory network is not established.
`verify-8210-sim-toolkit-call` independently covers card-owned SET UP CALL,
physical consent before SETUP, assigned/connected signaling, successful
terminal response, physical release and reviewed call/idle frames. It does
not validate speech.
The independent `verify-8210-sim-toolkit-call-decline` gate requires physical
refusal, the user-rejection terminal response and absence of call signaling.

The 8850 research-HLE profile independently passes `verify-8850-sim-toolkit`:
its own nine-byte TERMINAL PROFILE, STATUS/FETCH DISPLAY TEXT, physical OK,
successful TERMINAL RESPONSE, reviewed 84x48 result/registered-idle frames
and persisted EF_LOCI. `verify-8850-sim-toolkit-interactive` additionally
accepts card-owned GET INKEY and GET INPUT: physical digit 5 plus OK and
digits 4/2 plus OK produce the expected terminal responses, with reviewed
prompt and registered-idle pixels. This uses unchanged own MCU/PMM and the
Phase 2+ laboratory card. `verify-8850-sim-toolkit-menu` additionally proves
card-owned SET UP MENU, physical menu navigation and accepted item-1 ENVELOPE,
with reviewed entry/list/result pixels and physical End recovery to numeric
`001 01` registered idle. Selection remains on the item list; it does not
prove a menu-triggered application or network service.
`verify-8850-sim-toolkit-menu-state` saves the open item list, restores
identical CPU registers, RAM checksum and emulated time, then repeats physical
selection with the same accepted SIM ENVELOPE and reviewed pixels. It does
not establish restoration during an in-flight APDU or native DSP execution.
`verify-8850-sim-toolkit-sms` separately proves card-requested SEND SHORT
MESSAGE to `5551234`, one decoded SMS-SUBMIT, CP/RP acknowledgements,
successful TERMINAL RESPONSE and RR release, followed by physical idle
recovery. This remains laboratory HLE signaling, not native or public-network
SMS delivery. `verify-8850-sim-toolkit-call` separately proves card-owned
SET UP CALL, physical consent before SETUP, traffic assignment and CONNECT,
successful TERMINAL RESPONSE, then physical End and complete CC/RR release
to idle. Reviewed consent and active-call pixels do not establish speech.
`verify-8850-sim-toolkit-call-decline` independently requires physical Cancel,
user-rejected TERMINAL RESPONSE (`22`), no outgoing request, SETUP,
assignment or connection, then physical End to registered idle.
Other proactive
commands and native DSP execution are not established by these gates.
`verify-8890-sim-toolkit-busy` independently verifies nine-byte profile
download, STATUS/FETCH and the explicit `20 01` screen-busy terminal response
during physical date entry, followed by successful date-editor recovery and
persisted laboratory registration. It does not prove displayed proactive
text or successful physical dismissal on 8890.

`verify-8890-sim-toolkit` separately proves retained-clock DISPLAY TEXT,
physical dismissal and exact own idle recovery. Its interactive extension
`verify-8890-sim-toolkit-interactive` independently proves GET INKEY and GET
INPUT, exact `5`/`42` terminal replies and five own frames. NSB-6 requires
an additional physical OK after the INKEY digit; the checker enforces that
product difference. Both gates seed the clock physically from own PMM and
require retained registration/SIM location. `verify-8890-sim-toolkit-menu`
additionally proves SET UP MENU, physical navigation and item-1 selection,
exact ENVELOPE and four own localized menu/recovery frames.
`verify-8890-sim-toolkit-menu-state` independently restores the open list
with exact CPU/RAM/time and repeated physical selection, accepted ENVELOPE
and identical localized pixels. The retained-clock seed and NSB-6's extra
INKEY confirmation remain product-specific; in-flight APDU restoration and
native DSP behavior are not promoted.
`verify-8890-sim-toolkit-sms` additionally proves card-requested SMS through
exact laboratory CP/RP submission/acknowledgements, terminal success and RR
release, with physical idle recovery. `verify-8890-sim-toolkit-call` additionally
proves physical Call consent and complete laboratory CC/RR setup/release;
`verify-8890-sim-toolkit-call-decline` proves physical Cancel, result `22`
and no call establishment. Own localized frames and retained registration
are required. Native DSP speech and end-to-end audio remain unpromoted.

The 6210 additionally passes `verify-6210-sim-toolkit-interactive`, proving
physical `5`/`42` GET INKEY/GET INPUT replies, exact successful text TLVs,
five independently reviewed frames and own registration/storage checks.
`verify-6210-sim-toolkit-menu` separately proves SET UP MENU installation,
physical ordinary-menu navigation, successful item-1 ENVELOPE and physical
End to registered idle, with four additional own frames. It does not promote
menu-triggered network services by itself. `verify-6210-sim-toolkit-sms`
independently proves physical item selection, card-owned SEND SHORT MESSAGE,
one exact `SAT` SMS to `5551234`, CP/RP success, successful terminal response,
RR release and physical return to reviewed idle. External SMS delivery and
native DSP remain unpromoted.
`verify-6210-sim-toolkit-call` separately proves card-owned SET UP CALL,
physical consent, exact destination `5551234`, complete laboratory CC/RR
setup and release, successful terminal response and reviewed idle recovery.
Speech and external calls are not established by this signaling gate.
`verify-6210-sim-toolkit-call-decline` independently proves physical Cancel,
card result `22`, no GSM call establishment and reviewed menu/idle recovery.
The 6210 research-HLE profile independently passes `verify-6210-sim-toolkit`:
its own nine-byte TERMINAL PROFILE, card-owned STATUS/FETCH DISPLAY TEXT,
physical successful clearance and exact registered-idle recovery. Other
proactive commands and native DSP are not promoted by this gate.

`verify-6210-ussd` and `verify-6210-call-divert` independently establish physical
supplementary requests, correlated laboratory responses, RR release, reviewed
result/idle pixels and EF_LOCI persistence. `verify-6210-call-divert-lifecycle`
extends forwarding through register/active-query/deactivate/inactive-query;
`verify-6210-state-divert` additionally restores exact CPU/RAM/time and observed
protocol replay before a fresh physical active query.
`verify-6210-call-divert-cold` separately proves network-owned subscription
NVRAM across independent processes: physical registration, retained active
interrogation and host forwarding without paging, plus a fresh-storage inactive
control. It uses unchanged own provisioning and separately reviewed idle pixels.
These gates do not establish
native DSP, speech or displayed forwarding-number content.

`verify-6250-pin-retry` independently proves physical9999 rejection, PIN1
retry loss, physical1234 recovery/reset and coherent registered idle/Menu.
Two CHV1 selectors, exact contacts/protocol order, complete card contents and
three own-product RGB frames pass. `verify-6250-pin-retry-cold` separately
preserves the failed allowance across two independent processes, requires
complete predecessor/cold-input/final card transitions and byte-identical
inherited NVRAM, then physically restores retries and registered menus. All
predecessor artifacts remain unchanged. `verify-6250-puk-recovery` adds physical
PIN1 exhaustion, cold PUK/new-PIN/confirmation and a third-process new-PIN
authentication with preserved-location registration. Exact card transitions,
CHV selectors, physical contacts and11 stage frames pass.
`verify-6250-wrong-puk-recovery` separately proves one wrong PUK's durable
10->9 retry loss without installing its proposed PIN, cold correct-PUK
recovery and cold new-PIN authentication across four processes and15 exact
frames. `verify-6250-permanent-puk-loss` independently consumes all ten PUK1
allowances after physical PIN1 exhaustion, preserves the complete card through
twelve cold processes and validates48 own-product frames. The final card stays
at zero allowances with no credential APDU or service attempt from the tested
physical sequence; its actual UI is SIM blocked/unlabelled numeric entry, not
an inherited sibling rejection screen. `verify-6250-pin2-change` separately
proves ordinary physical5678->8765 CHANGE CHV2, unchanged allowances and exact
complete-card preservation, with eleven own route/editor/result/idle/Menu
frames. Its Settings security item needs four Down presses, not the6210's
five. `verify-6250-pin2-change-cold` additionally inherits the complete installed
card/state in a separate process and accepts8765 as the current PIN2 in another
successful physical change to2468. All22 frames, retained-location registration,
complete card transitions and unchanged predecessor artifacts pass. This is
credential acceptance via CHANGE CHV2, not standalone VERIFY.
`verify-6250-pin2-retry-cold` separately proves wrong-current-PIN2 rejection,
no installation of the proposed value, durable3->2 retry loss, cold PIN1
authentication with PIN2 retries still2 and a successful installed-PIN2 change
resetting retries3. All33 frames and complete card/predecessor checks pass.
`verify-6250-pin2-exhaustion` separately consumes all three PIN2 attempts via
physical Settings changes across four processes, with9804/9804/9840 results,
44 exact frames and complete-card preservation apart from retry loss. PIN1
authentication, coherent registration and physical Menu remain usable; both
PUK allowances stay10.
`verify-6250-fdn-cycle` independently proves physical FDN On/cold-On/Off/cold-Off
on an explicit extended lab card. PIN2 gates ADN invalidation/rehabilitation;
cold active-FDN boots perform their own identity-file rehabilitation. All four
processes authenticate/register, read ten FDN records and preserve complete
card contents except expected location/validity transitions. Thirty reviewed
frames and full NVRAM/predecessor checks pass. That validity-only gate does
not qualify record writes or calls; separate gates below do.
`verify-6250-fdn-record-cold` additionally proves PIN2-protected physical Save
of A/12345 to EF_FDN record1 and cold Search/Details without another PIN2
verification or write. Its record branch inherits the organically activated
On-phase NVRAM; full-card checks permit only six encoded record bytes to change
and preserve the cold card exactly. Six processes and42 reviewed frames include
the complete validity regression.
`verify-6250-fdn-record-lifecycle` further qualifies PIN2-protected Edit,
cold12346 Details, canceled erase with no UPDATE, confirmed erase of only
record1 and cold empty Search with FDN still On. Eleven processes and70 reviewed
frames include both smaller gate paths and preserve complete credentials,
retry allowances and unrelated records.
`verify-6250-fdn-rejections` separately rejects activation and protected Add
with physical wrong PIN2, preserves validity/records, consumes exactly one
PIN2 attempt and cold-recovers from each complete two-attempt predecessor.
Correct activation/Save reset allowances, and final cold Details retrieves
A/12345. Eight processes and56 reviewed frames pass with full source/storage
and immutable predecessor checks. This covers activation/Add rejection, not
every editor.
`verify-6250-fdn-dial-controls` independently proves listed12345 reaches the
complete outgoing lab CC/RR lifecycle, unlisted12346 with FDN On is locally
rejected without call-control setup, and physical PIN2-protected Off allows
12346 while preserving the saved contact. Ten processes and60 reviewed frames
compose validity/Save/cold Details with complete card/source/predecessor checks.
Dial cases explicitly use the private automatic lab peer (`CALLHOST=0`), not
SIP or external RF. Native speech and exhaustive number-format coverage remain
unproved.
`verify-6250-pin-request` independently qualifies physical PIN-request Off/On
and final cold PIN authentication in three processes with25 reviewed frames.
The disabled card registers without startup VERIFY; a physical ENABLE CHV1
restores the cold Enter PIN code prompt. Complete card comparisons permit only
the expected initial registration and enable-flag changes, preserving all
credentials and retry allowances. Native speech remains unproved.
`verify-6250-pin-change-cold` separately qualifies ordinary physical
CHANGE CHV1 (1234->2468), then cold authentication with2468, registration and
Messages. Thirteen own frames and full card/NVRAM/predecessor checks pass;
PIN2, both PUKs and retry allowances remain unchanged. This is not a
wrong-current-PIN rejection qualification.
`verify-6250-puk2-recovery` extends the PIN2-exhaustion chain to six processes: a blocked-card
cold boot authenticates PIN1/registers, physically enters PUK2/newPIN2 and sends
one successful UNBLOCK CHV2 followed by firmware VERIFY of the new value. Another
cold process accepts that value in a physical Settings change. All66 frames,
complete card transitions and immutable predecessor checks pass.
`verify-6250-wrong-puk2-recovery` adds physical wrong-PUK2 rejection with no
replacement installation/verification, durable10->9 PUK2 retry loss, correct
cold recovery and another cold new-PIN2 change. All77 frames and complete
card/predecessor checks pass across seven processes.
`verify-6250-puk2-permanent` consumes all ten PUK2 allowances through physical
transactions, ending with9840/PIN2 code rejected. A subsequent cold boot retains
the entire zero-allowance card while PIN1 authentication, laboratory registration,
registered idle and Messages Menu still work. The cold PIN2 selection is an
unlabelled notice with no credential APDU, not a qualified editor. All162 frames
and complete storage/predecessor checks pass across fifteen processes. No
factory-provisioning or native-speech promotion follows.

`verify-6250-ussd` separately establishes the physical USSD exchange and reviewed
NHM-3 result/idle frames on its declared derived-PMM research comparison. The
default-cell signaling gate is not an ARFCN19/20 coherence promotion.
`verify-6250-call-divert` separately verifies inactive interrogation and
physical result dismissal; it does not inherit the 6210 forwarding lifecycle.
The separate `verify-6250-call-divert-lifecycle` validates its own physical
register/query/deactivate/query session and reviewed frames; restoration
is independently tested by `verify-6250-state-divert`: registration and
activation precede save, exact architecture/protocol/pixels replay, and a
fresh physical query after load proves active forwarding before deactivation.
`verify-6250-call-divert-cold` additionally validates cold-process forwarding
persistence: physical registration, retained fresh-process active interrogation
and host call routing without paging, followed by an independent fresh-storage
inactive-query control. The normal machine and native speech remain unpromoted.
`verify-6250-sim-toolkit` independently verifies its nine-byte TERMINAL
PROFILE, card-owned DISPLAY TEXT, physical OK, successful TERMINAL RESPONSE
and reviewed idle recovery. `verify-6250-sim-toolkit-interactive` separately
proves card-owned GET INKEY and GET INPUT, physical `5` and `42`, exact
successful terminal-response text TLVs and five own frames through idle.
`verify-6250-sim-toolkit-menu` additionally proves card-owned SET UP MENU,
physical navigation and item-1 selection, exact firmware ENVELOPE and four
own menu/recovery frames with independently checked registration/storage.
`verify-6250-sim-toolkit-sms` additionally validates card-requested SEND SHORT
MESSAGE through exact CP/RP submission and acknowledgements, successful
TERMINAL RESPONSE, RR release and physical idle recovery. This is laboratory
SMS signaling. `verify-6250-sim-toolkit-call` additionally proves physical
Call consent, exact card completion and full laboratory CC/RR setup/release.
`verify-6250-sim-toolkit-call-decline` independently proves physical Cancel,
card result `22`, no GSM call establishment, and menu/idle recovery.
Neither gate promotes native speech or end-to-end audio.

`verify-8890-minute-redraw` extends the retained cold clock to a serviced minute
interrupt and reviewed 13:48 idle. The four GSM900/PCS incoming/outgoing
speech-control gates independently establish own-ROM command-8 `860b`/`840a`
publication on physical Send/End. Neither control words nor RTC redraw prove
PCM transport: NSB-6 framing/edge ownership remains unvalidated.

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
