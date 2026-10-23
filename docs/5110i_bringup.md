# Nokia 5110i / NSE-2 bring-up

## Current boundary

The acquired v5.53 MCU/PPM-A pair is normalized and qualified. Its initial
verifier upload is captured byte-for-byte, but no DSP executes it in the
ordinary profile. The separate
`noki5110i` research profile renders the own firmware's readable 84x48
`CONTACT SERVICE` frame with initially erased external EEPROM and no DSP
completion. Fresh isolated runs execute EEPROM GPIO, CCONT and LCD traffic
through eight seconds without a repeated reset. This is graphical failure
presentation, not usable boot, physical key delivery, SIM or phone-service
acceptance. The archive labels the pair `5110i`; both firmware headers
identify `NSE-2`, version `05.53`, dated `11-01-01`.

Separate integrity-only EEPROM experiments clear both decoded checksum
families but do not change the ordinary profile's failure frame. The
`nse2r4t` compatibility profile executes the own verifier on the acquired
NSE-1 ROM4 backend, obtains identity `4` and reaches organic readiness state
one. This clears `0e/0f`, but its integrity-repaired run has a blank LCD at
eight seconds, not accepted usable boot. Its fitted mask, clock, physical
decoded input and phone services remain unproved. The next boundary is
startup report `14`: state `10`, phase `6` and report bits `0e` leave the
normal readiness conjunction incomplete. Physical Menu contacts change GPIO
columns, but firmware still masks all columns and does not decode them.

`make verify-5110i-static` independently qualifies the acquired ZIP and
Windows SFX, verifies their exact two member names/extents/hashes and
compares their decoded payloads byte-for-byte. Neither installer executes.
The resulting `roms/noki5110i/5110i_5.53A.fls` is an unchanged concatenation
of its own MCU and PPM, not a donor composition. The generated manifest
retains source/member hashes and the decoded contracts. Input provenance
is indexed in [the ROM collection](../roms/README.md#nokia-5110i-nse-2-v553).

## Own static contracts

| Surface | Evidence | Limit |
| --- | --- | --- |
| Flash input | MCU is `0x190000` bytes; PPM A is `0x70000`; combined CPU-order image is 2 MiB. | No fitted flash-part identification. |
| Reset | ARM entry `200040`; own PC-relative stack literal `10fb98`; ARM/Thumb interworking enters `2000ec`. | Stack placement is not a physical SRAM-size measurement. |
| EEPROM descriptor | Header byte `f4` decodes to two address bytes, 32-byte pages and 8 KiB capacity. | Descriptor interpretation, not fitted silicon or a supplied EEPROM dump. |
| DSP catalogue | Table `29bba0`, 112-byte initializer to `10ba14`, 28 slots, only slot 27 null. | Slots 11 and 26 are populated; an older product's null-slot assumptions do not apply. |
| Verifier | Slot 25 at `2a0f6c`, header `0f00 0000 00ba 0700 00b4 0000`, 186 program words. | Descriptor decode, not execution or a completed ownership exchange. |
| Resident ABI | Verifier stores `4` to DSP data `0803`; three pinned CALLs target `8023`, `8023`, `802b`. | ROM4-style ABI evidence, not identification of the fitted mask or authority to supply a verdict. |
| EEPROM GPIO | Own byte reader `295746`, START/STOP entry points `295814/295886`: signal `20020`, direction `20024`, SDA bit 0, SCL bit 2. | No factory contents or measured write latency. |
| GENSIO | Own CCONT reader `296704`: select `22` at `28`, TX `2a`, RX `2d`, status `29` bit 2. LCD accessors `296e46/296eac`: command/data `2c/2b`, ready bit 0. | Shared device grammar is supported by these own consumers; fitted IC/serial timing is unproved. |
| Keypad | Scanner `29806c`, decoder `299010`, maps `2b382c/2b3848`, selector `10c6b6`. Five rows, row signal/direction `31/2f`, columns `30`, IRQ mask `33`; special column 4 maps Power. | Own layout matches the existing NSE-1 input layout, not proof of delivered physical input. |
| SIMI | Own initialization `286520` uses IIR `38` and the standard FIFO controls. | Empty socket; no SIM conversation accepted. |

The packages contain no separate external EEPROM or resident DSP image.
Do not attach the NSE-1 provisioning blob or infer the 5110's 2 KiB EEPROM
geometry from the similar product name. The acquired NSE-1 mask remains a
separately sourced compatibility hypothesis, not an implicit NSE-2 ROM.

## Graphical failure acceptance

```sh
make verify-5110i-boundary RUN_DIR=tmp/nse2-fresh-boundary
```

The runner refuses an existing directory, independently qualifies both
wrappers and supplies only unchanged own flash. Absolute run-local ROM,
NVRAM and configuration paths avoid retained state from other runs. Its
input manifest records the binary hash and the declared minimal boot-ROM
flash-entry HLE. The base TE28F160 is a capacity-compatible trial, not fitted
flash identification. The 64 KiB SRAM window is a minimum research map,
not physical-size/alias evidence. ADC inputs and shared clocks remain
research defaults; the 84x48 controller presentation is compatible with the
observed frame, not an identified fitted LCD part.

Read-only taps record eight full-halfword MCU shared-memory initialization writes,
continued CCONT/LCD/EEPROM activity and live own-flash execution. The gate
requires the reviewed failure-frame pixels, rejects repeated reset and DSP
RX/startup publications, and explicitly leaves usable boot/native DSP/key
delivery false. No EEPROM is seeded, no card inserted and no DSP service,
radio or external application responder enabled. The shared writes are not
claimed to be completed ownership exchanges or execution of the verifier.
The failure frame alone does not identify whether NV, DSP or other self-test
conditions selected it.

## NV and startup contracts

The own block reader is `293d92`: arguments are EEPROM address in `r0`,
destination in `r1`, and length in `r2`. It calls address setup `293c04`
with `r1=0`, returns zero if setup fails, otherwise reads intermediate
bytes through `295746(1)` and the final byte through `295746(0)`, then
issues STOP through `295886` and returns one. The final byte is read even
when length is less than two; zero-length safety is not established.
START drives SDA low while SCL is high; STOP drives SDA high while SCL is
high. These waveform semantics, rather than product-name analogies,
identify the two entry points. The static qualifier pins the complete
block-reader body. None of this establishes a valid NV record or identifies
the failing self-test by itself; its callers provide the validation evidence below.

### Observed NV validity failure

The erased-input boot executes validator `258edc`, which calls byte-sum
helper `28a0c4` for these five blocks. The helper accumulates modulo 65536;
the validator complements the zero-extended result and compares the full
32 bits against a stored word. The complete helper and validator bodies
are pinned by the static qualifier.

| Data start | Bytes | Stored word address | Expected for erased data | Observed stored word |
| --- | --- | --- | --- | --- |
| `0000` | 20 | `0014` | `ffffec13` | `ffffffff` |
| `0018` | 12 | `0024` | `fffff40b` | `ffffffff` |
| `0028` | 24 | `0040` | `ffffe817` | `ffffffff` |
| `0044` | 480 | `0224` | `ffff21df` | `ffffffff` |
| `0228` | 20 | `023c` | `ffffec13` | `ffffffff` |

Passive entry taps capture 43 block-reader calls in the fresh eight-second
run, including all ten data/checksum reads above. Checksum observations
occur at 0.186--0.245 seconds and prove all five comparisons cannot match.
They do not establish transport success returns, identify field semantics,
or prove these failures are the sole cause of the screen. No missing trace
hit is treated as absence of a firmware path.

Own self-test wrapper `27dc02` calls the validator and writes `fd` on a
non-one return, zero on success, to `10fca0+12`. A passive write tap
observes the actual `fd` store at `27dc14` at 0.245410308 seconds. This
establishes a failing NV self-test, not merely a hypothetical comparison.
The wrapper body and its result-base literal are independently pinned.

The boundary gate now requires all five ordered erased-data checksum
observations and the subsequent own `fd` verdict in addition to the unchanged
graphical and hardware-activity predicates. Evidence:
`tmp/nse2-nv-selftest-verdict`; this is an ignored run
artifact, not a required repository input. The observer reads only bounded
SRAM buffers/stack words, never extra MMIO or guest writes.

### Checksum-only diagnostic result

```sh
.venv/bin/python tools/run_nse2_boundary.py tmp/nse2-integrity-only --checksum-only
```

This explicitly separate fixture creates an 8192-byte erased EEPROM and
changes only the five four-byte checksum words above. No data payload,
identity, lock, calibration or donor record is supplied. Its input manifest
records the initial image hash and declares authentic provisioning false.
The fresh process receives only this generated external EEPROM and own flash.

Two fresh runs produce five matching comparisons and the actual self-test
store `00` at `10fcb2`; the seven-second frame remains the identical
CONTACT SERVICE hash. The same execution/shared-memory/silent-DSP/bus gates
apply to this experiment; it does not bypass the original acceptance checks
or promote usable boot. The default runner still requires the erased-input
`fd` result. Evidence: `tmp/nse2-checksum-selftests` and
`tmp/nse2-checksum-checked`.

A passive trace of writes to `10fca0..10fcbf` distinguishes additional
firmware results after integrity passes:

| Result byte | Value | Store PC | Own predecessor |
| --- | --- | --- | --- |
| `10fcae` | `0e` | `2333ac` | Getter `28d7b0` returned other than one; semantic owner unresolved. |
| `10fcaf` | `0f` | `2333b0` | Same getter outcome, not an independently identified DSP failure. |
| `10fcac` | `0c` | `233470` | Validation of `0240..031d` against stored halfword `031e`, followed by a nonzero guard using `0290`. |

The last validator calls `23315c`, which sums the specified EEPROM byte
range modulo 65536 and subtracts the two bytes obtained through `280678`
from `0274`. This is a separate checksum family and is deliberately not
repaired by the five-word diagnostic. Next recover this record's full
contract and the writer behind `28d7b0`; do not infer record contents from
the desired verdict. Correct integrity alone is now falsified as sufficient
to remove the failure frame.

The getter is a byte load from `10b9f4` (own literal at `28dabc`), not a
transport transaction. Its loader-owned lifecycle is described below;
neither a non-one value nor the `0e/0f` result labels identify missing DSP
silicon by themselves.

### Separate record integrity and loader boundary

`--record-checksum-only` adds just the two-byte word at `031e` to the
five-word diagnostic above. Its value `db24` is derived from the unchanged
erased payload and the decoded `23315c` exclusion rule; no other bytes are
seeded. The statically pinned consumer at `2333ce` checks that value and
the nonzero guard involving `0290`. Fresh runs observe the actual success
store `00` at `23340a` to result byte `10fcac`, replacing the prior `0c`.
The frame remains the same CONTACT SERVICE image. This clears another
integrity defect without authenticating any record's semantic contents.
Evidence: `tmp/nse2-own-record-checksum`, `tmp/nse2-record-final`.

The `10b9f4` lifecycle is loader-owned. Own routine `27816c` initializes
shared words and starts its upload; its poll at `2781fa` compares shared
offset `+4` with sentinel `ffff`. Expiry writes zero to status byte
`10b9f5` at `278226`. The failure tail at `2782d4` clears that status again
and clears `10b9f4` at `2782d8`. Passive write taps observe precisely:

| Byte | Value | Store PC |
| --- | --- | --- |
| `10b9f5` | `01` | `2781f4` |
| `10b9f5` | `00` | `278226` |
| `10b9f5` | `00` | `2782d6` |
| `10b9f4` | `00` | `2782d8` |

This occurs at approximately 0.0086--0.0089 seconds, before either NV
validator. It survives both integrity repairs. The boundary gate requires
the observed failure sequence, not absence of a hypothetical success hit.
The poll/failure instructions and sentinel/state literals are pinned by
static qualification. The later service routine `2787ac` can set the state
byte to one when it is zero and shared halfword `100e4` is zero; this is a
different lifecycle, not authority to synthesize either condition.

The actual initial upload and consumer-side shared-memory protocol below
constrain backend selection. A silent transport cannot complete a
DSP-owned exchange. A separately labeled ROM4-mask compatibility experiment
may be justified by the own upload ABI, but it must not promote a candidate
mask to fitted identity or replace the exchange with completion constants.

### Measured initial transfer

The loader reads catalogue slot 25 via own literal `27848c = 10ba78`
(`10ba14 + 25*4`), not slot 0. Descriptor `2a0f6c` supplies its 186-word
verifier at `2a0f78`. The Thumb wrapper at `29acb4` enters ARM code; the
ARM `LDM/STM` loop copies ten 32-bit registers per iteration, twenty DSP
words. The caller rounds the count up, so the observed transfer is:

| Surface | Observed own-MCU behavior |
| --- | --- |
| Staging destination | `10e00..10f8f`, contiguous full-halfword writes at ARM PC `29acc4`. |
| Payload | 186 words exactly equal own ROM bytes `2a0f78..2a10eb`. |
| Rounded tail | 14 zero words, matching own ROM `2a10ec..2a1107`; next descriptor starts at `2a1108`. |
| Control publication | `100f6..10102` receives `0100 0300 0001 0000 0001 0001 0200`, in that order. |
| Actual polls | 22 full-halfword reads of `10004`, all `ffff`, from `2781fa/27821a`; then the observed failure-status stores. |

The rounded 400-byte transfer SHA256 is
`c6b5ba7d61aa08e90014499f3e292bd4655b54a0b2f89722a9c52d4ad3c3c658`;
its 372-byte verifier SHA256 is
`0f3b131e112daa10f868c1582faf74088ba70aadea2d59027c12f2823dada7f0`.
The runner independently compares all 200 captured words against the
qualified own flash, including zero padding, masks, addresses and ordering.
It checks control publication before the actual sentinel reads and rejects
capped or incomplete captures. This proves staging, not DSP program loading,
instruction execution or an acknowledged exchange. No extra MMIO reads are
issued by the observer. Evidence: `tmp/nse2-upload-final`.

### Native compatibility acceptance

```sh
make verify-5110i-native-verifier RUN_DIR=tmp/nse2-native-integrity
make verify-5110i-native-verifier-erased RUN_DIR=tmp/nse2-native-erased
```

`nse2r4t` changes only the DSP backend of the ordinary machine composition:
the HLE device is removed and the existing native C54x device is attached.
Own MCU/PPM, GPIO, empty SIM socket and analogue defaults remain unchanged.
The explicit compatibility inputs retain their NSE-1 filenames and pinned
SHA1 hashes:

- Program export: `a05a1e96a8c36ec5a47e1ea059d15afa54ca5739`,
  131070 bytes; its missing word `ffff` stays at region erase zero.
- Data export: `024c7f970f4ef754d3e90471de48a167515f930d`, 131072 bytes.

The backend initializes from these acquired program/data exports; this is a
compatibility experiment, not proof of the fitted NSE-2 mask or physical
power-on RAM contents. The 52 MHz DSP clock is the NSE-1 compatibility
assumption, not an NSE-2 measurement. The shared minimal MCU boot-ROM HLE
also remains declared. No donor EEPROM, inserted SIM, synthetic DSP peer
response, firmware-state write or calibration adjustment is added.

The native verifier writes DSP data `0803=0004` with observed DSP PC `0f0f`.
At that store, a read-only snapshot of program `0f00..0fb9` exactly matches
the own 186-word payload; the acquired program export initially contains
zeros there. Thus this is own uploaded execution, not an inherited verifier
image. The MCU subsequently reads `10004=0004` instead of the silent
profile's sentinel, leaves the timeout branch, and later sets `10b9f4=1`
at `2787e2`. The loader then reaches ordinary runtime; eight native samples
through eight seconds report no illegal instruction. The integrity-repaired
run writes success to result bytes `10fcae` and `10fcaf` and has a blank
84x48 seven-second frame. The erased control independently completes the
native startup exchange while retaining its NV integrity failure.

Native traces stop the initial staging capture at the first actual startup
poll; the successful path's larger ROM-checksum transfers are not silently
claimed as a complete dump. Acceptance checks the full own initial payload,
actual native identity store, observed MCU reply, subsequent ready-state
store, reset count, MCU/native execution horizons and the appropriate NV
fixture verdict. All usable-boot, physical-key, graphical-boot and fitted-mask
claims remain false. Evidence: `tmp/nse2-native-final` and
`tmp/nse2-native-erased-final`.

### Physical input and startup frontier

```sh
make verify-5110i-input-frontier RUN_DIR=tmp/nse2-input-frontier
```

The fixture presses only the own-ROM-derived physical Menu contact
(`COL.1`, mask `04`) at 3 and 5 seconds, releasing it after 200 ms. Both
contacts produce the expected GPIO column transitions `1f -> 1d -> 1f`.
Firmware's last keyboard-mask write is `3f` at `299066`, so all five
columns remain masked and the controller reports `changed=00`; the contact
does not reach decoder `299044`. This is positive evidence of physical
column response with firmware masking, not decoded input acceptance inferred
from a missing trace hit.

The mask belongs to keyboard initialization `299048`, called from `25c70a`
on startup mode 2. Its ABI-preserved `r4` identifies task context `10c660`;
the observer follows actual writes to its consumed-message word `10c662`.
It observes reports `17`, `16`, `15` and the phase-changing input `10`,
but no `14` through eight seconds. Samples from seconds 2--8 show:

| Firmware state | Actual value | Consumer contract |
| --- | --- | --- |
| Context state `10c664` | `0010` | Own 17-entry table at `25c59c` sends state 16 to `25c732`. |
| Phase `10ffac` | `06` | `25c7d2` requires low nibble 6. |
| Report flags `10c5c9` | `0e` | `25c7dc` requires low nibble `0f`; `14/15/16/17` set bits `1/2/4/8`. |

The phase test passes; the report conjunction does not. The loop remains
live, consuming periodic `c8` inputs. It is not the previously studied
3210 shutdown-code path. The own report-14 stub is `2969a6`, posting code
`14` to task 1 via `2558e4`. An immediate-Thumb-BL census finds one direct
caller at `22d9da`; this does not exclude table/argument-driven producers.
The caller's subsystem/entry predicate remains unclassified: do not name it
battery, radio or an external peer merely from neighboring product findings.
The startup table, readiness gate, report stub and state literals are pinned
by static qualification.

The LCD command trace shows normal display mode (`20`, then `0c`) before
the contacts; the captured frames remain blank. No display-type override,
unmask, startup report or UI message is injected. The gate requires actual
contacts, corresponding masked column changes, the incomplete RAM predicates
and uncapped message evidence. It fails deliberately if the frontier advances,
so that progress is reviewed rather than hidden by a stale negative claim.
Evidence: `tmp/nse2-input-final`.

The acquired NokiX scripts archive's external-EEPROM repair directory has
NSE-1/3/6/8 and NSM-1 blobs, but no NSE-2 image. A historical
[NSE-2 EEPROM thread](https://forum.gsmhosting.com/vbb/f131/nse-2-5110i-eeprom-51879/)
is a retrieval lead only; no own dump was acquired or added as an input.

## Next boundary

1. Classify the own `22d9da -> 2969a6` report-14 producer and recover its
   entry predicate. Compare that contract with actual startup inputs before
   proposing any device behavior or provisioning fixture.
2. Recover own EEPROM record semantics and defaults where the producer or UI
   actually consumes them; never substitute a donor identity or calibration.
3. Recover fitted flash identity, physical memory/clock and analogue inputs
   without changing returned IDs or samples merely to satisfy self-tests.
4. Keep native-mask compatibility distinct from fitted-mask identification.
   Do not add completion constants, donor provisioning, firmware state writes
   or radio responses to obtain a frame.

Static parser tests cover member identity/extents, catalogue bounds/null
slots, verifier identity and resident CALL corruption. They do not establish
emulated hardware behavior or replace the future runtime gates.
