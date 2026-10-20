# Nokia 5130 NSK-1 bring-up

## Current boundary

Own NSK-1 v5.61 service streams are qualified and normalized. The explicitly
labelled `nsk1r4t` native compatibility instrument runs both families with
own erased EEPROM and acquired NSE-1 resident DSP input. Three explicit
standard-family external-input comparisons separate the startup dependencies:
the published record-pair example removes the erased-record restart;
security-checksum repair activates the eleven application tasks/report `15`;
and a declared nominal CCONT ADC2 hypothesis supplies report `14` and reaches
the complete checklist `0f`. The combined fixture renders exact **Insert SIM
card** pixels at 10 and 20 seconds and forwards physical Menu/Up/C contacts
as logical `19/17/1a`. The own SIMI register/clock/FIQ setup now selects the
shared controller. With the explicit synthetic card inserted, native T=0
initialization reads identity/service files and scans fifty ADN records,
then renders **Security code**. A separately decoded retry-byte fixture permits
physical `12345` acceptance and Menu/Select navigation into the Phone book
list. This is native compatibility boot, SIM initialization and physical
code/menu acceptance. A two-process gate additionally saves `ADA/123` to the
SIM, cold-boots only its own persisted files and physically searches/renders
the contact without rewriting it. The extended four-process gate physically
deletes it and verifies erased ADN storage after another cold search.
Registration, calls and SMS remain unproved.

Ordinary erased storage remains a failed-startup control. The record-only
and checksum-only fixtures remain blank, with checklists `0a` and `0e`.
The combined fixture deliberately retains its firmware-owned record update
and reason-`04` restart rather than suppressing them. Identity and calibration
are not borrowed. `2915` separately stops after its first loader exchange.
Physical SIM wrong-PIN/cold-correct-PIN verification also passes using the
shared laboratory card. Physical PIN exhaustion, cold PUK recovery and cold
replacement-PIN verification also pass; wrong-PUK/permanent-block behavior
remains untested on this product.
Fitted-mask, physical-clock and measured analog calibration evidence remain
incomplete; no independent NSK-1 DSP mask has been acquired.
Do not inherit
NSE-1/NSB-1 EEPROM contents, DSP identities, acknowledgement counts or clocks.

## Acquired inputs

Source: [archived NSK-1 service package](https://archive.org/download/Nokia_DCT3_firmwares/nsk1_561.zip).
Archive SHA-256:
`2cd19ef2618cb6678274704041845301918fef0de052f1b077c8399ec38a81e6`.
The existing collection manifest records its acquisition. ZIP CRC checking
and strict contiguous Wintesla decoding pass for all 18 members: two MCU
streams and eight PPM streams per family (`A/B/D/E/F/O/S/Y`). Every decoded
stream contains the NSK-1 product marker. Installers are not executed.

The filename families are kept separate; `2915` is a filename discriminator,
not an established hardware revision or DSP-mask identity.

| Stream | CPU span | Decoded SHA-1 |
| --- | --- | --- |
| `nsk1__05.610` | `200000..2adeff` | `7ff6a6371d06cb4ab8b09cc5ce766484ae84fa36` |
| `nsk1__05.61a` | `2c0000..2fffff` | `93e23bbc816918cc82efc52a017caa802869eea3` |
| `nsk12915.610` | `200000..2aebff` | `ccf88c1a5c20200d34ee636d5ae10d96970a3a5c` |
| `nsk1ppm5.61a` | `2c0000..2fffff` | `5ba9c0c34ac93d73886c5d548942b36e3c7106d0` |

The first family embeds `V 05.61`, the second `V  5.61`; both give
`18-11-99`, `NSK-1`, `(c) NMP.`. The PPM headers match their own MCU family.
Normalization preserves all supplied bytes and fills only the unsupplied
MCU-to-PPM gap with `ff`; it does not create EEPROM or calibration data.

```sh
7z x -y roms/archive-dct3-packages/nsk1_561.zip -otmp/nsk1-v561-package
.venv/bin/python tools/extract_dct3_wintesla.py \
  --mcu tmp/nsk1-v561-package/nsk1__05.610 \
  --ppm tmp/nsk1-v561-package/nsk1__05.61a \
  --flash-output roms/noki5130/5130f561a.fls \
  --expect-flash-sha1 a2a99ec6c52104eebcbae281cf8e845bdc8ea1d9
.venv/bin/python tools/extract_dct3_wintesla.py \
  --mcu tmp/nsk1-v561-package/nsk12915.610 \
  --ppm tmp/nsk1-v561-package/nsk1ppm5.61a \
  --flash-output roms/noki5130/5130f561a-2915.fls \
  --expect-flash-sha1 64068df6b7950b47f34ac141d0ef9c51dc666cca
```

Both outputs are 1 MiB CPU-order images. Their header-declared
`200024..2bffff` big-endian halfword sums match stored values `3d67` and
`e26b`, respectively. This is a bounded integrity calculation, not execution
of the own validator or whole-image authentication. ARM big-endian entry
`200040` reads the header at `200000`, rearranges its bytes and writes the
result to `040000` before setting banked stacks. Disassembling that entry
as Thumb is invalid; later interworking boundaries must be established.

No fitted internal boot/DSP image, external EEPROM or factory identity is
supplied by this package. Native compatibility requires an explicit,
separately labelled instrument; source acquisition alone is not support.

## Startup and upload structure

`tools/nsk1_static_check.py` pins both normalized hashes and checks the
catalogue initializer, all 28 pointer slots and descriptor payload bounds.
Each variant has 25 nonzero descriptors; slots 11, 26 and 27 are null.
The copy initializer declares 112 bytes destined for MCU RAM `10b260`.
Its pointer table starts at `296d10` in the standard stream and `296d00`
in the `2915` stream. Slot 25's descriptor is respectively `29c2e4` and
`29d044`, with raw header `0f00 0000 00ba 0700 00b4 0000` and 186 payload
words. The bootstrap's familiar ABI prefix does not prove a fitted DSP mask
or that firmware selects this slot at runtime. Other descriptor addresses
and lengths genuinely differ between variants; they are not interchangeable.

ARM entry sets banked stack pointers using literal `10f370`, clears the
own BSS starting at `100020`, then uses `ADD/BX` at `2000e4/2000e8` to
enter Thumb at `2000ec`. These establish a 64-KiB addressed RAM requirement
for this startup, not a fitted SRAM capacity or mirror decode.

EEPROM initializer `28efb4` (standard) / `28efa4` (`2915`) reads the
literal header address `200005` and derives the same bit fields as NSB-1:
capacity `1 << ((descriptor & 7)+9)`, transfer page
`1 << (((descriptor >> 3) & 7)-1)`, and address-mode bit 6. Own descriptor
`92` therefore declares 2048 bytes, two-byte transfer pages and one-byte
addressing. The shared helper `dct3_eeprom_descriptor.py` decodes that
grammar while retaining distinct product values and no fitted-part claim.
This says nothing about provisioned identity/security/calibration contents.

Own EEPROM START `2909c6` and STOP `290954` construct register address
`20020` and manipulate SDA output bit 0 and SCL output bit 2. They also
set SDA direction bit 0 at `20024`. The `2915` counterparts are `2909b6`
and `290944`; their five checked GPIO instruction fragments are identical.
`nsk1_static_check.py` checks those fragments on both pinned images, with
mutation controls for each fragment. This establishes output wiring only:
SDA input sampling is independently checked at reader `290886`: direction
bit 0 is cleared at `20024`, SCL rises, and bit 0 is sampled at `20020`.
Fitted EEPROM part/page wrap and electrical timing remain unproved.

Own CCONT reader fragment `291830..29184a` selects `22` at `20028`, writes
the command at `2002a`, polls bit 2 of `20029`, and receives at `2002d`.
LCD helpers `292172/2921d8` use commands/data at `2002c/2002b`, polling
bit 0 of `20029`. These match the shared GENSIO register contract.
The keypad scanner `2933c4` separately uses row signal `31`, direction
`2f`, column input `30` and five rows/columns; IRQ mask manipulation uses
`33`. These do not overlap the serial interface. The default later-product
keypad layout does overlap it and must not be retained for this composition.
The static checker covers these own instruction fragments and both variants'
literal base addresses. The key-table contract below independently confirms
power column `10`; runtime logical-key/UI acceptance is still absent.

```sh
.venv/bin/python tools/nsk1_static_check.py roms/noki5130/5130f561a.fls
.venv/bin/python tools/nsk1_static_check.py \
  roms/noki5130/5130f561a-2915.fls --variant 2915
```

Both real images pass, along with initializer/payload/null-slot mutation
controls. The cold-transfer observations below preserve the own MCU's catalogue
selection and DSP release/identity grammar; no release result is synthesized.

## Cold-transfer contract

The standard image's cold-transfer entry is `273488`; the separately
located `2915` counterpart is `273478`. The former loads catalogue cell
`10b2c4` from literal `2737ac`, exactly slot 25 of the initialized table,
then copies the descriptor payload using its declared count/transfer fields.
It initializes shared words `10000/10002/10004/10006` with zero plus `ffff`
sentinels, and constructs seven argument halfwords beginning at `100f6`:

```
0100 0300 0000 8000 0001 0001 0200
```

The argument-write fragment is at `2734c6..2734f6` (standard) and
`2734b6..2734e6` (`2915`). After the firmware's release/control update,
`27351a` compares shared `10004` against `ffff`; later alternating
handshake loops are at `2735c8/2735d2`, with final `10002` sentinel polling
at `2735da`. Runtime transfer counts and successful results are not yet
observed. Do not replace any of these reads with fabricated values.

The later initializer `273624` clears the shared window, sets ring cursors,
loads descriptor 0 through catalogue base `10b260` at `27373c`, and copies
its first five header halfwords to the argument area before signaling it.
The standard descriptor-0 header matches the established NSE-1 loader
grammar (`fd00 ff80 0244 0500 0078 0000`), while the `2915` payload count
is `0241`. Shared grammar does not make the payloads, variants or resident
masks interchangeable.

These own MCU contracts support the separately labelled native compatibility
composition against the acquired NSE-1 ROM4 input. Its baseline keeps own
MCU/PPM and erased external storage; any derived fixture is separately
declared with its input provenance below. Bounded read-only upload/CPU
observations cannot promote board/clock/fitted-mask correctness from a
successful handshake or justify seeding NSE-1 EEPROM to pass later checks.

## Native compatibility observation

`nsk1r4t` uses the own 1-MiB image, an initially erased 2-KiB serial EEPROM,
the common boot-ROM substitution and the explicitly named NSE-1 program/data
ROM inputs. The 52-MHz DSP clock, capacity-compatible flash/EEPROM parts,
and default analog inputs are compatibility assumptions,
not recovered NSK-1 board calibration. The bounded 64-KiB SRAM / 1-MiB flash
map is reused from the existing NSE-3 extent, not a fitted-part attribution.
No DSP HLE, synthetic SIM, radio/service responder or automatic physical key
sequence is enabled by the machine composition; the separately selected
input fixture drives only mapped physical contacts.

```sh
.venv/bin/python tools/nsk1_rom4_compare.py mame/mame roms tmp/nsk1-native-qualified
.venv/bin/python tools/nsk1_rom4_compare.py mame/mame roms \
  tmp/nsk1-2915-native-qualified --variant 2915
```

Each command requires a new isolated directory, pins all input hashes, checks
the own first slot-25 upload byte-for-byte, retains the passive trace and
five-second PNG, and verifies unchanged executable flash and EEPROM capacity.
Its result is an observation report, **not** a graphical-boot acceptance gate.

| Observation through five seconds | Standard | `2915` |
| --- | --- | --- |
| Own cold release PC | `273504` | `2734f4` |
| First upload matches own slot 25 | Yes | Yes |
| Descriptor-0 release observed | Twice, `273786` | No |
| Total DSP releases | 3 | 1 |
| Native illegal instruction sampled | No | No |
| Final DSP PC | `408d` | `0f17` |
| Final MCU PC | `289406` | `2893f8` |
| Final presentation | Blank | Blank |

The standard family's sampled early result is `0000/ffff/0004/0004`;
`2915` reports `0000/ffff/0004/0003`. Their uploads are genuinely different;
do not transfer the standard family's later progress to `2915` or infer
which resident mask is fitted from either result alone. Standard native
runtime records RF reads and completion strobes, but neither decoded RF
reception nor speech is established.

The final standard MCU PC belongs to the loop `289402..289408`, reading a
firmware-owned byte after the startup/idle predicate chain, not the cold
loader's sentinel wait. The `2915` loader-result branch is a separate
static/runtime question.
Unmapped writes to `600000/600100` are retained in the trace; they are not
evidence for fitted writable storage there. Both experiments retain their
blank frame rather than manufacturing a presentation or completion.

Checkpoint validation: both isolated compatibility runs above reproduce;
the NSB-1 native-bootstrap regression and default 3210 structural gate pass.
All 24 MAME patches apply to the pinned upstream revision. The tools suite
passes 3,738 tests with one skip; these do not establish NSK-1 UI support.

## Presentation and low-NV validation

The LCD receives real commands and pixels. The own sequence initializes
the controller at about 145 ms (`2921e8`), clears all six banks, sets normal
display mode with command `0c`, and later sends nonzero bytes. Intermediate
captures include the all-black test frame; the final white image is not
evidence that the LCD interface never ran.

The passive sampler now captures all five times, not only the final frame:

| Five-second cumulative observation | Standard | `2915` |
| --- | --- | --- |
| LCD commands | 100 | 110 |
| Pixel bytes | 3024 | 3024 |
| Nonzero pixel bytes | 1008 | 922 |
| Startup reasons at 0.1/0.5/1/2/5 s | `0a/0a/04/04/04` | `0a/0a/0a/0a/0a` |

For a bounded read-only startup trace, add `--debug-startup` to the runner.
The probes use debugger temporary counters only; they do not change guest
registers, RAM, storage, keys or peer results. Their retirement condition is
acceptance of the low-NV validation contract.

On the standard family, own routine `25a2fa` reads 12 bytes at EEPROM zero
and `25a306` reads eight bytes at `000c`. It builds the task-3 request with
command/length `15/14`; the trace at post primitive `255268` confirms all
20 supplied data bytes are `ff`. Adjacent requests `14/0c` and `16/18`
likewise originate in own EEPROM reads at `0014` and `0020`. These are
observed input bytes, not a borrowed handset's provisioning or an invented
reply contract.

The later record object reaches `25a814` via dispatcher call `22c6f2`.
Its byte 9 is `32`; the validator examines the packed bytes beginning at
offset `0c` and reaches decision `25a994` with state 2 and validity 0.
Its first observed payload begins `b2ade0d9 99085170 cbb40000`; the later
warm-lifecycle object differs. These are transformed results, not plaintext
EEPROM fields. The arithmetic and modeled key provenance are checked below;
their complete storage-policy meaning remains distinct.

The same routine's continuation calls restart entry `295136` with reason
`04` from `25a9f4`; subsequent startup retains reason `04`. The guard at
`25a9b0..25a9c8` also reads register offset `6a`, so a validity-zero result
alone must not be described as an unconditional restart. This locates the
observed restart in firmware's own validation lifecycle, rather than
proving an ADC failure, missing DSP reply or bad LCD geometry. No validity
flag, branch, packed record or internal completion is patched.

The record-only experiment below is not a complete synthetic external
profile and must not be treated as factory data.
Do not reuse `make_5110_eeprom_profile.py`'s NSE-1 MSID, identity, FAID or
fault-latch values: sharing capacity and a compatible resident DSP does not
make those handset records NSK-1 evidence.

### Record exchange arithmetic

The complete captured command-`13` response is
`340e00829f623628a1e763cc898cb419`. The independently tested MSID codec
decodes it to `e688a8f2 00160010 a8a9aa26`: its first word matches the
own command-`13` request, and its chip field matches the compatibility
backend's modeled COBBA serial. It is unchanged across both observed
lifecycles. This is evidence about the virtual compatibility machine,
not an original NSK-1 serial or fitted-mask identification.

The rejected `35/32` response corresponds to command **`16`**, not a
direct reply to `15`. It contains two decoded blocks and the echoed
24-byte encoded request. The acquired ROM4 lock table at data `b6df` and
encoding schedule at `b6f7`, keyed with the observed chip field, reproduce
both complete replies byte-for-byte. Each decoded block's private marker
is removed by the native response builder: the arithmetic recovers
`b35b` cold and `f7db` warm, while the response carries zero in those
two bytes. Neither marker is thereby a valid stored record.

Own MCU helper `259782` explains the input difference. It reads twelve
EEPROM bytes at zero, duplicates them, replaces each adjacent byte pair
with its unsigned product low-byte-first, reverses byte order while
bit-reversing/complementing, then XORs the resulting 24-byte pad into
the request. Erased input produces cold request `7f80` repeated twelve
times; the warm request is the raw 24 `ff` bytes. The independent helper
reproduces both. This is not nondeterministic DSP key material or transport
corruption, and does not authorize changing the firmware's staging flag.

```sh
.venv/bin/python tools/nsk1_record_exchange_check.py \
  tmp/nsk1-record-exchanges/error.log --initial-erased
```

The ordinary standard-family runner also requires this comparison. It
rejects changed envelopes, ordering, input mapping, MSID checksum, decoded
blocks and echoes. `dct3_rom4_record_codec.py` now single-homes the pure
arithmetic used by both NSK-1 and NSM-1 checkers, without importing NSE-1
identity/provisioning constants. Its encoder accepts explicit data and
chip bytes; it does not select a passing policy or identity.

### Constrained external record fixture

The [2003 first-hand MBUS discussion](https://nokiafree.org/forums/archive/index.php/t-38381.html?s=905ef8616030d0920e8836236655ced0)
publishes a numeric example pair for MCC/MNC/MSIN and GID lock data:
`0010100000000010007954c2` / `0000000029000000000054c2`.
It is a specific 001-01 example, **not** original NSK-1 factory provisioning
or a universal unlocked profile. The private marker `54c2` independently
matches the acquired resident data word `b703`. No implementation from the
post is transcribed: encoding uses the native-observed word transform.

`nsk1_record_profile.py` requires a qualified own erased-control capture,
rechecks its executable/resident hashes, unchanged persisted flash and
erased low EEPROM, and derives the chip field from its actual MSID exchange.
It starts a fresh 2-KiB erased image and writes only encoded example bytes
`0020..0037` plus the own low-block checksum at `003c..003f`. Reader
`25ac1a` compares `sum16(0000..003b)` against that big-endian 32-bit field.
Identity, FAID, retry, fault-latch and calibration fields are not populated
from NSE-1, another handset, or the source run's evolved storage. The
manifest labels the fixture diagnostic and retains its data/trace hashes.

```sh
.venv/bin/python tools/nsk1_rom4_compare.py mame/mame roms tmp/nsk1-profile-source
.venv/bin/python tools/nsk1_rom4_compare.py mame/mame roms \
  tmp/nsk1-record-only-21s --record-profile-from tmp/nsk1-profile-source \
  --debug-startup --seconds 21
```

This native run produces the expected decoded blocks and private markers,
then reaches accepted copy `25ab08` -> `25ab0c`: 24 decoded bytes are copied
into context `10ba34+1c`. The intermediate decision is state 2 / validity 1.
The gate requires this real copied data, not only the intermediate flag,
and unchanged final low 64 EEPROM bytes. There are two DSP releases rather
than the erased control's three; reason `0a` remains through the 10- and
20-second samples, with no sampled illegal DSP instruction. The reason-`04`
restart is absent without changing a guest register, flag or reply.

The LCD still clears to white: five-second counts are 68 commands / 2016
pixel bytes / 504 nonzero bytes, unchanged at 20 seconds. This closes only
the constrained record-format experiment, not useful graphical boot,
complete provisioning, SIM acceptance or network service. The ordinary
machine remains erased by default. Replace this diagnostic in favor of a
complete independently grounded profile, rather than promoting its example
policy or remaining erased fields to factory defaults.

## Key tables and startup readiness

Own decoder `2948dc` uses normal table `2adbac`, special table `2adbc8`
and layout selector `10be96`. The `2915` tables are separately read from
their own literals (`2ae90c/2ae928`). Layout zero uses raw index
`row * 5 + column`: the existing Menu contact (column 1 / row 2) selects
logical `19`, Up (1/4) selects `17`, and C (1/1) selects `1a`.
The cold comparison at `2951be` requires raw `84`; special-table column 4
maps to logical power `0d`, independently confirming power mask `10`.
The static checker pins the decoder, both tables and the comparison, with
mutation controls. This is firmware lookup evidence, not a successful
runtime UI input transaction.

```sh
.venv/bin/python tools/nsk1_rom4_compare.py mame/mame roms \
  tmp/nsk1-input-qualified --record-profile-from tmp/nsk1-profile-source \
  --debug-startup --seconds 21 --physical-keys
```

The isolated fixture presses/releases Menu at 12 s, Up at 14 s and C at
16 s. All six contacts are recorded, but no key reaches forwarder `294862`
and no new rendering follows. At every contact, layout is zero, IRQ mask
is `ce` (IRQ0 disabled), column mask is `3f`, byte `10ffb0` is `06`, and
checklist `10be35` is `0a`. There are no reads or writes in the watched
standard SIMI window `20036..2003f` through that interval. An own control
write at `2810e4` independently addresses `20039`; this is not evidence
that a missing controller reply blocks an already active SIM transaction.
Do not force input masks or SIM initialization to hide the earlier state.

The startup consumer gives this state a precise scope:

| Own code | Contract |
| --- | --- |
| `25bbec..25bc24` | Input codes `14/15/16/17` set checklist bits `1/4/2/8`, respectively. |
| `25bc40..25bc46` | Input `10` selects low-nibble state 6 via setter `291968`. Its source is not established by the scalar-post trace. |
| `25bc92..25bca6` | State must be 6 and the checklist low nibble must be `f`; otherwise execution returns to receive helper `25ad34`. The live comparison sees `0a`. |
| `291ad2` / `22a1c0` | Literal report-14 stub and its mapped direct caller. Recover the caller's preceding lifecycle. |
| `291af6` / `24168c` | Literal report-15 stub and its mapped direct caller after the eleven-byte aggregation test at `241672..241682`. Its eleven direct flag-setting inputs are mapped below. |

Only `16` and `17` of this checklist family are observed through scalar
post primitive `2553b8`; the live bitmap confirms `14/15` are not latched
in this run. Other observed task-1 scalars are `32/33/35/37`, not substitutes
for checklist bits. The mapped direct callers are not an absence proof for
indirect/table-driven producers. The task-2 control dispatcher `22c5f8`
also receives real payload `0d00`; its arrival is not proof of full storage
or hardware self-test success, nor a reason to invent different status bits.

Aggregator `2415ac` dispatches inputs `0a..12,14,15` to the eleven bytes at
`10bd88`; `13` is a no-op and `64` clears the array. Input `0a` falls through
to the all-flags check; the other flag setters branch there. Only eleven
nonzero bytes permit publication of report `15`.

| Input | Flag index | Direct call site |
| --- | --- | --- |
| `0a` | 0 | `20e3f0` |
| `0b` | 1 | `2187e4` |
| `0c` | 2 | `2657f8` |
| `0d` | 3 | `23b2ba` |
| `0e` | 4 | `2495e4` |
| `0f` | 5 | `209b8c` |
| `10` | 6 | `24d354` |
| `11` | 7 | `2227a2` |
| `12` | 8 | `269362` |
| `14` | 9 | `207560` |
| `15` | 10 | `2643e0` |
| `64` | reset all | `267c80` |

`tools/nsk1_startup_census.py IMAGE [--variant 2915]` validates the dispatcher,
table and handler instructions and scans all 524,287 aligned Thumb-BL
candidate positions. Both variants recover 12/12 direct calls and their
literal arguments, with zero undecoded candidates to this target. This is
not closure over indirect calls, ARM callers or interpreted data. Input
numbers alone do not establish task ownership.

The standard 21-second physical-input trace observes only input `64`, from
`267c80`. The write watch records BSS clearing and the explicit reset loop;
all six contact-time snapshots contain eleven zeros. No flag-setting write
is observed. A shared activation predecessor is therefore a stronger next
candidate than eleven independent missing replies, but remains a hypothesis.

### Application activation gate

The own 12-byte task descriptors begin at `2a7c80`. Descriptor 10 points to
`213081`, whose Thumb entry calls `20e3ec`; descriptor 14 points to `2495e1`.
Their first actions publish aggregator inputs `0a` and `0e`, respectively.
Supervisor `28907e` explicitly activates the eleven corresponding task indices
at `289242..289280` through `254cd2`. The observed activation calls instead
contain only `7,8,3,4,1,19,22,9,2,6,5` (decimal task indices).

At `289218`, the observed predicate is zero, boot reason `0a`, class byte
`02`, flags `00`, stage `01`, startup byte `02`. The branch therefore goes
to `289286`, skipping the eleven-task activation sequence. This is not the
later reason-`0a` exclusion at `289238..289240`.

Predicate `282934` waits while bit 2 of status byte `10fde5` is set, then
returns whether bit 6 is set. The write watch sees initialization `00 -> 40 -> c0`,
then `c4`, followed by `84` at the checksum-failure store `22c454`.
The observed checksum comparison at `22c3dc` is computed `db24` versus
stored `ffff`. The `0d00` control handler later clears bit 2 (`84 -> 80`);
it did not cause the earlier loss of bit 6.

Checksum routine `22c158` normally allocates `de` bytes, reads EEPROM
`0040..011d`, sums bytes modulo 65536, then subtracts the two bytes of the
word read by `27e15a` at `0074/0075`. Caller `22c3be..22c3e8` compares the
result with the BE16 at `011e` and also rejects a zero combined value with
the word at `0090`. The allocation-failure branch reads only `9e` bytes;
do not silently treat that distinct path as equivalent. No checksum is
repaired in the record-only control. Even a correct checksum does not prove
the erased block's remaining fields or every other self-test valid.

### Checksum-only external fixture

```sh
.venv/bin/python tools/nsk1_rom4_compare.py mame/mame roms \
  tmp/nsk1-checksum-comparison --record-profile-from tmp/nsk1-profile-source \
  --security-checksum --debug-startup --seconds 21 --physical-keys
```

The fixture changes only `011e/011f` from `ffff` to `db24` relative to the
record-only control, before execution. Its manifest records that delta and
the initial EEPROM hash separately from the accepted record-pair provenance.
The ordinary erased machine and the record-only acceptance gate are unchanged.

Firmware then updates storage itself, including the record pair and security
fields `0073..0075/007a`, recomputes the security checksum to `da54`, and
restarts with reason `04`. On the next native exchange the new record is
accepted. The supervisor's predicate returns one and all eleven application
tasks activate in the decoded sequence, producing all eleven flag inputs and
report `15`. The late checklist is `0e` rather than `0a`; report `14` remains
absent. Standard SIMI-window writes begin (18 observed), but no reads occur
and physical input remains masked. The display is still blank at 20 seconds.

The existing arithmetic checker validates both complete `35/32` read
exchanges against their respective captured requests. A new `35/34` update
notification lies between them; its full 54-byte payload is retained, not
treated as a read response. The notification's update semantics are not yet
qualified. The observer now captures the bounded envelope-declared payload
length instead of truncating every record reply to 52 bytes. This experiment
is activation evidence, not graphical boot, factory provisioning or complete
record-update conformance. The next frontier is the report-14 caller's own
preceding lifecycle and the now-active SIM controller contract.

### Report-14 power lifecycle

Own task descriptor 19 selects entry `229e24`, which loads context `10b2d0`.
The context stores its event at `+32` and state at `+34`. Dispatcher
`22b6fe..22b710` bounds the state to `0..1d` and selects one of thirty BE32
targets at `22b714`. It is not an ordinary returning helper: continuation
code receives another event and redispatches, sometimes through separate
receive tails. An entry-only ROM tap is therefore not a complete event
trace. The fixture instead watches the actual state/event RAM writes and
captures late snapshots; repeated identical writes are suppressed and the
transition log is capped at 128 records.

The checksum fixture observes states `18 -> 1a`, then the firmware restart,
then `18 -> 1a -> 4 -> 3`; states 4 and 3 first appear at approximately
4.821 and 6.307 seconds. All six 12..16.2-second snapshots show state 3,
event `26`, sample `10fe58=03ff`. Other observed event values include
`49/4a/4b`. This is a live power-state machine, not an unscheduled task.
The numeric events are not yet assigned hardware semantics.

Reader `289ca0` calls `295ab0` with selector 2, applies the live floating
gain/offset and returns the 16-bit sample. The current research board's
CCONT channel 2 default is `03ff`; it is not an evidenced NSK-1 battery
voltage. The state-26 handler at `22b998` uses a **signed** comparison of
the sample with `01fe` and a four-count startup window before entering the
state-4 path through `22b2ce`. This identifies an input-dependent lifecycle,
not proof that changing the sample produces report `14`.

The report-14 block `22a192..22a1c4` sets a byte state to `0a`, manages
timers `a8/a7`, calls `263610`, and publishes through `291ad2`. One decoded
predecessor is event `26` at `229f60..229f64`, entering `22a13a`. It calls
`26283a` and tests countdown `10a051`; zero clears a flag and enters the
report block, while nonzero selects another timer continuation.

`26283a` decrements the countdown only when sample validator `262766`
succeeds. Source 7 is read through `292862`; its own ten-byte channel table
at `2adbd0` is `00 04 05 06 07 03 01 02 02 01`, selecting CCONT channel 2.
After live gain/offset, the sample is multiplied by 1500 and divided by 232.
Both resulting sample words must lie in the **signed, inclusive** range
1800..5500. The source-8 branch and differential correction are conditional;
this is not a claim that every branch uses the same raw channel.

The live comparison sees `19d6` (6614), exactly `floor(1023*1500/232)`;
the report-14 gate repeatedly observes countdown `0a`, never zero in the
bounded trace. Thus the saturated generic ADC input cannot satisfy this
validator in the observed calibration regime. This explains the immediate
countdown gate, not every possible route to report `14`.

The startup census now validates these opcode fragments, scale literals,
ADC table and countdown pointer on both acquired firmware variants. Mutation
tests protect the extraction. Physical units and fitted battery calibration
remain separate evidence requirements: independently ground the NSK-1 board
input/transfer contract before promoting a nominal-voltage fixture to the
product default. The saturated control is retained for comparison.

### Nominal analog-input comparison

The `nsk1r4t` research machine exposes a configuration input for CCONT ADC2.
It defaults to the unchanged saturated control (1023). The sole alternative
is 554: the declared nominal 3.6-V / typical CCONT 6.5-mV-per-bit hypothesis
also used in the explicit NSB-1 instrument, not a measured NSK-1 pack value.
The input is applied to the CCONT source at reset, before firmware converts
it. No guest ADC result, countdown, report, interrupt mask or UI state is
written by the harness. Other machines lack this optional configuration port
and retain their existing product selector tuples.

```sh
.venv/bin/python tools/nsk1_rom4_compare.py mame/mame roms \
  tmp/nsk1-nominal-comparison --record-profile-from tmp/nsk1-profile-source \
  --security-checksum --nominal-adc2 --debug-startup --seconds 21 --physical-keys
```

The runner writes a normal MAME configuration file before startup and records
the requested raw source in its manifest. Device reset logs must independently
confirm `22a` on both sides of the firmware restart. The firmware comparison
then sees `0dfd` (3581), its countdown reaches zero, report `14` is emitted,
and the task-1 checklist reaches `0f`. Physical Menu, Up and C reach forwarder
`294862` with codes `19/17/1a`; IRQ0 is unmasked. SIM-register reads begin,
but this does not certify SIM transaction success.

`check_nominal_startup` requires that structural chain, exact forwarded keys,
sampled legal native execution, and both 84x48 **Insert SIM card** frames.
Their RGB SHA256 is
`a879c658a532b6526d3343287b42848f8a16bf10b7215086c1e215db45ee67cc`.
The saturated checksum control still reproduces `0e` and blank presentation.
This is an explicit research-input acceptance, not promotion of 554 to an
evidenced board default or proof that a fitted NSK-1 DSP mask matches NSE-1.

## SIM controller and inserted-card contract

Own initializer `280ff8` acknowledges IIR `38`, programs SIM clock `3a`,
manages MAD2 FIQ mask bits 6/7, enables clock-control bit 5, initializes FIFO
controls `3d/3e`, and writes control `39=32`. Activation at `280c22` sets
control bit 7; RX data is read from `37`. These fragments and the base
literal `281188=20000` are pinned independently on both firmware variants,
with mutation tests. The reused SIMI model owns the FIFOs, serial cadence,
FIQ6 and card activation. Cold insertion exercises UART delivery, not the
separately unvalidated hot-plug/FIQ7 lifecycle.

The research machine enables that controller and an explicitly synthetic
card peer, but defaults the physical socket to empty. The unchanged
empty-socket graphical gate still passes after controller enablement.

```sh
.venv/bin/python tools/nsk1_rom4_compare.py mame/mame roms \
  tmp/nsk1-card-comparison --record-profile-from tmp/nsk1-profile-source \
  --security-checksum --nominal-adc2 --insert-sim \
  --debug-startup --seconds 21 --physical-keys
```

The runner configures insertion before boot, rather than posting an internal
SIM event. The handset selects legacy DCS application directory `7f21`.
The card applies [GSM 09.91 option 1](https://www.etsi.org/deliver/etsi_etr/100_199/174/01_60/etr_174e01p.pdf):
one backing GSM application, with visible `7f21` preserved in directory
responses. The prior empty `7f21` directory accepted selection but rejected
its EFs; it was not a missing MCU event or native DSP reply. The shared card
conformance gate separately verifies directory identity/restoration,
authentication and Telecom isolation.

The inserted-card gate requires own `A0 B0` reads of ICCID (`2fe2`, ten
bytes), phase (`6fae`, one), the requested eight-byte SST prefix (`6f38`),
and IMSI (`6f07`, nine). It requires exactly the ordered absolute ADN scan
1..50 at `6f3a`, without a failed READ RECORD, plus the same complete
readiness/physical-key predicates as the empty control. Frames at 10 and
20 seconds must match the 84x48 Security code prompt RGB SHA256
`f82d19b12314200132f0d19ab17a2d86b202d65ba7bc817d1a5c6cbb90a59dfc`.
This is phone-lock presentation, not successful code entry, CHV verification,
persistent contact mutation or radio/service acceptance.

## Phone security and menu contract

Own validator `28f534` accepts five-character input, encodes/compares it and
passes its Boolean result to retry gate `28f4e0`. The latter loads byte
`10bee8` and rejects all results at counts at least five. Below five, a
failed comparison increments the byte, while a successful comparison clears
a nonzero count. Both persist through record `0709` via `28c9da`.

Own lookup `28c690` uses directory `2a8b34`; group 7 points to `2a88bc`.
Its direct entries map `0701` to eight bytes at EEPROM `02a0`, encoded-code
view `0708` to four bytes at `02a0`, and retry `0709` to one byte at `02a4`.
The static checker validates the gate, literals, directory and overlapping
field extents independently on both variants. The NSK-1 offset is **not**
the NSB-1 offset `04ac`.

The erased-retry control physically enters `12345` and reaches the own gate
with `match=1/retry=ff`: the code matches, but the retry gate refuses it.
The positive fixture changes **only `02a4` to zero** beyond the existing
record/checksum inputs, before execution. The same physical code then reaches
`match=1/retry=0`. Three subsequent physical Menu contacts complete the UI
continuation, open the Phone book menu and select its list. This is explicit
synthetic retry initialization, not a recovered factory default, credential
change or live firmware-state bypass.

```sh
# Negative control: matching code, erased retry counter, remains locked.
.venv/bin/python tools/nsk1_security_compare.py mame/mame roms \
  tmp/nsk1-profile-source tmp/nsk1-security-negative
# Positive control: only the independently mapped retry byte is initialized.
.venv/bin/python tools/nsk1_security_compare.py mame/mame roms \
  tmp/nsk1-profile-source tmp/nsk1-security-positive --initialized-retry
```

Both isolated 31-second runs require all eighteen physical contacts, the
exact decoded nine-key sequence, matching-code/retry observations, full
checklist snapshots, legal native execution samples through 30 seconds,
both complete record read exchanges, unchanged executable flash, preserved
identity and encoded-code bytes, and the expected persisted retry byte.
The intervening native record-update notification remains separately retained,
not silently treated as a read response.

The negative gate requires Code error at 26 seconds and the unchanged
Security code prompt at 28/30 seconds. The positive gate requires exact
84x48 Phone book list pixels at 26/28/30 seconds, RGB SHA256
`3600e3d1cab564b185d03403552ea401e1e76edd07b110184716700828a5df2a`.
The list exposes Search / Add entry / Erase; no persistent contact write,
network registration, call, SMS or fitted-mask claim follows from this gate.

## Physical SIM PIN

```sh
.venv/bin/python tools/nsk1_pin_roundtrip.py mame/mame roms \
  tmp/nsk1-profile-source tmp/nsk1-pin-roundtrip
```

The first process uses the qualified own compatibility inputs and an explicitly
generated PIN-enabled laboratory card, not donor storage. Physical `4321/OK`
issues one VERIFY CHV returning `9804`, displays Code error and persists CHV1
retries 3 to 2. A second process copies only persistent files and physically
enters `1234/OK`: VERIFY returns `9000`, restores retries to 3 and leaves every
other serialized card byte unchanged. PIN prompt, acceptance and the subsequent
separate phone Security code prompt have exact frame checks. The latter receives
its own physical `12345/OK`; this does not substitute for SIM authentication.
Both phases validate native record arithmetic, sampled execution, unchanged
flash and preserved phone identity/code fields. This proves ordinary PIN retry
persistence/recovery, not factory data, fitted DSP or network.

```sh
.venv/bin/python tools/nsk1_puk_roundtrip.py mame/mame roms \
  tmp/nsk1-profile-source tmp/nsk1-puk-roundtrip
```

The three-process extension starts with three normal PIN retries, not a seeded
blocked counter. Three physical `4321/OK` attempts produce `9804`/2,
`9804`/1, then `9840`/0. A cold process displays Enter PUK code, accepts
physical `12345678`, new PIN `4321` and matching confirmation, and issues
exactly one successful UNBLOCK CHV. Only the eight CHV1 bytes and its retry
byte may change; unrelated serialized card contents are compared in full.
A third cold process verifies physical `4321/OK`, restores the separate
phone-code prompt and preserves the entire recovered card. Each phase checks
native record arithmetic, execution samples through 40 seconds, persistent
identity/code fields, executable flash and ordered physical contacts. Exact
PUK/new-PIN/PIN/acceptance/phone-prompt frames guard the UI sequence.
Wrong PUK and permanent PUK exhaustion are not covered by this gate.

## Persistent SIM phonebook

```sh
.venv/bin/python tools/nsk1_phonebook_roundtrip.py mame/mame roms \
  tmp/nsk1-profile-source tmp/nsk1-phonebook-roundtrip --erase
```

The first isolated process uses the qualified own external-input composition
and physical security/menu flow. Down/Select enters Add entry; ordinary
multi-tap `2/3/2` and numeric `1/2/3` produce `ADA/123`. Exactly one
`A0 DC 01 04 20` succeeds for `EF_ADN` record 1. The 32 persisted bytes
contain name `ADA`, length `03`, type `81`, digits `21 f3`, and erased
extension/padding; the other 49 ADN records remain erased. The Saved frame
must match RGB SHA256
`8c01f87b60e87cb07ab22ffe2418b7a4510554681ede41c423e85abecaca5231`.

Cold preparation re-runs the save's raw evidence checks, verifies the own
input composition, then copies only its `inputs`, `nvram` and `cfg`
directories. No RAM, save state, snapshot or donor storage is copied.
The second process physically enters the phone code, opens Search, types
prefix `A` and confirms. It must read ADN record 1 without an UPDATE and
preserve the **entire** serialized SIM file byte-for-byte, not just the ADN
area. Three recall frames show `ADA` and match RGB SHA256
`22e30aaccff2be9f4f9628a8033d6e6c15b6615f2082af9de5a04ff1ba952a63`.
The number's persistence is verified in the exact SIM bytes; the name-only
recall frame is not claimed to display the number.

The save gate samples native execution through 50 seconds; cold recall
samples through 40 seconds. Both validate their own DSP upload/read exchanges,
unchanged executable flash, full readiness, code acceptance, exact physical
keys and preserved identity/encoded-code EEPROM fields. Shared GSM 11.11 ADN
validators are reused rather than giving this product a different record
format. This proves contact save and cold read on the native compatibility
composition, not full SIM security, network service or fitted mask.

With `--erase`, two further isolated processes complete the lifecycle.
The third starts from the qualified saved card, physically selects
Erase/one-by-one/ADA/confirmation and requires exactly one successful
record-one UPDATE. All 50 ADN records must then be erased, with every byte
outside ADN preserved. The confirmation and No phone numbers frames are
checked independently. The fourth copies only persistent files from that
verified deletion and physically searches prefix A. The first frame shows
No phone numbers; two settled frames return to the Phone book list (RGB SHA256
`3600e3d1cab564b185d03403552ea401e1e76edd07b110184716700828a5df2a`),
not a persistent not-found dialog. Exact erased card bytes and absence of
any UPDATE establish the cold-empty result; the menu image alone would not.
Native/security/executable checks apply to all four processes.

`nsk1_input_check.py` requires all physical contact observations and reports
the actual masks, flags, forwarded keys, scalar posts and control reply.
It explicitly does not certify interactive input. Redundant raw-record
debug probes have been retired in favor of the complete exchange checker;
the accepted-copy and restart diagnostics remain for the active fixtures.
