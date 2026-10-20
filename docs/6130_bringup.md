# Nokia 6130 NSK-3 bring-up boundary

## Current result

**Long-run limit:** the declared native compatibility composition requests a
reason68 restart at93.967473692s. Short physical UI/storage/security gates do
not establish sustained native operation. The own packet-supervision contract
below reproduces this failure; no keepalive or disabled watchdog is installed.

The separate native compatibility instrument reaches **Insert SIM card** and
rescans the physical matrix under an explicit nominal ADC2 hypothesis (554).
With the synthetic SIM inserted and an own-initializer retry-byte fixture,
physical12345 submission is accepted and another physical command opens a
readable Messages menu. A separate cold process preserves storage unchanged
and requires the phone code again. The displayed idle content is not accepted.
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
Physical offline SMS composition saves an unsent A draft to EF_SMS; a cold
Outbox read displays it without changing card bytes. Network SMS is unproved.
Physical Erase/cancel and a cold empty-Outbox check also pass.
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
the cold comparison preserves byte-for-byte. No unexpected credential-changing
APDU is admitted. The PUK gate below extends this boundary; permanent blocking,
PIN2 remains unaccepted; ordinary PIN change and request settings are covered below.

### PUK recovery and new-PIN persistence

`--pin-blocked` explicitly provisions the shared laboratory card with CHV1
attempts0. It supplies no handset state. Physical PUK12345678, new PIN4321
and confirmation produce `a0 2c 00 00 10` with the entered credential bytes.
The card returns9000, stores PIN4321, restores CHV1 attempts3/PUK1 attempts10,
and the UI reaches the separate phone code. A wrong-PUK12345679 control returns
9804, preserves the original PIN, keeps CHV1 blocked and decrements only PUK1
attempts to9. The complete generated credential/retry prefix is checked in both.

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
inputs, not permanent PUK blocking, PIN2/PUK2, ordinary CHANGE CHV, PIN-request
settings, radio or fitted hardware assumptions.

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
unchanged. PIN-request disable has its own gate below; permanent blocking and
PIN2 remain unaccepted.

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
the corresponding cold challenge behavior. PIN2, permanent PUK blocking,
correct idle content and radio remain unaccepted. A Battery low alert appears
around33s in the longer Off run under the unchanged nominal-input hypothesis;
that analog/power observation is not hidden by adjusting the fixture voltage.

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
