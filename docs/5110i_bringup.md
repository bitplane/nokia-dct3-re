# Nokia 5110i / NSE-2 bring-up

## Current boundary

Generated MAME packages use lowercase `5110i_5.53a.fls` for ROM-label
validity. This is a packaging name, not a change to the acquired source bytes.

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
unsent `A` draft on the SIM; a fresh cold process opens it in Outbox and
reads `A` without rewriting the card. Registration, calls and network SMS
are not accepted.
Separate card gates cover CHV1 authentication/change/cold acceptance,
retry exhaustion, PUK1 recovery and permanent card rejection; CHV2 change,
retry exhaustion and PUK2 recovery/permanent refusal; and protected FDN
record management with a local
On/Off dialing-permission contrast. These are handset/SIM software contracts,
not native network or speech acceptance. SIM authentication and the unrelated
phone-side lock are checked separately.
Longer execution is not stable: both menu-only and contact-entry runs
request reason-`68` reset at about 31.97 seconds. The short menu gates do
not cover this boundary.
Fitted mask identity, DSP clock and measured board calibration are unproved.

The ordinary `noki5110i` profile remains unchanged: erased EEPROM and a
silent DSP reproduce CONTACT SERVICE. `nse2r4t` supplies the compatibility
DSP but retains open-pin ADC2; it completes DSP startup while the analog
readiness counter cannot drain. These are explicit controls, not alternative
claims of usable boot.

The native boundary is receive-activity supervision during an advancing
full-scan request. The full bitmap is an explicit MCU default, not missing
provisioning. A separate pending native word transmission has decoded
state/status handling but no evidenced peripheral clock/completion source;
its connection to receive supervision is unproved. Preserve the initial
transport and ordinary timer expiry; do not invent heartbeats, shorten the
scan or retime clocks to evade the restart. Independent short SIM/software
work remains possible without claiming that this native boundary is closed.
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
make verify-5110i-sms-persistence RUN_DIR=tmp/nse2-sms-persistence
make verify-5110i-sms-delete RUN_DIR=tmp/nse2-sms-delete
make verify-5110i-sim-pin RUN_DIR=tmp/nse2-sim-pin
make verify-5110i-sim-pin-persistence RUN_DIR=tmp/nse2-sim-pin-persistence
make verify-5110i-sim-pin-blocked RUN_DIR=tmp/nse2-sim-pin-blocked
make verify-5110i-pin1-change RUN_DIR=tmp/nse2-pin1-change
make verify-5110i-pin-request-lifecycle RUN_DIR=tmp/nse2-pin-request
make verify-5110i-puk1-retry RUN_DIR=tmp/nse2-puk1-retry
make verify-5110i-puk1-exhaustion RUN_DIR=tmp/nse2-puk1-exhaustion
make verify-5110i-fdn-dial-policy RUN_DIR=tmp/nse2-fdn-policy
make verify-5110i-native-processing RUN_DIR=tmp/nse2-native-processing
make verify-5110i-native-ui-state RUN_DIR=tmp/nse2-native-ui-state
make verify-5110i-sim-write-state RUN_DIR=tmp/nse2-sim-write-state
make verify-5110i-sms-write-state RUN_DIR=tmp/nse2-sms-write-state
make verify-5110i-pin-state RUN_DIR=tmp/nse2-pin-state
make verify-5110i-pin-block-state RUN_DIR=tmp/nse2-pin-block-state
make verify-5110i-puk-state RUN_DIR=tmp/nse2-puk-state
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
| SIMI | Own `286520` initializes standard IIR/FIFO controls; SIM bootstrap, physical CHV1 verification/change/cold acceptance/retry exhaustion/PUK1 recovery, PIN2 change/retry exhaustion/PUK2 recovery/permanent refusal, organic ADN write and cold contact readback pass. FDN settings, protected record lifecycle and local On/Off dialing-policy contrast pass. | Network-connected call behavior remains unqualified. The Nokia phone code is phone-side. |

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

### SIM PIN authentication

`verify-5110i-sim-pin` uses the laboratory card's persistent PIN-enabled
profile, not a handset EEPROM change. Its three fresh processes establish:

- With no key input, `Enter PIN code` remains visible; no VERIFY occurs and
  the credential/retry prefix is unchanged.
- Physical `1234` and OK issue `A0 20 00 01 08` at 7.663 seconds. The card
  returns `9000`, retains three retries and unchanged credentials; firmware
  shows `Code accepted`, then the separate Nokia `Security code` prompt.
- Physical `1111` issues one VERIFY returning `9804`. Only persistent CHV1
  retries change, from three to two. Firmware shows `Code error` and returns
  to `Enter PIN code`.

The successful run additionally checks all 16 binary files, 50 ADN records
and ten SMS records. Protected IMSI/ADN/SMS reads must follow successful
VERIFY. The fourteen-second capture accounts for the physical PIN-entry
delay; ordinary PIN-disabled bootstrap retains its ten-second bound.
The accepted frame/status/credential contrast is not phone-code unlocking,
PIN management, PUK recovery or network registration. Legacy card profile
serialization is expanded by the card device; the checker pins its complete
credential/retry prefix rather than claiming a byte-identical file length.

`verify-5110i-sim-pin-persistence` then links three isolated processes:
wrong entry creates a saved two-attempt card; a fresh no-input boot retains
two attempts at the PIN prompt; another fresh boot receives the same saved
card and physical `1234` restores three attempts with `9000`. These saved
cards use the complete current serialization, so the latter two runs
compare every byte against the input, allowing only the retry reset on
successful VERIFY. Input path/hash and before/after retry counts are recorded.
The fixture accepts only the declared laboratory credentials and explicit
retry-state inputs. The distinct `verify-5110i-sim-pin-blocked` gate drives
three real wrong attempts across saved cards: `9804` with two attempts,
`9804` with one, then `9840` with zero. Each card changes only its retry byte.
The final error displays `SIM card blocked` then `Enter PUK code`, retaining
the original PIN and all ten PUK attempts. A fresh process receives that
organically exhausted card, shows the transient blocked notice at seven
seconds and reaches the PUK prompt at eleven seconds without any key input,
VERIFY or UNBLOCK command. The full saved card remains unchanged. This is
PIN exhaustion/cold blocking, not PUK authentication or recovery.

`verify-5110i-sim-puk-recovery` runs the complete exhaustion gate followed by
physical recovery and cold replacement-PIN authentication. Starting with the organically exhausted
card, entering PUK `12345678`, replacement PIN `2468`, and its confirmation
produces `A0 2C 00 00 10` at 18.266 seconds and status `9000`. The serialized
card changes only the four PIN1 digits and CHV1 retry byte (zero to three);
both PUK retry counts remain ten. The phone reaches its separate Security
code prompt. A fresh process importing that complete card accepts physical
`2468` through VERIFY (`A0 20 00 01 08`, `9000`), completes all sixteen binary
file reads, fifty ADN reads and ten SMS reads before fourteen seconds, and
leaves the entire card unchanged. No phone-side authentication or sustained
native RF operation is established by these probes.

`verify-5110i-puk1-retry` starts from the card saved by three actual PIN1
exhaustion. Physical `12345675` and twice-confirmed replacement `2468`
issue `A0 2C 00 00 10` at about 18.266 seconds. Status `9804` changes only
PUK1 retries from ten to nine; PIN1 remains blocked, its old credential
remains unchanged, and the phone returns to the PUK prompt. A fresh
no-input process imports that exact saved card, shows the blocked notice
then PUK prompt and preserves all bytes, including nine remaining attempts.
`tools/nse2_puk1_check.py` checks physical contacts, exact protocol/status,
complete-card deltas and frames. Correct PUK recovery from this nine-attempt
card then resets PIN1 retries to three and PUK1 retries to ten, changes only
PIN1 to `2468`, and reaches the phone-code prompt. Cold physical `2468`
authentication completes protected SIM reads and preserves the full card.
`tools/run_nse2_puk1_retry.py` links all seven processes, records the
wrong-PUK input source/hash and retains each stage's checked artifacts.
`--puk-attempts 9` explicitly declares the reduced-retry input to the
existing blocked/recovery runner; its default ten-attempt validation remains
unchanged, and reduced counts are forbidden for unrelated PIN actions.
`verify-5110i-puk1-exhaustion` separately chains three wrong PIN entries,
ten physical wrong PUK entries and cold permanent refusal: fourteen fresh
processes. Each PUK failure changes only its retry byte; failures ten through
two return `9804`, while the one-to-zero transition returns `9840` and
displays `SIM card rejected`. The original PIN1/PIN2/PUK credentials and
independent PIN2/PUK2 retry counts stay unchanged. A cold boot importing the
actual exhausted card repeats the physical PUK/new-PIN entry sequence but
issues no security or write command, preserves the complete card and retains
the rejected-card frame before and after input. This is whole-SIM PIN1
security rejection, distinct from permanent PUK2 loss with usable unrelated
menus. `--exhaust` on the retry runner selects this branch instead of correct
recovery; the shared checker explicitly distinguishes zero-retry input from
recoverable cards. No credential recovery or network-service claim follows
from permanent refusal.

The recovery checker requires the complete saved blocked card, exactly one
UNBLOCK command following the 38 physical contact edges, successful status,
the PUK and phone-code frames, and the exact permitted storage delta. The
cold authentication checker still rejects UNBLOCK commands and requires
VERIFY before protected SIM reads. No handset state or credentials are
rewritten by either input fixture.

### PIN2 and PUK2 lifecycle

#### PIN1 change prerequisite and cold acceptance

`verify-5110i-pin1-change` establishes that Change PIN is gated by PIN-code
request being enabled. On the PIN1-disabled laboratory card, selecting the
item displays `Set PIN code request on`, sends no CHANGE and preserves the
complete card. Continuing to type credentials after that notice instead
becomes ordinary dialing input, so a key script without protocol checks
would misattribute the result.

With the same laboratory credentials and PIN1 explicitly enabled before
boot, physical `1234` authenticates CHV1 at about 7.663 seconds. The separate
phone-code fixture then authenticates the handset, and Settings / Security
settings / Change access codes / Change PIN accepts old `1234`, new `2468`
and its confirmation. `A0 24 00 01 10` at about 28.724 seconds returns
`9000` and displays `PIN code changed`. Only the four PIN1 credential bytes
change in the complete saved card; the enabled flag, retry counts, PIN2,
PUKs and application data remain unchanged. A fresh process imports that
exact card and the existing `--pin-action reauth` fixture successfully
verifies physical `2468`, completes protected SIM bootstrap and preserves
the whole card. This cold process reaches the separate phone-code prompt;
it does not claim phone authentication or native registration.

`tools/run_nse2_pin1_change.py` prepares a fresh full card, then runs the
disabled and enabled menu branches independently and links the actual
changed card into the existing cold PIN-authentication runner. All four
processes are isolated; source/card/executable hashes are recorded. The
checker requires physical boot/menu contacts, exact commands, successful
statuses, complete-card transitions and the refusal/change frames. The
cold stage retains the existing PIN bootstrap checker. The phone-code
retry fixture, nominal ADC input and acquired NSE-1 DSP-mask compatibility
assumption remain explicit; this is not authentic NSE-2 provisioning.

`verify-5110i-pin-request-lifecycle` establishes the toggle order independently
of the prepared enabled-card fixture. Select PIN code request, enter `1234`
and OK, then choose On or Off. The PIN is collected before the choice;
there is no preliminary VERIFY in the disabled-card On path. On sends
`A0 28 00 01 08` around 23.821 seconds and receives `9000`; only enabled
byte 3520 changes from zero to one. A fresh existing PIN-verification
runner imports that exact card, authenticates `1234`, completes protected
bootstrap and preserves every byte. A separate cold phone authenticates
its enabled SIM and phone code before selecting Off, sending
`A0 26 00 01 08` with `9000`; only byte 3520 changes back to zero.
A final cold boot imports that disabled card, sends no SIM security command,
preserves every byte and physically authenticates the separate phone code.
The existing PIN1 runner's `--request-lifecycle` mode links five isolated
processes including preparation and the accepted cold PIN-verification
runner. Unlike the independent CHANGE prerequisite fixture, it never edits
the input card's enabled flag: each stage imports the actual predecessor.
Exact toggle frames, boot/menu contacts, SIM commands/statuses and
complete-card transitions are required. Final cold-Off phone authentication
is checked separately from the absent SIM authentication. Source, card and
executable hashes remain part of each run's input record.

#### Fixed-dialling settings lifecycle

`verify-5110i-fdn` prepares a fresh card through the existing accepted SIM
bootstrap, then four isolated thirty-second native-compatibility runs establish the FDN
settings lifecycle on the explicitly FDN-capable laboratory SIM, not on the
default card. `sim_fdn_profile.extend(card, True)` supplies ten empty FDN
records and permits ADN access while invalid. PIN1 is disabled, PIN2 is
`5678`, and the separate phone-code retry fixture remains explicit.
Physical Settings / Security settings / Fixed dialling / On followed by
PIN2 produces successful CHV2 VERIFY (`A0 20 00 02 08`), INVALIDATE of
ADN (`6F3A`), and REHABILITATE of LOCI/IMSI (`6F7E`, `6F07`). Only byte
3575 changes from one to zero in the complete 3898-byte saved card.
A new process importing that exact card displays `Fixed dialling: On`,
performs the LOCI/IMSI rehabilitation during startup, and preserves every
card byte. Physical Off and PIN2 in another process rehabilitate ADN and
change only byte 3575 back to one. The final cold process displays Off,
issues neither VERIFY nor file-validity commands, and preserves the card.

`tools/run_nse2_fdn.py` links the exact saved card between phases and records
source/input/executable hashes, complete before/after cards, physical input
scripts, SIM traces and snapshots. `tools/nse2_fdn_check.py` verifies
all four phases, including exact card deltas, command ordering,
successful CHV2/file-validity statuses and physical contact timing. Its
negative tests reject protocol, input and storage corruption. The runner
also verifies phone-code acceptance and the exact cold On/Off frames.
The settings gate does not establish fixed-number editing, outgoing-call
restriction, network operation, or native stability beyond the known restart
boundary.

`verify-5110i-fdn-records` adds protected records through the separate Fixed dialling / Numbers menu with
FDN Off. Physical Add, PIN2 `5678`, name `A` and number `12345` issue CHV2
VERIFY (`9000`) followed by `A0 DC 01 04 20` to EF_FDN (`6F3B`) at about
27.69 seconds. The complete card changes only record one's encoded name,
length, TON/NPI and BCD digits; the other nine records, ADN, credentials and
validity stay unchanged. A cold Search with held `#` displays `12345`, reads
record one without PIN2 VERIFY and preserves the complete saved card. A
fresh empty-card control supplying PIN2 `5675` receives `9804`, changes only
PIN2 retries from three to two, and sends no UPDATE RECORD. The runner's
`--records` option links eight fresh processes including preparation and
the four settings phases. Write imports the final Off card; read imports
the successful write, while the wrong-PIN2 branch independently imports
the retained empty Off card. `check_record` in `tools/nse2_fdn_check.py`
checks protocol, physical input and complete-card transitions, and the
runner verifies phone authentication and exact result frames. This proves
record creation/readback and failed-write protection, not active-FDN dialing
restrictions.

`verify-5110i-fdn-edit-lifecycle` adds five processes to that chain. Physical
Search / Options / Edit authenticates PIN2, retains the name and replaces
the final digit with `6`. Successful EF_FDN UPDATE changes only byte 3600
from `F5` to `F6`, representing `12345` to `12346`. A cold held-number view
displays `12346` without VERIFY or storage changes. Erase cancellation
authenticates PIN2 but submits no UPDATE and preserves the whole card.
Confirmed erase issues one successful UPDATE of record one to all `FF`;
the final cold Search displays `No phone numbers`, performs no security or
write command and preserves the erased card. Exact result frames and
complete-card transitions accompany physical contact/protocol checks.
The full lifecycle uses thirteen isolated processes including preparation;
it does not establish active-FDN dialing restrictions or native stability
beyond the known restart boundary.

`verify-5110i-fdn-dial-policy` extends the complete record lifecycle with
four independent processes. Activation imports the previously written and
cold-read `A/12346` card, not a seeded number list, and changes only ADN
validity through the established physical On/PIN2 path. Listed `12346` and
unlisted `54321` each start from that same active saved card. The listed
number reaches the firmware call-progress presentation; the unlisted number
displays `Call not allowed`. A matched `54321` attempt from the retained
FDN-Off card reaches the same call-progress frame as the listed attempt.
All three preserve complete card storage and issue no application-phase
security or write command. Active-card boots perform only their expected
LOCI/IMSI rehabilitation. Physical digit/contact timing, pre-Call number
frames, result frames, phone authentication and source hashes are checked.
This seventeen-process gate proves a local FDN permission contrast, not
native registration, a connected call, speech, or long-run stability. A
call-progress image or elapsed counter is not network acceptance evidence.

`verify-5110i-pin2-change` uses two isolated thirty-second processes with
PIN1 disabled and the explicitly declared phone-code retry fixture. Physical
phone-code acceptance precedes Settings / Security settings / Change access
codes / Change PIN2. The first process supplies current PIN2 `5678` and
confirms replacement `2468`; firmware sends `A0 24 00 02 10` at about
29.072 seconds, receives `9000`, and renders `PIN2 code changed`. The card
changes only PIN2 credential bytes within the supplied legacy profile;
the device appends its current-format fields. PIN1, PUKs, retry counts and
the supplied application storage remain unchanged. The second process imports that complete card,
physically supplies `2468`, and restores `5678` through another successful
CHANGE. Its complete-card comparison proves persisted credential acceptance
and rejects unrelated mutations. The gate checks physical contact ordering,
all five prompt/result frames and exactly one CHV2 CHANGE per process.
This is ordinary PIN2 management, not PIN2 exhaustion/PUK2 recovery, FDN,
network service or operation past the known native restart.

`verify-5110i-pin2-retry` separately chains wrong entry, cold correct change
and cold reverse change. Entering old PIN2 `5675` still opens both new-PIN
editors: the firmware does not authenticate it at the first OK. Only the
final CHV2 CHANGE returns `9804`, shows `Code error`, and decrements PIN2
retries from three to two without changing credentials or PIN1 retries.
The next process imports that exact saved card; correct `5678` changes
PIN2 to `2468` and restores its three retries with `9000`. The final cold
reverse change accepts `2468` and restores `5678`. Both cold stages compare
the complete card, not only the legacy credential prefix. The checker reads
the CHV2 retry field explicitly; the unchanged CHV1 count is not a proxy.

`verify-5110i-pin2-blocked` chains three actual wrong CHANGE transactions,
persisting retry counts two, one and zero. The final status is `9840`
instead of `9804`, and the final frame is `PIN2 code blocked`. PIN2 itself,
PIN1 retries and both PUKs remain unchanged. A fresh fourth process imports
the complete zero-retry card, authenticates the phone and navigates to
Change PIN2 without submitting credentials. Its earlier frames show the
blocked notice; later frames show `Enter PUK2 code`. No VERIFY, CHANGE or
UNBLOCK is sent in that cold control, and every card byte is unchanged.
This proves organic PIN2 blocking and its cold recovery prompt, not PUK2
authentication or recovery.

`verify-5110i-puk2-recovery` includes the complete exhaustion/prompt gate,
then imports its zero-retry card into a fresh phone. Physical PUK2
`87654321` and confirmed new PIN2 `2468` produce `A0 2C 00 02 10` with
`9000`, followed by the firmware's automatic `A0 20 00 02 08`, also `9000`.
The result frame is `Code accepted`. Full-card comparison permits only PIN2
credential changes and its retry reset from zero to three; PIN1, both PUKs,
PUK attempts and unrelated storage remain unchanged. Another fresh process
accepts `2468` for ordinary CHANGE and restores `5678`.

The recovery fixture physically enters the phone code at 9–12 seconds,
instead of the ordinary fixture's 12–15 seconds, and starts menu navigation
at 15.5 seconds. It waits until the PUK2 editor is visible before entering
credentials at 24.5 seconds. The exchange finishes around 29.08 seconds,
before the known native restart. The verifier checks this explicit earlier
input schedule without relaxing the default, and permits only the reviewed
late CHV2 VERIFY after the unchanged PIN1-disabled bootstrap. No firmware
timer, clock, state or input-dispatch result is altered. This remains a
short-run SIM recovery test, not native long-run acceptance.

`verify-5110i-puk2-retry` includes organic PIN2 exhaustion, then four more
cold stages. Wrong PUK2 `87654325` submits one UNBLOCK returning `9804`:
only PUK2 attempts change from ten to nine, PIN2 stays blocked and unchanged,
and no automatic VERIFY is issued. A fresh no-credential menu observation
retains nine attempts, the PUK2 prompt and byte-identical card storage.
Correct PUK2 on that saved card restores PIN2 attempts to three and PUK2
attempts to ten, changes PIN2 to `2468`, and is followed by automatic VERIFY.
The final cold reverse CHANGE accepts `2468`. Every saved-card transition
checks the whole input card; PIN1 and PUK1 are independent of this retry path.
Permanent loss is covered by the separate exhaustion gate below.

`verify-5110i-puk2-exhaustion` supplies that separate permanent-loss proof.
It first rebuilds organic PIN2 blocking, then links ten fresh wrong-PUK2
processes from ten attempts down to zero. Each source path/hash and complete
card transition are checked; only the PUK2 retry byte may change. The final
UNBLOCK returns `9840` and renders `PIN2 code rejected`, distinct from
recoverable `Code error`. No automatic VERIFY follows a rejection.

A final cold process imports the complete permanent card, completes the
ordinary SIM read bootstrap and phone-code authentication, and physically
selects Change PIN2. It shows the permanent refusal, returns to the Change
PIN2 menu item, and a later physical Down press decodes as key `18` and
moves to Change security code. No security APDU is submitted and every card
byte is unchanged. This establishes persistent refusal and responsive
unrelated menus, not whole-SIM rejection or PIN1 authentication (PIN1 is
disabled in this fixture). Root `stages.json` retains the predecessor chain;
root acceptance is written only after all ten rejections and the cold control
pass. Native RF and the later restart remain outside this software claim.

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
`verify-5110i-sms-persistence` saves in one process, then supplies only that
SIM file to a fresh phone process. Physical Messages / Outbox / Read opens
the stored `A` (reader-frame SHA-256
`4877eac15193be5a99aad660db563aa3d4780c90ec47d349eb19448789fa31a9`).
No UPDATE RECORD is issued and the entire serialized card remains identical
to its pre-launch snapshot. The runner records the saved card's source/hash.
This proves cold offline draft readback, not SMS-SUBMIT delivery, network
registration or operation beyond the known restart.

The separate four-process `verify-5110i-sms-delete` gate saves a draft,
then tests Erase and Back/cancel from independent copies of that saved
card, and finally cold-boots the erased card. Both action runs show the
same `Erase message?` prompt. Physical OK produces one successful
176-byte UPDATE RECORD and changes exactly the first SMS status `07→00`,
retaining its old payload and every other card byte. Back instead sends
no update, preserves the complete card and returns to the exact A reader.
The cold erased-card run sends no update and renders `No messages in outbox`.
The shared `sim_sms_draft.py` assertions also serve NSK-3; the local frame
hashes and physical menu sequence remain NSE-2-owned.

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

Supervision is an activity test, not a scan-result validator. Own MCU
`29909e..2990a2` increments the counter before reading the packet type at
`2990a4`; all four initial type-`74` packets count. Timer message `dd`
branches at `2991b2` to the counter check at `2991b8`. A nonzero counter
rearms timer `1d` and clears the count; zero calls policy `2902c2(1)`.
The observed failing expiry at 31.000192 seconds has message `dd` and
count zero, before reason `68` and the known restart. Thus completing the
scan would supply activity, but the deadline does not specifically require
type `8b` or any other result type. The absent source may be another real
DSP/peripheral transaction; this contract does not authorize a fabricated
heartbeat. `--failure-trace` with `--mcu` checks this failure observation
without accepting the repeated-startup run as stable.

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
execution or a complete writer census. The request-specific mapping below
identifies the origin of `06e3` bit `0020`; a generic write to `06e3` is
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
complete census of type-`07` producers. The runtime lifecycle below bounds
this constructor's relevance without forcing its request into the stream.
The coherent native-processing run observes exactly three writes to that
byte: `00` at `20010c` (0.00265 s), `01` at `20cafc` (1.12047 s),
and `02` at `20feb6` (4.70184 s). The thirty-second terminal count
accounts for all three and the value remains `02`; the constructor entry
tap records zero entries. Thus this boot does not enter the candidate's
mode-4 lifecycle. Absent type `07` is not evidence that it must be added.
The active type-`1a` measurement, rather than this dormant type-`07` path,
is the observed unfinished result source. These are passive observations on
the unchanged physical-menu fixture, not firmware-state edits or proof of
all possible call routes.

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
The counter-level trace accounts for the eleven modeled wait periods;
it does not establish physical timing or justify changing the clock to
beat supervision. Eleven `3204` entries arrive at consecutive modeled frame
periods. Their
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

The long extent is selected by the consumed request, not an observed
uninitialized selector: at `3d5e`, AR2=`081f` reads control `0011`; the
decoder writes `0284=007c` at `3d8d` (write observation PC `3d8f`) and
sets `195d=0010`. Its next input word is `9800`. At `3da3`, TC is set,
so execution skips supplied-list conversion `3da5..3db0` and reaches
`3db1` with extent 124 unchanged. The uploaded scan observes that same
extent at `092d` and writes `0285=007b` at `0934` for inclusive BANZ.
The processing checker pins the selector bytes and observes this ordered
path and both extent writes. This establishes consistent decoding for the
captured request; it does not prove fitted mask identity, correct RF timing,
or the absence of all other request/lifecycle defects. Do not shorten the
extent or substitute a supplied list to evade receive supervision.

Own MCU constructor branch `20fd9c` explains the full-scan bit. Getter
`28531a` copies 48 bytes from runtime bitmap `10a75c`; two observed copies
contain `003f` followed by 23 `ffff` halfwords. The constructor's population
count is 374 (`0176`), independently reproduced from the copied words,
and category is 2. Pinned branch `20fd44..20fd4e` selects full scan when
count is at least 124 and category is not 6; `20fd9c` ORs bit 0 into the
packet control byte (initial `10`, becoming `11`). Thus the MCU and DSP
agree on why this request selects the long scan. Expected receive-supervision
traffic remains unresolved; consistent decoding alone does not establish
the bitmap's provenance, which the writer census below identifies separately.
The checker requires actual source copies and
the positive branch observation, not absence at mid-line PC hooks.

The bitmap itself has a positively identified firmware-default producer.
Constructor `224a4c` tests byte `10abd0`; its zero branch fills 48 bytes at
`10ac2c` with `ff`, writes first halfword `003f`, and publishes through
setter `285170` (return `224a71`). That setter invokes record publication
`26efdc` using descriptor `493f`, then copies the bytes into `10a75c`.
The thirty-second writer census observes twelve cold zero stores followed
by three complete twelve-word copies; all three setter callers are
`224a71`. The first copy occurs around 1.776 seconds, with two more just
before the scan request. The checker accounts for all 48 writes and their
callers against its terminal count. Thus this boot's 374-bit bitmap is an
explicit software default, not unexplained erased RAM or a factory-image
guess. Its alternative-list flag's lifecycle and the scan's physical
completion/receive-activity contract remain distinct questions; do not
replace the default bitmap with a shorter list to avoid the restart.

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

The passive sampling-control census closes a logging gap: the device's
initial port trace ends before measurement setup. Resident `4414` writes
port `0a` from `AR1 - [2193]`, adding `[00ee]` when negative; it writes
the `3a98` sentinel to `0b`, then AR1 to `09`. The observed mode-`c`
sequence is `0a=028f`, `0b=3a98`, `09=0332`. Its completion mode-`4`
sequence writes `0b=0029`, then `0a=3a98`. At thirty seconds there are
1,629 writes to `09` and 3,255 each to `0a/0b`, terminal values
`0332/028f/3a98`. The processing gate pins the activation instructions,
first twelve writes per port and full-count relation. These are programmed
control/timing values, not established RF units, compare ownership or
interrupt wiring. In particular, no sampling interrupt is synthesized from
these observations; all three ports remain storage-only pending evidence.

The live vector page routes INT0 to `3204`, INT1/INT2 and vector slot
`ffe0` to common peripheral dispatcher `3598`, and HPINT to `3772`.
This is firmware routing, not an electrical assignment of sampling events.
The dispatcher reads `port1 & ~port2` and has twelve reviewed mask/branch
tests: `1000 -> 369a`, `001c -> 362f`, `0100/0200 -> 8ec5/8ed3` or
`b7dc/b787` (selected by `06f3 & 0400`), `0400 -> 8e89`,
`0080 -> 371d`, `0002 -> 3621`, `0001 -> 362d`, and
`0020/0040 -> 3764`. These tests and their selector are byte-pinned by
the processing runner; this is not a complete peripheral-source mapping.

In the thirty-second native observation, `369a` enters ten times. Tapped
subpaths `363c/364e` and handlers `371d/3764/8e89/8ec5/8ed3/b7dc/b787`
have zero entries. `371d` consumes ports `13/14/15`; `3764` acknowledges
mask `0040` and sets `06e4 & 2000`. Neither fact establishes a connection
to sampling controls `09–0b`. Decode the inactive consumers and their
enables before assigning any additional request bit or interrupt source.

The `0100/0200` consumer pair has a distinct word-transfer contract:
default `8ec5` calls `9162`, which dispatches through data table `b3d1`
indexed by `06c6` and reads status port `36`; both `8ed3` and alternate
`b787` read input port `35`. The alternate protocol publishes output through
port `34` at `b821`. These consumers establish a buffered stateful peripheral
path, not its physical identity or a relationship to sampling ports `09–0b`.
The apparent raw PORTR-`0a` candidate at `71ef` is rejected: instruction
`71ee: f274 74fa` is CALLD `74fa`, followed by delay-slot STM `#000a,BRC`.
An operand-pattern port census is not an instruction-boundary proof; the
processing checker preserves this negative fixture alongside the confirmed
consumer bytes. No new peripheral behavior follows from this classification.

Full-run I/O observation identifies pending work on this separate peripheral.
Control port `36` receives `0080` twice during boot; port `34` transmits
`1eff` at 0.342534 seconds (store `920a`, observed PC `920d`). There are
no further port-`34` writes and no port-`35` reads through thirty seconds;
port `36` has four reads and two writes. At the transmit write, state
`06c6=1`, flags `06f3=1`, pending `06c5=9`, pointer `06c2=0701`.
At thirty seconds only state has advanced to 2. Resident data table `b3d1`
maps states 0..5 to `9185/9199/91b0/91be/91fb/9200`; state 2 therefore
awaits continuation through `91b0`. Request `0100` enters that transmitter
through `8ec5 -> 9162`. The current backend stores port `34` but provides
no transmit-completion source. This is a concrete pending-operation boundary,
not proof of physical baud, external peer identity, an incoming response,
or a cure for receive supervision. Any completion model must retain those
distinctions and let the real firmware drain its own queue.

The word-port control contract has independent firmware checks. Initialization
acknowledges and masks request bits `0700`. The active transmit branch sets
mask `0400` and clears `0300`; handler `8ec5` calls transmitter `9162`
**before** acknowledging `0100` at port 1. A completion arriving while the
handler queues another word must therefore not be discarded by that older
acknowledgment. Idle/stop path `90e7` tests pending state and status-port-`36`
bit `1000` before proceeding. Reset helper `9129` transforms control with
`(old & ffe1) | 801c`, waits for status `4000`, then clears `8000`. These are pinned
firmware requirements, not measured hardware timings. The meaning of the
framing/clock bits, transmit word duration and external peer remain unknown;
do not choose a baud rate merely to clear the boot supervision deadline.

Setup helper `9009` initializes receive-ring pointers to `0100`, transmit
pointers to `0700`, clears software protocol counters, and selects protocol
states from `06f3` and control-port bit `0080`. It does not program a
separate baud/divisor register in the reviewed range. No such register
was identified among the adjacent port candidates either; this bounded
inspection is not a proof that no clock control exists elsewhere. Keep
transmit timing unimplemented until a clock/framing contract or measured
trace supports it. Independent SIM/application coverage can advance without
fabricating this peripheral's completion or receive traffic.

The wider live-program disassembly identifies configuration helper `9232`:
`AR3=06f3` holds active flags and `AR4=06f4` pending flags. After its
branch-dependent flag normalization, `9295..92a4` preserves control-port
`36` bits with mask `fe01`, conditionally adds `0180` when pending flags
match `0300`, and merges accumulator bits `007e`. It commits pending
flags masked by `079e` to active flags before calling setup `9009`.
Raw direct-call candidates are `3bcd` and `8e02`; these are not recovered
caller arguments or proof that the configuration branch executes at boot.
The `007e` field is not yet identified as a divisor or framing parameter.
The decoded host path `3b92..3bd0` writes a request-derived word to `06f4`
at `3bb1`, computes separate values for `06f6` and `06f5` using constants
`015b` and `0d8b` with arithmetic shifts, and calls `9232` at `3bcd`.
An input low byte `ff` selects `ffff` for `06f5`. This establishes a
host-controlled configuration path, not the physical units of those values.
Initialization writes `06f4=0`; alternate paths `b8cd/b8d3` write
`0800/081c`. Neither alternate constant is evidence of the boot setting.
The host dispatcher `398d..3995` indexes **data-space** table `b00f`;
within its 82 entries, command `4c` alone selects `3b92`. The identically
addressed program-space words are executable code, not this table.
`inspect_word_host_command` checks the decoded dispatcher and the separate
data image, including rejection of a substituted program-space image.

The derived values are software-counter thresholds: `8e4b` compares
`06f5` against `06b3` (`ffff` bypasses that check); state-zero transmitter
path `9185` compares `06c4` against `06f6`. Both counters increment at
`8e6e` when active flag bit 0 permits it. This does not establish the
physical period of that service routine. At the seven-second native
snapshot, `06f4=0`, `06f5=0363`, `06f6=0`, `06b3=0448`, `06c4=0462`:
the configuration retains its initialization defaults at this observation,
not a demonstrated host-command override. Do not reinterpret these software
thresholds as physical serial divisors to invent a transmit-completion rate.

Read-only `nse2_word_service_observe.lua` data-write taps qualify the
counter service independently of opcode-fetch observation. A nine-second
native run records 1,247 increments each at post-store PCs `8e71`
(`06c4`) and `8e74` (`06b3`); consecutive increment intervals range from
65.712 microseconds to 169.602462 milliseconds. Total writes are 1,249
and 1,251 respectively, including initialization and transmitter resets.
The observed increments are therefore not a fixed-period serial clock.
These are emulator scheduling observations under the current native model,
not measured silicon timing. Opcode reads expose `PC=address+1` at these
entries, not the opcode address itself: an exact-PC-only filter falsely
reported zero. A nine-second unfiltered control records 1,247 reads each at
`3f4e` and `8dd0`, 46 at `3924`, and none at `9232`; every observed read
has that one-word PC offset. This rejects the earlier cache-bypass explanation.
The observer now reports exact/plus-one/other read counts separately. These
are fetch observations, not instruction-retirement counts or a universal
PC convention; use actual data/IO effects to corroborate execution.
Static callers explain the software cadence: host-processing entry `3924`
calls service `8dd0` only when `06e1 & 0004` is set, otherwise proceeding
to host-ring handling at `394f`. Alternate processing entry `3f4e` calls
the same service unconditionally, with protocol dispatch looping back to
`3f50`. These byte-checked call sites do not identify which caller supplied
each runtime increment. Neither is evidence for a dedicated physical
word-clock interrupt. The service can run while the separate transmit
completion remains missing; its counter activity must not be mistaken for
successful port-`34` transmission or incoming peer traffic.

Reproduce this bounded static/cadence contract without a pre-existing run:

```sh
python3 tools/run_nse2_word_contract.py tmp/nse2-word-contract
```

The runner uses two isolated fresh native processes: a seven-second live
program/data export and a nine-second read-only service observation. It
zero-pads data words `0000..007f` rather than reading mapped CPU/serial
registers; the command table at `b00f` is sampled from ordinary data space. It
records the compatibility-mask/calibration/retry-fixture assumptions and
executable hash in each `inputs.json`, then writes checked results to
`acceptance.json`. A pass proves this bounded software contract only;
physical completion and long-run stability remain explicitly false.
Its disassembly census covers all 38 raw candidates for ports `34/35/36`
(31 control/status, five output, two input) with byte-matched linear I/O
decodes, and rejects `71ef` as an operand of CALLD `74fa`. Uncovered,
stale or conflicting decodes are explicit tool outcomes; overlapping exports
are not resolved by majority vote. This is linear-decode coverage, not a
control-flow or runtime-reachability proof and not a physical-clock absence
proof. `c54x_rom4_port_census.py --disassembly FILE` exposes the same
classification for other acquired program images without changing emulation.

The seven-second data snapshot also resolves the pending transmit-buffer
contents: producer `06c1=070b`, consumer `06c2=0701`, with words `0700..070a`
holding two complete byte-framed records:

| Record | Bytes | Destination / source / type | Declared payload |
| --- | --- | --- | --- |
| First | `1e ff 00 d0 00 03 01 01 e0 00 ff 2d` | `ff / 00 / d0` | `01 01 e0` |
| Second | `1e 14 00 f4 00 01 03 00 1d e1` | `14 / 00 / f4` | `03` |

Both have the serial-FBUS marker `1e`, a big-endian length, zero padding
for odd payload length, and independent alternating-byte XOR checksums.
This framing comparison is supported by
[Gammu's FBUS v2 description](https://docs.gammu.org/protocol/nokia.html#frame-format-for-fbus-version-2-direct-irda).
Do not assign standard sequence-field semantics or an external peer identity
to these particular payloads from that comparison alone.
`inspect_word_pending_frames` validates exact pointers, bounded record extents,
padding, both checksums and the native bytes; mutation tests cover each.
This identifies FBUS-shaped service traffic in the pending queue, not an RF
sample stream or successful wire delivery. The first word `1eff` is the only
observed output so far. The framing does not independently establish the
controller's clock, completion source, external wiring or supervision link.
Read-only queue-write and builder-entry taps resolve their origin: both
entries at `8db4` return to `3f09`, from CALLD `3f05`, with arguments
`A=1eff/B=3/AR0=00d0` and `A=1e14/B=1/AR0=00f4`, respectively;
both source buffers are `1200`. Data dispatch table `b00f` maps host
command `05` to handler `3ee7`, which extracts the header/type/length,
copies payload to `1200`, consumes the host object through `3900`, and
calls the word builder. Native `92df..931f` constructs header, payload,
odd-byte padding and XOR checksum, then commits producer pointer `06c1`.
The nine-second trace contains exactly 22 writes to `0700..070a`: eleven
initial clears and eleven frame words, at post-store PCs `9301/9304/9307/9313/931d/931e`.
Thus these queued records are MCU-command-forwarded service traffic, not
evidence of an unsolicited DSP RF-result producer. The reproduction checks
the dispatch entry, builder bytes, both caller stacks and uncapped queue
coverage. Whether physical service delivery affects supervision is still open.
The MCU producer at shared byte `100a4` commits cursors `0006` and `000b`
at 0.338334154 and 0.338557154 seconds, respectively. Both stores report
PC `277f7a`, LR `277fcb`; their RAM stacks contain worker return `28de47`.
Own-image Thumb decode at `28de30..28de46` reads the queued object's length
at offset 2, checks ring capacity through `2780f4`, and passes its bytes
at offset 3 to sender `277f96`. Thus the worker and native forwarding
path are connected. Keep that producer boundary separate from physical wire delivery.
The observer selects the high halfword at `100a4` (DSP producer `0852`),
not the adjacent low-halfword DSP consumer `0853`; recording initialization
of the latter would not establish MCU publication.
Worker snapshots distinguish dynamic object `101438`, beginning
`00020a051eff00d000030101e000`, from ROM object `2b0754`, beginning
`000208051e1400f400010300`. Only the object extents, not trailing heap/ROM
bytes, are compared. The dynamic object is constructed at `279a64..279b28`:
header-byte stores set class/length/type (type `05` at `279a72`), copies
through `29ad08` supply the frame header/body, and sequence/flag stores
produce the trailing `e0` byte before queue publication through `28fcb2`.
The runtime data-write trace witnesses that type store and both complete
worker objects; `inspect_word_worker_objects` rejects changed sources or
contents. The static object's own ROM bytes match its worker snapshot.
This recovers the immediate packet-construction boundary; the higher-level
caller's purpose, service peer contract and delivery dependency remain open.
The dynamic wrapper's saved outer return is `279c5f`; its original source
is `101420`, beginning `1eff00d000010100` before fragmentation. Own-ROM
decode identifies the enclosing dequeue loop at `279bfc`: `279c02..279c0a`
tests frame markers `1e` and `1c`, and `279c5a` calls wrapper `2799c4`
for either marker. Other branches process types `8d/8e`; the loop releases
processed objects and receives the next through `2559e8`. This establishes
a firmware-owned frame-routing/fragmentation path, not a radio-result
constructor. The checked caller/source record prevents attributing the D0
object to a different wrapper invocation. Physical endpoint ownership still
requires separate evidence.
Startup helper `28b7bc` supplies the discovery source. It allocates two
seven-byte objects, copies template `2b074c` (`1f ff f0 d0 00 01 01`),
and posts each to task 7 through `256050`; the second copy changes its
marker to `1e` at `28b804`, positively witnessed in the runtime writes.
Separate frame-copy helper `279d36..279d4c` allocates/copies the frame and
posts it to task 8 through `2558e4`. The packet/sequence wrapper and native
builder remain downstream, not independent radio producers.
`inspect_word_startup_contract` pins the own-ROM task-post code, template
and runtime FBUS marker store. This establishes paired startup service
discovery, not a required response, physical baud or radio-completion
dependency. RAM addresses in this trace are reused allocations; do not
treat every earlier write to `101420/101438` as the same object's lifetime.

Primary family-level hardware evidence supports an accessory-serial
interpretation, not an RF-mode label: Nokia's NSE-3 System Module manual,
Original 11/97, page 3-45, connects FBUS/infrared to MAD's AccIF and lists
9.6, 19.2, 38.4, 57.6 and 115.2 kbit/s IR rates
([source](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6110/03SYS%20%5B73%5D.pdf)).
This is a related-product architecture reference, not the NSE-2 register
specification: it neither maps DSP I/O `34/35/36` to AccIF nor defines
control `0080`, status `1000/2000/4000`, interrupt routing or byte packing.
AccIF is therefore a candidate physical owner, not a promoted device name.
Multiple supported rates also rule out deriving the configured clock merely
from an `1e` packet marker. Recover the register/clock selection or a matched
trace before attaching a serial completion timer or selecting 115,200 baud.

The nine-second service run's native opcode fingerprint is 564 exact forms
in 101 high-byte groups, SHA-256
`2214638d14968de30e21a3c62fd9305a00716965ce8568658263cae6f186fede`.
The reproduction now requires this fingerprint and supported, exact-length
decoding of all 17 captured `6f` extension tuples. Nine synthetic
zero-extension decoder records are unsupported; their actual fetched
extensions pass the independent tuple check. They are not nine demonstrated
execution failures. A separately completed default C54x fixture passes all
293 required markers and the intentional completion exit (MAME return code 3,
runner return code 0). Its 5,690 distinct `[opassert]` records include all
564 forms observed here, with no missing opcode. This is actual completed
fixture assertion coverage, not a source-text inventory or full ISA proof.
The word-contract gate itself still does not run that expensive fixture.
Reproduce the separate baseline with:

```sh
python3 tools/run_c54x_core.py --mame mame/mame --rompath roms --output-root tmp
```

Allow approximately 35 minutes on the current host; large groups report only
after tens or hundreds of thousands of save/load cases. Require the runner's
terminal PASS, not elapsed time or partial markers, before using its
`error.log` assertion inventory. Compare exact opcode sets against the
bounded service log; an assertion for a neighboring encoding does not cover
the observed form. Neither this comparison nor the fixture establishes
physical port timing, a fitted NSE-2 mask/clock, native RF operation or
sustained handset stability.
For focused CPU controls, use the same runner with `--core-controls`.
It additionally runs isolated `instr-observer` and `portr-bypass` fixtures.
The former distinguishes execute-owned callbacks from ordinary memory reads,
repeats and annulled fetches across atomic/staged frontends and save/load;
the latter checks 6,656 volatile-PORTR consumer/restore cases, including
512 dispatcher cases. These controls pass on the current build and are
recorded separately in `acceptance.json`. They do not prove all 564 observed
forms, physical peripheral banks, port-`36` semantics or a silicon clock.

Vector `ffd0` branches to ISR `3416`. Its `34da` path reads port `36`,
tests mask `2000`, and can write port `34` at `34e8` or `34f4` while
maintaining software buffering state `01f9`. Thus the output port also has
an interrupt-owned consumer, not just the startup transmitter at `920a`.
`inspect_word_configuration_contract` byte-checks these instructions and
the vector against the live export; mutation tests reject changed sites.
These static contracts do not establish interrupt timing, peripheral
identity, runtime execution of the ISR branch, or a supervision fix.

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

## Bounded native/UI save-state continuation

`verify-5110i-native-ui-state` prepares a fresh laboratory card, then runs a
separate phone-authenticated native process. At 18.5 seconds it saves the
emulator state; physical Menu at 20 seconds opens Phone book. Loading that
checkpoint and repeating the same contact reproduces the exact frame hash
`601a551845a9a2dd5a2df5f54f2add55b8ce4ec2edcdab445fb886b84eb414fd`.
The entire serialized SIM card remains unchanged. Initial SIM read bootstrap
and phone-code acceptance retain their separate checks.

Pre-save and post-load observations match time, all 37 exposed MCU register
values, the checksum of mapped 64-KiB MCU RAM, all 36 exposed C54x register
values, the checksum of data words `0080..7fff` and all 65,536 page-zero
program words. DSP mapped registers are excluded from the RAM checksum;
their exposed values are observed separately. A 2.7-second native I/O-write
window compares every recorded relative timestamp (nine decimal places),
port, value and mask. The qualified run has 8,180 matching writes in each
leg; that count is an observation, not a required hardware constant. The
16,384-entry diagnostic cap fails closed on saturation/overflow. Exact
restoration of these observed fields and replay is not exhaustive hidden
pipeline-state or electrical-clock proof.

`tools/run_nse2_native_state.py` uses run-local save storage, checks that a
nonempty state artifact exists, hashes it, and requires completed replay,
physical contacts, exact frames and unchanged card storage. Execution ends
at emulated time 26 seconds, before the known receive-supervision restart.
It does not establish sustained operation, registration, connected calls or
speech; the forty-second repeated-startup diagnostic remains a negative
control. No native clock, firmware state or peer response is changed.

### Pending SIM write restoration

`verify-5110i-sim-write-state` prepares a fresh full card, physically enters
the existing Add name `A/123` workflow and saves after the real
`A0 DC 01 04 20` header reaches the card, before any body byte is sent.
The Save contact uses a separate 70-ms hold so it is already released at
the checkpoint; the ordinary phonebook fixture retains its 150-ms default.
The qualified header time is 27.585246462 seconds and the checkpoint is
27.585249230 seconds. Mixed trace precision is compared with a bounded
100-ns header-timestamp tolerance, not exact textual timestamp equality.

Read-only saved-item observations require both saved and restored card
state to be `ins=DC`, body length zero, expected length 32, receiving-body
true and EF `6F3A` selected. SIMI has one queued, not-ready RX byte, a
pending TX-ready cause, zero IIR and an empty TX FIFO. Crucially, ADN record
one is all `FF` both before save and immediately after load: the reference
leg's committed record is actually rolled back, not merely overwritten
with identical data. Observed MCU registers and RAM checksum also match.

A run-local wire artifact records and independently checks reference and
restored TX/RX events over 0.5 seconds, including their order and relative
timestamps at nine decimal places. Both qualified legs have 195 events,
starting with pending procedure byte `DC`, the exact 32-byte record body,
and status `9000`; later record-read traffic also replays identically.
The diagnostic cap is 2,048 events and fails closed. Each leg has exactly
one post-checkpoint ADN commit and the exact Saved frame. Only record one
changes in the complete card. A third, independent cold process physically
searches for `A`, reads the saved contact and preserves the whole card.

This qualifies the declared pending card/SIMI transaction and cold storage
continuation, not every serial phase, electrical timing, native network
service or operation beyond the known restart. Saved-item access is
read-only; only physical inputs and emulator save/load APIs are driven.

`verify-5110i-sms-write-state` exercises a different transaction phase. The
physical offline draft Save sends `A0 DC 01 04 B0` to EF_SMS. Firmware flushes
body bytes at cumulative counts `15,30,45,60,...,165,176`; these are observed
firmware bursts, not a new physical FIFO-capacity or baud claim. A checkpoint
after the fourth body flush retains 60 of 176 bytes at 25.934035922 seconds.
Read-only card observations require receiving-body true, expected length
176, EF `6F3C` selected, the exact first 60 draft bytes retained and the
stored SMS record still free. The saved/restored MCU observation and partial
buffer match; the reference commit is rolled back to a free record on load.
The shared observer also requires an empty RX FIFO, zero IIR, a pending
TX-ready cause and an empty TX FIFO: this is waiting to transmit more body
bytes, unlike the ADN gate's queued procedure-byte checkpoint.
Both legs then transmit the remaining 116 bytes, receive `9000` and render
the same Saved frame. All 118 recorded TX/RX events match over 0.5 seconds.
Only the exact unsent `A` record changes in complete card storage. A separate
cold Outbox/Read displays `A` and preserves the whole saved card.
`--sms-partial` selects this contract in the same isolated runner, observer
and checker used by the ADN gate. The shared trace names identify SIM
transaction state, while explicit file, body-length, partial-buffer, queue,
frame and complete-card assertions keep the two acceptance cases distinct.
Each gate uses preparation, restore and cold-read processes; neither extends
the native stability horizon. The ordinary SMS fixture retains its default
150-ms Save hold and has a separate regression run.

### Pending SIM PIN verification restoration

`verify-5110i-pin-state` uses three isolated native processes: an untouched
PIN-enabled card wait, pending physical wrong-PIN VERIFY/save/load, and
independent cold PIN wait using the resulting complete card. The checkpoint
at 7.663201154 seconds is after `A0 20 00 01 08` but before its body:
card instruction `20`, body length zero/expected eight, receiving state one,
and CHV retries `3/3`. SIMI retains one unread, not-ready procedure byte,
IIR zero, TX-ready pending and empty TX FIFO. Both replay legs send
`1111 ff ff ff ff`, receive `98 04`, decrement CHV1 once to two retries,
and render the exact Code error frame. All 41 wire events in the half-second
window match in time and bytes; the full ARM/RAM/card/controller checkpoint
matches after load. Complete card comparison permits only that retry byte
to change; cold restart retains the two-retry card and unchanged credentials.

The snapshot occurs while physical OK is held. Its remaining release is
replayed from the same physical contact timeline after load, rather than
changing the ordinary key duration or leaving a stuck input. Forced contact
values are harness state, not firmware/card pokes. The shared transaction
observer also serves ADN and partial SMS writes; their acceptance contracts
remain distinct. This proves bounded SIM authentication restoration, not
native registration or stability beyond the reason-`68` boundary.

`verify-5110i-pin-buffered-state` checks a different ownership boundary:
at 7.665799425 seconds SIMI holds five TX bytes `31 31 31 31 ff`,
while the card still has zero/eight body bytes and three retries. The
procedure byte is already consumed and no TX-ready cause is pending. Exact
ARM/RAM/card/controller restoration preserves that FIFO prefix; the remaining
three `ff` bytes and subsequent status traffic match across all 35 timed wire
events. Each leg receives `98 04`, consumes one retry, and renders Code error;
an independent cold process preserves the complete two-retry card.
The save request occurs at the fourth body write, but scheduling reaches the
fifth before pre-save. The observer asserts the actual five-byte count and
prefix instead of assuming request-time state was captured.

`verify-5110i-pin-status-state` saves at 7.665808846 seconds, after the
card has committed the wrong PIN but before firmware reads `98 04`.
The card body counters and receiving state are zero, retries are already
`2/3`, and SIMI holds two unread, not-ready bytes with TX-ready pending.
Architecture, card state and controller state restore exactly. All 32 timed
wire events match, beginning with the queued status, and both frames show
Code error. The card's VERIFY result is logged once before the checkpoint
and never executes again after save or load; whole-card comparison and a
fresh cold process preserve the single retry decrement. This tests committed
card state plus pending controller delivery, not command re-execution.

The separate `verify-5110i-pin-block-state` gate reaches one retry through
two actual physical wrong-PIN/cold-card transitions before the checkpoint.
It does not seed an exhausted counter. Save/load then restores the last
VERIFY header with retries `1/3`; both legs send the wrong credential once,
receive `98 40`, end at `0/3`, and render the exact PIN code blocked frame.
All 11 wire events match; complete-card comparison permits only the final
retry decrement, preserving PUK attempts and credentials. An independent
cold process renders the PUK prompt without a VERIFY or PUK attempt and
retains the card unchanged. This five-process boundary is distinct from
the ordinary `3 -> 2` retry outcome and its subsequent STATUS traffic.

`verify-5110i-puk-state` independently reaches an exhausted PIN through
three physical wrong attempts, then restores a real pending
`A0 2C 00 00 10` command at 18.265665615 seconds. Before its body, both
snapshots contain the old `1234` credential, zero PIN retries, pending
16-byte body, and the exact ARM/RAM/SIMI state. Each leg sends the real
PUK and replacement `2468`, receives `90 00`, resumes SIM initialization
and reaches the reviewed phone-side Security code prompt. All 560 timed
wire events match. Full card comparison permits only PIN1's credential
and retry restoration to three; PUK/CHV2 credentials and unrelated storage
are preserved. The sixth cold process authenticates with `2468` and retains
the complete resulting card unchanged. This is not phone-code acceptance
or native radio acceptance.

Physical-contact replay uses stable `column:mask` identities, not Lua field
wrapper identity: repeated field lookups can produce distinct wrappers for
one real contact. A held final OK must override all earlier releases of
that same contact. The PUK replay's repeated OK sequence and strict timed
wire comparison exercise this requirement. Wire artifacts are retained
before an equality assertion, so a failure still provides its first
divergent event instead of losing the evidence.

## Proactive SIM capability boundary

```sh
.venv/bin/python tools/run_nse2_toolkit.py tmp/nse2-toolkit
```

Two independent31s cold runs retain the existing own-checksum/retry and
nominal-ADC research inputs, full native compatibility backend and physical
A/123 contact-save workflow. Only the external card's Phase-2/Phase-2+
configuration differs. The Phase-2+ run sends own TERMINAL PROFILE
`03 01 80 00`. The card's application becomes pending after its existing
eight-second delay; the next observed ordinary command is the physical
EF_ADN UPDATE at27.59s, which completes with`91 16`. The ME then FETCHes
the exact22-byte DISPLAY TEXT and automatically returns
`81 03 01 21 80 02 02 82 81 03 01 30`. This is a capability refusal,
not displayed Toolkit text or user completion. The Phase-2 control has no
TERMINAL PROFILE/FETCH/TERMINAL RESPONSE traffic. Do not infer absent
notification from the earlier25s authentication-only control, which issued
no new APDU after the card became pending.

Both runs retain the reviewed Saved pixels and complete A/123 SIM contents;
their whole flash and card endpoints match. EEPROM endpoints differ at
exactly18 bytes:`06df + 33*i`, for`i=0..17`, each`00->01`. These are unused
empty-field payload bytes, as bounded below. The checker requires complete endpoint hashes rather than
masking these bytes or claiming unchanged handset storage:

| Card | Complete EEPROM SHA256 |
| --- | --- |
| Phase2 | `9af2e8f7d726da1807577341af5885c7455ef4c7c4b7ca694afd67f753dfb645` |
| Phase2+ | `ba749b88d2e1f770b62a1b58b2255d29689315f11a29bd50e0037c368ed2190d` |

The paired accepted capture is`tmp/nse2-toolkit-final`. Physical contacts
and real SIMI TX/RX taps establish the protocol; no firmware state or DSP
reply is injected. This bounded refusal does not establish other Toolkit
commands, fitted-mask compatibility, post-restart stability or radio service.

### Empty-field EEPROM write contract

```sh
.venv/bin/python tools/run_nse2_toolkit.py tmp/nse2-nv-paired --observe-nv
```

Both profiles independently execute18 writes at`06d0..0901`, stride33,
after physical phone-code acceptance. At own writer`293c8e`, each complete
33-byte RAM buffer at`10d618` already equals its final EEPROM record;
the byte difference therefore precedes serial EEPROM storage. Every write
follows encoder`28a1d0`, called from`24599e` with empty input`10d68c`
and destination`10d608`. Its empty-input branch writes only the leading
zero length and returns at`28a242`. The two LDM/STM pairs at`2459d0..2459da`
copy all16 bytes from SP+4 to SP+14, including uninitialized trailing payload.

The corresponding decode call at`245bb2` receives that16-byte field;
decoder`28a17c` branches directly to zero-terminated empty output when its
leading length is zero, without reading the remaining15 bytes. All18 bank
differences occur at field offset15, inside that inactive payload. This
explains the specific drift as firmware-persisted empty-field padding, not
evidence of a special Toolkit settings write or EEPROM corruption. Both
profiles perform the writes; their stale padding differs. It does not prove
all record readers or broader provisioning validity.

`nse2_nv_source_check.py` pins the own encoder, zero-return, decoder, field
copy and writer instruction spans; requires all18 ordered empty-input/write
pairs; and matches every complete source buffer to the saved bank. The
paired run still requires the exact whole-bank hashes above and unchanged
card/flash comparison. No padding is initialized, erased or exempted from
acceptance. The sibling NSK-1 pattern was a search hypothesis; own NSE-2
code and runtime buffers establish this conclusion.

## Remaining work

### Backend ownership constraint

NSE-2's native profiles explicitly remove `dsp_hle` and instantiate the full
`nokia_dsp_c54x` backend. They do not use the staged backend's runtime-HLE
handoff. That separate mechanism requires verified loader state, execution
outside uploaded code and the specific absent ROM6 routine `2C75`; it then
suspends staged execution and releases its transport ownership. The full
ROM4 backend running NSE-2 reaches none of that guard's prerequisites.
`nokia_dsp_hle::native_owns_transport()` currently queries staged ownership,
not a general arbiter for simultaneously active full-native and HLE peers.

Consequently, NSE-2 network parity is not available by merely enabling a
radio peer or copying another handset's hybrid configuration. Keep the
existing full-native composition and restart control unchanged. An explicit
future research-HLE branch would need its own request/reply contract and
single-owner transport boundary, not co-driving native rings, a timed switch,
a firmware-PC escape or a fabricated native completion. This audit does not
prove that such a branch is impossible; it establishes why the existing
staged-ROM6 handoff is not reusable as-is. Relevant implementation authority:
`make_5110i_config` / `nse2r4v` in `driver/nokia_dct3.cpp`,
`nokia_dsp_staged.cpp`'s `runtime_hle_handoff` guard and
`nokia_dsp_hle.cpp`'s ownership check.

| Boundary | Established | Required evidence / next action |
| --- | --- | --- |
| Native receive supervision | Initial RX is delivered and consumed; full scan advances; timer-`dd` tests generic activity and expires with zero. | Identify a legitimate later producer or peripheral completion. No guessed heartbeat, shortened bitmap or policy disable. |
| Pending word peripheral | Ports `34/35/36`, state/mask/ack grammar and pending first TX are mapped. | Recover clock/framing/completion ownership from software or measured traces; adjacent setup exposes no evidenced divisor. Its relationship to supervision remains unproved. |
| Provisioning | Own checksum and explicit retry-byte fixtures permit physical phone authentication. | Own semantic/factory evidence, not donor EEPROM or promotion of a diagnostic byte to a factory default. |
| Bounded software continuation | SIM security, persistent contacts/drafts, FDN policy, native/UI replay, pending SIM UPDATE/cold-readback, pre-body PIN VERIFY/PUK UNBLOCK, buffered PIN TX and committed-PIN/unread-status restoration have independent pre-restart gates. Retry exhaustion and cold authentication with the recovered PIN are qualified. | These checkpoints cover specific observed phases, not every serial interleaving. Do not infer sustained handset operation from bounded replay. |
| Sustained handset service | The reason-`68` restart is retained as a negative control. | Prove operation beyond that boundary before promoting native registration, network SMS, calls or speech. Fitted DSP/clock and electrical calibration remain separate prerequisites. |

For authentication replay, distinguish the controller's buffered TX body from
the card's received body. `nokia_simi_device::txd_w` queues bytes locally;
`tx_fifo_control_w` delivers the queued batch synchronously through the card
callback when firmware flushes it. A save during FIFO filling therefore tests
SIMI buffer restoration, not partially received card data. Do not introduce
artificial byte delivery or split a firmware flush merely to create a card-side
checkpoint. Likewise, an observation tap sees a write before its mapped handler;
the subsequent save snapshot must establish the actual buffered byte count.
