# Nokia 2100 NAM-2 bring-up

## Current result

The hash-pinned v5.84 MCU and PPM-E streams execute through a product-local NAM-2
profile. The firmware completes CCONT and LCD traffic, performs a 64-exchange DSP
bootstrap, acknowledges DSP service command 4, completes the type-05 external-
service discovery transaction and accepts a class-0x40 application registration.
The compact type-74 completion clears the initial `CONTACT SERVICE` frame. The
current frontier is the blank firmware-owned frame that follows registration.

This is a bounded portability frontier, not a boot or interactive promotion.
No 3210, 3310, or 5210 keypad, display, SIM, service, radio, or nonvolatile-state
contract is inherited merely because its values appear compatible.

## Established contracts

- The normalized stock image occupies `0x200000..0x3effff` and executes from the
  ordinary DCT3 flash mapping in a 16-Mbit device window.
- Direct static analysis resolves 652 MAD2 accesses from 326 literal seeds and
  recovers the familiar PUP, keypad GPIO and UIF register regions.
- The 18-entry CCONT descriptor vocabulary is byte-for-byte identical to the
  already decoded later-MAD2 table.
- Runtime establishes the GENSIO route independently: control offset `0x2d`
  receives `0x22`, CCONT transfer uses `0x2c`, receive status is polled at
  `0x6d`, and response data is read at `0x6c`.
- With that route configured, a ten-second run records 283 LCD command writes,
  8,741 LCD data writes and 22 complete display dumps without a soft reset.
- Runtime commands address banks 0 through 8 and ordinary bank transfers contain
  exactly 96 bytes. Board documentation specifies a 96x65 display, while the
  recovered text establishes reversed segment order. The modeled controller RAM
  is therefore 96x72 with a 96x65 viewport and mirrored X scan.
- The separately acquired complete v5.21 NAM-2 image converges on the same final
  `CONTACT SERVICE` frame under the v5.84 service contract. Populated PMM content
  alone therefore does not supply a cross-version service contract.
- NAM-2 v5.84 publishes its command-`0x64` application status only after accepting
  the peer registration. The status byte derived from RAM `0x13fdb3` bit 7 remains
  zero (`...64 03 00 4f...`), whereas established interactive profiles publish
  the corresponding ready value as one (`...64 03 01 4f...`).
- The command-`0x64` constructor at `0x258ce0` reads that bit directly. The seven
  literal-pool references resolve to eleven consumers; the mapped consumers gate
  service dispatch, transport notification and lower configuration work. No
  product-local setter has yet been established.
- NAM-2 performs an organic five-row keypad scan. Its column mask remains `0x3f`
  at the blank frontier, so an injected host key reaches the physical matrix but
  is intentionally ignored by firmware. This is evidence that application/MMI
  initialization has not completed, not a keypad-device failure.
- Primary NAM-2 service material specifies 0.5 V at BSI. Modeling the corresponding
  CCONT sample (`0x0b6` with a 2.8 V reference) changes neither bit 7 nor the blank
  frontier, excluding BSI alone as the missing readiness condition.

The runtime GENSIO observation supersedes the conservative static census's zero
direct SELECT sites. That census excluded dynamic/table-derived addressing and
therefore established bounded absence only.

## First unresolved boundary

The display, DSP bootstrap, service discovery, application-registration and keypad
wiring contracts are established for v5.84. The next pass must enumerate every
writer of `0x13fdb3` bit 7 and identify the product-local completion that owns it.
Existing handset response constants must not be inherited on packet-shape
similarity alone. SIM remains dormant at this boundary; its controller and card
profiles must not be promoted until execution reaches their firmware consumers.
