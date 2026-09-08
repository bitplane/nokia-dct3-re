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
- Public NAM-2 flash maps reserve `0x3f0000..0x3fffff` for EEPROM/PMM. The
  supplied v5.84 MCU+PPM package ends before that partition, so it is not a
  complete product-state image.
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
  the peer registration. The constructor at `0x258ce0` derives its status byte
  from RAM `0x13fdb3` bit 7.
- Runtime write observation closes that bit's lifecycle: initialization sets it
  through `0x40`, `0xc0`, `0xc4`, `0x84` and `0x80`, then the global initializer
  deliberately clears it at `0x2e1b18`. The instruction-equivalent 3210 routine
  at `0x29bc70` clears the corresponding bit, and an interactive 3210 settles
  with that bit clear. A set bit is therefore not an application-readiness
  requirement.
- The global initializer then evaluates seven predicates at `0x2f8396..0x2f83ca`
  and spins at `0x2f83e6` while any predicate is false. The first predicate,
  `0x3003c8`, is the only one reached in the coherent run and returns zero.
- `0x3003c8` composes lower-idle check `0x2fd208` with queue-empty check
  `0x2be6c0`. The queue state is empty. The lower-idle check's RAM predicates are
  also clear, but it requires MAD2 FIQ-mask register offset `0x0a` bit 3 to be
  set before querying object `0x79`. Firmware reduces the mask from `0xff` to
  `0xe0` by 0.208 seconds and never restores bit 3, so this mask predicate is the
  first established failing condition.
- The four bit-3 setter regions belong to the MBUS lifecycle. `0x2f7f90`
  initializes the controller, `0x2f7c24` acknowledges FIQ3 and unmasks it to
  start activity, `0x2f7d2a` handles FIQ3 and remasks it, and `0x307758`
  services per-byte FIQ2 events. Public MADos material identifies FIQ3 as the
  independent 423.1 Hz `FIQ_MBUSTIM` source; modeling that clock makes this
  firmware-owned sequence run without a product-specific kick.
- The resulting transmitter emits the checksum-valid physical MBUS frame
  `1f ff 00 d0 00 01 01 01 31` at 9,600 baud. With no peer it retries the
  same frame. Reusing the DSP-framed D0 acknowledgement/completion semantics,
  including corrected service-node addressing, is rejected and still retries;
  packet-family resemblance is therefore insufficient evidence for a reply.
- Public M2BUS documentation establishes terminal node `0x1d`, type-`0x7f`
  transport acknowledgements, terminal startup `D0/04`, and phone response
  `D0/05`. A diagnostic-only run supplied those bytes through RX/FIQ2 and NAM-2
  organically emitted the documented `D0/05`, validating the application
  exchange. The controller now presents transmitted bytes as physical
  single-wire echo and firmware consumes them organically; absence of the
  external acknowledgement still causes the expected retry. No responder from
  that trial is retained.
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

The display, DSP bootstrap, service discovery, application-registration, keypad
wiring, MBUS controller-start contract, transmit echo and 423.1 Hz FIQ3 source
are established for v5.84. Two external evidence inputs now bound progress. The
supplied v5.84 archive has no matching `0x3f0000..0x3fffff` EEPROM/PMM partition.
Substituting the populated tail from the complete v5.21 image changes early
state but does not settle the v5.84 initializer, so that version-mismatched donor
is rejected. The remaining M2BUS uncertainty is the expected external endpoint
plus exact oscillator phase and differing-line collision behavior. A matching
v5.84 product-state capture and a physical NAM-2 M2BUS trace or primary MAD2
evidence would distinguish these independently; neither a borrowed PMM nor a
timed responder is retained.
SIM remains dormant at this boundary; its controller and card profiles must not
be promoted until execution reaches their firmware consumers.
