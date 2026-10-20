# Nokia 6130 NSK-3 bring-up boundary

## Current result

The separate native compatibility instrument reaches **Insert SIM card** and
rescans the physical matrix under an explicit nominal ADC2 hypothesis (554).
With the synthetic SIM inserted and an own-initializer retry-byte fixture,
physical12345 submission is accepted and another physical command opens a
readable Messages menu. A separate cold process preserves storage unchanged
and requires the phone code again. The displayed idle content is not accepted.
The saturated control remains blank after diagnostic record acceptance.
Neither fixture proves fitted calibration, complete key mapping, full SIM operation,
registration, calls or speech. Ordinary `noki6130` remains the conservative
erased-source failure checkpoint, not a supported phone profile.

The acquired v5.61 installer contains two independently qualified MCU/PPM-A
pairs. Both normalize to a1MiB flash extent200000..2fffff. Conservative
`noki6130` executes both variants from erased external storage and renders
the exact CONTACT SERVICE frame. The silent DSP leaves shared bootstrap
cells unacknowledged; no usable boot, input or phone service is accepted.
Own checksum-only EEPROM diagnostics additionally clear CONTACT SERVICE to a
blank frame in the native compatibility instrument; they do not reach idle.
Factory EEPROM content, fitted flash/RAM geometry, display viewport and DSP
mask compatibility remain unproved.

The separate published-record diagnostic now reaches native record acceptance
and persistence across a cold process. It still renders only a blank frame:
input, idle and phone services remain unproved. This fixture is not factory
provisioning and is not promoted into the handset configuration.

### Diagnostic record persistence

Own request builder `26ec6a` reads EEPROM `0020..0037`; `26e0be` applies the
retained-record transform using the first twelve EEPROM bytes. The diagnostic
generator preserves the own erased-source identity/calibration and encodes the
repository's documented public numeric example using the independently decoded
modeled chip field. It modifies only the record and the two known checksums.
The example's source attribution is retained in `dct3_lock_record_example.py`;
the remote page currently returns403, so this is not a fresh source verification.

The native peer first accepts `3532`, processes command17 and returns a62-byte
`3534` update, including its one's-complement halfword checksum. Firmware writes
the updated record and checksum to EEPROM, then requests reason04 through
`23d836` (return address `23d83b`), distinct from the invalid-record restart at
`26f330`. After that restart the record remains valid. A separate cold process,
with only persisted NVRAM copied, accepts it without a restart or any EEPROM
change. The accepted-copy routine clears bit3 of the second record's byte8;
that normalization is firmware-owned, not a fixture write.

```sh
.venv/bin/python tools/nsk3_record_check.py roms/noki6130/source \
  tmp/nsk3-startup-checksum-control roms/nsk3r4t \
  tmp/nsk3-record-qualified tmp/nsk3-record-cold-qualified
```

The gate checks native upload handoffs, independent record arithmetic, update
checksum, accepted copies, restart ownership, exact persistent storage and blank
frames. The original erased-source and checksum-only negative controls retain
their stricter failure expectations. The next unresolved question is what keeps
this accepted-record lifecycle from presenting an interactive UI; record validity
alone is not evidence of correct display, power or application initialization.

### Accepted-record UI observation

`nsk3_ui_observe.lua` extends the cold observer with passive LCD-write and
matrix-scan counters and one physical column1/mask04 contact at8.0..8.2s.
This is a raw matrix contact, not an accepted NSK-3 logical key mapping.
The retained `tmp/nsk3-ui-cold-contact` run starts from the qualified persistent
record. Startup join `23ad9c` observes flags00 at own pointer10ff84. LCD traffic
reaches1040 writes by1s and2084 by2s, then stops through16s; matrix reads remain6
and row writes13 throughout the sampled interval, including the contact.
Thus the blank frame is not yet attributable to a display-controller failure:
no subsequent scan or redraw responds to this contact. The next census should
follow application startup/readiness and the keypad-interrupt enable path, not
assume that accepted security records imply a live UI. The scan counter and
logical key mapping remain research observations, not physical-input acceptance.

The follow-up `tmp/nsk3-ui-mask-writer` cold capture localizes the inhibit:
`2aa870` enables scanning from return address2accc1. Column mask writes are
1f/60/60 during initialization, 7f/20 during the own2aa754 scan, then3f at
`2ac31c` at0.284766846s. This last store belongs to routine `2ac2fe`, which
initializes four software-state bytes and explicitly ORs1f into the column mask.
MAD2's later IRQ maskce leaves IRQ0 unmasked; the column mask itself prevents
the contact from latching it. Rowe0/direction1f and columnmask3f persist through
16s. Direct Thumb calls to2ac2fe were located at270438,270506 and27071a;
their lifecycle selection, and the corresponding unmask path, remain unresolved.
Do not clear the mask as a fix: the next boundary is the firmware-owned caller
and re-enable contract. The record/cold-storage gate also passes this capture.

`tmp/nsk3-ui-second-group` identifies caller270506 (return27050b). Its
startup receive join27052c observes37,c8,32,33,c8. The first low-nibble
group10ff94 reaches6, satisfying2705d4. The second group10c42d changes
00 ->08 ->0a ->0e, written by270550;2705e0 requires0f. Missing bit0 is
selected specifically by incoming report14 at27055e, while16/15/17 select
bits1/2/3. This is a real startup readiness gate, unlike the previously
retired 3210 shutdown-report interpretation. The own report14 stub2a88f2
posts `(1,14)` through266b7c; a direct Thumb caller was located at235962.
Its surrounding state-machine path and external measurement inputs remain to
be decoded. No report or readiness bit is synthesized by the observer.

The analog trace `tmp/nsk3-analog-samples` resolves the immediate prerequisite:
event26 repeatedly visits2358ec while byte10a689 remains0a. Routine27851a
only decrements this ten-sample counter when278446 accepts both converted
measurements in the inclusive range0708..157c. Observed samples are19d6/19d6
and return0, above the upper bound. Consequently23592e..235962 does not publish
the readiness report. Own reader2a989a maps source7 through table2c7a54
(`00 04 05 06 07 03 01 02 02 01`) to CCONT ADC2; source8 is a cached sample,
not necessarily a second physical channel. Conversion/calibration and the
meaning of the physical input still require decoding before any changed-input
fixture. This observation does not authorize selecting an ADC value solely to
cross the startup threshold.

`nsk3_analog_contract.py` now anchors the boot4 conversion, signed limit
comparisons, countdown path, scale literals and own channel table; mutation
tests reject changes to every anchored fragment. Its independent read confirms
the1500/232 scale and1800..5500 inclusive validator. Gain/offset are read at
10fe34/10fe38. The saturated identity conversion is6614, matching the observed
19d6; live coefficient values still need direct capture before promoting that
match into a calibration claim. The existing NSK-1 nominal3.6-V/typical6.5-mV
input is a possible explicitly labeled comparison, not measured NSK-3 data or
an authorized product default.

### Nominal-input graphical comparison

Live coefficients are3f800000/00000000 (gain1/offset0), captured in
`tmp/nsk3-analog-coefficients`. The research-only `nsk3r4t` input port defaults
to1023 and offers554 as the pre-existing nominal3.6-V/typical6.5-mV-per-count
CCONT hypothesis. This is not a measured NSK-3 electrical transfer contract.
The normal `noki6130` configuration has no such port.

The isolated `tmp/nsk3-nominal-input` run converts554 to0dfd, accepts both
samples, drains the counter, organically receives report14 and completes the
second readiness group0f. Firmware restores column interrupts and presents
Insert SIM card. The raw contact at8s increases scan reads6->42 and row
writes13->79. Logical key identity and application response are not accepted
from this scan-only observation. EEPROM is byte-identical to the qualified
cold record, and executable flash remains unchanged. Exact frames at5/15s
are separately pinned; their differing hashes are not collapsed into one oracle.

```sh
.venv/bin/python tools/nsk3_nominal_input.py roms/noki6130/source \
  tmp/nsk3-startup-checksum-control roms/nsk3r4t \
  tmp/nsk3-record-qualified tmp/nsk3-record-cold-qualified tmp/NEW-NOMINAL
# Run nsk3r4t in that isolated directory with nsk3_ui_observe.lua, for16s.
.venv/bin/python tools/nsk3_nominal_check.py roms/noki6130/source \
  tmp/nsk3-startup-checksum-control roms/nsk3r4t \
  tmp/nsk3-record-qualified tmp/nsk3-record-cold-qualified tmp/NEW-NOMINAL
```

The rebuilt saturated control `tmp/nsk3-saturated-post-input` still passes the
strict blank-frame/record-persistence gate. The next functional boundary is
own-ROM key translation and physical SIM insertion, not synthetic report14.

### Synthetic card and logical-key boundary

The separate native research instrument now offers an explicitly inserted
synthetic card with its socket empty by default; ordinary `noki6130` remains
unchanged. `nsk3_nominal_input.py --insert-sim` changes only this physical
configuration input in addition to the declared nominal ADC hypothesis.
`nsk3_sim_observe.lua` observes the real SIMI TX/RX byte lanes and the own
translator return sites2ac2ec/2ac2fc without guest writes.

`tmp/nsk3-sim-inserted` receives ATR3b1005, completes the ff00ff PPS exchange
and sends SELECT7f21 followed by STATUS, then further file traffic. The UI
presents Security code at5/15s. This is initial card communication, not yet a
complete SIM initialization/security acceptance gate. Firmware also initializes
additional EEPROM regions; these writes have not yet been semantically classified.

Own normal key table2c7a30 has five rows:
`3e3e3e3e3e / 1119010203 / 0e17040506 / 0f18070809 / 101a0c0a0b`.
Thus the inherited column1/mask04 contact means logical17, not its host Menu
label. The separate `tmp/nsk3-security-physical` probe verifies decoded digits
1..5 and command19 from physical contacts. Own validator tracing confirms19
submits ASCII12345; this meaning is not inferred from another handset's labels.

### Phone-code retry contrast

Own validator2a6510 submits ASCII12345 from28a1d2. At2a64c0 the comparison
result is1, but retry RAM10c520 isff. The guard rejects counts>=5 before
testing the matching result. Directory2c1aa0/group7 at2c1798 maps record070a
to EEPROM03b4,length1; record0709 maps encoded code03b0,length4, inside
record0701's eight-byte context. Own helper2a63c4 clears context+4 and writes
070a through2a394e. `nsk3_security_contract.py` anchors these independent
code, literal and directory facts, with mutation tests.

The explicit `--initialize-phone-retry` fixture changes only03b4 fromff to00
in the external EEPROM before startup, using this own initializer value rather
than donor data. It does not change the credential, RAM, messages or branches.
`tmp/nsk3-security-validator` records matching code/retryff with no accepted
return; `tmp/nsk3-security-accepted` records matching code/retry00 and reaches
the validator's result1 return at2a650e. `nsk3_security_check.py` checks this
contrast and the physical input/ASCII submission chain. This is a retry-state
diagnostic, not a factory-default claim.

After acceptance the idle-content frame has repeated glyphs, but physical
command19 opens a readable Messages menu (Select/Exit and envelope icon).
This falsifies a general post-security display failure; the unresolved issue is
the idle content, not whether menus can render or receive physical input.

### Interactive menu and cold storage

`nsk3_menu_input.lua` repeats the established physical code sequence, then
presses command19 at13.5s. Both `tmp/nsk3-post-security-menu` and
`tmp/nsk3-menu-repeat` reproduce the exact15s Messages frame. The first run's
persisted NVRAM is copied into a fresh process with the same physical card
configuration: `tmp/nsk3-post-security-cold` leaves the whole EEPROM unchanged
and presents Security code again. This is persistent storage, not proof that
phone-lock policy should have been disabled.

```sh
.venv/bin/python tools/nsk3_menu_check.py roms/noki6130/source \
  tmp/nsk3-startup-checksum-control roms/nsk3r4t \
  tmp/nsk3-record-qualified tmp/nsk3-record-cold-qualified \
  tmp/nsk3-post-security-menu tmp/nsk3-post-security-cold
```

The gate covers qualified own firmware/record inputs, physical ASCII submission,
validator acceptance, native uploads, exact prompt/menu frames and the reviewed
firmware-written EEPROM hash across cold processes. It does not establish full
keypad labels, arbitrary SIM support, persistent phonebook, radio, calls or SMS.
Next test phonebook access through this live UI and classify idle content
independently; no missing handoff message should be synthesized.

## Own firmware inputs

Source: `roms/archive-dct3-packages/nsk3_561.exe`, downloaded from
<https://archive.org/download/Nokia_DCT3_firmwares/nsk3_561.exe>.
SHA-256: `43d9413dd8972ca76fbfe899228cbc3ca9df3d2b028267fa9fdc4230ce15baae`.
The installer is extracted as an archive, never executed.

| Label | MCU member | MCU bytes (hex) | PPM-A member | Catalogue | Flash SHA-1 |
| --- | --- | --- | --- | --- | --- |
| boot4 | `nsk3__05.610` | `c7e00` | `nsk3__05.61a` | `2ae384` | `5469c8fe048036673124dd2aece4ef71da1cce22` |
| boot3 | `nsk3___5.610` | `c8b00` | `nsk3___5.61a` | `2ae374` | `bac9e599da510c97c59eca0412db987d7e0bf946` |

Both MCU streams start200000; both PPM streams start2d0000 and contain30000
bytes. Only the unprogrammed gap is filled withff. No identity, calibration,
lock or EEPROM data is fabricated. PPM-A members differ and are not swapped.
Own MCU strings distinguish `V 05.61` and `V  5.61`, both dated22-11-99 and
naming NSK-3.

```sh
7z x -y -oroms/noki6130/source roms/archive-dct3-packages/nsk3_561.exe
.venv/bin/python tools/nsk3_static_check.py roms/noki6130/source \
  --output roms/noki6130
```

The checker pins decoded member hashes, spans and identity strings, then
validates the28-slot catalogue using the existing bounded descriptor parser.
Slot25 has header `0f00 0000 00ba 0700 00b4 0000`, followed by186 DSP words.
Its C54x instruction at payload byte24 stores an immediate to data0803:
4 in boot4 and3 in boot3. These labels record a software bootstrap distinction,
not a fitted-mask identity or compatibility with the acquired NSE-1 ROM4.
The372-byte bootstrap payloads differ at only four byte positions: the
identity immediate and three absolute CALL operands. Boot3 calls8008 twice
and8010 once; boot4 instead calls8023 twice and802b once. The checker anchors
all three C54x CALL instructions. Resident ABI selection is therefore a real
code difference, not merely alternative product text or a PPM choice.

## Reset and peripheral contract

Both images enter ARM at200040 and switch to Thumb at2000ec, with initial
stack literal10fa5c. Header descriptorf4 declares two-byte EEPROM addresses,
32-byte pages and8192-byte capacity. This is not the 5130's2KiB contract.
The profile uses capacity-compatible trial flash and a bounded64KiB RAM
decode covering the stack; neither is a fitted-part identification.

The static checker anchors the own instruction fragments in both variants
(boot3 code sites are10h lower than boot4):

| boot4 site | Contract |
| --- | --- |
| `2a765e/2a772a/2a779c` | EEPROM SDA on PUP bit0, SCL on bit2 |
| `2a8a62` | GENSIO select28/value22, command2a, RX2d, status29/ready bit2 |
| `2a8fa0/2a9006` | LCD command2c/data2b |
| `2aa788` | Five-row scanner, row31/direction2f/column30 |
| `2ac6ea` | Raw power-key comparison84 |
| `29916c` | SIMI IIR38/control39 initialization |

The default mandatory DSP backend has disabled bootstrap and service
contracts: it supplies neither a completion nor a foreign mask. Ordinary
host key labels, physical timing and SIM operation remain unvalidated.

## Cold execution checkpoint

```sh
make build
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsk3-boot4-qualified -- noki6130 -bios 561a4 \
  -rompath ../roms -video none -sound none -nothrottle -seconds_to_run 16 \
  -log -autoboot_delay 0 -autoboot_script ../tools/nsk3_boot_observe.lua
.venv/bin/python tools/nsk3_boot_check.py roms/noki6130/source \
  tmp/nsk3-boot4-qualified --variant boot4
```

Use a fresh run directory. Boot3 uses BIOS561a3, a separate directory and
`--variant boot3`. Both16-second runs retain unchanged executable flash and
initialize the erased8KiB EEPROM to the same firmware-owned bytes. The5s/15s
frames are identical CONTACT SERVICE. Shared cells remain0000/ffff/ffff/ffff
after12 MCU writes; the sampled PC is the supervisor wait2a02f6..2a02fc
(boot3 minus10h), not a direct DSP-poll instruction. No native, graphical
idle or fitted-hardware claim follows from this failure presentation.

## Next boundary

The native checkpoint closes the boot4 upload boundary. The startup contract
below independently separates integrity failures from a format-invalid
decoded record. Next recover a constrained own-capture record input and its
accepted-copy contract; do not import 5130 EEPROM state or inject a verdict.

## Native ROM4 compatibility checkpoint

`nsk3r4t` uses the same own boot4 executable, erased8KiB EEPROM and declared
partial hardware, replacing only the silent peer with the acquired NSE-1
ROM4 program/DROM and its52MHz research clock. It is a separate instrument,
not the normal machine or evidence of a fitted NSK-3 mask. Boot3 is not paired
with this resident ABI.

```sh
mkdir -p roms/nsk3r4t
cp roms/noki5110/nse1_rom4_dsp_program.bin \
  roms/noki5110/nse1_rom4_dsp_data.bin roms/nsk3r4t/
make build
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsk3-native-checkpoint -- nsk3r4t -rompath ../roms \
  -video none -sound none -nothrottle -seconds_to_run 16 -log \
  -autoboot_delay 0 -autoboot_script ../tools/nsk3_boot_observe.lua
.venv/bin/python tools/nsk3_boot_check.py roms/noki6130/source \
  tmp/nsk3-native-checkpoint --variant boot4 --native-roms roms/nsk3r4t
```

Use a fresh run directory. The first release contains the exact own372-byte
bootstrap. Two passes each accept4 into loader state10b840 before64 alternating
zero handoffs at MCU100fe/10100. The loader copies63 groups of512 words plus
510 words and two sentinel words; the checker observes all128 handoffs,
their ordering and final identity/verdict stores4/0 at10b846/10b844. The first
pass ends around0.1182s, the second around0.50195s. A firmware reset request
at2ac696,0.385927692s carries reason04 stored at10ff60; it is retained as an
unresolved startup observation, not suppressed or attributed by analogy.

By1s the native DSP is sampled at resident408d and remains there through16s;
no unsupported/illegal instruction is logged in this bounded run. The MCU
returns to its supervisor wait and both reviewed frames still say CONTACT
SERVICE. Own executable flash stays unchanged; the firmware-initialized EEPROM
has a separate reviewed hash from the silent control. Loader acceptance does
not establish interactivity, SIM initialization, stable radio, speech or
long-run native execution. Those remain unaccepted.

## Storage startup contract

The own record consumer26f150 receives class74/primitive3532. Its format
check reaches26f2d0 with valid0 in both boots. On the first failure, marker
10fdca changes5a->a5, checksum10fdcc is recomputed and caller26f330 invokes
restart2ac65e with reason04 (observed LR26f335). After restart, the same
failure instead changes the marker to55 and returns. This is distinct from
the dormant immediate-reason4 power-key caller2ac6fe. The bounded raw Thumb
scan finds eight BL candidates to2ac65e, including data matches; this is not
an indirect-caller closure.

Requests13..16 are observed at own post primitive266a2c, target3. Identity
reply26ef5c reproduces the requested flash field82153f2d through the shared
MSID decoder and yields modeled chip field00160010. Both record replies
match the independently implemented ROM4 decode and encoded echo exactly.
The invalid decoded blocks are therefore consistent transforms of the erased
inputs, not evidence of a missing DSP response. This does not establish
factory fields or fitted silicon identity.

Two integrity contracts are separately recovered from own code:

| Consumer | Stored field | Computation |
| --- | --- | --- |
| `23aa20`, compared at`23aca4` | `011e..011f` BE16 | Sum`0040..011d`, excluding bytes`0074/0075`, modulo65536 on the normal allocated path |
| `26f556`, called by`2928ca` | `003c..003f` BE32 | Sum`0000..003b`, modulo65536 |

The erased-source comparator trace (`tmp/nsk3-startup-checksum-control`) has
`ffff != db24` in the first comparison and
`ffffffff != 00003bc4` in the second. The two-byte security-only fixture
passesdb24/db24; firmware later writesda54/da54 itself. Its24-entry startup
result sweep still has item12h/code12h, and CONTACT SERVICE remains. Adding
only the four-byte low-block checksum makes all24 entries acceptable and
clears the failure frame to blank. Neither fixture fixes decoded-record
validity or suppresses the early reason04 restart.

```sh
.venv/bin/python tools/make_nsk3_checksum_profile.py roms/noki6130/source \
  tmp/nsk3-native-checkpoint roms/nsk3r4t \
  tmp/nsk3-both-checksums-final/nvram/nsk3r4t/eeprom --low-block
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsk3-both-checksums-final -- nsk3r4t -rompath ../roms \
  -video none -sound none -nothrottle -seconds_to_run 16 -log \
  -debug -debugger none -autoboot_delay 0 \
  -autoboot_script ../tools/nsk3_startup_observe.lua
.venv/bin/python tools/nsk3_startup_check.py roms/noki6130/source \
  tmp/nsk3-native-checkpoint roms/nsk3r4t tmp/nsk3-both-checksums-final --low-block
```

The generator requires the qualified own native control and a fresh output;
it changes checksums only, preserving every identity, lock and calibration
byte. Omit`--low-block` and use a separate run directory for the security-only
control (`tmp/nsk3-checksum-results-final`). The checker guards both complete
upload passes, both24-item result sweeps, native record arithmetic, marker
ordering, unchanged flash and whole EEPROM differences. Firmware changes
only0073,007a and the011e checksum after the initial fixture. The blank5s/15s
frames are a bounded negative presentation result, not graphical boot or UI
acceptance. No firmware RAM, result flag, callback or peer reply is injected.
