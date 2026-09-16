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
- The five-row NSM Family-A keypad uses power mask `0x02` and a dedicated MAME
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
- `make verify-5210-save-state` saves registered standby, restores it, and uses
  physical Menu/Enter keys to reach the exact Messages frame. This protects the
  radio, SIM, UI and product-device state across serialization.
- `make verify-5210-radio-paging` proves one IMSI-addressed page, random access,
  Paging Response, contention resolution, clean release and return to the
  subscriber's paging group.
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
  exact return to registered standby. No NSM-5 PCM clock or analogue route has
  been recovered, so this gate proves call and speech control rather than media.

## Remaining scope

The gates validate v5.40 PPM E, board bring-up, authenticated and
unauthenticated registration, operator presentation, save/load, the main menu,
application entry, and clean return navigation. They do not yet validate
mobility loss/recovery, outgoing calls, outgoing SMS, call media/audio, non-SMS persistent
application writes, or another 5210 firmware revision. The unexercised external-service
channel and placeholder MAD2 inputs must not be promoted to cross-product
facts.
