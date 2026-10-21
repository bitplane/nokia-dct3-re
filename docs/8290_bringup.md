# Nokia 8290 / NSB-7 inputs

## Current boundary

The acquired NSB-7 v5.22 package now has a qualified, contiguous MCU/PPM-A
image. `noki8290` is a fail-closed MCU research configuration: it reaches
the supervisor and renders the firmware's complete `CONTACT SERVICE`
failure frame. Idle, organic keypad/SIM, registration, calls and SMS are
not established for that fail-closed machine. The separate `nsb7hle`
research composition, with own checksum-only journal repairs and a declared
nominal VBATT input, reaches `Insert SIM card` and physically enters digit
`1` into the dial editor with an empty socket. With the synthetic card
inserted, SIMI startup file reads and the phone Security code editor
reproduce across cold runs. A separate one-byte erased-retry fixture permits
physical acceptance of the own-ROM default phone code. Usable idle/menu,
SIM writes and RF services remain unestablished.
In the normal machine, DSP exchange/service replies are disabled; own internal
ROM regions are explicitly undumped rather than attached sibling files.
No PMM, resident DSP mask, hardware profile or provisioning is supplied by
this normalization. The [Nokia 8290 user guide](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-8290-en.pdf)
identifies the product as NSB-7 and GSM1900; the [NSB-7 service manual](https://www.manualslib.com/manual/2171641/Nokia-Nsb-7-Series.html)
is the independent hardware-reference starting point.

## Qualified package contract

`roms/archive-dct3-packages/nsb7_5.22.exe` is a ZIP self-extractor with five
members, not the adjacent-gzip installer format. Its SHA256 is
`5c28267a14e5d3e36ed4dc166987da42e07b04a83ff88499dc38ee4485df0040`.
The own restore INI selects `AMERICA_A`, names both phone streams and gives
erase limits `200000..3cffff`. Both phone headers identify `V  5.22`,
`08-11-01`, `NSB-7`; the PPM identifies pack A.

| Member | CPU start | Bytes | Decoded SHA1 |
| --- | --- | --- | --- |
| `nsb7___5.220` | `200000` | `160000` | `6f2c32bfdb5dce75019f85ac56271fbf05a216cf` |
| `nsb7___5.22a` | `360000` | `70000` | `e09921c52e90888992792eceb57d2241f4eb091c` |

The joined image is `1d0000` bytes, SHA1
`4dacc50f57fe295168a53d3e5855a81acf35ab96`, with no padding or donor tail.

```sh
.venv/bin/python tools/nsb7_package_check.py \
  roms/archive-dct3-packages/nsb7_5.22.exe \
  --output roms/noki8290/8290f522a.fls
```

## MCU baseline and direct-access boundary

The ARM entry at `200040` selects the `20000` MAD2 window. A constant-index
MMIO census finds 243 literal seeds and 747 resolved candidate accesses;
these are linear-scan candidates, not exhaustive control-flow ownership.
The reviewed CCONT read path selects `22` at `305796`, writes `2c` at
`3057b4`, polls `6d` bit 2 at `3057b8`, and reads `6c` at `3057c0`.
Thus the byte-write-ready GENSIO mechanism is selected independently.

The six-second sample is in supervisor `2f82fe..2f8304`, with global
control `05`; shared observations remain `0000,ffff,0001,0001` at
`10000/10004/100fe/10100`. These are observations, not a DSP success verdict.
The 84x48 presentation matches the failure text, but keypad wiring and the
remaining board defaults are provisional, not complete NSB-7 hardware acceptance.

```sh
make build
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsb7-boot -- noki8290 -rompath ../roms -video none \
  -sound none -nothrottle -seconds_to_run 7 -log -verbose \
  -autoboot_delay 0 -autoboot_script ../tools/nsb7_boot_observe.lua
.venv/bin/python tools/nsb7_boot_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-boot
```

Use a fresh run directory for the erased-storage baseline. The checker pins
own source bytes, sampled continuation/shared state and exact RGB failure
pixels. It does not test task messages, input, native DSP or phone services.
The own native-upload and transport-HLE evidence below remain separate from
this normal-machine baseline.
A sibling's working HLE or EEPROM is not evidence for those contracts.

## Own identity-6 native upload boundary

`nsb7stage` is a separate research composition, not the normal machine.
It selects identity 6 from the own compiled fragment, a declared unmeasured
13 MHz execution clock and the own reset-release bit-0 path. No resident
mask is supplied, and identity 6 is not a measured fitted-part assertion.
The MCU routine at `2cb2e4..2cb2f2` compares the identity with 6 before
selecting one of two descriptor roots. Both branches exist in this image;
only the identity-6 composition is qualified below.

| Own descriptor (file offset) | Destination / words | Payload SHA1 |
| --- | --- | --- |
| `11ee88` verifier | `0f00` / 223 | `6646da3c5be9c70deda7e0b5b9f257d5d2ace815` |
| `11dca0` program fragment | `ff80` / 104 | `440bf49f1eba4cadb12f7f7581c992b0025807d6` |
| `1183a4` support upload | `fd00` / 638 | `70c931c40b3e7cdebc8d09f2488e3cc1e3c848d9` |
| `11ddcc` second loader | `0a00` / 623 | `89080bea5785eb944586574a3e1463145d8d4460` |

The support upload is not the second loader: the latter's descriptor is
`0a00,1000,026f,0200,03e8,0000`, and the staged device compares every one of
its 623 words against the own source before entry. The extraction tool's
`--product 8290` verifier/fragment/loader modes pin the corresponding source
hash and descriptor; `--loader` denotes the support upload, consistent with
that tool's existing contract.

Native execution releases the 223-word verifier twice, publishes
`0000/0006/0006/0006` at DSP PC `0f64`, executes the 126-word first loader,
verifies the complete 623-word second loader at `0a00`, then stops at absent
resident program address `2c75`. This is not a returned success or completed
runtime handshake. No substitute opcode, return or runtime HLE is enabled.

```sh
.venv/bin/python tools/nsb7_native_boundary_check.py \
  mame/mame roms tmp/nsb7-native-boundary
```

The fresh-directory runner requires the ordered native publications and
controlled missing-code stop. The MCU failure-frame baseline remains separate.
Native continuation itself requires an authentic resident source; the
separate runtime HLE contract below does not remove that boundary.

## Runtime transport and compact self-test

`nsb7hle` explicitly suspends native execution at `2c75` after the own
uploads, then enables shared service transport and request-correlated D0
discovery. It emits `1eff00d000030101e000`, consumes the D0/04 response and
acknowledges it with `1e0200d0000305014100`. Setup requests `13/14/15/16`
precede `70:0d00`.

The own class dispatcher `24b81a..24b844` resolves class `74` and calls
`248b00` at `24b856`. Its command cascade selects `0d` at `248b74`.
Flag `13fde1` bit 2 arms this path; it cancels timer `19`, clears fault
index `0f`, and consumes reply byte `+9` bits 0/1 for fault indices `10/11`.
That contract qualifies the existing compact request-correlated `74:0d00`
HLE response, not a silicon measurement or security-record verdict.

Runtime observation sees flags `84` and faults `ff,ff,ff` at consumer entry,
then flags `80` and faults `00,00,00` at the join `248c28`. Firmware submits
the `0a09` follow-up before that traced join. Neither the transport nor this
consumer result establishes idle: the display still shows CONTACT SERVICE.

The remaining concrete failure is independent of the three cleared faults.
Writer `248948` sets fault index `0c` to `0c` after checksum comparison
`24892c..24893e`: computed `bd42`, stored logical EEPROM word `1e0=ffff`, and
marker word `170=ffff`. Helper `2486dc` sums `c0` bytes from logical offset
`120`, then subtracts both bytes of word `154` through helper `2e3eb4`.
These are offsets in a 32 KiB shadow, not record IDs or physical flash
addresses. The unprovisioned comparison fails equality; the separate nonzero
guard would pass for these values. This is not evidence for another missing
DSP reply.

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsb7-service -- nsb7hle -rompath ../roms -video none \
  -sound none -nothrottle -seconds_to_run 9 -log -verbose \
  -autoboot_delay 0 -autoboot_script ../tools/nsb7_service_observe.lua
.venv/bin/python tools/nsb7_service_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-service
```

The observer is bounded and read-only. The checker requires the native
handoff, discovery, actual response consumption and still-failing own
checksum comparison; it cannot pass as normal-machine boot acceptance.
Do not transplant another product's PMM, force the fault clear, or answer an
unobserved identity/record query to bypass this check.

## Logical EEPROM replay and checksum-only fixture

Reader `30d658` calls `2cdd08`, which bounds `offset+length` by `8000` and
copies from RAM base `10d5c8` (literal `2cde3c`). The persisted own flash
contains an EEPROM journal at file offset `1d0000` (CPU `3d0000`), with
header word `+18=00c8`. A complete initial 32 KiB record at `+20` is followed
by ordinary write records. Replaying the first 64 KiB window, including all
11 initial-run records and their byte checksums, matches every byte of the
runtime shadow. This does not establish the fitted flash part or its erase
geometry; `00c8` is an observed header field, not an imported NSE-5 version.

The own erased calibration span independently computes `bd42` using
`sum(shadow[120:1e0]) - sum(shadow[154:156])`, modulo 65536. The explicit
research fixture appends only `08e001e0bd42` at file offset `1d80e6`.
The header encodes a two-byte write, valid record checksum and logical
destination `1e0`. All executable bytes and every logical byte except the
two application-checksum bytes remain unchanged. No calibration values,
identity, marker, credential or firmware RAM state are manufactured.

```sh
# Fresh unprovisioned capture; omitting verbose avoids the large CPU trace.
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsb7-storage -- nsb7hle -rompath ../roms -video none \
  -sound none -nothrottle -seconds_to_run 7 -log -autoboot_delay 0 \
  -autoboot_script ../tools/nsb7_storage_observe.lua
.venv/bin/python tools/nsb7_storage_check.py \
  tmp/nsb7-storage/nvram/nsb7hle/flash \
  --shadow tmp/nsb7-storage/nsb7_nv_shadow.bin \
  --trace tmp/nsb7-storage/error.log \
  --checksum-fixture tmp/nsb7-checksum/nvram/nsb7hle/flash
```

Boot that generated external input using the same observer and run directory.
The own comparison reads `bd42/bd42/ffff` and takes success store `248942`,
clearing fault index `0c` organically. Independent replay still matches all
32 KiB after the new boot and a separate cold process using only that own
retained flash; the accepted checksum persists without another fixture write.
The calibration-only fixture's screen remains CONTACT SERVICE: checksum
validity is not sufficient to establish idle, nor does it make erased
calibration into authentic factory data. The unprovisioned baseline remains
protected separately. Next trace the surviving startup result and its owner,
not another guessed DSP completion.

## Prefix checksum and complete startup-result table

Initializer `2e95b6..2e95c4` sets 29 bytes at `13fbe0` to `ff`.
The observer and unprovisioned service checker cover all 29, not only the
first 18 around the compact self-test. With only the calibration checksum
corrected, index `12` (hex) remains `12`. Its first writer is `2e8ffc`,
which stores `fd` when `29656e` returns anything other than one; `2488cc`
then converts that failure to the index value `12`.

Reader `29656e` copies logical bytes `0000..011f`, calls `2f1db0` with
length `011c`, and compares its result with the big-endian 32-bit word at
`011c`. The checksum helper accumulates bytes modulo 65536. For the own
erased prefix this produces `00001ae4`, versus stored `ffffffff`.
No identity-field semantics are inferred from that integrity check.

The separate `--prefix-checksum-fixture OUTPUT` option appends
`101b011c00001ae4` to the own journal, retaining every logical byte except
the four checksum bytes. In particular it does not create an IMEI, phone
code, SIM identity or calibration. Use the previously qualified flash and
matching shadow with `tools/nsb7_storage_check.py`; outputs must be fresh.

Firmware then compares `00001ae4/00001ae4`, returns one, and stores zero at
result index `12` without the failure conversion. Both checksum comparisons
and full-shadow replay remain valid after a cold restart with the own
retained flash. Flags at six seconds are `40`; the rendered frame is blank
rather than CONTACT SERVICE, and the frame remains blank at 15 and 30 seconds
with control `05` and flags `40`. Neither that flag nor blank pixels establish
idle or input delivery. The remaining task is to recover the own keypad
contract and test application activity physically, alongside the surviving
startup continuation. The normal erased-storage baseline and native
missing-resident-code boundary are unchanged.

## Keypad startup boundary

Own scanner `308240` scans five rows with no row-pin shift. Its normal
table at `343d7c` and special table at `343d98` match the existing five-row
logical host layout; the special Power contact is column mask `10`.
This table equality supports input reuse, not broader board equivalence.

With both checksum-only repairs, read-only debugger observations confirm
initializer `30835c` executes. MMIO write taps record initialization at
1.129 seconds, a scan at 1.154 seconds, and a later write at `30ada8` at
1.224 seconds setting column mask `6b` to `3f`. Routine `30ad82` explicitly
ORs its low five bits with `1f`; this is firmware-owned masking, not a host
input failure inferred from missing logs. Physical Menu and digit-1 presses
at eight and ten seconds leave the columns masked and produce no observed
scanner IRQ or decode breakpoint. Global control remains `05`.

`tools/nsb7_input_observe.lua` reproduces those physical contacts and
read-only mask/initialization observations with the debugger enabled. It
does not unmask columns or write guest state. The next question is which
startup dependency releases that keypad state. Interactive boot remains
unproven.

The saved return address at the mask write is `29eee7`, identifying direct
caller `29eee2` (the other direct BL sites are `29ee14` and `29f0d6`). This
is the branch selected by the startup result `2`; it resets the state
through `301ed0`, then receives readiness reports. Its continuation at
`29efa2` requires the low nibble at `13ff00` to be `6` and that at `1308b1`
to be `0f`. Only after those tests does it call `30acf2` at `29efde`.
Runtime writes establish state `6` at 2.189 seconds and readiness `0e`
at 2.853 seconds. The report decoder maps `14/16/15/17` to bits `1/2/4/8`;
the missing bit therefore belongs to report `14`, not a guessed key event.
This identifies the next producer question without claiming that the
readiness gate is the sole remaining UI dependency.

`tools/nsb7_keypad_conformance.lua`, on a separate fresh `noki8290` run,
tests 20 physical matrix contacts against all five row drives (100 scans),
plus the Power special contact and release. It writes controller MMIO and
is explicitly conformance evidence, not organic input delivery. Those
checks pass; the normal failure-frame oracle and native `2c75` controlled
stop also reproduce after the keypad configuration change.

### Missing analog readiness report

Own wrapper `301f6c` clears its local diagnostic flag, then posts task
`1` / report `14` through `289a38`. An aligned Thumb BL scan of the entire
qualified image finds one direct caller, `2463b2`. This does not exclude
indirect or table-driven publishers. The call is in the analog routine
containing the firmware strings `VBAT Checks` and `Start limited fast VBAT
read`; no DSP reply is inferred from the report number.

Its 21-entry state table is at `247ab8`; state is the halfword at
`130020` (context `r6+1c`). Runtime writes with the current default board
inputs enter `11` at 1.228 seconds, `4` at 4.691 seconds, then `3` at
5.155 seconds. Repeated state writes are active dispatch, not evidence of
an inactive task. Read-only observation of the firmware's own text routine
`30ae82` independently identifies `CHECK TEST MODE`, `CHECK POWER ON REASON`,
`INIT CHARGING`, then `BOOT UP CHARGE`, `WAIT CHARGER VOLTAGE SETTING`, and
`CHARGER DISCONNECTED`. These strings name software branches, not proof
that the physical charger input was asserted.

State 3 entry `2472e0` distinguishes events `49` and `41`. On `49`, it
tests its countdown and context byte `+0d`, then predicate `305800` before
proceeding. Decode that predicate and the preceding power/ADC branch before
changing board inputs; do not synthesize report `14`, unmask the keypad,
or assert a charger cause to escape the branch. Raw analog defaults remain
provisional. The NSB-7 service manual independently describes BLB-2 battery
identification and a 100 kohm BSI pull-up, but does not establish the current
emulator tuple as a measured NSB-7 electrical profile.

### Nominal VBATT and physical UI acceptance

`nsb7hle` alone supplies ADC channel 2 = `554`, a declared nominal
3.6 V / 6.5 mV-per-bit CCONT transfer hypothesis. It replaces only the
full-scale channel-2 placeholder; BSI, BTEMP, all other ADC inputs, charger
status, executable bytes and the checksum-only EEPROM contents are unchanged.
This is not measured NSB-7 calibration or a recovered complete battery model.
The normal `noki8290` and native-only `nsb7stage` profiles retain their
previous inputs and acceptance boundaries.

This single input change produces readiness `0f` at approximately 3.391
seconds. Before the scripted physical contacts the keypad mask is `20`.
Menu at eight seconds produces IRQ0, raw matrix position `06`, logical
key `19`, and an `Insert SIM card` frame. Digit-1 at ten seconds produces
IRQ0, raw `07`, logical `01`, and the dial editor displaying `1` with
`Clear`. The early six-second frame is still blank; it must not substitute
for the later physical-input evidence. The analog task still visits charging
branches later, so the result does not validate the whole power lifecycle.

Both physical-contact frames and the ordered readiness/IRQ/scan/decode
evidence reproduce in a new process using the preceding run's own retained
flash. `tools/nsb7_input_check.py IMAGE RUN` requires the explicit runtime
HLE handoff, complete readiness sequence, enabled pre-press masks, decodes
between each physical press/release, and exact pixels for both frames.
It rejects normal/native profiles and makes no SIM, RF or speech claim.
Run `tools/nsb7_input_observe.lua` with `-debug -debugger none`; seed only
the own flash containing the two checksum-only repairs described above.

The status retry's `9004` query resolves index `10` in table `343ee0` to
serial command `70`, CCONT register `0e`, mask `04`. That is a status query,
not an ADC sample, and was left untouched. The own SIMI activation and
card transport are qualified separately below, without an application-level
SIM reply.

## SIMI and physical card boundary

The own constant-index census resolves 54 candidate accesses in the
`36..3f` register window. Reviewed initializer `2eef68` clears IIR `38`,
sets clock register `3a` to `03`, enables CTSI `0d` bit 5, programs RX/TX
FIFO controls `3d/3e`, and writes control `39=32`. Receive handler `2eeee6`
reads/acknowledges IIR `38`; its `40` cause drains RX fill `3c` through data
`37` at `2eee80`. The controller/card are enabled only in `nsb7hle`;
the fail-closed normal and native-only profiles remain unchanged.

With the existing standards-oriented synthetic card, firmware subsequently
writes control `33/b3`, receives ATR `3b1005`, exchanges PPS `ff00ff`, and
issues SELECT, GET RESPONSE, READ BINARY and READ RECORD commands through
the real controller byte path. Own wire observations contain 938 TX bytes,
including MF/DF and ICCID/phase/IMSI reads, all 50 ADN records, and a final
`a0f2000016` presence-monitor request. No APDU or firmware message is
injected. The capture is bounded below its 4096-byte TX and 8192-byte RX
limits. This establishes startup reads, not successful credential checks,
SIM update commands or a complete SIM feature matrix.

The physical Menu fixture submits an empty phone-security editor and
produces `Code error`; digit-1 then produces `Security code: *` with
`OK/Clear`. This is phone-side security presentation, not SIM PIN failure
or proof that a stored phone code is accepted. The own phone-code/retry
record contract is qualified below without a firmware-memory patch or a
sibling's security bytes.

`tools/nsb7_sim_observe.lua` retains the configured physical card, runs the
same two contacts, and records read-only controller traffic.
`tools/nsb7_sim_check.py IMAGE RUN` checks the explicit HLE boundary,
activation/FIQ consumer, complete captured TX framing, startup commands,
ADN record reads, ATR/PPS reception, final STATUS and exact security-editor
pixels. It reproduces in a new process retaining only the preceding run's
own flash and synthetic-card NVRAM. The empty-socket input observer instead
asserts the physical `Remove SIM card` contact at startup and continues
to reproduce its separate `Insert SIM card` / dial-digit acceptance.

## Phone-code and erased retry contract

Own reader `295a28` reads three packed bytes at logical EEPROM offset
`0110`, expands their nibbles, and checks the first five decoded characters.
At `295a72`, invalid digits select ROM string `12345` at `295e08`; return
value zero records that fallback. Runtime observes input `ffffff` and
output `3132333435`, not a provisioned or borrowed phone code.

Initializer `2c8e7a` calls this reader and encodes the result through
`302c5c` into runtime record `130a8c`. On physical `12345`, validator
`302e78` observes input `313233343500` and equal encoded/stored words
`d83b30d8`. Its subsequent retry guard `302e24` nevertheless rejects counts
greater than or equal to five. The initial eight-byte record is read from
logical `09d4`; byte `09d8`, corresponding to runtime `130a90`, is erased
`ff`. This is independently observed at the NV reader and retry consumer.
It is not a failed character decode or mismatched code.

`tools/nsb7_storage_check.py --security-retry-fixture OUTPUT`, with the
matching own runtime `--shadow`, appends short journal record
`04e109d800`: only logical byte `09d8` becomes zero. Zero is the retry-clear
value used by the firmware at `302e64`; it is not a guessed code or lock
mode. The fixture leaves every identity, packed-code and encoded-code byte
unchanged, along with both prior checksum fields. It refuses a non-erased
retry byte, unknown source image, mismatching shadow or occupied append
space. This is explicitly a generated research fixture, not authentic
factory provisioning.

`tools/nsb7_security_observe.lua` physically enters digits 1..5 and Menu at
8..13 seconds. The unchanged erased-retry control shows equal encoded words,
retry `ff`, and `Code error`. The one-byte fixture shows the same encoded
words, retry `00`, and `Code accepted`. Both exact frames are checked by
`tools/nsb7_security_check.py IMAGE RUN` (add `--rejected` for the negative
control), alongside each physical key decode and independently replayed
own code/retry storage. Acceptance repeats in a new process retaining the
preceding run's own flash and card NVRAM.

The post-acceptance frame has `Menu/Names` softkeys but malformed main-area
content. Do not call it a usable idle or registered screen. The next
application boundary is physical menu activation and the display/resource
state after acceptance; no security verdict or UI state should be injected.
