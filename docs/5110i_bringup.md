# Nokia 5110i / NSE-2 bring-up

## Current boundary

Own v5.53 MCU/PPM-A executes its verifier on the explicitly borrowed NSE-1
ROM4 backend. The nominal-voltage research profile `nse2r4v` reaches the
firmware's **Insert SIM card** screen. Two physical Menu presses reach the
own decoder as raw key 11 / key `19`. With the synthetic card physically
inserted, firmware completes SIM read bootstrap and presents **Security code**.
One physical `12345` + OK attempt produces **Code error**. Authentication,
persistent contacts, application-menu, registration, calls and SMS are not accepted.
Fitted mask identity, DSP clock and measured board calibration are unproved.

The ordinary `noki5110i` profile remains unchanged: erased EEPROM and a
silent DSP reproduce CONTACT SERVICE. `nse2r4t` supplies the compatibility
DSP but retains open-pin ADC2; it completes DSP startup while the analog
readiness counter cannot drain. These are explicit controls, not alternative
claims of usable boot.

Next: recover the phone-side security contract from own-product consumers.
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
| Keypad | Scanner `29806c`, decoder `299010`, maps `2b382c/2b3848`, selector `10c6b6`; five rows, signal/direction `31/2f`, columns `30`, IRQ mask `33`. | Existing NSE-1 contact layout matches own table; only Menu has runtime decode acceptance here. |
| SIMI | Own `286520` initializes standard IIR/FIFO controls; inserted synthetic card completes read bootstrap. | Phone-code authentication and persistent contacts remain unaccepted. |

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
the own decoder; the seventeen-second frame is **Code error**. Exact editor
buffer contents remain unobserved, so this does not establish the stored
phone code. No additional candidate codes are tried. The integrity-only
EEPROM has no authentic phone identity or lock payload.

At OK, an EEPROM `000c`/eight-byte read reaches formatter `2593b8`, whose
nibble-to-ASCII conversion is identity-related; the read alone does not
identify the stored security-code record. Recover the actual comparison
and record consumers before selecting any semantic fixture.

## Remaining work

1. Recover phone-side authentication and verify persistent contacts with
   physical input; SIM read bootstrap alone does not establish either.
2. Recover semantic EEPROM requirements from their own consumers; retain
   integrity-only fixtures as diagnostic inputs, not factory defaults.
3. Qualify application/menu behavior and longer lifecycle/save-state tests.
4. Keep fitted flash/RAM/DSP/clock/analog evidence distinct from compatibility;
   no registration, native RF or speech claims without their own acceptance.
