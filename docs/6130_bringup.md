# Nokia 6130 NSK-3 bring-up boundary

## Current result

**Readable cold idle:** the own `nsk3r4t` boot4/PPM A compatibility
composition now passes physical Net monitor exit and a separate cold restart.
This is a54s UI/storage qualification, not sustained native operation or
registered idle. `Headset` is the existing accessory/profile presentation,
not an operator name or validated unattached-board wiring.

### Physical Net monitor exit and storage contract

Own descriptor0604 at `0x2c1778` is `0000060403a90001`: its selector
is EEPROM `03a9`, length1. Physical Test00 changes `ff` to `3f`.
The5190/6190 offset04a1 and value41 must not be imported into this ROM.
Own logical-key table `0x2c7a30` independently supplies Menu, Up, zero
and the right softkey contacts used by the fixture.

After physical SIM PIN1234 and separate phone-code12345 authentication,
Menu28s/Up30s encounters a Battery low notice. Menu32s acknowledges it;
Menu34s opens Test01. Zero37s/zero39s produces Test00, and Menu41s
returns readable Headset idle. Right-softkey44s opens Names; another at46s
returns idle. A cold process repeats only authentication, showing title-free
idle at25/29/31s and Headset idle at36/42s without repeating the selection.

Relative to the primed predecessor, firmware changes211 EEPROM bytes:
selector03a9 plus210 bytes in30 repeated33-byte spans within1966..1d43.
Own records072a/072b/072c and aliases072e/072f/0730 anchor these areas.
Their full indexing and write semantics remain unmapped; they are not
classified as harmless metadata or excluded from acceptance. The complete
8192-byte post-selection bank is identical after cold restart, SHA256
`26b7dd4abc3ea4d9c712c470ebaf03eaf77d3c6b9e1077351ff8daa0f8503ae2`.
The primed predecessor hash is
`7554f3abb2f69da09454f36042e7a8d4d91195c72d5da7d63045649f1d5cc25d`.
Own executable flash, original complete PIN-enabled laboratory card and
external input configuration remain unchanged. No EEPROM edits, ADC tuning,
firmware-state writes or synthesized completions are used by this sequence.

`nsk3_diagnostic_display_check.py` checks own descriptor/key bytes, complete
storage hashes and cold links, authentication/APDU ordering, nine physical
contact pairs, nine reviewed selection frames and five cold frames. Passive
native observations require the unsupported-instruction stop flag clear at
each snapshot; they do not prove fitted-mask compatibility or RF execution.
Six unit tests reject contract, bank, contact, native-marker and provenance
mutations. The current executable acceptance is:

```sh
.venv/bin/python tools/nsk3_diagnostic_display_check.py \
  roms/noki6130/source tmp/nsk3-diagnostic-idle-baseline \
  tmp/nsk3-diagnostic-display-accepted \
  tmp/nsk3-diagnostic-display-accepted-cold
```

Add `--mame mame/mame` with two fresh output directories to execute both
isolated54s processes rather than recheck captured evidence.

### Native idle save/load and physical continuation

`nsk3_native_state.lua` starts a fresh process from the physically selected
cold bank above, authenticates the original PIN-enabled card and phone lock,
then saves at40s. The shared native-state fixture observes37 ARM registers,
36 DSP fields and rolling checksums of the backed64KiB ARM research RAM,
ordinary DSP data and program. Load exactly restores those observations.
The2.7s reference/restored windows repeat7764 DSP I/O writes with identical
relative timestamps, ports, values and masks. Physical Menu in both branches
renders the reviewed Messages frame, RGB SHA256
`2ab2adf527d11235a1ce8e0dfbc5b640e0f1011d70c7aadff363cd573b61ff9f`.
Complete EEPROM/card/flash and external inputs remain unchanged; the saved
artifact is nonempty and hashed by the checker.

The checker validates complete architectural records, including `ILLEGAL:0`,
before separating them from protocol/error-message scanning. Actual error
lines, malformed or extra snapshots, nonzero stop flags, incomplete upload,
bad authentication, mismatched replay and changed pixels remain failures.
Three focused mutation tests cover this separation and the product-specific
restoration contract. Checksums are not bytewise memory comparison; bounded
I/O equality is not exhaustive device-state coverage or long-run stability.
No guest state is supplied outside emulator save/load APIs and physical keys.

```sh
.venv/bin/python -c "from pathlib import Path; from tools.nsk3_cold_input import prepare; from tools.nsk3_diagnostic_display_check import execute; r=Path.cwd(); o=r/'tmp/nsk3-native-idle-state'; prepare(r/'roms/noki6130/source',r/'tmp/nsk3-diagnostic-display-accepted-cold',o); execute(r/'mame/mame',r/'roms',o,'nsk3_native_state.lua')"
.venv/bin/python tools/nsk3_native_state_check.py roms/noki6130/source \
  tmp/nsk3-diagnostic-display-accepted-cold tmp/nsk3-native-idle-state
```

The execution command requires a fresh output directory. The cold predecessor
contains only the physically selected persistent inputs, not RAM/save states.

### SIM Toolkit transport and tested command refusal

The own Phase-2+ cold card fixture reads EF_PHASE03 and sends TERMINAL
PROFILE `A0 10 00 00 04 03 01 80 00`. This four-byte profile differs from
the5190/6190 profiles; neither their capabilities nor successful UI commands
are assumed here. Ordinary STATUS returns9116, then own firmware FETCHes the
card's22-byte DISPLAY TEXT `DCT3 SAT` BER-TLV. It automatically returns
`810301218002028281030130`, result30, accepted by the card with9000,
before the physical68s left-softkey contact. No proactive text is displayed:
the65s frame remains Headset idle, and the later physical contact opens the
ordinary Messages menu. This qualifies transport and a tested-command
capability refusal, not successful DISPLAY TEXT or whole-ROM Toolkit absence.
The result meaning is defined by
[GSM11.14/ETSI TS101267, general-result coding](https://www.etsi.org/deliver/etsi_ts/101200_101299/101267/08.16.00_60/ts_101267v081600p.pdf):
30 denotes a capability limitation, distinct from31 (unrecognized command).

A matched Phase-2 control uses identical own cold handset/card storage and
physical input timing, reads EF_PHASE02 and sends no TERMINAL PROFILE,
FETCH or TERMINAL RESPONSE. Both74s runs preserve the complete EEPROM,
flash and original PIN-enabled card, with matching own authentication,
upload, native no-stop observations and reviewed idle/Messages pixels.
Only the declared external SATCFG input differs. No firmware result,
application context, credential, radio reply or replacement clear is supplied.
Six focused tests reject wrong profile/result/payload/status, late refusal,
malformed/capped wire data, missing contacts/native observations and changed
external-input provenance. The5190 successful DISPLAY TEXT/control captures
also pass after sharing the strict wire/parser mechanics; product profiles,
storage and pixels remain independently checked.

```sh
.venv/bin/python tools/nsk3_toolkit_check.py roms/noki6130/source \
  tmp/nsk3-diagnostic-display-accepted-cold tmp/nsk3-toolkit-display-accepted
.venv/bin/python tools/nsk3_toolkit_check.py roms/noki6130/source \
  tmp/nsk3-diagnostic-display-accepted-cold tmp/nsk3-toolkit-control-accepted --control
```

Add `--mame mame/mame` and use a fresh output directory to execute either
fixture. The diagnostic card deliberately tests an unadvertised command;
this is not a claim that an ordinary SIM application should request it.

**Long-run limit:** the corrected native compatibility composition requests a
reason68 restart at93.975196s. Short physical UI/storage/security gates do
not establish sustained native operation. The own packet-supervision contract
below reproduces this failure; no keepalive or disabled watchdog is installed.
The110s own FDN-enabled cold fixture retains the corrected upload and reproduces
packet windows1/1/0 through `nsk3_liveness_check.py`. The reboot limit therefore
survives the upload-bank correction; the old93.967473692s measurement belongs
to the earlier fixture and is not a timing target.

**Native packet boundary:** the last8b object is delivered intact and cached
by mailbox0c in initial state3/phase1. The mapped acknowledgement server
(mailbox0e) receives no input through45s. MCU work-queue routes are also
dormant. These are measured boundaries, not proof that acquisition needs an
injected acknowledgement. Native control capture and the remaining RF/data
contracts are described below; registration, connected calls and speech remain
unaccepted.
In the corrected authenticated/FDN-enabled fixture the packet arrives at
45.796785s, still entering state3/phase1 and the cache path. The pre-correction
38..40s timing must not be imposed on that distinct input composition.

### Executable upload bank

The own boot4 loader writes811 distinct program words in `2800..2b2a`.
The old `2800` overlay end discards the upload, exposing resident program
words and zero data instead. For example, uploaded `2800=10f8` reads as
program `a594` and data `0000`; nine coincidental matching values are not
upload retention. The research `nsk3r4t` configuration now selects a `3000`
overlay extent. This is an upload-derived bank model, not fitted-mask or
complete physical-memory identification; normal `noki6130` is unchanged.

`nsk3_native_upload_observe.lua` shares only read-only observation mechanics
with the6150 fixture. Both old/corrected40-second runs use the own qualified
FDN-enabled cold storage, physical PIN/phone-code input and listed-number
dial action. Existing handoff, authentication, SIM conversation, unchanged
whole-card and exact-frame checks pass in both. Only the corrected run passes
`nsk3_native_upload_check.check_upload`, retaining all811 uploaded words in
both memory views at1s. Its566 observed first opcode words all have decoded
boundaries, including observed extension variants. These are short-run
software/mapping qualifications, not native registration or speech proof.

The full program-write census, not just the `2800..2fff` tap, observes2290
writes to2289 distinct destinations during the first second. All2288 unique
uploaded destinations below3000 retain their final uploaded words in program
space. The sole higher destination is `ff87`: writes0004/0100 at exposed
PC0f12/4a0d leave the acquired resident word0004 unchanged. Guest reads at
exposed PC0f15/0f6a/4a11 each return0004. Live4a0b is
`MVDP *AR2,ff87`, followed by `MVPD ff87,*AR3-` at4a0f; the latter result
feeds the type74 report ending `0a090004`. This is an observed immutable-ROM
read path, not evidence for an additional writable code bank or a new RF
device. In particular, do not extend RAM toff87 just to retain every write.

`nsk3_native_program_census.lua` composes the same physical fixture with
`dct3_native_program_write_census.lua`; the latter suppresses its own memory
sampling from the guest-read observer. Checker
`nsk3_program_census_check.check_census` requires the complete14-page census,
all uploaded-word visibility, the two high writes and exactly three guest
identity reads. Its40s run also passes the existing input/SIM/whole-card and
exact-frame checks. The write-retention inventory is explicitly a1s loader
horizon, not a proof that no later runtime overlay can exist. No further
program-memory geometry change is supported by this census.

`nsk3_native_upload_long.lua` composes the same physical dial with existing
passive native-control and liveness observers. The110s run uses own persisted
EEPROM/SIM/flash/config only, with no new provisioning or guest writes.
The first lifecycle (trace prefix before `nsk3_restart_entry`) passes the
existing dial/SIM, unchanged-card, exact26s frame and retained-upload checks.
Applying a one-boot APDU checker to the full rebooting trace correctly fails
its transaction cardinality; do not weaken it to accept multiple boots.
Native pending/completed remains3/010e with5/8 writes. At65/90s the scan
counter advances from `ffffd1d0` to `ffffe6f9` with matching producer/consumer
cursors and no qualifying result. The45s program capture remains available
for live-code analysis. These observations do not establish an absent RF
contract or justify manufacturing a keepalive.

```sh
.venv/bin/python tools/nsk3_liveness_check.py roms/noki6130/source \
  tmp/nsk3-native-upload-long
```

The separate native compatibility instrument reaches **Insert SIM card** and
rescans the physical matrix under an explicit nominal ADC2 hypothesis (554).
With the synthetic SIM inserted and an own-initializer retry-byte fixture,
physical12345 submission is accepted and another physical command opens a
readable Messages menu. A separate cold process preserves storage unchanged
and requires the phone code again. The unmodified diagnostic selector retains
marked idle content; the physical Test00 contract above qualifies its removal.
Physical Add entry now saves A/12345 to SIM EF_ADN; a separate cold process
retrieves the entry through Search/Details and displays12345 without rewriting
the card. An explicit PIN-enabled laboratory card additionally passes physical
wrong-PIN, cold retry persistence and correct-PIN authentication, then reaches
the separate phone security-code prompt. A blocked-PIN fixture additionally
passes physical PUK recovery, wrong-PUK rejection and cold authentication using
the new PIN. Physical PIN-request enable also persists and causes the expected
cold PIN challenge; physical disable persists and skips that challenge on cold
boot without bypassing the separate phone lock. Other SIM/security operations
and radio remain unaccepted.
Ordinary physical Change PIN also persists4321 and authenticates after cold boot.
Physical Change PIN2 now persists8765 and authenticates an ordinary change
back to5678 after a separate cold boot. Physical PIN2 exhaustion, cold PUK2
prompt/recovery and recovered-PIN2 cold authentication are separately checked.
An independent cold chain also exhausts all ten PUK2 retries, preserves the
permanent block across restart and proves physical PIN1/phone unlock plus
responsive security-menu navigation without a new credential APDU.
Physical offline SMS composition saves an unsent A draft to EF_SMS; a cold
Outbox read displays it without changing card bytes. Network SMS is unproved.
Physical Erase/cancel and a cold empty-Outbox check also pass.
Physical FDN activation and deactivation now pass on an explicit laboratory
FDN-capable card, including cold retention of the complete card image and
the IMSI/LOCI rehabilitation handshake. PIN1 authentication precedes phone
authentication; PIN2 authorizes the ADN validity change. Cold captures show
the selectable On/Off/Numbers list, not a current-state caption: its highlight
is not used as a state oracle. Physical PIN2-protected FDN Add entry also saves
A/12345, and a separate cold process displays12345 without rewriting the card.
A wrong-PIN2 control changes only its retry counter, not FDN data. Physical
in-place editing changes only the last-number byte to12346, retained and
displayed after cold boot. Erase cancellation preserves the whole card;
confirmed erasure clears only record1, and cold Search displays No phone
numbers. A local FDN rejection contrast also passes: enabled/unlisted54321
renders Call not allowed, while enabled/listed12346 and disabled/unlisted54321
return to the existing idle-like frame without that prompt. Neither control
has a captured Calling/connected state; network call acceptance is unchanged.
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

### FDN acceptance

The shared `sim_fdn_protocol_check.py` owns only standard APDU ordering,
credentials and selected-file validity checks. `nsk3_fdn_check.py` retains
own-ROM qualification, physical phone/PIN contacts, upload handoff checks,
whole-card transitions and EEPROM cold links. No menu layout or firmware
address is imported from the 6150 checker.

```sh
.venv/bin/python tools/nsk3_fdn_check.py roms/noki6130/source \
  tmp/nsk3-fdn-complete-on tmp/nsk3-fdn-complete-on-cold \
  tmp/nsk3-fdn-complete-off tmp/nsk3-fdn-complete-off-cold
```

Each capture runs for34s using `nsk3_fdn_input.lua`, `nsk3_fdn_cold.lua`,
`nsk3_fdn_disable.lua`, then `nsk3_fdn_cold.lua`. Preparation uses
`nsk3_cold_input.py`; only the first preparation adds `--fdn --fdn-adn-access`.
Later inputs copy the preceding saved card and EEPROM, not RAM or save states.
The fixture raises its transmit observation cap to4096 bytes; the old1024-byte
captures were insufficient to qualify the complete exchange. The gate rejects
cap saturation. These short runs do not supersede the native restart limit.

Record save/cold retrieval and the independent wrong-PIN2 control:

```sh
.venv/bin/python tools/nsk3_fdn_record_check.py roms/noki6130/source \
  tmp/nsk3-fdn-write-confirm tmp/nsk3-fdn-read-cold tmp/nsk3-fdn-write-wrong \
  --edit tmp/nsk3-fdn-edit-confirm --edit-cold tmp/nsk3-fdn-edit-cold
.venv/bin/python tools/nsk3_fdn_erase_check.py roms/noki6130/source \
  tmp/nsk3-fdn-edit-cold tmp/nsk3-fdn-cancel-confirm \
  tmp/nsk3-fdn-erase-confirm tmp/nsk3-fdn-erased-qualified
```

`nsk3_fdn_write.lua` uses the own physical matrix and an additional OK between
name and number entry; the shorter sibling sequence does not complete this
editor. The checker requires one exact PIN2-authorized UPDATE RECORD for
ASCII A/BCD12345 and no unrelated card changes. Cold retrieval pins the Number
12345 frame; the independent `nsk3_fdn_write_wrong.lua` control enters5675,
requires9804 and permits only PIN2 retries3-to2. No ADC, firmware state or DSP
response is changed to make these short UI transactions succeed.
`nsk3_fdn_record_edit.lua` retains the name and changes the final digit through
physical Clear/6/OK. `nsk3_fdn_record_erase.lua` and its cancellation wrapper
start from the same edited card. `nsk3_fdn_empty.lua` stops after submitting
the empty Search name, before a further OK could dismiss the empty-list result.
The edit gate permits only byte3600 F5-toF6; the erase gate permits only the
selected32-byte record to become FF and pins the cold empty-search frame.

Local FDN rejection controls:

```sh
.venv/bin/python tools/nsk3_fdn_dial_check.py roms/noki6130/source \
  tmp/nsk3-fdn-edit-cold tmp/nsk3-fdn-dial-enabled \
  tmp/nsk3-fdn-dial-listed-fast tmp/nsk3-fdn-dial-unlisted \
  tmp/nsk3-fdn-dial-disabled tmp/nsk3-fdn-dial-unlisted-off
```

The enable/disable processes use the existing physical FDN fixtures and change
only ADN validity, retaining A/12346. Each dial process authenticates PIN1 and
the own phone code, enters its number and presses physical Send0e. The gate
checks both identity rehabilitation commands on enabled cold boots, none on
disabled cold boot, exact reviewed26s frames, unchanged whole cards, own flash
and input/EEPROM continuity. Early23.6/23.8s captures show entered digits, not
Calling; return to the idle-like frame is not promoted to positive call setup.

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
The independent phonebook gate below extends this menu boundary. Classify idle
content separately; no missing handoff message should be synthesized.

### Persistent SIM phonebook

Own table2c7a30 and visible navigation establish column1 row1/2/3/4 as
left command19, Up17, Down18 and right command1a. The native research profile
now labels these separately from the inherited NSE-1 single-Navi layout.
Digits retain their own asserted ROM-table contacts; the remaining physical
labels are not fully audited.

`nsk3_phonebook_input.lua` physically unlocks, opens Names, selects Add entry,
enters A/12345 and saves. `tmp/nsk3-phonebook-save-verified` displays Saved to
SIM card at23s. The observed TX stream contains one absolute UPDATE RECORD
`a0 dc 01 04 20` followed by the exact32-byte ADN payload. Record1 contains
ASCII41, erased alpha padding, length04, TON81 and BCD21 43 f5; records2..50
remain erased. No SIM/NVRAM result is prewritten by the input script.

`tmp/nsk3-phonebook-reload-verified` starts in a separate process using only
the saved NVRAM and physical configuration. `nsk3_phonebook_reload.lua`
unlocks again, searches and selects Details. The exact17s frame displays12345;
the full serialized card is byte-identical and its complete bounded TX stream
contains no UPDATE RECORD. Both final-build runs pass:

```sh
.venv/bin/python tools/nsk3_phonebook_check.py roms/noki6130/source \
  tmp/nsk3-phonebook-save-verified tmp/nsk3-phonebook-reload-verified
```

This accepts one physical SIM-backed phonebook save/cold-read lifecycle under
the declared research inputs, not arbitrary contacts, handset-local storage,
all SIM security, correct idle content or network service.

### SIM PIN and cold retries

`nsk3_nominal_input.py --insert-sim --pin-enabled` supplies the shared explicit
laboratory card (PIN1234), not donor storage. The wrong physical4321/command19
sequence issues `a0 20 00 01 08` with the padded PIN, returns9804, presents Code
error and changes persistent CHV1 attempts3->2. A fresh process copies only
NVRAM/config; `nsk3_cold_input.py` records pre-launch EEPROM/card bytes and hashes
without RAM or save states. Physical1234 then returns9000, restores attempts3
and displays the distinct phone Security code prompt. The entire card differs
from the qualified cold input only at the CHV1 attempt byte; credentials and
other retry fields remain unchanged.

```sh
.venv/bin/python tools/nsk3_pin_check.py roms/noki6130/source \
  tmp/nsk3-pin-wrong tmp/nsk3-pin-cold-qualified
```

The gate checks own unchanged executable flash, native upload, physical contacts,
VERIFY bytes/status, exact PIN/error/phone-code frames, pre-launch card identity
and the whole persistent retry transition. The shared card generator uses an
older serialized prefix; the device appends its later storage defaults, which
the cold comparison preserves byte-for-byte. The first rejected card is also
bound to all3570 bytes, including preferred PLMN00f110 and the unchanged
SMSS/AoC defaults; an unrelated tail mutation cannot qualify merely by
persisting across cold authentication. No unexpected credential-changing
APDU is admitted. The gates below extend PUK, ordinary PIN/PIN2 change and
request settings and PUK1/PUK2 permanent loss. Physical PIN1 exhaustion and
recovery also have a separate cold-chain gate below.

### Physical PIN1 exhaustion and recovery

`tools/nsk3_pin_exhaust.py` requalifies the first physical wrong PIN4321,
then uses its actual saved card in two independent cold processes. Both
physically enter4321; VERIFY CHV1 returns9804 then9840 as attempts2->1->0.
Only PIN1's retry counter changes across the complete card. No blocked-state
fixture or credential edit is used for these transitions.

A further cold process starts from that exhausted card, shows **Enter PUK
code**, and physically enters12345678/new4321/confirmation. Exact UNBLOCK
bytes return9000, restore PIN1 attempts3/PUK1 attempts10 and change only PIN1.
Another cold process authenticates4321 through VERIFY9000, preserves the whole
card and reaches the reviewed separate phone-code prompt. Each link checks
prelaunch card/EEPROM identity, own unchanged flash and native uploads; no RAM
or save state is copied.

```sh
.venv/bin/python tools/nsk3_pin_exhaust.py roms/noki6130/source \
  tmp/nsk3-pin-wrong tmp/nsk3-pin1-exhaustion --check-only
```

A fresh output without`--check-only` executes the chain;`--resume` requalifies
retained predecessors. Completed logs are losslessly compressed. This bounded
52s software-security result does not qualify radio, fitted-mask compatibility,
factory provisioning or long-run packet-supervision behavior.

### PUK recovery and new-PIN persistence

`--pin-blocked` explicitly provisions the shared laboratory card with CHV1
attempts0. It supplies no handset state. Physical PUK12345678, new PIN4321
and confirmation produce `a0 2c 00 00 10` with the entered credential bytes.
The card returns9000, stores PIN4321, restores CHV1 attempts3/PUK1 attempts10,
and the UI reaches the separate phone code. A wrong-PUK12345679 control returns
9804, preserves the original PIN, keeps CHV1 blocked and decrements only PUK1
attempts to9. The entire saved card is checked, including the appended
laboratory preferred-PLMN/default fields; cold continuations use their actual
predecessor card rather than regenerating its contents.

The qualified cold copy records the recovered card before execution. Physical
4321 authentication succeeds and the full current card remains byte-identical;
volatile authentication is not assumed to survive restart. Exact PUK/PIN/phone
code frames, physical contacts, UNBLOCK/VERIFY payloads, status counters, native
uploads and own unchanged executable flash are checked:

```sh
.venv/bin/python tools/nsk3_puk_check.py roms/noki6130/source \
  tmp/nsk3-puk-recovery tmp/nsk3-puk-wrong tmp/nsk3-puk-cold-pin
```

This accepts a physical PUK recovery lifecycle under the declared research
inputs. Other gates cover ordinary CHANGE CHV and PIN-request settings;
radio and fitted hardware remain unaccepted. Permanent PUK1 loss has a separate
cold-chain gate below.

### Permanent PUK1 loss

Starting with the qualified first wrong PUK1 above, nine independent cold
processes physically enter12345679 and proposed PIN4321. Each starts with the
actual predecessor's card/EEPROM and own executable, copying no RAM or save
state. Exact UNBLOCK payloads return9804 until the final9840; only PUK1's
attempt counter changes. Final PIN1/PIN2/PUK1/PUK2 attempts are0/3/0/10.
No credential, other counter or unrelated card byte is edited by the harness.

The terminal frame says **SIM card rejected**. A further independent cold
process preserves every card byte and renders the same exact frame at5s and24s,
before and after the physical credential-entry control. It issues no
VERIFY/CHANGE/UNBLOCK or card-write command. This52s compatibility result
does not itself qualify physical PIN1 exhaustion (separately tested above), radio, fitted DSP compatibility
or resolution of the93.967s packet-supervision restart.

```sh
.venv/bin/python tools/nsk3_puk1_exhaust.py roms/noki6130/source \
  tmp/nsk3-puk-wrong tmp/nsk3-puk1-exhaustion --check-only
```

Omit`--check-only` with a fresh output to execute the chain;`--resume`
requalifies retained predecessors. Completed logs are losslessly compressed
and accepted in either plain or gzip form. The runner verifies cold source
hashes, complete card transitions, own flash/uploads and reviewed rejection
pixels; source hashes guard accidental input reuse, not execution attestation.

### PIN-request enable

Observed UI route: Menu4 -> Security settings5 -> PIN-code request1 -> Select,
then PIN1234/OK, Up to On, Select. Numeric shortcuts focus this leaf; they do
not replace its physical Select. CHANGE PIN is unavailable while the card's
PIN request is disabled, so this is also a prerequisite for that workflow.

`nsk3_pin_request_enable.lua` drives this sequence after phone-code acceptance.
`tmp/nsk3-pin-request-enabled` issues ENABLE CHV `a0 28 00 01 08` with padded
1234, returns9000 and presents PIN-code request On. The full serialized card
changes only its enabled byte0->1. A qualified cold copy challenges for PIN,
accepts physical1234, presents the separate phone lock and preserves the card:

```sh
.venv/bin/python tools/nsk3_pin_request_check.py roms/noki6130/source \
  tmp/nsk3-record-cold-qualified tmp/nsk3-pin-request-enabled \
  tmp/nsk3-pin-request-cold
```

This enable gate is separate from the disable and ordinary CHANGE CHV gates
below; credentials were not changed to make this setting work.

### Ordinary PIN change

The enabled card authenticates1234 on startup, then the separate phone code
unlocks the UI. The observed route is Menu4/Security5/Change access codes6,
Select, Down from Change security code to Change PIN, Select. Physical old
PIN1234, new4321 and confirmation produce one CHANGE CHV
`a0 24 00 01 10`, returning9000 and displaying PIN code changed.

The full card comparison allows only PIN1's credential bytes to change. A
qualified cold input snapshot matches the changed card; physical4321 VERIFY
succeeds and leaves the whole card unchanged. This is independent of PUK
recovery and does not re-provision the PIN during cold preparation.

```sh
.venv/bin/python tools/nsk3_pin_change_check.py roms/noki6130/source \
  tmp/nsk3-change-pin-qualified tmp/nsk3-change-pin-cold
```

The fixture composes startup PIN and phone-code input through one idempotent
passive SIM observer; timing offsets schedule physical contacts only. All
hardware defaults and declared native/analog/provisioning hypotheses remain
unchanged. PIN-request disable and PIN2 change have separate gates below;
PUK2 recovery and permanent loss have separate gates below; permanent PUK1
loss is qualified separately above.

### PIN-request disable

The request-enabled card authenticates1234 and the separate phone code unlocks
the UI. `nsk3_pin_request_disable.lua` uses the same observed4-5-1 path,
enters1234, selects Down to Off and confirms. DISABLE CHV `a0 26 00 01 08`
returns9000, and the exact31s frame shows Off. The whole serialized card changes
only at enabled1->0. A qualified cold copy preserves the card, performs no
SIM VERIFY or credential mutation, and presents phone Security code directly.

```sh
.venv/bin/python tools/nsk3_pin_request_check.py roms/noki6130/source \
  tmp/nsk3-pin-request-enabled tmp/nsk3-pin-request-disabled \
  tmp/nsk3-pin-request-off-cold --disabled
```

The paired On and Off gates preserve credential bytes and independently verify
the corresponding cold challenge behavior. Permanent PUK blocking,
correct idle content and radio remain unaccepted. A Battery low alert appears
around33s in the longer Off run under the unchanged nominal-input hypothesis;
that analog/power observation is not hidden by adjusting the fixture voltage.

### PIN2 change and cold authentication

The physical route Menu4 -> Security5 -> Access codes6 -> Select -> Down
twice -> Select opens Change PIN2. With the explicit request-enabled lab
card, PIN1 and phone-code unlock precede old PIN2 5678, new8765 and its
confirmation. Firmware transmits `a0 24 00 02 10` plus padded old/new
credentials; SIM returns9000. Only the four PIN2 digits change in the full
3570-byte card. A separate cold process receives exactly those saved card
bytes, uses8765 as the old PIN2 and changes it back to5678 successfully.
This tests ordinary credential authentication across restart, not cached
authentication or a card-side command injected by the harness.

```sh
.venv/bin/python tools/nsk3_pin2_check.py roms/noki6130/source \
  tmp/nsk3-pin2-change tmp/nsk3-pin2-cold-restore
```

The checker verifies physical contacts, phone-code acceptance, startup PIN1,
exact CHANGE CHV bytes/status, whole-card mutation boundaries, cold card
identity, native uploads, own unchanged flash and exact Change PIN2 frame.
PIN2 exhaustion and PUK2 recovery have separate gates below; permanent block
is not accepted by this change gate.

The wrong-PIN2 control submits5675 and receives9804, displays **Code error**,
preserves all credential bytes and changes only PIN2 attempts3->2 at3517.
The saved rejected card is copied into a fresh process; correct5678 then
changes PIN2 to8765 and restores its attempts2->3. Its final card exactly
matches the independently accepted change fixture. These are physical CHANGE
CHV retry/cold-state tests, not direct APDU injection.

```sh
.venv/bin/python tools/nsk3_pin2_check.py roms/noki6130/source \
  tmp/nsk3-pin2-change tmp/nsk3-pin2-cold-restore \
  --wrong tmp/nsk3-pin2-wrong --retry-cold tmp/nsk3-pin2-retry-cold
```

The checker verifies complete rejected/cold card transitions and the exact
error/success frames. Exhaustion, PUK2 recovery and permanent PUK2 loss are
qualified below.

### PIN2 exhaustion and PUK2 recovery

Three physical wrong-PIN2 CHANGE attempts are linked by independent cold
copies, not edited retry fixtures. Whole-card checks prove attempts3->2->1->0,
statuses9804/9804/9840, and no credential/PUK counter changes. The terminal
frame says **PIN2 code blocked**. A separate cold process preserves that
card unchanged; selecting Change PIN2 presents **Enter PUK2 code** without
another CHANGE or UNBLOCK command.

Physical PUK2 87654321, new PIN2 8765 and confirmation produce
`a0 2c 00 02 10` with those exact bytes. Firmware then automatically sends
VERIFY CHV2 `a0 20 00 02 08` using8765; both return9000, after the startup
PIN1 VERIFY. Only PIN2 and its attempts0->3 change in persistent card data.
A further cold process receives that saved card and authenticates8765 in an
ordinary change back to5678. This closes recovery/persistence, not native
network service or a fitted DSP assumption.

```sh
.venv/bin/python tools/nsk3_puk2_check.py roms/noki6130/source \
  tmp/nsk3-pin2-wrong tmp/nsk3-pin2-wrong-second tmp/nsk3-pin2-blocked \
  tmp/nsk3-pin2-blocked-cold tmp/nsk3-puk2-recovered tmp/nsk3-puk2-cold-auth
```

The gate checks every cold card link, physical input, exact APDU ordering,
whole-card mutation limits, native upload/own flash, and blocked/prompt/success
frames. The optional `--wrong tmp/nsk3-puk2-wrong --retry-cold
tmp/nsk3-puk2-retry-cold` pair qualifies wrong PUK2 87654322 returning 9804:
only its retry counter changes from 10 to 9, with PIN2 still blocked. Correct
PUK2 after a separate cold boot restores PIN2/PUK2 counters to 3/10 and stores
PIN2 8765, producing the same saved card as direct recovery.

### Permanent PUK2 loss

`tools/nsk3_puk2_exhaust.py` requalifies the three physical wrong-PIN2
attempts and first wrong PUK2, then continues from each actual saved card
through nine more independent cold processes. Each physically submits
87654322 and proposed PIN2 8765; no retry counter or credential is edited.
Statuses remain `9804` until the final attempt returns `9840`. Only PUK2's
counter changes, reaching zero with PIN2 still blocked; PIN1 remains valid.

Two further cold processes use that terminal card. Physical PIN1 1234 and
the separate phone code 12345 authenticate. Selecting Change PIN2 opens no
credential dialog and issues no CHANGE/UNBLOCK or other card mutation;
the exact Change PIN2 menu pixels remain displayed. A separate physical
Down at 34 s is decoded as `18` and moves to **Change barring password**
(frame SHA256 `052dcbc2adb5dfd2acfc763260ab9dcb46a44ffc5ff1e57028e6146384008f95`).
The entire card remains unchanged in both processes. This is bounded menu
operation through 52 s, not resolution of the 93.967-s packet-supervision
restart or proof of radio service, fitted DSP compatibility or hardware timing.

```sh
.venv/bin/python tools/nsk3_puk2_exhaust.py roms/noki6130/source \
  tmp/nsk3-pin2-wrong tmp/nsk3-pin2-wrong-second tmp/nsk3-pin2-blocked \
  tmp/nsk3-puk2-wrong tmp/nsk3-puk2-exhaustion --check-only
```

For a fresh chain, use a new output directory without `--check-only`;
`--resume` requalifies completed predecessors before continuing. Optional
`--compress-logs` verifies exact decompression before replacing completed
verbose logs with `error.log.gz`; the checker accepts either format. Its
source/card/EEPROM hashes guard accidental input reuse, not cryptographic
execution attestation. Permanent PUK1 loss has its own cold rejection gate above.

### Offline SMS draft

The observed physical route is Messages -> Select -> Down twice to Write
messages -> Select, digit2 for A, Options -> Down twice to Save -> Select.
Menu state must be observed rather than inferred from a numeric shortcut.
The generic Saved to SIM card presentation alone cannot distinguish ADN from
SMS storage; acceptance requires the selected EF and its actual record bytes.

`nsk3_sms_input.lua` writes one absolute176-byte UPDATE RECORD to EF_SMS6f3c.
Record1 starts `07 02 81 f0 01 ff 00 81 00 00 01 41`, padded withff: unsent
status07, one-character GSM7 A, no recipient yet. ADN and all nine other SMS
slots remain empty. No SMS record is prefilled by the input fixture.
`nsk3_sms_reload.lua` starts in a qualified cold process, physically unlocks,
opens Outbox and displays A. It sends no UPDATE RECORD and preserves the
entire current serialized card and pre-launch snapshot byte-for-byte.

```sh
.venv/bin/python tools/nsk3_sms_check.py roms/noki6130/source \
  tmp/nsk3-sms-save-qualified tmp/nsk3-sms-cold-qualified
```

The gate checks physical code/command contacts, own unchanged executable flash,
native uploads, exact EF_SMS selection and update payload/status, empty remaining
storage, cold identity and exact editor/read frames. SMS experiments explicitly
raise the passive TX capture bound to4096 bytes; existing phonebook captures
retain their1024 bound. This is offline application/storage acceptance only,
not SMS-SUBMIT delivery, network registration, RF or native speech.

### Offline draft deletion

From the persisted Outbox A view, Options -> Erase presents Erase message?.
Physical OK performs one successful176-byte UPDATE RECORD; physical Back in
an independent cancel run performs none and returns to A. Confirmation changes
only the record status07->00, retaining its old payload in the free slot. This
is logical deletion, not secure wiping or whole-record erasure.

A separate cold process starts from the deleted card and physically opens
Outbox. The exact16.5s frame says No messages in outbox. It sends no UPDATE
RECORD and preserves the complete card; the pre-launch snapshot matches the
confirmed deletion. The cancel run preserves the original complete saved card.

```sh
.venv/bin/python tools/nsk3_sms_delete_check.py roms/noki6130/source \
  tmp/nsk3-sms-save-qualified tmp/nsk3-sms-erased \
  tmp/nsk3-sms-erase-cancel tmp/nsk3-sms-erased-cold
```

The gate checks own flash/native upload, physical confirmation/cancel contacts,
exact confirmation/read/empty frames, update payload/status and whole-card
transitions. No network delivery, SMS reception or radio completion follows
from this offline lifecycle.

### Native packet-supervision limit

The120s physical-menu capture `tmp/nsk3-native-long` requests reason68 through
restart caller23d83b at93.967473692s. A separate96s instrumented run,
`tmp/nsk3-native-liveness`, independently localizes the condition:

| Own boot4 site | Observed contract |
| --- | --- |
| `2ab4fc` / `2ab512` | Packet dispatcher increments halfword10c5a6, context10c5a4 |
| `2ab64a` | Window reads count; zero selects2ab506 |
| `2ab652..2ab65a` | Nonzero rearms timer1c/ticks09cd and clears count |
| `2a2a4a` | Argument1 policy from2ab50d selects reason68 |
| `23d83b` / `2ac696` | Restart caller and actual MCU reset command05 |

Configured-clock windows at about31/62/93s contain1,1,0. Counter clears at
31.000258077s, a later packet increments it at38.808311231s, the62s window
clears it, and the empty third window selects the policy and delayed reset.
Earlier startup clears/increments are separately visible; the count is not
assumed to have been1 since reset. Physical wall-clock calibration is unproved.

```sh
.venv/bin/python tools/nsk3_liveness_check.py roms/noki6130/source \
  tmp/nsk3-native-liveness
```

This command qualifies a known failure and explicitly reports stable_boot=false;
it is not a long-run acceptance gate. Opcode/literal anchors and mutation tests
bind the ownership to this own ROM rather than importing the5130 addresses.
The next question is task12's consumption of the last observed native response
and its legitimate follow-up request/clock/RF lifecycle. No arbitrary heartbeat,
counter write, disabled supervision or foreign packet replay is justified.

### Native packet census

`tmp/nsk3-native-packets-full` observes complete declared packet lengths at the
own dispatcher counter write through45s. The bounded sequence is74,74,74,74,
99,8b; the last arrives38.808311231s and the99 arrives4.834193154s. This is
not an assertion that all later lifecycles or all producer classes are closed.

The last8b has162 payload bytes: leading word0020 followed by space for40
two-halfword pairs. The first fields include0374/ff6c,0375/ff6c and0372/ff6c;
all forty second fields decode as signed-148. Channel-shaped values range
512..885; their RF mapping remains unproved. The leading word32 and capacity40 must not be
conflated: consumer semantics, valid-entry count and physical RSSI units remain
unproved. No available carrier or camped state is demonstrated.

Own subtract-cascade/jump-table arithmetic maps type8b to table entry2ab560,
target2ab58c ->2823b0. That wrapper writes header13b8 and posts to task12
through266a2c. Argumenta8 belongs to the gated trace wrapper27e270, not the
queued message namespace. Runtime observation confirms the same object and
header13b8a28b at posting, dequeue266eb0 (caller21a1dd), and dispatch21a2ea.
The consumer enters with owner106ce8 in state03/phase01 and no cached object.
Runtime observation at21a32e confirms a160-byte body copy into allocated
cache102eb4, byte-identical to packet+6 (the40 serialized pairs, excluding
the leading word). The branch21a35c then skips processing2176a0 with state3
and phase1 unchanged. The state/phase lifecycle and subsequent native requests
remain unresolved; this does not establish why that lifecycle was selected.

Read-only byte write-watches on106cfc and106d01 identify task12's own
initialization stores21a1a6 (state3) and21a1b2 (phase1), after the initial
RAM clear20010e. No further writes to either byte occur through45s in
`tmp/nsk3-owner-writers`. Static decoding confirms literal3/1 stores before
the receive loop266c80. These are initial values, not an observed transition
into an error state. This watch covers this run and qualified owner address;
it is not an exhaustive static writer census or a meaning assigned to state3.

An own-ROM static exit candidate is header043f: dispatch literal21a570
feeds the first subtract case, targeting21a674. That handler passes
packet byte2 to2190f4 via21a6a6;2190f4 allocates/initializes a separate
40-entry structure and writes state1 at21913a. The owner literal2192e4
is106ce8, matching the runtime-watched owner. Thus state3 is not an
immutable native limitation: this MCU-owned input has a concrete state1
transition. Its producer, prerequisites and runtime occurrence remain open.
The reset/cleanup helper2170ec instead explicitly restores state3 at217108.
These are independently decoded candidates, not closure over all writers;
linear scans across literal pools cannot establish function ownership.

The aligned0000043f literal at254868 is loaded at2544bc and passed through
2541c8 to constructor287a38. Its own cascade base03e9 (literal287d7c)
selects287ae2 for043f, copying bytes5/4 of context10c468 to message bytes2/3,
then selecting task0c at287aec. Thus the state1 input carries MCU context,
not the native8b measurement body. In the45s `tmp/nsk3-043f-route` run,
the predecessor254476, constructor287a38 withr0=043f and consumer21a674
all have zero observations, while the full packet/cache gate still passes.
This identifies a dormant concrete producer route, not proof that no other
producer exists. The predecessor's dispatch input and context lifecycle are
the next unresolved boundary.

The predecessor is selected by dispatcher input0834:2536bc loads literal
253994, compares r5, and2536c2 calls254476 only on equality. The small
`tools/nsk3_state_contract.py` checker qualifies these instruction/literal
relationships, the043f load and the owner/state1 store against the own image;
mutation tests reject changes to each anchor. This establishes one concrete
0834 ->043f ->state1 route, not the meaning or producer of0834.

Own upstream dispatch20af86 forms0a30 (a3<<4), compares the received value
at[sp], and calls20e9d4 only on equality. Its20e9da load selects0834 from
20ed20, with common constructor208a48 reached through20e9f4. That
constructor's0834 case208df8 copies context10a7d8 bytes+1f/+1c into
message+2/+3. These independently match the sibling chain's shape but use
own-ROM selectors, instructions and context. The qualified contract checker
now anchors these relationships too. The0a30 producer and its prerequisites
remain unresolved; no computed/table-driven producer closure is claimed.

The bounded MOVS r0,a3 / LSLS r0,4 / direct-BL pattern scan finds five
own calls to constructor21a782:21c44a,21c68c,21eb00,21eff8,21f4b0.
Each is preceded by a store to owner register r6+1d (2,3,1,2,3).
This is pattern coverage, not exhaustive construction coverage. Runtime
`tmp/nsk3-controller-lifecycle` observes constructor inputs0a35 (caller21a761),
0a25 (21d0a1),09ec (21f391), but no0a30. The receive helper21a8cc sees
15870000/15920000/158b006d and then043c0000 (caller21f395). Own subtract
arithmetic maps043c to21ab4c, which clears context byte6 and rejoins cleanup;
the helper retains nonzero status043c for return. Therefore this object must
not be described as a missing09ec response merely because it differs from
the caller's09f0 success test. A separate45s trace
`tmp/nsk3-controller-result` confirms actual helper return043c at21f394,
entry21ca46 withstatus043c, then shared tail21cf96 withvalue47.
That tail stores the diagnostic/status halfword at[r4+22] and continues
through21bd76 rather than returning like a standalone C function. The
internal dispatcher continuation and its next receive boundary remain
unresolved;043c has not been assigned success/failure semantics. The full
packet/cache gate passes on the lifecycle run, and state3/phase1 remain
unchanged. No missing-response conclusion follows merely from09f0 absence.

The45s `tmp/nsk3-controller-wait` capture confirms continuation reaches
receive loop21c86c with controller owner10ade0, selector47 andstatus043c;
no later controller helper result is observed. Static branch decoding sends
043c through21be34->21c86c, not onward through the cleanup following21cfa8.
The received object is ROM-resident at2c6750. Two aligned pointer literals
refer to it (212f68,2143a0), loaded by212c8e and214018 respectively; both
pass it through237bea after the trace wrapper27e29a. Which producer runs,
its controlling predicates and237bea's destination semantics remain to be
verified. ROM residency must not be mistaken for a synthetic harness reply.
The45s `tmp/nsk3-043c-producer` run identifies212c8e as the active candidate
(r4=108dd4,r6=108b2c), then sender237bea caller212c9f withpointer2c6750.
Candidate214018 is unobserved. Sender237bea explicitly posts to mailbox12
hex via266a2c; this controller mailbox is distinct from measurement mailbox
0c (decimal task12). The containing routine212c04 branches on227590 and,
conditionally,2275a6's range check before converging on this reply. Those
predicate meanings remain open:227590 reads record0605 through2a3994 into
a stack byte;2275a6 returns halfword[10feaa]. These are independently
anchored inputs, not yet identified as physical measurements or policy flags.
`tmp/nsk3-043c-inputs` observes212c04 argument2/caller2144cd,
record0605=ff and cachedffff. The optional predicate is record==1 and
signed cached range[0200,0376); otherwise212c4e clears108b30. Both branches
rejoin212c56, and argument!=1 continues to the043c publication. Thus erased
0605 does not alone suppress this reply, and assigning it1 would be an
unevidenced fixture. Caller2144b0 supplies argument2, while the separate
2144b4 selector can supply1 via2144c6. Their upstream dispatch/predicates
remain open; this observation does not establish that either is defective.

Shared-tail runtime qualification matters here: `tmp/nsk3-radio-context`
observes neither138f case214496 nor its2144a0 pointer test. The separate
`tmp/nsk3-radio-argument` run observes2144b0 with original object102eb4,
header03e90100. Header03e9 reaches213f54, byte2=1 selects214146,
and the later2141d8 comparison branches to2144b0 because1!=7. Therefore
the actual argument2 is not caused by138f's non-null context pointers.
This is a concrete MCU mode command; its producer and relation to controller
request09ec remain to be proved. The138f path is an alternate static route,
not the observed cause. Do not edit the unrelated context pointers to test it.

`tmp/nsk3-mode-caller` identifies03e9 construction at287a38 caller2541a7,
then post266a2c caller287d77 to mailbox0b withsameobject102eb4/header03e90100.
The03e9 constructor table selects287d54, copying byte10a3d1 to message+2
and10a3c5 to+3 (literals2880c4/2880c8), then a sequence byte to+4.
The active predecessor is receive-helper result07d6, not07e5:
`tmp/nsk3-service-result` records07d6 at2882d4/caller25455f. Dispatcher
table254578 entry29 points254188, whose07e5 comparison fails and reaches
2540f2; that alternate path can rejoin254196 before03e9 construction.
The observed construction caller remains2541a7. An earlier083b result
returns to254181. The07e5 comparison is an alternate route, not proof of
the input that executed; no07e5 queued post or2880b6 translation is observed
through45s. The source/meaning of actual07d6 and its causal relation to09ec
remain open. No native response format follows from this internal namespace.

The07d6 input is a real queued object, not merely a helper translation:
`tmp/nsk3-07d6-caller` observes constructor208a48 caller20e397 withr0=07d6,
then post208e3a to mailbox11 withobject101760/header07d601fb, followed by
the same object at helper receive28818e and result07d6. Producer20e392
is reached through20e388 when the preceding mode/status predicates do not
take the209d6c path. Its controlling received status and mode-byte provenance
remain unresolved. This measured route replaces a search for an imaginary
07e5 message producer; it still does not close computed/indirect producers.

`tmp/nsk3-mode-decision` closes the earlier09ec relationship:20e356
observes received status09ec, r7=0, owner10a7d8 and owner byte1=1.
The comparison at20e370 matches09ec (literal20e534), but the mode!=10
branch reaches20e388 and constructs07d6. The same capture contains the
ordered07d6 construction/post,03e9 construction/post, argument2 selection,
043c send/result and controller wait. `nsk3_packet_check.py --mode-chain`
qualifies that sequence without assigning mode1 semantics. The remaining
question is who sets owner byte10a7d9 and which legitimate lifecycle changes
it; this is not evidence of a missing native response to synthesize.

Mode provenance is now measured in `tmp/nsk3-request-mode`: the sole
post-clear10a7d9 write is209a24 copying byte2 of object101760/header09ec0191.
The09ec constructor enters with controller flag10adee=1,state10ade4=0,
source10ae3e=1, selecting that source byte rather than the alternate2/9/10
constants. The source itself is written1 at21f388 from literal r5=1 set
at21f36a, after21f36e finds controller byte6!=1. Thus the mode is a
deliberate firmware-owned branch, not an erased scalar default or unsolicited
DSP byte. Controller byte10ade6 remains0 through45s: after RAM clear,
observed writes are21ad20,21d16a,21a88c and21ab50, all0.
`tmp/nsk3-controller-shared` observes actual entry21f36a caller21d71f,
owner10ade0/flag0; entry21f358 is unobserved. Routine21d714 calls that
shared entry at21d71a. Static dispatch21ca46 selects21d714 for0a22
(literal21cd7c), but its actual input still needs a runtime check;
shared-entry ownership must not be inferred from adjacent code. Runtime
`tmp/nsk3-mode-callsites` observes none of the five directBL sites; entry
21d714 can also be reached by conditional branch21d678. The qualified
`tmp/nsk3-controller-threshold` capture confirms that branch's comparison:
owner10ade0 halfword+20 (10ae00) equals0176, threshold0176, selectingBGE.
Writes after clear come from21ac3e and set0176 each time. Routine21ac20
explicitly writes that sentinel when controller byte+11 is0, after filling
a48-byte context withff and setting its first halfword003f. Its alternate
byte+11!=0 path calls21ac94. Thus neither an observed0a22 input nor an
erased halfword causes the mode branch: it is an explicit missing-context
sentinel path. Context contents, the+11 lifecycle and mode1's domain meaning
remain unproved; no sentinel/flag replacement is proposed.

The alternate context path21ac94 calls295c1e, which reads exactly two
bytes at NV offset0074 through2a61f6, accepting only signed range[0200,0376).
Rejected values clear flag10adf1 at21acb8 and recurse into sentinel setup.
`tmp/nsk3-context-flag` observes that rejection write, then a later clear
at21aa44; no nonzero flag write occurs through45s. Its persisted EEPROM
bytes0074/75 are0000, outside the accepted range. This is a concrete stored
context prerequisite, not a demonstrated provisioning defect: a phone with
no remembered context must still have an ordinary acquisition path. Do not
invent a cached value merely to bypass it. The static contract/mutation
checks now anchor the NV read, range comparisons, sentinel store and branch.

The independent5130 mapping suggests an acknowledgement boundary, now
qualified against this own ROM: helper2881e0 subtracts base0447
(literal2883ec), thenff+a0, so input05e6 selects2880b6. Nonzero message
byte2 loads return07e5 from2883d4; zero consumes without that result.
The static contract and mutation tests cover the arithmetic and byte gate.
The qualified45s `tmp/nsk3-completion-surface` capture observes neither a
05e6 post to mailbox11 nor the helper translation, while mode-chain/cache
checks still pass.
This is an MCU acknowledgement candidate, not a DSP opcode; other05e6
literals in unrelated catalogues do not establish this mailbox's producers.

Three own constructor calls24862a/248bb2/2492a6 select05e6 versus an
alternate using owner byte+13a; constructor24709c copies owner byte0 to
message byte2. Its receive call is24753e. The45s qualified capture
`tmp/nsk3-completion-server` enters that call once but observes no return
at247542, no24709c construction and no05e6 post to mailbox11. The
`--completion-wait` checker requires a45s sample and rejects receive/output
events; it does not claim all ingress routes are enumerated. Mutation tests
anchor all three direct sites and the constructor/receiver. The active
frontier is legitimate server ingress, not flipping+13a or injecting05e6.

Mailbox identity is own-runtime-qualified: at249472 the kernel current-task
byte[100022] is0e (`tmp/nsk3-server-identity`), before entering247524.
The kernel receive primitive266c80 independently reads that same task byte.
Dispatch247550 uses base040b and recognizes040c/040d, then05fa/05fb.
`nsk3_state_contract.direct_server_sends` inventories363 aligned directBL
candidates to266a2c. Restricting to MOVS r0,0e in the final two halfwords
selects eight posts:213998,2146de,2147ac,214874,281748,281950,2820d2,
287f3a. The mutation-tested inventory does not cover shared-tail, computed
or indirect destinations. These candidates include MCU control paths and a
lower decoder; do not classify the server as exclusively DSP-fed.
The first four select ROM template2c670c/header040c0000;281748/281950
select2c6204 or2c6208 (040d0000/040d0300). The2820d2 path posts a dynamic
object after lower-decoder tracing;287f3a is a separate constructor tail.
Their ingress conditions and payload semantics are not yet established.

Cross-ROM lookup now uses explicit raw byte order:
`find_thumb_signature.py ... 0x23a728:20 --byteorder big` finds constructor
24709c uniquely (16/20 exact bytes after relocatedBL masking). The sibling
decoder269f28:20 finds own281e98 uniquely (20/20 exact), while a separate
constructor26e25c:20 has no match; absence is retained, not fuzzed away.
Own decoding independently confirms native type80 at2ab522..528 branches
to2ab5cc and calls281e98. Its path2820ce..d2 selects mailbox0e. Packet-body
predicates and any040b header rewrite remain to be decoded; the observed
74/74/74/74/99/8b census contains no80. This is an available but unexercised
transport route, not authority to fabricate its input.

The decoder's namespace is byte-addressed, not a GSM message-type claim.
After its preliminary owner gate, packet byte4==80 reaches281f50, writes
IPC header040b, copies original byte4 to+b and byte6 to+c, then selects
mailbox0e at281f6a. Byte4==b0/b1 also reaches this branch when byte6!=1;
byte6==1 takes the release path. A separate gated path2820a2 writes040b,
copies byte4 to+b and a computed0/1 to+d, then posts to0e at2820d2.
Its counter/context predicates are not fully classified and must remain
separate from the direct80 branch. Own instruction/literal mutation checks
anchor both040b rewrites, destinations and the native type80 entry. These
static routes do not establish a valid RF body or explain the absent packet.

Dynamic constructor287da4 preserves requested header at287e4c, accepts
05dd..05e3 and060a/060b through its own cascade (base05dd at287f5c),
and selects mailbox0e at287f36. Object-null arguments allocate a12-byte
message; caller field2 is stored before dispatch. The reproducible aligned
direct-call inventory finds33 candidates. One explicit05dd site253cc4
passes null object/field2=3 when controller byte+a!=4, then sets+a=2.
Its higher-level input is not yet classified. Qualified45s capture
`tmp/nsk3-server-dynamic` observes no constructor entry and retains the
empty-server/mode-chain/cache gates. Thus the MCU dynamic route is dormant
too, but an absent constructor observation does not prove DSP-only ownership.

The own selector2539ba compares helper result07ea (pool253d24), then
branches253cac ->253cc4. Helper287f70 can return07ea locally at2882d2:
receive mode2, no prior result, and nonzero[10a400+4] select that return.
The explicit mode2 call2557d0 is selected only when controller byte+a is
neither2 nor3; otherwise2557f8 selects mode1. Queueing handler287fc8
accepts07d3/07d4 (base05ee at2883f4 plus the decoded offsets); nonzero
message byte2 stores the object to10a404 at288000, while zero takes a
different forwarding path. These are internal work/return namespaces.
In qualified45s `tmp/nsk3-server-queue`,10a404 has only zero writes
(RAM clear and25414c initialization); observed helper entries use mode0
at254181/25455f/255737, never mode2. The queue producers and ordinary
mode2-selection lifecycle remain unresolved; no07ea post is required by
this path. Contract mutation checks anchor selector, queue owner/store and
mode-selection instructions without claiming exhaustive producer coverage.

Queue eligibility is narrower than header production. Constructor208a48's
base07d1 cascade sends07d3 to208e44. A nonzero argument builds a buffer
via208718 and writes outer byte2=0. A zero argument uses retained pointer
[10a7d8+54]; only `((buffer[4] >> 1) & 7) == 4` writes outer byte2=3,
otherwise0. All paths select mailbox11 via208eaa. Thus the many literal
07d3 loads supplying nonzero scalar arguments are not queue-populating
forms. The45s qualified `tmp/nsk3-queue-producers` capture observes no
208a48 construction with07d3/07d4, retains empty queue/mode0 and passes
the mode-chain/cache/server checks. The retained-buffer producer and other
constructor families remain open; no queue content is to be invented.

### Physical Send/End and matched radio input

The research profile now overrides inherited NSE-1 column0 labels using
the own table2c7a30:row2/column0 is logical0e (Send), row3/column0 is0f
(End). Previously Send was unexposed and End inherited a Scroll Down label.
The correction is confined to `nsk3r4t`; no normal product gains native-call
support from it. `nsk3_radio_input.lua` uses only physical port contacts:
accepted phone code, Menu/Back, dial12345, Send22s and End30s. The matched
`nsk3_radio_no_call.lua` omits only Send/End.

Qualified pair `tmp/nsk3-radio-call-qualified` and
`tmp/nsk3-radio-no-call-qualified` reproduces own logical0e/0f decoding in
the Call/End run only. Both retain the measured mode-chain, exact packet
cache and empty acknowledgement server through45s, with no queue or dynamic
server construction. Flash remains own-image exact and SIM files match.
EEPROM files differ at19 offsets, including packed12345 at1a90; their NV
record semantics remain unqualified and the checker reports rather than
conceals those differences. This proves physical input, not connected calls,
network registration, native speech or EEPROM-history fidelity.

```sh
.venv/bin/python tools/nsk3_call_input_check.py roms/noki6130/source \
  tmp/nsk3-radio-call-qualified tmp/nsk3-radio-no-call-qualified
```

The retained slot10a82c (owner10a7d8+54) stays null in the baseline45s
`tmp/nsk3-retained-buffer` capture. Own helper209a4a copies message+4 to
that slot for decoded inputs09c9/09d5/09e2; this is a candidate session-input
surface, not a proved required boot transition. No input is synthesized.

Cold recent-number control `tmp/nsk3-recent-cold` copies the action run's
persisted EEPROM/SIM/flash/config only. After physical phone-code acceptance,
Menu/Back and one physical Send, frame23s says **No last dialled numbers**
(RGB SHA256c713d38b2581c34f8cb87ee30f728712bd28d48c886fa1d3d595d5717a3a5299).
`nsk3_recent_call_input.lua` types no post-unlock digits. Checker option
`--recent-cold` verifies input EEPROM/SIM identity, own flash, physical recall,
logical Send and that exact negative frame. Thus packed12345 in the prior
EEPROM diff is not proof of usable call-history persistence. Cold EEPROM
housekeeping changes are not declared history fidelity; the record contract
and whether a successful call is required remain unresolved.

### Native compatibility control census

`nsk3_native_radio_observe.lua` adds passive DSP data-space write taps at
06d0/06d1 and live program reads, composing the existing physical/menu and
MCU observers without hardware/guest writes. Qualified45s capture
`tmp/nsk3-native-control` observes pending values0,1,2,8,3; completion writes
end at010e. Samples5/20/40/45s retain pending3,completed010e,06e3=0,1949=0.
Counts5/12 remain below the64-write caps. No index4 request is observed.
Live8-word regions3000,313f,47f6,4a49 match the declared NSE-1 ROM4 program
byte-for-byte; the checker pins its acquired SHA1 before comparing them.
This does not qualify the fitted NSK-3 mask, all uploaded program regions,
physical RF inputs, a periodic-packet obligation or the semantics of every
control word. Do not request index4 merely because a candidate completion
publisher lies downstream in the sibling map.

```sh
.venv/bin/python tools/nsk3_packet_check.py roms/noki6130/source \
  tmp/nsk3-native-control --mode-chain --cache --completion-wait \
  --native-control roms/nsk3r4t/nse1_rom4_dsp_program.bin
```

The next native work is the operation/request and RF-input contract, using
live-code qualification before importing resident-only addresses. The MCU
queue and acknowledgement findings constrain hypotheses but are not missing
hardware events to manufacture.

The separate90s qualification `tmp/nsk3-native-scan` samples the scan counter,
result fields and RF-sample queue. At40/45/65/90s, counter1262/1263 is
ffff:c292/c6cd/d7bb/ece3, advancing10833 steps while still negative.
Fields1344 and1266 remain0, threshold219b is0574, and producer06b7 and
consumer06b8 match at each late sample. This rules out a permanent empty
sample-queue stall at the sampled boundary; it does not prove physical RF
encoding. Live45s program capture is131072 bytes, SHA256
bcf379e954b2a7a17a47e5e4c92a9b3368feafe048b2f7fc3c93aa06944ec3b7.
The gateway2420 isf073/0a71, targeting uploaded initializer0a71 rather than
the resident-only0a5c address. The checker anchors its live qualification
and counter instructions at0ae6/0b08, with mutation tests.

The passive write census `nsk3_rf_queue_observe.lua` strengthens the snapshot
result: between40s and90s both producer06b7 and consumer06b8 change10833 times,
exactly matching the scan-counter advancement. Initial writes occur at0.135s,
the queue is configured at4.815s, and sustained consumption appears around39s.
Producer updates observed at3347 and consumer updates at475b are native code,
not observer writes. Thus equal late pointers mean a repeatedly drained queue,
not an unchanged empty one. This still does not validate the source samples,
their units, RF filtering or physical cadence. The qualified90s capture
`tmp/nsk3-rf-queue` also passes the existing packet, mode, cache, native-control
and live-program scan checks.

```sh
.venv/bin/python tools/nsk3_rf_queue_check.py tmp/nsk3-rf-queue/error.log
```

The live6130 measurement gate is independently observed, not inherited from
the5130 interpretation. Eight program-fetch observations at0ae6 (exposed
PC0ae7) between39.0285s and39.0608s return A=0 with threshold219b=0574;
each has matching producer/consumer cursors advancing by four words. Live
4701 calls2424, consumes the queue, and reduces intermediate values through
47e6's multiply/accumulate path. The producer uses PORTR0027 at330a, which
the backend routes to COBBA's unattached RF callback. Eight observed port reads
at330c/3312/3318/331e are zero, and the first measurement-return snapshot of
all72 queue words1a00..1a47 is also zero. Thus the zero return is consistent
with observed zero input, not independent evidence of a reduction defect.
This locates the zero-result boundary across native queue consumption and measurement return;
it does not prove arithmetic silicon fidelity or the electrical sample format.
`nsk3_scan_metric_observe.lua` captures live disassembly at45s; its checker
pins live instruction fragments and rejects changed return/threshold values.

```sh
.venv/bin/python tools/nsk3_rf_queue_check.py tmp/nsk3-scan-source/error.log \
  --metric-program tmp/nsk3-scan-source/nsk3_native_program_45s.bin --source-input
```

The shared software-RF contract in `5190_bringup.md` applies as an evidence
checklist, not as proof of this board's wiring: sample/component ordering,
buffered sample-time progression and receive-window control are still needed
before attaching a standards-derived waveform. Callback read spacing is CPU
block processing, not an established ADC sample period. A guessed nonzero
source or a threshold-crossing score would not establish acquisition.

The modeled COBBA RF receive callback is unattached/deterministic silence;
the active scan's zero qualifying results are not evidence for a real
receiver's noise/offset contract. Its timeout has not expired at90s, before
the independently observed~94s packet-supervision reset. That timeline
mismatch does not identify a clock defect or justify changing thresholds,
speeding the scan, fabricating RF samples or adding periodic traffic.

```sh
.venv/bin/python tools/nsk3_packet_check.py roms/noki6130/source \
  tmp/nsk3-native-scan --mode-chain --cache --completion-wait \
  --native-control roms/nsk3r4t/nse1_rom4_dsp_program.bin \
  --native-scan tmp/nsk3-native-scan/nsk3_native_program_45s.bin
```
`nsk3_packet_check.py --controller --cache`
checks the measured send/result/wait ordering and retains unknown reply
semantics; mutation tests reject wrong source, object, status, selector/order.

```sh
.venv/bin/python tools/nsk3_packet_check.py roms/noki6130/source \
  tmp/nsk3-cache-qualified --delivery --cache
```

The checker rejects incomplete captures, including the initial32-byte probe,
and distinguishes observed body fields from radio/physical-unit acceptance.
The next bounded trace should identify legitimate writers/transitions of
the task12 state/phase before inventing an RF stimulus
or periodic response.

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
