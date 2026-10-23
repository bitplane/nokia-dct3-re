# Nokia 5110i / NSE-2 bring-up

## Current boundary

Own v5.53 MCU/PPM-A executes its verifier on the explicitly borrowed NSE-1
ROM4 backend. The nominal-voltage research profile `nse2r4v` reaches the
firmware's **Insert SIM card** screen. Two physical Menu presses reach the
own decoder as raw key 11 / key `19`. With the synthetic card physically
inserted, firmware completes SIM read bootstrap and presents **Security code**.
One physical `12345` + OK attempt matches the cached code but the erased
retry counter rejects it, producing **Code error**. A separate one-byte
zero-attempt NV fixture lets the same physical code return `accepted=1`.
The seventeen-second frame has transient placeholder-like glyphs; by
nineteen seconds a Menu softkey is visible. Physical Menu presses then open
**Phone book** and **Search / Add name / Erase**. A physical Add name
workflow now saves `A / 123` through a successful SIM UPDATE RECORD and
checks its serialized card storage. A separate cold boot reads the saved
card and physically searches for `A`; an erased-card control instead shows
**No phone numbers**. Physical Messages / Write / Save also stores an
unsent `A` draft on the SIM. Registration, calls and network SMS are not accepted.
Longer execution is not stable: both menu-only and contact-entry runs
request reason-`68` reset at about 31.97 seconds. The short menu gates do
not cover this boundary.
Fitted mask identity, DSP clock and measured board calibration are unproved.

The ordinary `noki5110i` profile remains unchanged: erased EEPROM and a
silent DSP reproduce CONTACT SERVICE. `nse2r4t` supplies the compatibility
DSP but retains open-pin ADC2; it completes DSP startup while the analog
readiness counter cannot drain. These are explicit controls, not alternative
claims of usable boot.

Next: identify the legitimate DSP activity expected during the advancing
measurement, preserving the proved initial transport and ordinary software
timer expiry. No queue migration or clock defect is established. Keep the
existing rejection and restart controls.
Do not substitute donor EEPROM, force firmware state, or claim phone service
from the graphical prompt or key decode alone.

## Reproduction

Each runner refuses existing directories and uses absolute run-local ROM,
NVRAM and configuration paths. Inputs and the MAME binary are hashed in
`inputs.json`; process status and checked results are retained separately.
Run the gates serially, with distinct fresh directories.

```sh
make verify-5110i-static
make verify-5110i-boundary RUN_DIR=tmp/nse2-erased
make verify-5110i-native-verifier-erased RUN_DIR=tmp/nse2-native-erased
make verify-5110i-native-verifier RUN_DIR=tmp/nse2-native-integrity
make verify-5110i-input-frontier RUN_DIR=tmp/nse2-openpin-control
make verify-5110i-nominal-boot RUN_DIR=tmp/nse2-nominal-boot
make verify-5110i-nominal-input RUN_DIR=tmp/nse2-nominal-input
make verify-5110i-sim-bootstrap RUN_DIR=tmp/nse2-sim-bootstrap
make verify-5110i-phonecode-boundary RUN_DIR=tmp/nse2-phonecode-boundary
make verify-5110i-phonecode-accept RUN_DIR=tmp/nse2-phonecode-accept
make verify-5110i-menu RUN_DIR=tmp/nse2-menu
make verify-5110i-phonebook RUN_DIR=tmp/nse2-phonebook
make verify-5110i-sms-store RUN_DIR=tmp/nse2-sms-store
make verify-5110i-native-processing RUN_DIR=tmp/nse2-native-processing
```

The nominal boot gate requires the reviewed unattended seven-second frame
hash `7da9bef12e0bf163dfdf8770e215957b58b4911baa9ba987c05ddf8fadfeae8a`.
The input gate requires decoder activity during both actual Menu contacts;
it does not claim that a menu opens without a SIM. The separate open-pin
gate requires the observed masked-input frontier and must be revised if
that control advances. `--seconds` permits bounded 8--120-second native
observations; execution checks must cover the requested horizon.

## Own inputs and hardware contracts

`verify-5110i-static` qualifies both acquired `nse2_553.zip` and its Windows
SFX wrapper, without executing the installer. Exact member names, extents
and hashes must match, and normalized contents must be byte-identical.
The archive calls the phone 5110i; both headers identify `NSE-2`, `05.53`,
`11-01-01`. Provenance and individual hashes live in
[the ROM collection](../roms/README.md#nokia-5110i-nse-2-v553).

| Surface | Own evidence | Limit |
| --- | --- | --- |
| Flash | MCU `190000` bytes + PPM A `70000`; unchanged 2 MiB CPU-order image, SHA256 `00ab7e062de9c89718eb81031528a646a79317ba0bad5a272a6d4d3b09c77781`. | Base TE28F160 is a capacity-compatible trial, not fitted identity. |
| Entry/memory | ARM `200040`; stack literal `10fb98`; interworking reaches Thumb `2000ec`. | 64 KiB SRAM is a minimum research decode, not physical-size/alias proof. |
| EEPROM | Header `f4`: two address bytes, 32-byte pages, 8 KiB; SDA/SCL are bits 0/2 at signal/direction `20020/20024`. | Native 24C64 is descriptor-compatible; no own factory dump or measured write latency. |
| GENSIO/CCONT | Own `296704`: select `22` at `28`, TX `2a`, RX `2d`, status `29` bit 2. | Shared serial grammar, not measured electrical timing. |
| LCD | Own `296e46/296eac`: command/data `2c/2b`, ready bit 0. | Observed 84x48 presentation; fitted controller identity remains unproved. |
| Keypad | Scanner `29806c`, decoder `299010`, maps `2b382c/2b3848`, selector `10c6b6`; five rows, signal/direction `31/2f`, columns `30`, IRQ mask `33`. | Existing NSE-1 contact layout matches own table; digits 1--5 and Menu have runtime decode acceptance. |
| SIMI | Own `286520` initializes standard IIR/FIFO controls; inserted synthetic card completes read bootstrap, organic ADN write and cold contact readback. | SIM PIN authentication remains unaccepted; the tested phone code is phone-side. |

EEPROM START/STOP entry points are `295814/295886`, identified by SDA
transitions while SCL is high. Block reader `293d92` takes address,
destination and length in `r0/r1/r2`, calls setup `293c04`, ACKs intermediate
bytes through `295746(1)`, NACKs the final byte through `295746(0)` and issues
STOP. It returns zero on setup failure, one after reading; a length below
two still reads a final byte, so zero-length safety is not established.
Static qualification pins the relevant instruction bodies and literals.

## Integrity-only EEPROM

No acquired NSE-2 external EEPROM is available. The NokiX scripts archive's
repair directory contains NSE-1/3/6/8 and NSM-1 blobs, not NSE-2. The
[historical NSE-2 EEPROM thread](https://forum.gsmhosting.com/vbb/f131/nse-2-5110i-eeprom-51879/)
is a retrieval lead only; no dump from it is an accepted input.

Diagnostic fixtures start with 8192 erased bytes and change only decoded
checksum words. They are not authentic identity, lock or calibration data.
Validator `258edc` uses `28a0c4` to sum bytes modulo 65536, complements the
zero-extended sum and compares a full 32-bit stored word:

| Data start | Bytes | Stored word | Value for erased data |
| --- | --- | --- | --- |
| `0000` | 20 | `0014` | `ffffec13` |
| `0018` | 12 | `0024` | `fffff40b` |
| `0028` | 24 | `0040` | `ffffe817` |
| `0044` | 480 | `0224` | `ffff21df` |
| `0228` | 20 | `023c` | `ffffec13` |

`--checksum-only` repairs those five words. The actual self-test store at
`27dc14` changes result `10fcb2` from `fd` to `00`, but alone this does not
remove CONTACT SERVICE. `--record-checksum-only` additionally sets halfword
`031e` to `db24`: helper `23315c` sums `0240..031d` and subtracts the two
bytes read at `0274`. Consumer `2333ce` checks that halfword and the nonzero
guard involving `0290`; the actual result at `10fcac` becomes `00` instead
of `0c`. All data payload bytes remain erased. Both integrity families are
required by the nominal acceptance fixture; semantic provisioning remains
an independent question.

## Native compatibility boundary

`nse2r4t` removes the DSP HLE and attaches the existing native C54x backend.
Own MCU/PPM, empty card socket, GPIO and analog defaults are unchanged.
Its compatibility program/data exports retain their NSE-1 provenance:

- Program: 131070 bytes, SHA1 `a05a1e96a8c36ec5a47e1ea059d15afa54ca5739`.
  Undumped word `ffff` stays at region erase zero.
- Data: 131072 bytes, SHA1 `024c7f970f4ef754d3e90471de48a167515f930d`.

Initialization from these acquired exports does not prove NSE-2 fitted mask
identity or power-on RAM contents. The 52 MHz clock is the NSE-1 compatibility
assumption, not an NSE-2 measurement. Minimal MCU boot-ROM flash-entry HLE
also remains declared. No synthetic DSP response or application peer is used.

Own catalogue `29bba0` initializes 28 slots at `10ba14`; only slot 27 is
null. Literal `27848c = 10ba78` selects slot 25, descriptor `2a0f6c`, header
`0f00 0000 00ba 0700 00b4 0000`. Its 186-word verifier begins at `2a0f78`.
Its pinned resident CALL targets are `8023`, `8023` and `802b`, supporting
the ROM4 ABI compatibility test without identifying the fitted mask.
The Thumb wrapper at `29acb4` enters an ARM LDM/STM loop copying twenty words
per iteration: the MCU stages 200 full halfwords at `10e00..10f8f`, exactly
matching the own payload plus fourteen zero padding words. Their SHA256 is
`c6b5ba7d61aa08e90014499f3e292bd4655b54a0b2f89722a9c52d4ad3c3c658`.
The control sequence at `100f6..10102` is
`0100 0300 0001 0000 0001 0001 0200`.

On the silent control, 22 actual reads of `10004` return `ffff`, followed
by timeout stores at `278226/2782d6/2782d8`, clearing `10b9f5/10b9f4`.
On the native backend, the own verifier writes DSP `0803=4` with observed
PC `0f0f`; a read-only program snapshot `0f00..0fb9` matches all 186 own
words. The acquired program export initially has zeros there. The MCU then
reads response `4`, continues loading, and sets `10b9f4=1` at `2787e2`.
Results `0e/0f` clear. This establishes uploaded native execution and its
startup exchange, not RF acquisition or speech. The larger successful-path
ROM checksum transfers are not claimed as a complete capture: the initial
staging observer stops at the first startup poll.

## Analog readiness and input

Task entry record 19 at `2ac990` points to `22d640`, whose context is
`10bb0c`; event/state words are `10bb3e/10bb40`. Its 30-state dispatcher
is at `22ef1e`, table `22ef34`. The report-14 stub `2969a6` posts to task 1
through `2558e4`; an immediate BL census finds one direct caller, `22d9da`.
This does not exclude data-driven producers. A 30-second open-pin observation
shows live timer events `49` and eventual state 3, not a stalled scheduler.

The entry to report 14 depends on an averaging counter at `10a562` draining.
Reader `2644e0` obtains source 7 through `297546`; own map `2b3850` selects
CCONT ADC2. It applies calibration, scales by `1500/232`, and accepts both
converted samples only in inclusive range `1800..5500`. `2645b4` decrements
the counter only on an accepted pair. Actual writes and stack samples show:

| Input profile | Converted pair | Counter/result |
| --- | --- | --- |
| Open-pin ADC2 `3ff` | `19d6/19d6` (6614 each) | Reader returns false; count stays ten; report bits remain `0e`. |
| Nominal ADC2 554 | `0dfd/0dfd` (3581 each) | Ten valid decrements reach zero; report 14 arrives organically. |

`nse2r4v` changes ADC2 and makes a synthetic card available relative to
`nse2r4t`; its physical socket defaults empty. The ADC value is independently
derived from the battery-family hypothesis: the original
[NSE-1 service manual, battery connector table](https://www.eserviceinfo.com/preview_html.php?fileid=26879&previewid=13251)
specifies nominal 3.6 V; CCONT's documented typical VBAT scale is 6.5 mV/bit
(see [the silicon reference](8810_bringup.md)), giving round(3600/6.5)=554.
This is a cross-product nominal-input experiment, not measured NSE-2 board
calibration, not a sweep, and not an NV gain/offset change chosen to force a
verdict. Fitted battery, wiring and calibration remain provisional.

Own startup mode 2 initializes the keyboard with mask `3f`. Context
`10c660`, state word `10c664`, dispatches state 16 via table `25c59c` to
`25c732`. Continuation `25c7d2` requires phase `10ffac & 0f == 6` and report
bits `10c5c9 & 0f == 0f`. Reports `14/15/16/17` set bits `1/2/4/8`.
The open-pin control receives the last three and remains masked with `0e`.
The nominal profile receives `14` after the counter drains, unmasks columns
at `25c8c0`, and reaches state 4 / phase 3. State 4 is not treated as another
missing shutdown report.

The Menu fixture presses `COL.1`, mask `04`, at 3 and 5 seconds for 200 ms.
On the open-pin control, columns move `1f -> 1d -> 1f` but remain masked.
On the nominal profile, each press produces actual own-table reads
`raw=11 key=19` at `299044`. No IRQ, report, callback, UI state or display
type is forced. Native execution, completed averaging, report/unmask order,
the reviewed graphical prompt and physical decoding have separate checks.

## SIM and phone-side security boundary

Insertion is a run-local configuration of the physical `SIM_REMOVED` input,
not a firmware flag or injected message. The PIN-disabled synthetic card
completes 16 binary-file reads, all 50 ADN records (32 bytes each), and all
ten SMS records (176 bytes each), with status `9000`. No VERIFY CHV command
is issued. The last record completes before ten seconds; the acceptance
run observes at least twelve seconds. These are read-bootstrap results,
not proof of phonebook writes, SMS operation or SIM PIN authentication.

The seven-second frame is **Security code**, not a SIM PIN prompt. A
separate fixture presses five digit contacts and OK once. All six reach
the own decoder; the seventeen-second frame is **Code error**. Read-only
entry observation at `294454` confirms the submitted buffer is `12345\0`.
The four-byte comparison at `2946a0` receives equal values `d83b30d8` and
returns zero. The subsequent retry guard `29461c` receives `matched=1` but
reads `ff` at `10c6fc`; values >=5 return failure before the match can reset
the counter. Thus this result is retry rejection, not a mismatched code.
No additional candidate codes are tried. The initial integrity-only EEPROM
has no authentic phone identity or lock payload.

At OK, an EEPROM `000c`/eight-byte read reaches identity formatter `2593b8`
through wrapper `2869f2`. Encoder `294454` packs the submitted digits via
`28a1d0`, then XORs four bytes with identity characters 11--14 and the four
ROM bytes at `2b3a74`. This is an own-ROM contract, not a donor record codec.
Descriptor group 7 at `2aeaf0` maps `0701` to EEPROM `04a0`/eight bytes,
`0708` to `04a0`/four bytes, and `0709` to `04a4`/one byte. The coherent
run loads `04a0`/eight bytes into `10c6f8`; the retry byte is therefore
`04a4`, not identity address `000c`. A zero-attempt fixture may change only
that external NV byte; it must not overwrite the cached counter or verifier
result. With only initial EEPROM `04a4=00`, the same physical `12345` reaches
the same matching comparison, passes the retry guard and returns `accepted=1`
at `2946b2`. Two independent cold runs reproduce the seventeen-second frame
hash `df66881e695c25b4a157c80b40cf56ad246bb45a9d54c1054264222a639db931`.
That frame contains placeholder-like glyphs and is not itself an idle/menu
oracle. It is transient: at nineteen seconds a Menu softkey is present.
This explicitly synthetic retry fixture is not authentic factory provisioning;
it neither supplies a donor identity nor changes the cached counter/result.

Two physical Menu contacts at 20/22 seconds reach raw 11 / key `19` and open
Phone book followed by Search / Add name / Erase. The menu acceptance masks
only the animated icon rectangle `(0,20,76,18)`, retaining title, softkey and
scrollbar; the static submenu is checked byte-exactly. This establishes
application navigation, not contact writes, registration or speech. The
ordinary silent and open-pin controls remain unchanged.

### Physical contact save

The separate `--phonebook` fixture selects Add name, enters `A / 123` and
presses OK before the known restart deadline. The own firmware sends
`A0 DC 01 04 20` for EF_ADN record 1 at 27.585 seconds; the card commits
32 bytes and returns `9000`. The `Saved` frame is byte-exact across repeats
(SHA-256 `8f9188505262676c92fd1ec55e94d6e9dbf2854bc5d14fc4d2ac3dc0903d6909`).
The shared SIM checker verifies record 1 encodes `A / 123` and leaves the
other 49 ADN slots erased. This proves an organic write and serialized
storage. Cold readback is checked separately below; stable long operation
remains unproved.

```sh
.venv/bin/python tools/run_nse2_native.py tmp/nse2-contact-save \
  --record-checksum-only --nominal-vbat --insert-sim --security-code \
  --phone-retry-zero --post-auth-menu --phonebook --seconds 30
```

Startup acceptance remains scoped to its actual contract: readiness/report
14 before three seconds, and the complete SIM bootstrap before ten seconds.
A timestamped mailbox trace cap after startup is explicitly reported, not
treated as lost initial evidence. Early/unknown caps still fail. Later
application SIM reads do not count as duplicate bootstrap records; missing
initial records still fail. The contact gate separately checks physical
contacts, the write, successful status, storage bytes and Saved frame.

`verify-5110i-phonebook` runs three isolated processes. First it saves the
contact. Next it initializes a fresh phone instance with only the SIM NVRAM
from that save (recording its source SHA-256), repeats authentication and
physically searches for `A`. The resulting `A`/Call frame has SHA-256
`4755eca080af19866a8da926e4185de6b1508f6271a3f4d116dc590d12d0e76a`.
The entire saved card remains byte-identical, with no UPDATE RECORD during
search. Finally the same physical search on an erased card produces
`No phone numbers` (SHA-256
`34478708c13e25cc3cb0778a9fa40f9cb812c6eb56636909c1ce133a1d616900`).
Thus readback differs from editor text or a fabricated contact. This proves
cold SIM contact persistence and firmware retrieval, not displayed number
formatting, call signaling, registration or operation past the restart.

### Offline SMS draft save

Physical Menu / Messages / Write messages / Options / Save commits one
unsent `A` draft at 25.943 seconds. Firmware selects EF_SMS and issues
`A0 DC 01 04 B0`, with successful `9000` status. The 176-byte record
starts `07 02 81 f0 01 ff 00 81 00 00 01 41` and is padded with `ff`:
unsent status, one GSM7 character, no recipient yet. ADN remains erased
and the other nine SMS slots remain empty. The assertion is shared with
the independently accepted NSK-3 draft fixture in `sim_sms_draft.py`;
no product-specific address or radio behavior is shared by that helper.
Repeated native runs reproduce the `Message saved` frame (SHA-256
`491f056f1e92fdedbe7e334f423818c8b1770db0838a78bf25743e0ec2f0459c`).

`verify-5110i-sms-store` checks physical contacts, exact EF/record/length,
single successful commit, serialized draft/empty remainder and frame.
This is offline storage only. Cold Outbox readback, deletion/cancellation,
SMS-SUBMIT delivery and network registration remain separate acceptance.

## Long-run native receive boundary

A forty-second menu-only control and a slower contact-entry workflow both
request the same reset; contact entry is not its cause. Contacts entered
after that restart belong to a new security editor and are not a valid
phonebook test. The accepted save above completes before this boundary;
it does not waive the long-run failure.

Own receive dispatcher `29908c` uses context `10c764`; received packets
increment its halfword at `+2` (`29909e..2990a2`). Timer input `dd` checks
that count at `2991b8`; zero selects the unique argument-1 call to policy
`2902c2` at `299098`. A nonzero window clears the count and rearms timer
`1d` with `09cd` ticks. Policy reads NV descriptor `0607` and other guards;
its selected reason is therefore not proof of one physical missing signal
or a universally unconditional watchdog.

In the read-only capture, `28db6e` writes reason `68` to `10ff78`, with
LR `290361`, at 31.000862462 seconds. Delayed event `d5` reaches reset
entry `299c16` via `236168`, LR `23616d`, at 31.969434538 seconds. Native
execution then repeats its own verifier/identity startup. This is a
firmware-requested restart, not a CCONT watchdog expiry. The normal native
runner correctly rejects the repeated upload/control/poll sequence; its
acceptance has not been relaxed for this reset.

The bounded observer records reason writes and reset-entry stack without
changing firmware. Diagnose a retained capture with:

```sh
.venv/bin/python -m tools.nse2_lifecycle_check TRACE
```

The diagnostic reports `stable_long_boot=false`; it is not a successful
long-run gate. A guessed receive heartbeat, disabled supervision or
rewritten packet count must not be used to promote this native profile.
Resolve the actual native receive producer/transport and keep the acquired
NSE-1 mask's compatibility status explicit.

The passive first-window census rules out a wholly broken initial RX/FIQ
route. DSP `3805` publishes four producer changes at DARAM `08e4`, ending
at `00a9` by 0.942549 seconds. MCU consumer `08e5` catches up and own
dispatcher `2990a2` increments `10c766` to four. Lifecycle store `20feae`
clears that count at 4.701843 seconds; this is not the timer-`dd` clear.
At 7/15/30 seconds the RX producer/consumer are both `00a9`, with exactly
four observed native producer writes, and TX producer/consumer are both
`0041`. Native CPU samples are idle at `09ac`, with no illegal instruction
reported. These observations locate the gap after real initial packet
exchange, not at an assumed missing receive interrupt. They do not prove
which external DSP input or resident software state should produce the
next packet, nor prove the borrowed mask correct for NSE-2.

`nse2_lifecycle_check TRACE --transport` requires this complete window and
the actual count-clear observation alongside the restart evidence. It
rejects added/unconsumed RX publications or a non-drained TX ring instead
of labelling every reason-`68` reset as the same cause. Retain the functioning
initial transport as a positive control for further timing/completion work.

In the acquired program mask, word `09ab` is `f4e1` (IDLE); sampled PC
`09ac` is its continuation, not an unexplained busy loop. The following
`61f8 06bc 0001 / f820 09ab` tests work flag `06bc` bit 0 and branches
back to IDLE when it is clear. Aggregate writes disprove a missing work-bit
or stopped-scheduler explanation: by thirty seconds writer `32c1` sets
bit 0 in all 1,625 observed writes, while `446d` clears it in 1,628 writes.
The one-second sampled word `0820` and later idle PC samples must not be
promoted to a claim that the intervening work never runs. More specifically,
the runtime overlay places this IDLE inside routine `0926..0a26`, an advancing
nested measurement loop, not a generic scheduler entrance.

Own final MCU type-`1a` packet has header `441a` and 34 payload words:
`0011 9800` followed by 32 zeros. Native consumer `3909` advances TX from
`001e` to `0041` at 4.704069442 seconds. Handler writes `06e3=1` at `3db6`;
consumer `30cb` clears it immediately afterward. This is real request
consumption, not an unserviced TX request. The native thirty-second summary
also observes 679,232 port-`27` sample reads and 1,639 port-`32` writes,
but no port-`38/39` reads. Counts describe this capture, not fixed hardware
cadence or proof of valid RF acquisition. No further RX publication follows.
The next native question is processing/completion eligibility and its
external sample contract, not synthesizing a work bit or guessed ack.

### Result paths and complete host coverage

The processing fixture captures all 65,536 live page-zero program words at
seven seconds in `nse2_native_program.bin`. Its result audit reuses the
existing ROM4 publisher census and companion-mask qualification rather
than importing the sibling handset's uploaded addresses. The checked
snapshot retains nineteen raw direct CALLD candidates for `37ce`, no
direct B/BD candidates, and the same direct sites as the acquired mask.
This excludes neither indirect transfers nor other pages or direct ring
writes, and is not an autonomous-producer absence proof.

The shared `publisher_type_inventory` checks local AR4 immediate loads at
18 of the 19 sites: `314d→8a`, `3fe2→8e`, `3ff5→9a`, `4810→8b`,
eight sites in `4a11..4be9→74`, `4f20/50db→80`, `6073→88`,
`62fe→84`, `6ec6→83`, and `8e1c→8d`. The remaining `9404` site
inherits its type through the separately checked serial helper (`9a/9b`).
This is a checked local literal association inventory, not control-flow
reachability or proof of a periodic producer. In particular the eight
service-response sites must not be counted as eight new autonomous peers.
The static exporter supplies disassembly context for all nineteen sites.
The coherent thirty-second runtime census positively observes four entries
at `37ce`, all type `74`, with payload lengths `16,52,2,4`. Direct sites
`4b1a`, `4ba8`, `4a21` and `4a11` execute once each; the remaining fifteen
reviewed direct sites execute zero times. These counts agree with the four
native RX producer advances and MCU consumption. This attributes initial
traffic to control responses, not a periodic heartbeat or later measurement
results. The gate requires both positive common-entry coverage and all
nineteen site summaries; it still does not close indirect/direct-ring paths.

The type-`8b` publisher is `47f6`, loading type `008b` at `47ff` and
calling `37ce` at `4810` with 162 payload bytes. Its three page-zero direct
caller candidates are `0a17`, `3cab` and `6d7e`. The first follows the
nested measurement loop; this own overlay uses `0a17`, not the sibling
NSK-1 upload's `0a2c`. Companion table `b00f + 4b` selects `3c92`, which
reaches `3cab`: it is an explicit host-query path. The other caller is
behind the `06bc` mask-`0100` test at `6d19` and `06ab==0` at `6d6f`.
The byte-checked path at `6790` loads `06bc` into AR2 and `06e3`
into AR0. A nonzero mask-`0020` test consumes that control bit at
`679b`, initializes `06ab` at `67a1`, then sets the eligibility bit
at `67a3`. This identifies a conditional setter, not its runtime
execution or a complete writer census. The next boundary is the origin
and consumption of `06e3` bit `0020`; a generic write to `06e3` is
insufficient evidence that this request was armed. Both complete thirty-second
host-census captures contain exactly four `06e3` writes, matching the terminal
tap count: `0000/0000/0001/0000` at PCs `0f10/0d94/3db6/30cb`.
None arms `0020`. This bounds the observed conditional-request absence;
it does not close all possible setters or exclude other result paths.
The companion dispatch table uniquely maps host type `07` to `3c04`.
That handler returns if `06bc & 0100` is already set; otherwise it
sets `06e3 & 0020` at `3c15` and configures the measurement parameters.
The checked handler prefix and table mapping identify a request-driven
path, not an autonomous keep-alive. Type `07` is absent from the complete
host captures. The nearby `ORM 0020` at `3d4b` instead addresses `1838`
through AR4 and must not be attributed to `06e3` from proximity alone.
An own MCU constructor candidate at `2160a8` supplies a one-byte
type-`07` message (`10`) to task 3 at `216172`. Its mode check
(`2160f4..2160fc`, literal `216388`) requires byte `1094b7 == 4`;
additional context checks remain between that gate and construction.
The two aligned direct-call candidates are `216250` and `21764a`.
The generic envelope boundary `277f96` takes its type from buffer byte 0
and payload length from R0; `28de42` passes message byte 3 onward there.
This maps a concrete MCU candidate, not normal-boot reachability or a
complete census of type-`07` producers. The next question is the lifecycle
of `1094b7` and those callers, not forcing this request into the stream.
The coherent native-processing run observes exactly three writes to that
byte: `00` at `20010c` (0.00265 s), `01` at `20cafc` (1.12047 s),
and `02` at `20feb6` (4.70184 s). The thirty-second terminal count
accounts for all three and the value remains `02`; the constructor entry
tap records zero entries. Thus this boot does not enter the candidate's
mode-4 lifecycle. Absent type `07` is not evidence that it must be added.
The active type-`1a` measurement loop remains the next native result path
to resolve. These are passive observations on the unchanged physical-menu
fixture, not firmware-state edits or proof of all possible call routes.

Passive first-eight measurement stage observations separate setup from
processing: `0990→099d` averages 1.24639 ms, `09a6→09b1` averages
13.70942 ms, and successive `0990` entries average 14.96504 ms.
Mode is `000c` at `09a6` and `0004` with the completion work bit set
at `09b1`. The byte-checked setup writes `0546` to port `0e` at
`0957`; the current CTSI model converts its 1351-count period to
1.24708 ms. Each measured iteration therefore takes about twelve modeled
periods, predominantly in the completion wait rather than setup.
This is a timing decomposition, not evidence that the reload, clock,
interrupt cadence or execution-cycle model matches physical NSE-2 hardware.
The next distinction is which processing/frame transitions account for
those eleven wait periods; changing the clock to beat supervision would
not answer it.
The first completion wait now has a counter-level explanation: eleven
`3204` entries arrive at consecutive modeled frame periods. Their
`(mode, skip, accumulation)` sequence is
`(c,1,1),(c,0,1),(c,2,0),(c,1,0),(c,0,1),`
`(4,2,0),(4,1,0),(4,0,4),(4,0,3),(4,0,2),(4,0,1)`.
Five enter the skip path `32c5`; six reach processing at `326d`.
The second mode-`c` processing pass reaches `32b3` and clears mode bit 3;
the fourth mode-`4` accumulation reaches `32be` with accumulation zero
and sets the completion work bit. No missing frame is observed in this
wait. This explains the measured pacing as firmware counter progression
under the current model; it does not validate physical timing or the
MCU receive-supervision policy. Compare the full scan workload with that
policy before proposing a timing change.

A coherent forty-second failure capture combines the stage observations,
advancing loop and reason-`68` restart. The checked loop constants imply
`8 * 2 * 124 = 1984` measurements. Extrapolating the first-eight measured
iteration period gives 29.69063 seconds of measurement work, ending near
34.43286 seconds from a 4.74223-second start, even before outer-loop gaps.
The observed policy acts at 31.00086 seconds, 26.29906 seconds after arm.
This is a same-run projection, not observed completion or a proven lower
bound for every later iteration. It demonstrates the current timing-budget
mismatch without choosing between physical peripheral timing, mask/profile
compatibility and erased-NV policy. The ordinary runner still fails this
capture for repeated startup; no timeout or acceptance requirement was relaxed.

```sh
.venv/bin/python tools/nse2_native_result_check.py RUN/nse2_native_program.bin \
  --failure-trace RUN/error.log --mcu roms/noki5110i/5110i_5.53A.fls
```

The failure diagnostic requires the initial thirty-second progression and
the same capture's timer/restart observations. Its output retains
`scan_completion_observed=false` and `root_cause_proved=false`.
None of these static paths establishes an unconditional periodic packet.
The separate serial publisher retains its existing queued-data contract.

Host capture checks every pointer advance and payload length against the
final consumer-write count. All 21 consumed packets through thirty seconds
are covered; observed types are `05/0d/1a/3c/51/70`, with no `4b` in this
window. The 154-byte type-`3c` packet is valid in the 82-word ring; a former
128-byte probe cap omitted it. The bound now covers all 160 payload bytes
the ring can hold, and invalid/skipped/discontinuous records fail coverage.
This runtime absence is not a ROM-wide producer exclusion.

Live CPU exports also sample internal TIM=`b0ba`, PRD=`ffff`, TCR=`0010`
at 1/7/15/30 seconds: the core timer is stopped in these samples. Reading
backing data space would not establish these internal MMR values. This
does not explain why it is stopped or establish a missing timer interrupt.

`verify-5110i-native-processing` combines these checks with the unchanged
physical menu and failure-boundary predicates. For separate static work:

```sh
.venv/bin/python tools/nse2_native_result_check.py RUN/nse2_native_program.bin
```

Add `--trace RUN/error.log` to verify that every tapped `06e3` write is
accounted for through thirty seconds before asserting this request bit
was absent. Missing/capped writes, a changed terminal value, or an armed
request fail that diagnostic rather than silently preserving the conclusion.
Use `--mcu roms/noki5110i/5110i_5.53A.fls` to additionally check the own
constructor bytes, mode literal and aligned direct-call candidates.

The result explicitly leaves native completion, fitted mask identity,
unconditional heartbeat and indirect-producer closure unproved.

Check the independent scheduling and transport observations with:

```sh
.venv/bin/python -m tools.nse2_lifecycle_check TRACE --schedule --transport
```

This accepts a thirty-second pre-reset observation as diagnostic evidence
while still reporting `stable_long_boot=false`. It requires the own packet
body and positive bit-set/clear observations; it does not claim all host
packet formats have been decoded or that the borrowed mask is fitted.

The processing gate reuses the existing passive ROM4 codec/mode/comparison/
transfer/RF-consumer observers; it does not substitute NSE-1's expected
processing counts for NSE-2 evidence. In two cold thirty-second runs, only
two mode-1 frames execute (`32f4`), with sixteen filter-loop entries
(`330a`), two early reductions (`332b`), no ordinary reduction (`3347`),
and one `33ac` control continuation. Modes written to `00ac` progress
`0 -> 1 -> 2 -> c -> 4` at 4.717--4.750 seconds, then alternate `c/4`.
Meanwhile `09a4/4414` run more than 1,600 times and common frame exit
`33f3` runs more than 21,000 times. Thus the later control loop must not
be described as sustained mode-1 filtering.

All 679,232 observed port-`27` reads are zero, as are observed reduction
outputs. That is the unattached sample source actually modeled; it does
not establish electrical units or prove nonzero samples alone would
produce a packet. Resident `4dce` initializes mode 1 then mode 2 through
`4414`, with intermediate waits through `407c`. The later `09a4` call
selects mode `c`; the frame path clears its bit 3 at `32b3`, explaining
the observed `c/4` alternation without requiring a synthetic transition.
`verify-5110i-native-processing` verifies this known pre-reset boundary
and the physical menu together. Its result explicitly retains
`stable_long_boot=false` and `acquisition_proved=false`; thirty seconds
does not cover or waive the known restart deadline.

### Measurement progression and timing

The uploaded low program overlay must be read after boot: cold data RAM at
these program addresses is zero. `nse2_native_control_export.lua` exports
the selected control ranges and publisher contexts at seven seconds through MAME's read-only
debugger command API, then exits. Use a separately prepared native run,
`-debug -debugger none -autoboot_delay 0 -seconds_to_run 8`, and this file
as `-autoboot_script`. It writes run-local `nse2_control_*.dasm`; it is a
static-inspection instrument, not a handset acceptance run. A startup
`-debugscript` with the none provider did not provide these exports.

On the measured request path, `092d` compares `0284` with 1. Its other
branch writes `0285=007b` at `0934`; `097f` initializes outer AR6 to 7.
The inner loop loads AR5 from `0285`, advances through `0990..09c2`, and
uses repeat flag `0e13` to make two inner passes. `09cb` decrements the
outer counter and branches back. This establishes the loop structure,
not the electrical meaning of its samples or the correct physical cadence.

Positive fetch observations after register restoration show AR5=`ffff`
at each inner-loop exit. AR6 descends `7,6,5,4,3,2`, with the final
observed inner pass at AR6=1 by 29.793511 seconds. The first paired exits
are 6.597880/8.453530 seconds; AR6=2 exits at 25.927572/27.783223 seconds.
The native sequence is advancing but unfinished before the known restart;
an idle PC sample is not proof of a stalled measurement. Do not change
sample values, loop counters or the supervision deadline to make it finish.
Compare event/clock timing and protocol-owned activity against this actual
sequence before deciding whether the next missing boundary is RF data.

Policy descriptor `0607` maps to the one-byte EEPROM record at `049e`.
Erased content enables the observed argument-1 reason-`68` policy, but
there is no accepted factory value or semantic permission to disable it.
Own ROM initialization supplies stronger evidence than an inferred factory
value: function `290490` loops over indices 1 and 0, takes source bytes
from `2b3764` (`01 ff`) and record IDs from `2b3768` (`0401 0607`), and
calls record writer `290de2` at `2904a0`. The write wrapper reaches physical
EEPROM writer `293c8e` at `290e12`. Thus this initializer explicitly pairs
record `0607` with `ff`, not zero. Its direct caller `23565a` is conditional
on argument bit 1 (`LSRS #2` carry); ordinary boot execution of this
initializer has not been established. This is not a reader-only record.
Policy `2902c2` interprets bit 0 for argument-1 reason `68` and bit 1 for
argument-2 reason `69`, with additional runtime guards before restart.
Neither a complete writer census nor a universal factory default follows
from this initializer, but clearing the erased byte to zero cannot be
justified as restoring its ROM initialization value. The `--mcu` audit
pins the paired tables, writer wrapper and caller branch as well as the
measurement request constructor.
The diagnostic option `--scan-progress` requires the advancing loop in
addition to `--schedule --transport --processing`; it explicitly leaves
`scan_completed=false`.

### Receive-timer boundary

The observed timer API start is at 4.701797769 seconds: `254d3a` receives
timer `1d`, ticks `09cd`, LR `20fead`, with node state 1 and zero stored
ticks beforehand. It is not enough to describe the deadline simply as a
31-second boot-relative timer. The first observed FIQ5 status read at
`295644` is at 31.000004231 seconds: status `22`, Timer-1 current and
destination both `7fff`. The reason-`68` policy follows at 31.000862462,
26.299064693 seconds after the API start. This is ordered observation,
not proof that FIQ5 is defective or that changing a rate would be faithful.

MAD2 retains the calibrated Timer-0/Timer-1 rates documented in
[MAD2 fidelity](mad2_fidelity.md). The native DSP compare model separately
uses the 13 MHz/12 reference. Ordinary queue selection and expiry are
identified below; physical divider provenance and the native completion/
activity contract remain distinct questions. Ports `09/0a/0b` written by resident `4414` remain
storage in the backend; their event/sampling ownership is not established
by this timing coincidence. No new rate, compare behavior or policy value
has been selected from the desired boot result.

For a forty-second processing capture, the native runner correctly fails
on the repeated startup. `nse2_lifecycle_check TRACE --timer-boundary`
requires the raw arm/terminal/reset evidence and reports
`hardware_timing_fault_proved=false`. It is a failure diagnostic, not a
replacement long-run acceptance oracle.

The queue insertion at `254c7c` receives adjusted ticks `0b9b`, node/global
flags `03/03`, and active LR `254e5f` in system mode `1f`. The byte-pinned
selector ANDs these bytes and executes `LSRS #2; BLO`: carry is original
bit 1, not bit 2. Thus `(global_flags & node_flags & 02) != 0` chooses
global offset `10`; the other branch chooses offset `28`. Positive
observation at join `254c92` reads selected base `100020`, confirming
head address `100030` (offset `10`) for timer `1d`.
At expiry, active FIQ LR `255535` identifies walker `2554e4`, servicing
that same offset-`10` queue. Its cumulative target R4=`0b9b`,
elapsed banked R8=`0f11` and threshold R6=`0f8e` satisfy the decoded
`elapsed + 125` batching rule. The target has passed by 886 software ticks;
this is not a native-completion report fabricated by a device.

The expiry trace's user-bank LR `295d79` belongs to interrupted foreground
code, not the queue walker. ARM7 exception-time observations must select
register banks by CPSR; see [the cleanup traps](research_cleanup.md).
`--timer-queue` requires the bank-correct caller, positive selection and
expiry arithmetic, and reports `queue_migration_required=false`. There is
no evidence of an inter-queue promotion to recover. The former opposite
selection was a shift/carry decoding error; its link-watching probe is
removed. A passed deadline in this FIQ5-serviced queue is not alone proof
of late service or a hardware-clock defect. Resume at the native activity/
completion contract rather than changing queue state or timer rates.

## Remaining work

1. Recover native receive activity across the first timer-`dd` window. Short
   contact creation and cold readback are accepted independently; operation
   past the restart remains unproved.
2. Recover semantic EEPROM requirements from their own consumers; retain
   integrity-only fixtures as diagnostic inputs, not factory defaults.
3. Extend the accepted application-menu path to longer lifecycle/save-state tests.
4. Keep fitted flash/RAM/DSP/clock/analog evidence distinct from compatibility;
   no registration, native RF or speech claims without their own acceptance.
