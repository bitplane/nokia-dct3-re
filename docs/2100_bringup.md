# Nokia 2100 NAM-2 bring-up

## Current result

The hash-pinned v5.84 MCU and PPM-E streams execute through a product-local NAM-2
profile. The firmware completes CCONT and LCD traffic, performs a 64-exchange DSP
bootstrap, acknowledges DSP service command 4, completes the type-05 external-
service discovery transaction, accepts a class-0x40 application registration,
and completes its physical M2BUS terminal startup exchange. The compact type-74
completion clears the initial `CONTACT SERVICE` frame. The current frontier is
the blank firmware-owned frame that follows registration.

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
  with that bit clear. The clear is therefore an intentional phase transition,
  not proof that NAM-2 never requires the bit again.
- The global initializer then evaluates seven predicates at `0x2f8396..0x2f83ca`
  and spins at `0x2f83e6` while any predicate is false. Before the terminal
  model, its first predicate `0x3003c8` returned zero.
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
  `D0/05`. The retained product-local terminal answers the organic request at
  the byte boundary using the documented 2.5/3 ms idle periods and request-
  derived ACK fields. NAM-2 accepts both transport ACKs and organically emits
  `D0/05`; `make verify-2100-mbus` fixes that complete exchange as an acceptance
  gate.
- Closing M2BUS advances the first global-initializer predicate. The second
  predicate also passes; the third routine `0x2c9780` returns false because byte
  `0x10f206` is zero. The NAM-2 creation table at `0x330ef4` identifies
  `0x21d114` as task 18. Its descriptor has the same scheduler shape as the
  independently mapped 3210 task 18, and its straight-line initializer posts
  readiness report `0x12` at `0x21d218`. The coherent run leaves its entire
  state block `0x10f1d8..0x10f207` zero: task 18 remains unstarted rather than
  failing a transaction after initialization. Its broader ownership remains
  unresolved.
- Supervisor `0x2f80b6` contains the missing release explicitly. Its second
  batch starts tasks 10, 11, 13, 12, 14, 19, 18, 15 and 16 through scheduler
  entry `0x2ac3b0`. The coherent run takes the preceding failure branch at
  `0x2f8290`: service byte `0x10ec50` is already 1, startup byte `0x10fea1` is
  zero, and mode byte `0x10fd24` is 2, but helper `0x2e18a8` returns zero because
  `0x13fdb3` bit 7 remains clear. In that case firmware publishes event
  `0x187d`. This is now the sole mapped task-18 release prerequisite; it must be
  followed to its organic service response rather than setting the bit or
  starting the task directly.
- The post-map class-`0x00`/command-`0x5f` `EXIT ANYSTATE` stream continues while
  the initializer spins. It is the already-classified periodic external-service
  channel, not proof that the missing task is advancing. A 120-second run still
  has the same blank frame and zero task state, bounding out a short scheduling
  delay.
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

The display, DSP bootstrap, service discovery, application registration, keypad
wiring, MBUS controller, terminal timing, arbitration and complete startup
exchange are established for v5.84. The next software boundary is the service-
gated release of task 18: the task identity and readiness-report role are
established, and the release is narrowed to the firmware-owned `0x187d` service
request which must re-establish `0x13fdb3` bit 7. Its transport and reply remain
unclassified. They must be recovered before enabling another peer or SIM/radio
behavior. Separately, two
external evidence inputs continue to bound
product-state fidelity. The
supplied v5.84 archive has no matching `0x3f0000..0x3fffff` EEPROM/PMM partition.
Substituting the populated tail from the complete v5.21 image and recomputing
the v5.84 firmware's 16-bit checksum over the `0x3fc026` calibration record
changes early state but does not settle the v5.84 initializer, so that
version-mismatched donor is rejected. A matching v5.84 product-state capture
would still distinguish legitimate provisioning from erased-tail fallbacks.
SIM and radio remain dormant at this boundary; their behavior must not be
promoted until execution reaches their firmware consumers.

## Public evidence availability

Contemporary service references independently identify the NAM-2 partition map
and the need for product PMM data. An archived repair discussion names a
`2100FuBu0584` full backup, but its attachment is no longer available. Another
archive records that a donated virgin EEPROM still produced `CONTACT SERVICE`,
consistent with the requirement to rebuild handset-specific FAID/identity data
after grafting. These sources prove that suitable captures existed; they do not
provide bytes that can be used as the v5.84 oracle.

The surviving v5.84 download trail was checked by exact archive name and object
identifier. Its mirrors all resolve to
`proshivki.org__nam_2_v5.84.rar` (Shareflare object
`8272.89d0d3c4a214639b1e8d3eba69`, also mirrored by Letitbit) and consistently
describe a 1.11 MB `mcu+ppm(e)` package. That is the already-held Wintesla
payload, not a two-megabyte full flash. No indexed copy of the missing PMM tail
or the named `2100FuBu0584` backup was found. The Internet Archive CDX endpoint
was also unreachable during the check, so archive recovery remains a possible
external follow-up rather than negative proof that no capture survives.

- [DCT3 flash address table](https://www.nokia-tuning.net/index.php?s=flashadress)
- [Archived NAM-2 EEPROM discussion](https://nokiafree.org/forums/archive/index.php/t-17045.html)
- [Archived `2100FuBu0584` reference](https://gsmforum.ru/threads/podskazhite-kak-podnyat-ufsom-nokia-2100.6670/)
- [Surviving v5.84 MCU+PPM listing](https://www.shram.kiev.ua/mob/proshnokia.shtml)
