# Nokia 5210 NSM-5 profile

## Validated result

The local v5.40 PPM E Wintesla set reaches a stable, registered and interactive
standby screen through the normal firmware lifecycle. `make
verify-5210-frontier` checks the unattended firmware-rendered operator frame
against an exact stable-pixel oracle. The separate menu, Messages and
navigation gates use only physical matrix keys to open the main menu, enter the
Messages application, and return through both Back levels to registered
standby. No firmware RAM, message, rendered pixel, or completion flag is
injected.

The archive members, normalization command, and image hashes are recorded in
`roms/README.md`. The shared MAD2 inputs remain documented placeholders; this
profile does not establish NSM-5-specific internal-ROM identity.

## Product contracts

NSM-5 differs from the other validated products at explicit device boundaries:

- GENSIO control `0x22` selects CCONT without the NSE-8 control-bit
  convention. Receive-ready follows a completed command byte.
- Firmware organically issues DSP bootstrap command 4 with pending value 2
  and later type `0x70` payload `0d00`. The product enables the existing
  request-derived service completion and compact type-`0x74` completion only.
  A read-only run observed upload latch `0x13b528 == 1`, expected and actual
  self-test values both `0x9c49`, and the verdict retaining healthy bit 6.
- The external-service transport answers organic D0/01 discovery. NSM-5
  accepts class-`0x40` registration sequence `0x42` and channel-map sequence
  `0x43`, then organically exercises channel `0x5f`. Channel `0x62` remains in
  the accepted map but has not been exercised independently by this gate.
- The SIMI block and removable synthetic GSM card use the standard controller
  boundary. Card contents remain external input.
- The CCONT inputs represent a BLB-2 pack. Nominal BSI `0x150` and BTEMP
  `0x140` lie inside recovered windows `0x13e..0x172` and `0x133..0x157`;
  VBATT is `0x2c0`. The generic tuple was outside those windows and selected
  the fault screen even after the DSP self-test had passed.
- The 84-by-48 display uses reversed segment order. This is a product display
  property, not a capture-only mirror.
- The five-row NSM Family-A keypad uses operational power-column mask `0x10`
  and a dedicated MAME
  matrix containing softkeys, scroll, call, end, volume, digits, star and hash.
- NSM-5 emits the ROM6 type-`0x56`, 160-byte candidate-window request. Its
  virgin PMM supplies ARFCN 86 (`0x56`), which the product-owned direct-octet
  radio contract carries through candidate acquisition, random access and the
  assigned-channel confirmation. Firmware then emits its own Location Updating
  Request, accepts the laboratory network, updates EF_LOCI, releases SDCCH and
  returns to the serving paging channel. The idle UI renders `DCT3 LAB`.

## Acceptance gates

- `make verify-5210-radio-registration` proves candidate selection, ordinary
  Location Updating, both EF_LOCI writes, channel release, steady camp and the
  exact registered operator frame.
- `make verify-5210-radio-authentication` repeats registration with network
  authentication enabled. The phone issues RUN GSM ALGORITHM to the removable
  SIM, fetches `SRES || Kc`, emits MM Authentication Response, then completes
  the same registration lifecycle. Internal PC-keyed consumer taps remain
  product-local; NSM-5 is checked at the SIM and GSM protocol boundaries.
- `make verify-5210-radio-a5-1-incoming-call` extends authenticated service
  through a network Cipher Mode Command, redacted product-local DSP key
  publication, organic Cipher Mode Complete, bidirectional ciphered SDCCH and
  traffic bursts, and the complete physical Answer/End lifecycle. The default
  regression composition remains A5/0.
- `make verify-5210-save-state` saves registered standby, restores it, and uses
  physical Menu/Enter keys to reach the exact Messages frame. This protects the
  radio, SIM, UI and product-device state across serialization.
- `make verify-5210-radio-call-state` saves and restores while an answered call
  is active, resumes the restored branch, then uses physical End to complete
  CC/RR and speech-control teardown and return to exact registered standby.
- `make verify-5210-power-lifecycle` proves the corrected physical power-column
  contract: a short press opens the firmware-rendered profile/power menu, while
  a sustained press is repeatedly scanned by firmware and ends at CCONT digital-baseband
  rail-off. The earlier `0x02` assignment produced IRQ edges but no lifecycle
  transition and is retired.
- `make verify-5210-charger-wake` continues from organic rail-off: a physical
  charger edge restores the CCONT-powered baseband with cause `0x04`, firmware
  reads that cause and samples charger-present VCHAR. Product-local task RAM is
  deliberately not interpreted through the 3210 summary addresses.
- `make verify-5210-sim-phonebook` opens the product's `Names` menu with its
  right softkey, saves `Ada`/`123` through the firmware's editor and absolute
  `EF_ADN` update, then cold-boots with the same card image and renders the
  stored contact through Search. This proves a non-SMS application write and
  removable-card persistence without importing the 3210 menu route.
- `make verify-5210-radio-supplementary` enters `*123#` and `*#21#` through
  the physical keypad and submits them with the product's dedicated Send key.
  The resulting processUnstructuredSS-Request and InterrogateSS transactions
  complete through GSM 04.80, render the network response, and release RR.
- `make verify-sim-toolkit-5210` supplies a Phase-2+ card, observes the
  product's nine-byte TERMINAL PROFILE, then requires `91xx`, FETCH, rendered
  `DCT3 SAT`, physical OK and a successful TERMINAL RESPONSE. The profile
  length is product evidence rather than a borrowed 3210 constant.
- `make verify-5210-radio-paging` proves one IMSI-addressed page, random access,
  Paging Response, contention resolution, clean release and return to the
  subscriber's paging group.
- `make verify-5210-radio-paging-negatives` proves the registered handset
  ignores its own IMSI on the wrong DRX frame, another subscriber's valid
  identity, and a malformed identity LV. Each composition continues PCH fill
  without RR access, Paging Response, or EF_LOCI mutation.
- `make verify-5210-radio-incoming-sms` carries an ordinary `hello` SMS through
  paging, SAPI 3, segmented CP-DATA and SIM EF_SMS storage, then pins the
  firmware's `1 message received` frame. The NSM-5 DSP cipher-control
  publication begins `00eb`, independently of the NSE-8 `00f4` encoding.
- `make verify-5210-radio-incoming-sms-read` uses physical softkey presses to
  open sender `5551234`, display `hello`, and make firmware change the EF_SMS
  record status from unread `0x03` to read `0x01`.
- `make verify-5210-radio-incoming-call-ringing` pins the firmware-rendered
  caller `5551234` and ringing UI after paging, CC SETUP and traffic assignment.
- `make verify-5210-radio-incoming-call-answered` proves physical Send emits
  CC Connect, accepts Connect Acknowledge, displays `Call 1`, and publishes the
  independently observed NSM-5 DSP wire value `0x860b` as speech control
  `0x060b`.
- `make verify-5210-radio-incoming-call-lifecycle` adds physical End, CC/RR
  teardown, ARFCN-86 channel release, DSP wire `0x840a`/control `0x040a`, and
  exact return to registered standby.
- `make verify-5210-radio-media-resilience` applies the independent NSM-5
  System Module PCM contract: COBBA-GJP derives 1.000 MHz `PCMDClk` from
  13 MHz / 13 and 8.0 kHz `PCMSClk` by /125, carrying a sign-extended 13-bit
  sample in a 16-bit word. The coherent gate proves bidirectional GSM-FR,
  non-silent downlink, FACCH stealing, degraded-frame concealment, SACCH
  coexistence, exact active-call save/load replay and physical End teardown.
  The fitted analogue microphone and receiver selections remain unproved, so
  physical host duplex is not promoted.
- `make verify-5210-radio-outgoing-call-lifecycle` physically enters
  `5551234`, starts CM Service and emits the firmware's called-party SETUP. It
  then proves Call Proceeding, one traffic assignment, Alerting, Connect,
  physical End, CC/RR teardown, the NSM-5 `0x860b`/`0x840a` speech-control
  pair, and exact return to registered standby. The media-resilience gate owns
  the separate digital PCM claim.
- `make verify-5210-radio-outgoing-sms` uses only physical keys to enter
  Messages, compose the predictive-text character `a`, select Send, and enter
  destination `5551234`. The gate requires the firmware's CM Service Request,
  SAPI-3 establishment and SMS-SUBMIT, network CP/RP acknowledgements, handset
  final CP-ACK, clean channel release, and the firmware-rendered `Message sent`
  acknowledgement.
- `make verify-5210-radio-reselection-same-lac` and
  `verify-5210-radio-reselection-different-lac` exercise the NSM-5 direct-octet
  neighbour contract on an evidenced GSM-900 pair (ARFCN 86/87, BSIC 0).
  Firmware either resumes PCH without subscriber mutation or performs a fresh
  Location Update and EF_LOCI write when the LAC changes.
- `make verify-5210-radio-loss-recovery` proves standards-counter serving-cell
  loss, the product's padded `03 05 00 00` SCH request, synchronization after
  RF recovery, and resumed PCH without a spurious Location Update.
- `make verify-5210-radio-all-cell-loss` proves the finite NSM-5 synchronization
  search reports no usable cell and stops PCH without falsely selecting a cell
  or mutating EF_LOCI.
- `make verify-5210-radio-reselection-paging` proves an IMSI page can still be
  answered and released after same-LAC reselection.

## Remaining scope

The gates validate v5.40 PPM E, board bring-up, authenticated and
unauthenticated registration, operator presentation, save/load, the main menu,
application entry, clean return navigation, idle mobility, RF loss/recovery,
and paging after reselection. They do not yet validate call media/audio,
handset-local application writes, or another 5210 firmware revision. The unexercised external-service
channel and placeholder MAD2 inputs must not be promoted to cross-product
facts.
