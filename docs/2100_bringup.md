# Nokia 2100 NAM-2 bring-up

## Current result

The hash-pinned v5.84 MCU and PPM-E streams execute through a product-local NAM-2
profile. The firmware completes CCONT and LCD traffic, performs a 64-exchange DSP
bootstrap, acknowledges DSP service command 4, completes the type-05 external-
service discovery transaction, accepts a class-0x40 application registration,
and completes its physical M2BUS terminal startup exchange. The compact type-74
completion clears the initial `CONTACT SERVICE` frame. A checksum-corrected
fixture that preserves the v5.84 MCU/PPM bytes and attaches only the v5.21
donor's 64 KiB PMM partition does not make that foreign catalogue valid for
v5.84. On a freshly seeded fixture the phone clears the service frame, presents
the security editor and consumes physical `12345` plus Navi, but rejects it
because the identity-derived verifier changes during the same boot.

This is a bounded interactive compatibility result, not a default boot profile.
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
- The separately acquired complete v5.21 NAM-2 image uses a distinct DSP
  bootstrap: 992 alternating zero-valued handoffs on shared cells
  `0x0fe/0x100`, followed by reads of verdict cells `0x000/0x002/0x004`.
  Publishing those verdicts only after the measured count closes its DSP upload
  and yields the same service-control and discovery traffic as v5.84. It still
  converges on `CONTACT SERVICE`, so populated PMM alone does not supply its
  remaining version-local startup contract.
- NAM-2 v5.84 publishes its command-`0x64` application status only after accepting
  the peer registration. The constructor at `0x258ce0` derives its status byte
  from RAM `0x13fdb3` bit 6. Earlier notes called this bit 7 by reading
  `lsrs #7` as a direct index; the following carry branch tests original bit 6.
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
  gate. Modeling peer-byte occupancy on status bit 6 prevents the controller
  from starting a frame over the terminal's character. Exact absolute phase is
  not gated: repeated clean runs place the first D0/01 at 0.222849 s, and the
  preempted supervisor does not consume that timestamp as a deadline.
- Closing M2BUS advances the first global-initializer predicate. The second
  predicate also passes; the third routine `0x2c9780` returns false because byte
  `0x10f206` is zero. The NAM-2 creation table at `0x330ef4` identifies
  `0x21d114` as task 18. Its initializer posts readiness report `0x12` at
  `0x21d218`. Two direct event producers at `0x2b7e7c` and `0x2b833a` construct
  status `0x120c` after SIM status word `0x91`, identifying it as the SIM
  proactive-command/SAT task. The coherent run leaves its entire
  state block `0x10f1d8..0x10f207` zero: task 18 remains unstarted rather than
  failing a transaction after initialization.
- Supervisor `0x2f80b6` contains the missing release explicitly. Its second
  batch starts tasks 10, 11, 13, 12, 14, 19, 18, 15 and 16 through scheduler
  entry `0x2ac3b0`. The coherent run takes the preceding failure branch at
  `0x2f8290`: service byte `0x10ec50` is already 1, startup byte `0x10fea1` is
  zero, and mode byte `0x10fd24` is 2, but helper `0x2e18a8` returns zero because
  `0x13fdb3` bit 6 has been cleared. The same shift/carry audit establishes that
  the helper's preceding spin tests original bit 2, not bit 3.
- The first batch resumes task 2 at `0x2f8204..0x2f8206`, then calls helper
  `0x2e18a8` at `0x2f822a`. Scheduler resume preempts the supervisor: it enters
  at 0.184789 s but does not reach the helper result branch until 1.197912 s,
  after task 2 and the service exchange have run. The failure
  branch calls `0x2f1c1c`, which publishes startup tuple `0x11/0x44` and returns;
  it does not register a deferred release. An exhaustive scan of all 28 Thumb
  calls to scheduler entry `0x2ac3b0` finds no other task-18 resume site.
- Task 2's initializer at `0x256720` owns the readiness-byte transition. It sets
  bit 6, reads two logical product-state locations through `0x306e78`, and
  retains bit 6 only when the live computed reference equals `0x0270` and the
  reference or `0x0190` is nonzero. The coherent erased-tail run computes
  `0x4cb2` while both locations read `0xffff`, so firmware deliberately clears
  bit 6 at `0x25689e`.
- The product-state reader exposes a linked catalog, not two physical records.
  The v5.21 donor has five `PMMCAT` segments plus an `EEPROM` segment whose
  logical payload begins at tail offset `0xc026`; logical address `N` is stored
  at `0xc026 + N`. The catalog entry type and family-local index must not be
  confused with the logical addresses consumed by `0x306e78`.
- Task 2 separately validates logical block `0x0000..0x011f`. It sums bytes
  `0x0000..0x011b` and compares the result with the big-endian dword at
  `0x011c`. The unmodified v5.21 donor stores `0x99a6`, while v5.84 computes
  `0xa0aa`; this is why the donor's pending-status slot `0x12` remained live.
  Correcting only that integrity dword preserves the donor-derived reference
  `0x933d` at `0x0270`, keeps nonzero `0x2d10` at `0x0190`, and retains
  readiness bit 6 without changing firmware RAM.
- Timer slot `0x18` belongs to task 2: its runtime descriptor names owner task 2,
  state 2 and delay `0x007d`. Expiry is delivered through task 2's ordinary
  timer/message queue. It is neither event `0x187d`, a service request, nor
  evidence of a supervisor retry.
- Shared selector `0x100e2=1` starts a finite ROM-owned DSP code-block transfer.
  Firmware publishes 133 intermediate replies `0x100e4=2`, then clears the
  selector and publishes final reply 4 on chunk 134. Re-publishing selector 1
  is incorrect because it restarts the transfer at its descriptor head.
- This later transfer is not the missing release timing. With the readable
  donor catalogue, supervisor `0x2f80b6` enters at 0.201823 s and the first
  code-block reply appears at 0.213180 s. A diagnostic 50 us peer cadence
  completes all 134 chunks by 0.228037 s without releasing task 18. The later
  supervisor trace proves scheduler preemption, rather than linear execution,
  gives task 2 time to validate product state before the branch; transfer
  throughput is therefore independently excluded.
- A write-watch fixes the ordering independently of the static decode. Task 2
  starts at 0.353336 s,
  sets readiness bits 6 and 7 at 0.353350..0.353352 s, and clears bit 6 at
  0.358995 s in `0x256a84`. At its eventual branch the stock run has service 1,
  mode 2 and readiness byte `0x81`, so the cleared bit is observed directly.
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
  CCONT sample (`0x0b6` with a 2.8 V reference) changes neither bit 6 nor the blank
  frontier, excluding BSI alone as the missing readiness condition.

The runtime GENSIO observation supersedes the conservative static census's zero
direct SELECT sites. That census excluded dynamic/table-derived addressing and
therefore established bounded absence only.

## Donor-profile boundary

The display, DSP bootstrap, service discovery, application registration,
keypad wiring, MBUS controller, terminal timing, arbitration and startup
exchange are established for v5.84. Matching product state remains the first
unclosed input to ordinary MMI settlement. The bounded research fixture
preserves every v5.84 executable byte and borrows only the v5.21 PMM partition.

The corrected composition establishes these negative facts:

- the v5.84 logical-PMM reader returns erased data for the security-code record
  at logical `0x110..0x112` and the eight-byte verifier record at
  `0x668..0x66f`;
- the credential reader therefore takes its ROM fallback, `12345`, rather than
  reading the donor's visible BCD field;
- verifier initialization at `0x2ccaf0` calls the firmware cipher at `0x2fe780`
  with packed digits `05 12 34 50`;
- the identity preparation path `0x2e1918 -> 0x291528 -> 0x306f36` supplies the
  sixteen-byte string `00115<<00004014\0`, including two non-decimal nibbles;
- initialization derives verifier `d3 34 3e d8` from identity string
  `00115<<00004014\0`; physical submission later derives `d7 36 3b df` from
  `00116=000000243\0`. Both cipher calls use packed `05 12 34 50`, so the
  mismatch is identity/FAID state rather than a different entered code;
- the comparison at `0x2fe992` rejects the derived bytes and paints
  `Wrong code!` organically.

Those observations quantify the missing input without inventing it: v5.84
requires its own PMM catalogue/descriptors, logical records `0x110` and `0x668`,
and coherent identity/FAID material consumed by `0x2e1918`. The held archives
contain none of those v5.84 product bytes. Their values cannot be derived from
the v5.21 donor or from the reject branch; a matching PMM/full-flash capture is
required.

An earlier version of the fixture copied the donor's complete two-megabyte
flash image into MAME's persistent flash NVRAM. Because restored flash NVRAM
overrides the selected BIOS, those experiments silently executed v5.21 code
despite selecting BIOS `584e`. Their 281-byte identity-block census and
alternate-credential results therefore do not establish v5.84 verifier
semantics and are retired. The corrected generator rejects unexpected image
sizes, preserves bytes `0x000000..0x1effff` from v5.84 verbatim and imports only
the donor PMM partition at `0x1f0000..0x1fffff`. Unit tests protect that
composition. The first corrected run accidentally selected a stale
higher-numbered frame from the shared run directory, so it did not independently
prove the result. The gate now removes old snapshots; a fresh isolated run
reproduces both cipher calls and the same `Wrong code!` frame.
`make verify-2100-interactive` reproduces this bounded compatibility failure
through physical keypad input without firmware-state forcing.

The version-mismatched donor remains a diagnostic input rather than a
distributable v5.84 product profile. Inventing replacement state from the
invalid initialization result would be state forcing. The supplied archive
`2100sharp.pmm` is also not v5.84 PMM data:
it consists of 256 nine-byte flasher headers followed by 0x2000-byte payload
chunks, and stripping those headers produces the byte-identical v5.21 Sharp
full image catalogued as `2100f521sharp.fls`. The earlier experiment that loaded
only the MCU+PPM length and reported erased donor locations is discarded.

The emulation frontier is accepted product-state validation before idle MMI.
Promotion still requires a matching v5.84 product-state capture; the
later selector-1 code-block upload is fully bounded and is not a substitute for
that input.

Historical repair archives identify
`eeprom2100.fls` and 64 KiB 2100 PMM attachments, but no retrievable copy has yet
been recovered. The correct next input is a matching virgin or handset capture,
not a driver-side value synthesized to satisfy the comparison.
SIM and radio remain dormant at this boundary; their behavior must not be
promoted until execution reaches their firmware consumers.

The strongest surviving retrieval leads are exact but account-gated. The
GSMHosting thread exposes attachments `2100[1].pmm.txt` and
`2100_edited_bestcool.pmm.txt` (64.1 KiB each). The Elektroda archive lists
`2100fubu511pl.zip` (1.18 MiB), described as a v5.11 full backup. The historical
`ALL_PM_PMM_by_xTroy.zip` inventory names `eeprom2100.fls`, but its surviving
MCRF URL now returns 404. None of these bytes has been recovered, so they remain
retrieval leads rather than evidence inputs.

A collection audit on 2026-09-22 scanned all 39 local raw `.fls`, `.bin`, and
`.pmm` images with the PMMCAT parser. The only 2100 catalogues are the five
segments in `2100f521sharp.fls` and the same five segments inside
`2100sharp.pmm`'s flasher container. The v5.84 archive lists only
`NAM205.840`, `NAM205.84E`, and that v5.21 Sharp container. No untested local
2100 PMM candidate exists, so this audit does not authorize a new boot fixture.
An additional public listing advertises a 2 MB NAM-2 v5.84 service-box file,
but the download is subscriber-gated and its contents are unknown; size and
version label alone do not establish that it contains a matching PMM tail.

## Public evidence availability

Contemporary service references independently identify the NAM-2 partition map
and the need for product PMM data. Repair tooling describes PMM as the virgin
EEPROM input used after MCU+PPM flashing, and one surviving archive inventory
explicitly lists `eeprom2100.fls`. An archived repair discussion names a
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
- [Archived PMM collection inventory](https://www.mcrf.ru/forum/archive/index.php/t-22889.html)
- [2100 PMM attachment record](https://forum.gsmhosting.com/vbb/f131/2100-contact-service-234791/)
- [2100 v5.11 full-backup attachment record](https://www.elektroda.pl/rtvforum/topic162542.html)
- [Archived `2100FuBu0584` reference](https://gsmforum.ru/threads/podskazhite-kak-podnyat-ufsom-nokia-2100.6670/)
- [Surviving v5.84 MCU+PPM listing](https://www.shram.kiev.ua/mob/proshnokia.shtml)
- [Subscriber-gated 2 MB v5.84 service-box listing](https://www.telecelula.com.br/new/download_Procurar.asp?cod_categoria=7&cod_fabricante=2&pg=63)
