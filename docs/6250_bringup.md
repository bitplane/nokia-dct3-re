# Nokia 6250 v5.03 bring-up

## Current boundary

The acquired v5.03 PPM C and product-local PMM reach a correctly dimensioned
96x60 `CONTACT SERVICE` frame in research composition `nhm3hle`, after native
verifier/loaders and request-correlated service-control completion. With the
explicit initial-record PMM comparison and nominal channel-2 battery input,
the same composition reaches graphical boot, startup readiness `0f`, and scans
a physical row-1/column-1 press as raw key `06`. The research composition now
enables the existing SIMI/card boundary: ATR, PPS and file reads run through
the firmware, and a settled physical press opens the Messages menu. Ordinary
fresh and preserved-location network registration and `DCT3 LAB` presentation
are verified in the declared radio HLE. Physical incoming/outgoing call
signaling and complete release are verified, but speech/audio is not.
SMS delivery, reading, deletion and reply submission are verified. An organic
phonebook save and cold-start retrieval from SIM NVRAM are verified below. Normal `noki6250`
retains the fail-closed final publication wait at `429842`; the research
composition is not promoted to supported default boot.

## Inputs

### Remaining goal boundary

The original-input graphical-boot requirement is not complete. The acquired
PMM replays to an inconsistent application checksum, and its other sectors
contain no accepted replacement journal. The initial-record comparison is
an explicit external-input experiment, not an authenticated factory repair.
Suppressing the resulting fault or rewriting the stored checksum merely to
pass is outside the accepted method.

Native DSP execution also stops before missing mask program word `2c75`.
The product's uploaded verifier/loaders have been executed and compared;
they do not contain that mask routine. Runtime HLE is explicitly exclusive
after suspension, not proof that the missing instructions are emulated.

Speech command ownership is recovered, but the product PCM clock/framing
and codec route are not. The available original-manual text preview loses
numeric fields; checked public download links return HTML instead of the
archive, and the alternate PDF host was inaccessible. These results do not
justify donor bus settings. An intact product source or independently
evidenced firmware contract is required to resume that work.

Reproducible research acceptance covers physical menus/calculator, SIM
phonebook save and cold retrieval, fresh/preserved network registration,
incoming/outgoing call signaling and release, and SMS read/delete/reply.
These successes do not discharge the original-PMM, native-mask or audio
boundaries above. Existing `verify` and `verify-frontier` gates also pass.

### Acquired Members

- MCU/PPM: `roms/noki6250/6250-503mcuppmc.fls`, SHA1
  `95607ce39c383bda75f1e6aeae67a214b787b0a1`.
- PMM: `roms/noki6250/6250 virgin eeprom 005fa000.fls`, SHA1
  `57c29c8387caf864603d94a22bfb63ace427b7f9`.
- Legacy uniform-fill DSP audit members satisfy ROM declarations only; the
  HLE does not execute them. The missing boot mask is not fabricated.

## Recovered hardware contracts

The CTSI base literal at `4e7f70` is `00020000`. The release routine
`4e7d7e` writes `40` to CTSI+2, sets bit 2 at `4e7dc4`, and polls bit 4
using `LSRS #5`/carry at `4e7dca..4e7dce`. Its reset path clears bit 2 at
`4e7df0` and waits for bit 4 to fall. `PRODUCT_6250` therefore configures
the MAD2 chip-release mask `04` / running-status `10` contract. DSP code
execution is separate: after staging, `4297b6..4297be` sets bit 0 at
`20002`; `429858..42985e` clears it after retaining the publication.
Passive bus observation confirms startup `40 -> 44`, then launch `11`
with verifier word `f7bb` and fields `0100/0300/0001/d000` already present.
The core execution mask is consequently `01`, not the chip-release mask.

Its CCONT routine independently writes control `22` to GENSIO `2d` at
`4f91cc`, writes its command byte at `2c`, polls status `6d` bit 2 at
`4f91d6..4f91dc`, and reads response `6c`. Control bit 2 stays clear;
the existing byte-write receive trigger is selected for this product.

The profile disables the legacy unvalidated 64-transfer success publication.
Transport ownership acknowledgements remain, but neither final results nor
parked DSP identity values are inherited from another product.

## Reproduction

Run `noki6250` with private working/NVRAM/config directories, the acquired
product ROMs, `-noreadconfig -log -video none -sound none -nothrottle`,
`-autoboot_delay 0 -seconds_to_run 9` and
`-autoboot_script tools/noki6250_bootstrap_observe.lua` (absolute path).
The passive observer checks firmware-observed ready signals and the reviewed
ownership sequence; `6250 early hardware gate: PASS` is **not graphical boot**.
It also captures `6250_frontier.png`. Do not reuse preserved flash for a cold
comparison or interpret process exit alone as gate success.

## Staged verifier

The descriptor at flash `21e544` uploads 223 words to program `0f00`.
Its SHA1 is `6646da3c5be9c70deda7e0b5b9f257d5d2ace815`; a passive
live capture matches those bytes exactly. MCU routine `42975a` sets
`087b..0881` to `0100/0300/0001/d000/0001/0001/0200`. The ownership
flags have already fallen to zero at the observer's half-second snapshot.

The isolated `nhm3verify` core fixture consumes this product's 232 sparse
flash blocks (stride `20`, final two words `ffff`) and stops honestly at
peripheral read `002d`, PC `0f9f`. Explicit COBBA and PROM-version
sensitivity fixtures complete at `0f64`, with computed fingerprint
`c62d430c` and PMST `ffa8`. These inputs are comparisons, not measured
6250 silicon identity or an accepted phone self-test result.

Reproduce with `tools/nsm3_verifier_check.py mame/mame
roms/noki6250/6250-503mcuppmc.fls /tmp/6250-verifier --product 6250`.
This gate does not change the live handset's fail-closed configuration.

The same acquired flash contains a 104-word bootstrap fragment at descriptor
`21d384` (payload SHA1 `440bf49f1eba4cadb12f7f7581c992b0025807d6`)
and a 638-word loader at `2178f8` (payload SHA1
`a324738357565c523ddd8dc513e24491d4c89a1c`). Matching descriptors alone
do not establish their execution order or compatibility with missing mask code.

## Live native upload boundary

Research machine `nhm3stage` uses the same product ROM/PMM and configures
only product-local fragment/loader sources. The normal `noki6250` machine
remains on its fail-closed HLE profile. The native verifier starts around
0.0955 s and publishes `0000/0006/0006/0006` around 1.8613 s under the
existing declared COBBA inputs. Firmware then starts its loader, whose
control word is at `0880` rather than the earlier product's `087f`.

The loader issues selector `0014` then 124 selector-`0001` requests. Its
second upload is independently compared against 613 words from this flash
at `21d4bc` (SHA1 `b1df4b301d67c6c4421ae346478c465a7fd20ff0`). Native
execution suspends before absent program word `2c75`, around 1.8971 s. No mask
instruction is fabricated. This proves upload progression, not acceptance
of provisioning, graphical boot or physical peripheral identity.

Reproduce a fresh `nhm3stage` run with the same private ROM path and isolated
NVRAM/config directories as above, without the early observer. Expected
process status is 0 for the explicit silent observation. Validate
the resulting log with `tools/noki6250_staged_check.py error.log`; exit status
alone is insufficient. `make verify`, `make verify-frontier` and the tool
suite remain the regression guards for existing products.

The loader guard runs once per configured CPU clock during this research
stage instead of the coarse microsecond observer. It verifies the second
upload before its first fetch, then suspends at `2c75`; this is an isolation
mechanism, not a recovered physical DSP clock claim. Other staged profiles
retain their existing guard cadence.

## Runtime comparison

With `tools/noki6250_runtime_observe.lua`, silent `nhm3stage` emits ten
post-loader parameter commits at MCU `429e48`: three zero wire values,
then `8102/900f/8426/920c/920c/920f/920f`, with coefficient `3fff`.
At eight seconds the native PC remains `2c75`. The MCU eventually clears
pending itself; no DSP reply or inferred parameter success is supplied.
Validate using `noki6250_staged_check.py error.log --silent-runtime`.

Separate research machine `nhm3hle` transfers ownership to the existing
request-derived runtime HLE at that boundary. It uses the same acquired
ROM/PMM, with transport discovery enabled and no record-verdict override. D0
discovery traverses TX type `05` and RX `8e`; firmware subsequently sends
`70:0d00`, which is answered by the declared compact service-control peer.
With the acquired inconsistent PMM it renders the service-failure screen.
The separate initial-record comparison plus nominal channel-2 input reaches
interactive idle; its configured acquisition/channel and release contracts
are validated by the acceptance scenarios below.
Display storage/visible geometry is validated below, but the stream includes
commands `0a` and `11` unused
by the current PCD8544 model. The observed D0 discovery frame is
`1e0200d0000305014100`; its semantics need product-specific classification.

## Compact service-control contract

Receive worker `504642..50467e` forwards the `7x` family through `47f2f8`,
which constructs an envelope with class at `+3`, body at `+8` and posts
mailbox 2. Passive task-context observation (`100022`, recovered from
receive helper `3c363c`) identifies its receive caller as `307627`.
Dispatcher `30763a..3076ba` routes class `74` to `304494` except command
`32`, which has a separate path.

The subtract cascade at `3044b6..3044c2` selects command `0d` at `304512`.
Flag bit 2 of `17fd15` arms the wait; the handler cancels timer `17`, clears
that flag and interprets body byte `+9` bits 0/1 as faults. Clear bits clear
fault bytes `17fbf0`/`17fbf1`; set bits write `10`/`11`. Runtime research HLE
therefore uses the existing compact request-correlated `74:0d00` response.
This is declared peer behavior, not execution of the missing DSP self-test.

A fresh run observes exactly one consumer with class `74`, command `0d`,
status `00`, armed flags `84`, then endpoint flags/faults `00/00/00`.
Validate using `noki6250_staged_check.py error.log --service-control`.
The service-failure frame persists: this completion does not establish a
provisioning verdict, ordinary startup settlement or interactive idle.

## LCD geometry

Passive GENSIO captures show eight bank addresses `40..47`, each followed
by an X-address `80` and exactly 96 data bytes, repeated for whole frames.
The inferred storage is 96x64. Nokia's original
[6250 user manual, technical specifications](https://www.mobilgyujtemeny.hu/letolt/User%20manual/Nokia/nokia_6250_usermanual_en.pdf)
specifies a 96x60 visible display. `DISPLAY_6250` declares those dimensions;
the previous 84-column default wrapped data into the wrong columns and
corrupted the first text line. The corrected research snapshot is nonblank
96x60 and renders `CONTACT SERVICE` legibly.

Check the independent stream evidence using
`noki6250_staged_check.py error.log --service-control --lcd-stream`. Geometry
does not identify the controller silicon or validate all commands. Commands
`0a` in extended mode and `11` in basic mode remain unmodeled; no guessed
side effect was added. The observed lifecycle byte `17fe24=01`, written at
`4c11d8` during initialization, is not established as the screen's fault cause.

## Acquired PMM validity

Task 2 stores fault `0c` at `304330`: its sum from `3040a0` is
`6d57`, whereas logical NV `0254` contains `7095`; companion word `0170`
is `1b0b`. The sum covers bytes `0120..0253`, excluding the two bytes at
`0154..0155`. Reader `510e1c` delegates to `4030e8`, which copies from the
RAM shadow at `15c034`. A passive capture of that shadow independently
recomputes `6d57` and confirms both stored words. Its first 32 checksum-range
bytes match the acquired PMM at file offset `0146`, but a 64-byte contiguous
match does not exist: logical NV is not a proven flat PMM-file mapping.

The population trace resolves that distinction further. Copy primitive
`514648`, called by `48078c`, first loads `0a28` bytes from flash `5fa026`
to shadow `15c034`. It then applies a four-byte record from `5faae6` to
logical `0150` and a two-byte record from `5faaee` to logical `0254`.
The base image's checksum is valid (`6d61` computed and stored), but those
acquired record overlays change byte `0153` from `98` to `8e` and the
stored checksum from `6d61` to `7095`. The resulting `6d57` mismatch is
therefore already represented in the acquired PMM record stream, not a
spontaneous RAM corruption. The reader contract below establishes that
these overlays are accepted.

Read-only `tools/noki6250_pmm_check.py PMM --trace LOG` independently
replays 15 records, stopping at file `0ba2`, and exactly matches the captured
checksum-range shadow. Its report separates the valid initial image from
the final overlaid image. This confirms record order and write destinations,
and now checks the record checksum byte independently. Reader `402758`
requires sector state 1, decodes length/destination and applies records in
order until header `ffff` or stop bit `0200`. It skips deletion records
(`0100`) and does not validate the low checksum byte on this read path.
Writer helper `402876` computes that byte as the sum of both destination
bytes and payload, modulo 256. All 14 later records satisfy that contract;
the initial record stores `70` where its acquired payload computes `35`.
Thus the two checksum-region overlays are accepted, individually intact
records whose combined NV content is inconsistent. The input's "virgin"
filename is not evidence of factory-valid provisioning.

The complete acquired `6000`-byte file has one accepted EEPROM sector:
offset `0000` has the signature and reader state `0001`. Offset `2000`
has no EEPROM signature and state `ffff`; offset `4000` is fully erased.
The PMM checker reports these raw sector properties independently. There
is no second accepted journal in this acquired file from which to recover
a valid replacement. This does not establish how an intact factory image
would have been provisioned.

The decoded contract supports the separate initial-record comparison below,
not an unlabelled repair of the acquired input.
Changing `0254` to the observed sum without
establishing that contract would merely suppress a verdict and is not an
accepted correction. `noki6250_runtime_observe.lua` captures the fault and
checksum-range shadow without modifying either. LCD controller identity and
remaining command semantics also remain open.

### Initial-record provisioning comparison

`noki6250_pmm_check.py PMM --initial-record-fixture OUTPUT` creates a
separate, explicitly derived fixture: retain the acquired initial `0a28`
payload exactly, recompute its envelope checksum, erase later records in
the first sector, and preserve all remaining sectors. It refuses a base
payload whose application checksum is invalid and refuses source overwrite.
This is not a recovered factory dump and does not replace the acquired ROM.
Fixture SHA1 is `053627c9d1a8ba40e6b37653f4d77ee7cf0900d8`.

In the earlier full-scale channel-2 comparison, fresh storage and this
`nhm3hle` composition clear
fault `0c` organically at both 8 and 20 seconds. The compact response arrives
with armed flags `c4`, leaving flags `40`; the LCD is blank at both endpoints,
not an idle screen. This is not the current nominal-input boot result:
the channel-2 startup contract below resolves that separate readiness gate.
The native CPU remains suspended at `2c75`. Validate the
21-second run with `noki6250_staged_check.py LOG --initial-record-fixture`.
The expected MAME ROM checksum warning records that this is a derived input,
not the acquired image.

### Startup success and display lifecycle

Flag `40` is retained success, not an outstanding wait. Initialization
`304262..304268` sets it; checksum failure `304332..304338` and the final
fault-array scan `304382..3043a8` clear it on failures (ignoring sentinel
`ff`/`fe` and zero). The compact response clears wait bit `04`. Routine
`47f354` clears pending bit `80` at `47f370` at 3.734 s; the provisioned run
retains `40` afterward. Do not target that bit for clearing.

Passive LCD payload counts distinguish this frontier from invisible rendered
content: five 768-byte transfers occur by 3.001 s. Transfers 1/2/4/5 are all
zero; transfer 3 has 768 nonzero bytes (576 are `ff`) at 2.010 s. No further
LCD data arrives through 20 s. Thus the endpoint blank frame agrees with
the firmware's final zero payload, rather than proving that a menu is drawn
but hidden by the LCD model. Controller identity/command fidelity remain
separate unknowns. The next investigation is the post-self-test UI lifecycle
and organic physical-input handling, not another self-test completion reply.

### Keypad boundary

The own scanner `505c18` iterates five rows at `505c70..505cce`, with
row drive `20028`, direction `200a8`, columns `2002a` and mask `2006b`.
`PRODUCT_6250` now exposes five rows instead of the conservative four.
The power-column mask is still a conservative assumption, not recovered
NHM-3 wiring. Translator `50842a` indexes `288f7c` by `row*5+column`;
the first 25 entries are:

```text
5a 5a 5a 5a 5a
11 19 01 02 03
0e 17 04 05 06
0f 18 07 08 09
10 1a 0c 0a 0b
```

Inherited NHM-5 input labels do not describe this matrix. The physical probe
`noki6250_key_observe.lua` presses/releases column 1, row 1 (own logical
key `19`) at 6/6.15 s without writing MMIO or RAM. It observes no subsequent
scan or display transfer. At both edges the firmware keeps all columns
masked (`3f`), row `e0`, direction `1f`, MAD2 IRQ mask `8e`.
The mask is set by suppression routine `508462`, called from `3b197e`
at 2.011 s, and retained by subsequent scans. Follow that caller's lifecycle
and the corresponding re-enable contract; do not bypass the column mask to
manufacture interactivity. Scanner output is observed at RAM `174050`.

### Missing readiness report

Suppression is part of startup engine `3b1848`, not an isolated input bug.
The ordinary re-enable tails at `3b1a5a`/`3b1af6` clear the low five column
mask bits and call `5083d2(1)`. Before these tails, `3b1a2e..3b1a42`
requires phase byte `17fe38 & 0f == 6` and readiness byte
`172c85 & 0f == 0f`. Physical observation at 8 and 20 seconds instead
gives phase `06`, readiness `0e`.

The readiness inputs map directly: `14` sets bit 0, `15` bit 2, `16` bit 1,
and `17` bit 3 via `3b19c4..3b19ce`. Inputs `17`, `16`, `15` set the observed
mask to `08`, `0a`, `0e`; report `14` is absent. This is a real measured
readiness gate, distinct from the successful self-test flag `40`.
Context `172ca4` contains a diagnostic counter, last input at `+2` and
dispatch continuation at `+4`; its counter value `04` is not a boot mode.

The literal report-14 publisher is `4e966c`, posting value `14` to task 1
via `3c3588`. An aligned Thumb direct-call scan identifies caller `30bf3c`.
Its surrounding lifecycle includes firmware strings "VBAT Checks" and
"Start limited fast VBAT reads"; therefore the next investigation is this
analog/power readiness path and its inputs, not an invented DSP packet.
This direct-call observation is not an exhaustive exclusion of indirect or
data-driven producers. The default ADC tuple is still conservative for
NHM-3. The full-scale comparison below identifies the corrected channel-2 input.

The report owner is analog/power task 21 (receive caller `30af9d`, observed
current-task byte `100022`), with context `1704a4`, event at `+20` and state
at `+22`. The initial-record comparison runs through states `11 -> 13 -> 4
-> 3` at approximately 0.054, 2.015, 5.490 and 5.955 seconds. Timer event
`49` and periodic events `4a`/`4b` are delivered: this is not evidence of a
stopped analog task or missing timer.

State 3 (`30cea0`) accepts event `41`, or on event `49` tests predicate
`4f918a(00009004)` while its retry count is nonzero. The predicate's register
map `2893a8` (literal at `4f92a8`) resolves index `10` to serial command `70`,
mask `04`; it normalizes that bit to a Boolean. The serial address is
`(command >> 3) & 0f`, selecting CCONT register `0e` and its charger-reset
cause. Table `289af0` is not this reader's descriptor map. At 8/20 seconds,
context fields `+0e/+11` are `01/00`
and the retry count decreases from `04` to `00`, with readiness still `0e`.
The predicate is observed at 9.324/12.724 seconds with argument `00009004`,
cached status `13/00` and count `04/03`; neither status has bit 2 set.
These observations do not establish that ordinary power-on should set the
charger-reset cause. Recover the preceding branch selection and event
`41` producer before modifying the board inputs; do not inject the bit or
report 14 to bypass this lifecycle.

The firmware's diagnostic strings name the observed path "CHECK TEST MODE",
"CHECK POWER ON REASON", "INIT CHARGING", "BOOT UP CHARGE", then
"WAIT CHARGER VOLTAGE SETTING" and "CHARGER DISCONNECTED". These names
describe this analog routine, not proof that the board was charger-started.
Power-on-reason byte `17fe15` is set to `0a` at 0.064 seconds by `4c11b8`.
The selector `4c116e` reads MAD2 register `20001`; bit 0 set selects that
value (`lsrs #1` followed by carry-set branch `4c1176`). The observed read is
`01`, matching the model's reset default. Thus the charging-named analog
initialization is reached with the existing ordinary-reset latch, not because
that latch is missing or because a charger cause was injected. State 12 also
tests byte `17fd74`, observed zero at initialization and cleared at `30cce0`.
Follow the analog completion/connection lifecycle before changing reset causes.

The repeated event `26` invokes measurement accumulator `3daaa8`. It only
decrements initialization counter `1691e2` if `3da9d4` accepts both converted
samples in the inclusive range `0708..157c` (1800..5500). The observed pair is
`19b5/19b5`, with counter `0a`: both exceed the upper bound, so the counter
cannot drain and this path cannot reach report 14. This is a concrete input
validity failure, not a reason to synthesize a ready report.

Logical source 7 goes through `500d86` and the own-ROM mux at `288fa0` to
physical ADC selector 2. Its conversion applies calibration then scales by
1500/232 (`3da9d4`); the current raw selector-2 input is full scale `3ff`.
The acquired calibration is gain `3f7ecb5a` (0.9952904), offset zero. The
product-local nominal channel-2 input is now `230`: the firmware converts it
to `0e11` (3601), and report 14 posts at 3.439 seconds. Readiness settles at
`0f` and phase `03`; the physical probe is scanned after 6 seconds. The
endpoint frame is `Insert SIM`. This is a declared nominal board input,
not a measured transfer curve or a recovered factory PMM dump. No other
ADC channel, firmware state, report, or NV payload was changed.

Reproduce with the initial-record fixture and `noki6250_key_observe.lua`
using the fresh-run command above, then run
`python3 tools/noki6250_staged_check.py LOG --initial-record-fixture --physical-ready`.
The checker proves report delivery, settled startup predicates and matrix
scan; it does not prove menu semantics or pixel identity. Inspect the captured
96x60 frame separately. Default and coherent 3210 regression gates reproduce.

## SIM boundary and interactive menu

The own-ROM initializer `491fd8..492034` writes SIMI causes at `20038`,
control at `20039`, and FIFO controls at `2003d/2003e`. Reset routine
`491bb4..491c0c` drains `20037`, asserts control bit 0, then bit 7. These
match the existing controller grammar. `nhm3hle` enables the controller and
synthetic card as a declared boundary comparison; `PRODUCT_6250` remains
conservative outside this research composition.

Observed receive routine `491edc` consumes ATR `3b 10 05`; transmitter
`491cb8` performs PPS and SELECT/STATUS/READ BINARY/GET RESPONSE/READ RECORD
exchanges. The controller's existing FIQ route is sufficient for these
firmware-owned exchanges. This is runtime validation of the path, not a
complete static census of all SIM faults or electrical timing.

The initial-record fixture reaches a `Headset` idle-style frame with Menu
and Names at 20 seconds without late input. The literal label is an
observation; its accessory/profile ownership is not yet decoded. The physical
probe at 16/16.15 seconds opens the Messages menu (Select/Exit), captured
at 20 seconds by `noki6250_key_observe.lua`. The probe contains physical
input only; it does not select callbacks, write RAM, or post UI messages.
The first 6-second press occurs during SIM reads and is not the menu proof.

### Input layout and phonebook save

The separate power map at `288f98` is `5a 5a 5a 5a 0d`; only column 4
translates to Power. `PRODUCT_6250` now uses five rows and power mask `10`.
Its normal table at `288f7c` matches the existing five-row input definition,
so the named `noki6250` input set includes that definition rather than the
incorrect four-row 3310 layout. This establishes firmware translation;
all side-key semantics and physical electrical wiring are not independently
measured.

`tools/noki6250_phonebook_observe.lua` uses only physical cells to open
Names, select Add name, enter `A`, enter `123`, and confirm. Captures show
the Name editor, Phone number editor and `Saved to SIM card`. After normal
MAME exit, `nvram/nhm3hle/sim_card` contains a 3524-byte card image whose
first 32-byte ADN record is `41`, 17 erased bytes, `03 81 21 f3`, then ten
erased bytes; the remaining 49 ADN records are erased. Thus the save reaches
durable card storage, not just a UI acknowledgment.
Run this probe for 35 seconds with fresh per-run storage and the explicit
initial-record fixture; the runtime observer captures its own two endpoints
and the phonebook probe captures each editor/save stage separately.

For cold-start readback, exit the save run normally, preserve its NVRAM, and
start a new MAME process in the same run directory with
`tools/noki6250_phonebook_readback.lua` for 29 seconds. This physical-only
sequence selects Names/Search/List/Details; captures show `A` and number
`123` after reboot. It never issues an UPDATE RECORD. Both runs are checked
by `tools/noki6250_phonebook_check.py STAGE NVRAM FRAME`, where STAGE is
`save` or `readback`; use `6250_phonebook_7.png` and
`6250_phonebook_readback_5.png` respectively. The check compares the reviewed
96x60 luminance pixels and validates the exact persistent record plus all
49 erased records. Keep save/readback as separate processes: an in-process
UI reread is not this persistence test.

## Radio acquisition comparison

After SIM enable, passive DSP TX inventory observes a type `56`, 160-byte
candidate window: first big-endian channel `0013`, then 79 `ffff` entries.
Seven type `51` configuration blocks follow; their semantic contents are
not established by the packet inventory. Research `nhm3hle` selects only
the existing candidate-window acquisition strategy. No assignment, handover,
traffic-release or neighbor encoding constants are imported from another
product. The assigned confirmation below is independently 6250-derived;
the remaining later contract fields stay unvalidated.

With this declared HLE comparison, firmware organically sends type `02`
channel changes for ARFCN 19, type `4a` acquisition control, a type `0f`
68-byte neighbor list, type `0c` random-access requests, and an assigned
channel configuration at 10.697 seconds. Own confirmation consumer `464738`
compares RX body `+4` bit 0 at `464756` with pending-context byte 2
(pointer at `1728e0`). The observed assigned context is `0402/01/01`:
confirmation zero mismatches, producing only an empty establishment request.
Confirmation one matches; the firmware emits a Location Updating Request,
acknowledges the network's Accept and Channel Release, writes EF_LOCI offsets
4/10, releases SDCCH and maintains serving BCCH/PCH. The frame displays
`DCT3 LAB` and signal bars. Release context `0409` has its own completion
branch and the peer supplies zero; it is not a global success=one contract.

Research `nhm3hle` therefore sets assigned confirmation one. A cold restart
with retained NVRAM emits the persisted-location request and completes the
same protocol exchange. It refreshes EF_LOCI offset 4 between Accept and
Channel Release, but does not rewrite status offset 10. Both runs pass
`tools/radio_registration_trace_check.py LOG --profile nhm3`; add `--preserved`
for the second process. This validates registration reuse, not every later
call/SMS/DSP primitive. Reproduce with `nhm3hle`, the initial-record fixture,
fresh storage, `noki6250_runtime_observe.lua`, 40 seconds and `-verbose`;
inspect the `dsp_hle: TX packet` and `dspif_transport: RX enqueue` streams.

Missing native mask code and immutable peripheral identity remain explicitly
unvalidated; runtime HLE must not manufacture record/self-test verdicts merely
to reach idle.

## Incoming-call signaling

The incoming-call network fixture pages the registered handset. Firmware
publishes Paging Response, Call Confirmed, Alerting and Assignment Complete.
Physical Send at 20 seconds publishes Connect; the network acknowledges it.
Physical End at 24 seconds publishes Disconnect, followed by Release Complete
and the network's RR Channel Release. The LCD returns to `DCT3 LAB`.

Firmware's type-02 traffic-release request is
`040000001117001a600000130000001400000001`. The research profile selects
its observed parameter `14`; the peer returns type-89 zero and the own
consumer observes context `0409/00/00`. Firmware resumes serving-channel
configuration and PCH; HLE speech frames cease at release. Without this
profile field the peer did not recognize the release transaction, leaving
speech traffic active despite the LCD returning to idle.
Speech/audio ownership is also unvalidated; visible call UI and CC signaling
do not establish audible voice.

Reproduce with a private copy of
`fixtures/radio_incoming_call_answered/nhm3hle.cfg`, fresh NVRAM, the
initial-record ROM fixture, `tools/noki6250_call_observe.lua`, 35 seconds and
`-verbose`. `tools/noki6250_call_check.py LOG` checks the ordered signaling
through confirmed release and resumed PCH, and rejects continued speech
traffic after confirmation. It does not validate audio.

For an outgoing call, omit the incoming-call configuration and set the
harness-only `NOKIA_DCT3_6250_OUTGOING=1` when running the same physical
input script. It presses 1/2/3, Send at 20 seconds and End at 28 seconds.
The firmware publishes CM Service Request and SETUP containing number
`123`, acknowledges network Connect, then completes release back to PCH.
Validate with `noki6250_call_check.py LOG --outgoing --number 123`.
This uses the same evidenced channel contracts, not a new speech backend.

## Incoming SMS

The network SMS fixture produces an organic `1 message received` screen.
Firmware acknowledges segmented SAPI-3 CP-DATA, writes the ordinary `hello`
SMS-DELIVER into EF_SMS record 1, sends RP-ACK and releases SDCCH. The SIM
NVRAM contains the exact unread record after process exit. Its observed
no-cipher type-14 publication is `002effffffffffffffff0000`; this is a
6250-specific publication, not a borrowed NSM-5 control value.

Reproduce with a private copy of `fixtures/radio_incoming_sms/nhm3hle.cfg`,
fresh storage, the initial-record fixture and runtime observer, for 35 seconds
with `-verbose`. Validate with `radio_incoming_sms_trace_check.py LOG SIM_NVRAM
--profile nhm3`. This transport gate is distinct from the UI tests below.

Physical inbox reading and confirmed deletion are now independently exercised.
`noki6250_sms_observe.lua` presses Show, Options, Erase and, with harness-only
`NOKIA_DCT3_6250_SMS_DELETE=1`, OK at the confirmation prompt. The read frame
shows sender `5551234` and text `hello`; EF_SMS status becomes `01`. Confirmed
deletion changes status to `00`, preserves the message body, and returns to
the Inbox menu. No firmware state is written by the script.

Run the incoming-SMS fixture for 35 seconds with this script. Validate the
read outcome using `noki6250_sms_check.py LOG SIM_NVRAM FRAME` (capture 2),
or the deleted outcome with `--deleted` (capture 5). The checker requires
transport closure, exact stored payload/status, storage-update counts and
the reviewed 96x60 frame. These are separate fresh-process runs.

The same harness with `NOKIA_DCT3_6250_SMS_REPLY=1` physically selects Reply,
Empty screen, enters `Hi` through multi-tap keys, selects Send and confirms
the sender's number `5551234`. Firmware sends the exact SMS-SUBMIT through
SAPI 3 and displays `Message sent` after the laboratory network's CP/RP
acknowledgements. Run for 50 seconds and check capture 8 with
`noki6250_sms_check.py LOG SIM_NVRAM FRAME --sent`. The SIM argument is used
only by inbox modes; sent-mode proof comes from the exact outgoing payload,
recipient, acknowledgement/release sequence and reviewed frame.
The level-enabled incoming fixture can page again after this later release;
that second receipt is not counted as a second outgoing submission.

## Application interaction

Physical menu enumeration identifies Calculator as menu 7 in this ROM.
`noki6250_app_observe.lua` with harness-only
`NOKIA_DCT3_6250_CALCULATOR=1` opens it, enters 1, selects Add, enters 2
and selects Equals. The firmware displays `1 + 2 = 3`; the application,
arithmetic and menu layout are ROM-owned. Run the initial-record fixture
with fresh storage for 45 seconds and validate capture 14 using
`noki6250_app_check.py LOG FRAME`. This proves a complete application
input/result workflow, not every installed application or game.

## Speech-control boundary

The physical calculator workflow has a fresh-state acceptance runner:

```sh
.venv/bin/python tools/run_noki6250_calculator.py /tmp/6250-calculator-new \
    --rompath run_model_scout_6250/roms
.venv/bin/python tools/run_noki6250_acceptance.py /tmp/6250-incoming-new \
    --scenario incoming-call --rompath run_model_scout_6250/roms
.venv/bin/python tools/run_noki6250_acceptance.py /tmp/6250-outgoing-new \
    --scenario outgoing-call --rompath run_model_scout_6250/roms
.venv/bin/python tools/run_noki6250_acceptance.py /tmp/6250-sms-read-new \
    --scenario sms-read --rompath run_model_scout_6250/roms
.venv/bin/python tools/run_noki6250_acceptance.py /tmp/6250-sms-delete-new \
    --scenario sms-delete --rompath run_model_scout_6250/roms
.venv/bin/python tools/run_noki6250_acceptance.py /tmp/6250-sms-reply-new \
    --scenario sms-reply --rompath run_model_scout_6250/roms
.venv/bin/python tools/run_noki6250_acceptance.py /tmp/6250-phonebook-new \
    --scenario phonebook --rompath run_model_scout_6250/roms
.venv/bin/python tools/run_noki6250_acceptance.py /tmp/6250-registration-new \
    --scenario registration --rompath run_model_scout_6250/roms
```

The ROM path must contain the prepared acquired NHM-3 ROM members. The
runner creates a separate initial-record PMM comparison from the acquired
source, isolated configuration/NVRAM/log directories, and an execution
manifest. It refuses an existing run directory. Success requires the
ordered physical calculator actions and reviewed `1+2=3` pixel result,
not merely a successful emulator exit. This validates `nhm3hle` with
explicit comparison provisioning; it does not validate ordinary
`noki6250` boot or speech audio.

The call scenarios use the same isolated comparison provisioning. Incoming
copies the network-page configuration into the private run directory;
outgoing physically enters `123` and presses Send. Their checkers require
the ordered call-control exchange, physical Answer/End or dialing/End,
assigned-channel release confirmation, and return to idle PCH with no
continued speech-radio traffic. These are signaling tests, not voice tests.
The runner clears conflicting calculator/outgoing fixture environment
selectors before selecting its scenario.

SMS scenarios copy the incoming-message network configuration into the
private run directory. Read/delete validate delivery and acknowledgement,
the persisted SIM record and its read/erased status, and the reviewed UI
frame. Reply physically composes `Hi` for `5551234`; acceptance requires
the exact submission, network acknowledgements, release and sent frame.
Each scenario starts with fresh SIM storage, and conflicting SMS fixture
selectors are cleared before execution.

Phonebook acceptance saves `A`/`123` through physical input, validates the
reviewed save frame and ADN contents, exits MAME, then starts a separate
process with the same persisted storage. Cold-start acceptance requires
the reviewed contact-detail frame and byte-identical SIM storage after
readback. The first process's log and SIM image are retained separately;
the manifest records both commands. No emulator save state is used.

Registration acceptance runs both fresh and preserved-location boots in
separate MAME processes. Each must complete Location Updating, its expected
SIM writes, assigned-channel release and steady camping. The fresh log and
SIM image are retained before the second process; the second checker uses
the NHM-3 preserved-location policy, not the fresh-write ordering.

The 6250 command-8 helper is `429a34`: its jump table at `429a88` selects
`429de6`, which combines the low 12 parameter bits with `8000`, stores its
own shadow at `16e47e`, publishes shared `100a8`, and commits through DSPIF.
Compiler publication at `3fc33a` calls it with `0102` during startup,
`0426` after initialization, `0626` on physical Answer, and `0426` on End.
Transient startup value `0506` has a different caller (`4ae5d7`).

This is not the NSE-8/NSM-5 speech field `0201`. Own compiler `3fbc54`
reads keep-mask `fdff` from ROM `263160`; `3fbc62..3fbc6c` adds the selector
halfword from ROM `263154`. The five table entries are `0200`, and the
observed answer selector is 1 with desired word `0426` before the addition.
The inactive branch `3fbc40..3fbc46` applies the same keep-mask without
adding the field. Research HLE therefore decodes command 8 and predicates
speech requests on field `0200`; no physical bit meaning is assigned.

`noki6250_audio_observe.lua` passively traces helper calls, field selection
and writes during the incoming physical Answer/End fixture. Validate the
own ROM plus a 35-second run using `noki6250_speech_control_check.py ROM LOG`.
The normal call-signaling checker also passes after this configuration.

This does not establish working audio. The conservative 6250 profile has
no independently evidenced PCM bus clock/shape or COBBA analog route; its
zero frame clock leaves the HLE speech cadence disabled. Recover those
contracts from product documentation/firmware before supplying a donor
profile or claiming audible voice. Native mask execution remains a separate
unresolved boundary.

### Product documentation boundary

The Nokia NHM-3 service manual, `System ModuleOriginal.pdf`, is indexed in
[the 6250 service archive](https://www.eserviceinfo.com/downloadsm/26151/Nokia_6250.html).
Its [system-module text preview](https://www.eserviceinfo.com/preview_html.php?fileid=26151&previewid=12844)
identifies COBBA_GJP as the speech AD/DA owner, connected serially to MAD2WD.
The baseband description assigns input/output selection and gain control
to COBBA under MAD2 control; the audio sections describe the speaker and
microphone attachments. This supports the existing transport/codec boundary,
not a particular PCM configuration.

The preview has damaged font extraction: much prose is recoverable from
the embedded character mapping, but numeric values are missing. It therefore
does not establish PCM clock rates, sample framing, codec gains, or the
internal microphone selector. The archive download-start endpoint returned
an HTML landing page rather than its advertised RAR in the checked request.
An intact PDF or product-owned register initialization remains necessary
before enabling the 6250 speech cadence. Do not treat recoverable prose as
evidence for absent numbers or substitute a sibling handset's configuration.
