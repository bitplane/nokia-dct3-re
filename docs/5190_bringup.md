# Nokia 5190 NSB-1 bring-up

## Current boundary

The acquired service package supplies independently identified NSB-1 v6.71
MCU and PPM A/B streams. It is not a full phone dump and does not establish
graphical boot or phone services. The separate `noki5190` MCU research
configuration executes the own input, initializes the own EEPROM geometry,
installs its exact ROM4-catalogue program and reaches a supervisor flag loop.
The supervisor flag is cleared organically by FIQ; its sampled loop is not
a blocked lifecycle. In the ordinary silent configuration, the unanswered transition is the uploaded
program's resident-version publication to shared word `10004`, before the
MCU begins sparse-flash handoffs. Separate `nsb1r4t` executes acquired NSE-1
ROM4 as an explicit compatibility candidate: it passes own version selection,
63 block handoffs and native loader/resident bootstrap continuation. Correct
ring notification also permits sustained native execution and physical
Phone book menu navigation; this remains an unproved fitted-mask comparison.
LCD bus/pixel
acceptance reproduces firmware clear/fill/clear self-test with erased storage.
The qualified checksum/nominal-input comparison additionally reaches the
firmware's Insert SIM card screen. Fitted mask identity and DSP cadence remain unproved.
No scheduler or ownership flag is injected.

### Native clean-idle restoration

An independent54s cold process from the physically selected Field test00
storage authenticates the original laboratory PIN1234 and separate phone12345,
then saves at40s without an application key. All37 observed ARM registers,
36 DSP fields and rolling checksums of64KiB backed research ARM RAM,
ordinary DSP data and program restore exactly. The observed ARM checkpoint
is PC2a4672/SP10c6dc. The2.7s reference/restored windows each contain7764
identical DSP I/O writes, including relative times, addresses, data and masks.
Physical Menu in both branches renders the reviewed Phone book frame,
RGB SHA256 `ed5b9d247f48525c5f67b4ca71914bff1802af857d212e4dd5cb0e7e8c9a8b33`.

Complete8192-byte EEPROM remains SHA256
`4bd101b1c9089fed802aa5630135c4371f3bb070b136c7bfc15031917a598cad`;
own executable flash, original3570-byte PIN-enabled card and external CFG
are unchanged. Preparation copies qualified executable inputs and persistent
device/CFG files only, not guest RAM or a predecessor save. The resulting
save artifact is nonempty and hashed. Architecture, native startup/authentication,
exact pixels and complete storage have independent checker requirements.

```sh
.venv/bin/python tools/nsb1_native_state_check.py \
  tmp/nsb1-diagnostic-display-final-cold tmp/nsb1-native-idle-state \
  --mame mame/mame
```

Omit `--mame` to recheck retained evidence; execution requires a fresh output.
This is bounded native-compatibility restoration and physical continuation,
not exhaustive device-state coverage, SRAM geometry, fitted-mask identity,
factory calibration, sustained RF operation, registration or native speech.
The existing checksum/retry and nominal ADC hypotheses remain explicit.

### Executable upload extent

The own loader writes 825 program words in `2800..2b38`, including
`29e1=7714,29e2=085e,29e3=7194,29e4=1f2b,29e5=1284`.
The former `2800` overlay boundary discarded those writes; a read-only tap
observed the nonzero writes while program/data reads returned zero. The
`nsb1r4t` configuration therefore selects a `3000` overlay extent, retaining
the code in both views. This is an upload-derived bank model, not proof of
the fitted DSP mask or full physical RAM size; `noki5190` is unchanged.

`nsb1_native_upload_observe.lua` composes the qualified listed-FDN physical
dial fixture. The 46-second corrected run passes
`nsb1_native_upload_check.check_upload`, existing cold provenance/native/SIM
checks, unchanged whole-card comparison, and the existing exact digit and
progress frames. Its 566 distinct first opcode words all have decoded
boundaries, including observed extension variants; opcode `0000` is absent.
This is not exhaustive semantic qualification, registration or connected-call
acceptance. The prior own-input control passes the same input/card/frame
checks but fails upload visibility, distinguishing the mapping correction
from an incidental change of fixture or provisioning.

The compatibility instrument now uses an explicitly nominal ADC2 input of
554. With the six-byte checksum diagnostic it completes ten acquisitions,
posts readiness report `14` organically, and unmasks keypad columns. Physical
Menu reaches logical decoder result `19`. The five-second raster is transient;
the exact readable empty-socket frame is accepted at ten seconds. Physical
Phone book navigation, SIM contact save/cold search, and SIM PIN failure/cold
retry recovery are accepted in this compatibility configuration. General app
coverage, radio services and fitted ADC routing remain unproved.
The ordinary silent `noki5190` configuration is unchanged.

### Physical diagnostic-display exit and cold idle

The marks on the qualified idle frame are controlled by the firmware's
Field test display, not a broken LCD transport. Physical Menu, Up, Select
opens the own `Test` editor showing01. Entering00, confirming OK and pressing
Back twice leaves a readable idle frame. Own descriptor`2cc088` maps
record`0604` to EEPROM`04a1`, length1; the firmware changes that byte fromff
to41. That observation is not a full bitfield specification.

The isolated two-process gate starts from primed own saved devices, without
editing EEPROM or hardware inputs. The physical-selection process changes
exactly that one EEPROM byte; a separate cold process preserves the complete
result and reproduces the clean idle pixels after physical PIN/phone login.
Flash and the complete laboratory card remain unchanged. Predecessor hashes,
own descriptor/source identity, complete EEPROM equality, all eight physical
contact/release pairs and reviewed Field test/editor/idle frames are required.
The25s cold frame is independently pinned before the accessory title appears;
the36/42s frames match the post-selection idle hash
`934b3dcd1024275cbee8dc1e87ec35124fb617a917a0684a1e7bc1463ee59068`.

The title says`Headset` under the unchanged board defaults. This is not
registered-operator presentation, validated unattached-accessory wiring,
fitted-mask identity or long-run native stability.

An explicit one-byte empty-startup-text control falsified the borrowed
NSB-3 explanation for these marks: own record`0745` maps to`05ec`, length31,
but changing only its first byte to zero leaves the same25s pixels. The
final physical/cold gate retains all31 originalff bytes; the unnecessary
text fixture and exploratory implementation are removed.

```sh
.venv/bin/python tools/nsb1_diagnostic_display_check.py \
  tmp/nsb1-toolkit-control-accepted tmp/PHYSICAL tmp/COLD --mame mame/mame
```

Both outputs must be fresh. The predecessor is the qualified primed own
Phase-2 card control, not donor provisioning. Accepted captures:
`tmp/nsb1-diagnostic-display-final` and`tmp/nsb1-diagnostic-display-final-cold`.

### SIM PIN-request settings

The own PPM-A UI selects SIM card security through physical Menu4-2-1,
consistent with the [Nokia5190 owner's manual](https://www.manualslib.com/manual/299900/Nokia-5190.html).
The accepted four-process cycle disables and enables the request through
physical current-PIN entry, not a card-image patch. DISABLE CHV and ENABLE CHV
each return9000 and change only persistent request byte3520; all credentials,
retry counters and other card contents are unchanged.

After Off, a cold process performs no VERIFY CHV but still requires the
independent phone security code. After On, a cold process displays the reviewed
PIN prompt, physically verifies1234, then separately enters phone code12345.
Each process qualifies own executable/native execution, reviewed settings or
authentication frames, complete-card effects and linked saved-input hashes.
This extends bounded synthetic-card security coverage, not fitted mask,
factory provisioning, native radio registration or calls.

```sh
.venv/bin/python tools/nsb1_pin_request_check.py \
  tmp/nsb1-pin-roundtrip-gated/correct \
  tmp/nsb1-pin-request-off tmp/nsb1-pin-request-off-cold \
  tmp/nsb1-pin-request-on-qualified tmp/nsb1-pin-request-on-cold
```

Preparation uses the existing `prepare_cold_run` with the preceding qualified
saved devices. Warm runs use `nsb1_pin_disable.lua`/`nsb1_pin_enable.lua` for54s;
cold runs use `nsb1_security_fixture.lua`/`nsb1_pin_reload.lua` for28s. The
5190's Menu/Navi contact is column1/mask04 and Up is column1/mask10; the 6130's
softkey/navigation cells are not inherited by these fixtures.

### SIM Toolkit external-card boundary

The explicit Phase-2+ laboratory profile independently qualifies DISPLAY TEXT
on `nsb1r4t`. The own firmware reads EF_PHASE=03, publishes the five-byte
TERMINAL PROFILE `0f41ff7700`, receives one pending `91 16` STATUS and FETCHes
the exact `DCT3 SAT` proactive command. Physical Menu/Navi OK at68s precedes
the sole successful TERMINAL RESPONSE `810301218002028281030100`.
Both SIMI wire directions are observed below the protocol parser; no event,
callback, guest RAM or card-response injection bypasses the controller.

The complete persistent PIN-enabled card, own MCU/PPM, declared NSE-1 resident
inputs and predecessor-storage hashes are preserved. The only configuration difference
is SATCFG:1. A matched SATCFG:0 Phase-2 control reads EF_PHASE=02 and sends
no TERMINAL PROFILE, FETCH or TERMINAL RESPONSE under the same physical
authentication/dismissal timeline. MAME omits that default-valued field from
the saved CFG; the checker accepts its absence only for this control.

The reviewed proactive-text frame is distinct from the post-dismissal frame.
That fixture retains Field test01, so dismissal returns its existing marked
idle content. The separate physical diagnostic-display gate above qualifies
the clean layout without changing this Toolkit oracle. This is bounded card/application acceptance on an explicitly
unproved ROM4 compatibility instrument, not fitted-mask, native radio,
registration, speech or the rest of Toolkit. Separate interactive qualification
below uses the physically selected clean-idle storage, not this older display
fixture's Field test01 bank.

#### Physical GET INKEY and GET INPUT

An independent84s profile3 run starts from the own clean-idle bank in
`tmp/nsb1-diagnostic-display-final-cold`, with the original complete PIN-enabled
laboratory card. It retains own terminal profile `0f41ff7700`, displays and
physically dismisses `DCT3 SAT`, then FETCHes `Press 5` and `Enter 42`.
Physical Keypad5 plus Navi confirmation returns
`8103022200020282810301000d020435`; Keypad4/Keypad2 plus a separate confirmation
returns `8103032300020282810301000d03043432`. Card response statuses are
9115,911a,9000 in order. Exact wire payloads precede their contacts, and no
terminal result precedes the corresponding physical confirmation.

Reviewed70.5/73.5/76s frames show Press5, empty Enter42 and filled42; the79s
frame returns to the own clean Headset idle layout, not network registration.
The entire8192-byte EEPROM (selected bank SHA256
`4bd101b1c9089fed802aa5630135c4371f3bb070b136c7bfc15031917a598cad`),
original3570-byte card and own executable flash remain unchanged. The wrapper
requalifies cold source hashes, configuration, physical PIN/phone authentication
and native execution through83s. The declared NSE-1 resident-mask comparison,
nominal ADC2 and provisioning limitations remain; this is not fitted-silicon,
RF, speech or long-run native acceptance.

```sh
.venv/bin/python tools/nsb1_toolkit_check.py \
  tmp/nsb1-diagnostic-display-final-cold tmp/nsb1-toolkit-interactive-accepted \
  --interactive
```

Add `--mame mame/mame` with fresh outputs to execute. The common laboratory
schedule is `sim_toolkit_interactive_input.lua`; the NSB-1 wrapper selects
its own Navi column1/mask04, distinct from the6150's column1/mask02.
Wire validation and lab command bytes are shared, not terminal profiles,
storage contents or product pixel oracles. Other Toolkit commands remain open.

#### Installed Toolkit menu and ACK-only return

Separate90s profile4 cold runs append SET UP MENU `DCT3 menu` with Continue/Exit
to the same DISPLAY TEXT/GET INKEY/GET INPUT sequence. GET INPUT's response
receives9128, followed by FETCH40 and exact menu-installation response
`810304250002028281030100`, accepted with9000. Own terminal profile remains
`0f41ff7700`.

Physical Navi80 enters Phone book in the main menu. Up81 wraps to Field test,
Up82 selects the installed DCT3 menu, and Navi84 opens its two-item list.
Continue86 emits envelope `d30702020181100101`; a separate cold run uses
Down86 and Navi88 to emit Exit envelope `d30702020181100102`. Down is the
column0/mask08 contact, not column1's unused bit08. Both envelopes receive9000
after their physical selection. The complete selected EEPROM, original card
and own executable remain unchanged.

At87s/89s respectively the LCD exactly matches the pre-selection list with
the chosen item highlighted. Thus this product returns to its list under the
ACK-only profile, unlike the separately observed6150/6190 Wait outcomes.
Do not normalize those UI contracts or require a follow-up command on every
product. This qualifies the bounded return and the two selections, not arbitrary
Toolkit applications or continued repeated selection.

```sh
.venv/bin/python tools/nsb1_toolkit_check.py \
  tmp/nsb1-diagnostic-display-final-cold tmp/nsb1-toolkit-menu-continue-accepted \
  --menu-item 1
.venv/bin/python tools/nsb1_toolkit_check.py \
  tmp/nsb1-diagnostic-display-final-cold tmp/nsb1-toolkit-menu-exit-accepted \
  --menu-item 2
```

Add `--mame mame/mame` with fresh outputs to execute. The gate checks physical
authentication, own native execution through89s, complete storage and input
provenance, exact command/envelope/status ordering, product-owned pixels and
MAME console observer errors, not just successful process exit. Existing
provisioning, fitted-mask/clock, RF, registration and speech limits remain.

#### Toolkit follow-up SELECT ITEM

An independent92s external profile7 run follows the same installed-menu
Continue selection with9122 and FETCH34, delivering the card's Choose command
with Alpha/Beta. Physical column0/mask08 Down89 selects Beta; Navi90.5 returns
`810307240002028281030100100102`, accepted with9000. The88s/89.5s/91.15s
frames independently show Alpha, Beta and the restored Continue/Exit list.
The result list exactly matches its earlier open-list frame. This qualifies
the card-issued follow-up separately from the ACK-only return above.

```sh
.venv/bin/python tools/nsb1_toolkit_check.py \
  tmp/nsb1-diagnostic-display-final-cold tmp/nsb1-toolkit-select-item-accepted \
  --select-item
```

Add `--mame mame/mame` with fresh outputs to execute. The complete selected
EEPROM, original card and executable/resident inputs stay unchanged. The gate
checks own physical authentication, native execution through91s, cold/profile
provenance, console errors, exact protocol/action order and product-owned
pixels. No response precedes the physical confirmation; no firmware state,
message or completion is injected. This is not arbitrary Toolkit support,
repeated-selection coverage, fitted-mask fidelity, registration or speech.

#### REFRESH and terminal re-profiling

External profile9 uses the own physical Continue route after the established
text/input/menu commands. Its envelope receives910b; firmware FETCHes the
11-byte REFRESH qualifier00 command, re-downloads own terminal profile
`0f41ff7700`, then returns `810309010002028281030100`. The new profile and
REFRESH response both receive9000. The reviewed88s frame says `SIM update
started`;91s reproduces the exact qualified Headset idle frame.

```sh
.venv/bin/python tools/nsb1_toolkit_check.py \
  tmp/nsb1-diagnostic-display-final-cold tmp/nsb1-toolkit-refresh-accepted --refresh
```

Use `--mame mame/mame` with a fresh output to execute the92s gate. Its complete
8192-byte EEPROM output SHA256 is
`0a79d52b219fad8e29dc3ccbddfaa31b7e2d0da2330850231763f9d28e8884d8`;
only04cb changes02 to00. The whole output bank is pinned, not a changed-region
allowlist. Own group7 directory maps that byte to descriptor0712, nested in
48-byte record0702 at04b0. A separate read-only raw-writer trace observes
2aafe2 receiving the whole record from wrapper2a792e: earlier buffers carry02,
the post-REFRESH buffer carries00. General field semantics remain unassigned.
The own profile4 ACK-only control preserves the original whole bank; do not
borrow the6150's different record0766 update or its terminal profile.

The full card and executable flash stay unchanged. The REFRESH-specific16384
wire cap captures8670 RX and2042 TX bytes; ordinary fixtures retain8192 and
saturation remains an error. The gate rechecks authentication, cold provenance,
all exact preceding/REFRESH payloads, statuses, physical ordering, CFG, native
continuation and reviewed pixels. This qualifies REFRESH completion and idle
return, not a later restarted application cycle, fitted mask, RF or speech.

#### Pending Toolkit command restoration

An independent74s profile1 process starts from the same own clean-idle bank
and saves at65s with the organically fetched `DCT3 SAT` command awaiting
physical dismissal. Own ARM PC2a4674/SP10c6dc and active DSP PC47a1/IDLE0
are observed. All37 ARM registers,36 DSP fields and rolling checksums of64KiB
backed research ARM RAM, ordinary DSP data and program restore exactly.
The2.7s reference/restored windows repeat1170 DSP I/O writes including relative
times, addresses, values and masks.

One TERMINAL PROFILE `0f41ff7700` and one FETCH precede save; separate physical
Navi contacts in the two branches each emit the exact successful response,
accepted with9000, without refetching the command. Both resulting frames
equal the own clean Headset idle oracle. The complete selected8192-byte
EEPROM, original3570-byte persistent card and own executable remain unchanged.
Source hashes/CFG, physical PIN/phone authentication before save, card protocol,
prompt/returned pixels and a nonempty hashed saved artifact are checked.

```sh
.venv/bin/python tools/nsb1_toolkit_state_check.py \
  tmp/nsb1-diagnostic-display-final-cold tmp/nsb1-toolkit-native-state-accepted
```

Add `--mame mame/mame` with fresh outputs to execute. The shared replay
validator also serves the6150 and6190, but profiles, physical contacts,
storage, RAM extent and pixels stay product-owned. This is MAME save/load
and physical input, not guest-state forcing. It covers one fetched proactive
command awaiting a key, not an in-flight SIM character or every card command.
Bounded checksums/replay are not exhaustive device-state proof, sustained
native acceptance, fitted-mask/clock fidelity, registration or speech.

```sh
.venv/bin/python tools/nsb1_toolkit_check.py \
  tmp/nsb1-pin-roundtrip-gated/correct tmp/NAME --mame mame/mame
.venv/bin/python tools/nsb1_toolkit_check.py \
  tmp/nsb1-pin-roundtrip-gated/correct tmp/CONTROL --control --mame mame/mame
```

Both commands require fresh output directories and the qualified own original
PIN-card predecessor, not a donor provision. Captures:
`tmp/nsb1-toolkit-display-accepted` and `tmp/nsb1-toolkit-control-accepted`.

### Fixed dialling card state

An explicit FDN-capable laboratory card now passes physical activation,
cold enabled-state handshake, physical deactivation and cold disabled-state
verification on own PPM A. `nsb1_fdn_prepare.py` appends the shared FDN extension
to the qualified3570-byte card; handset EEPROM, executable/resident inputs and
calibration are not changed. The manifest records both original and extended
card hashes, and the initial extended card is preserved for gate comparison.

Own Menu4-2-4 reaches Fixed dialing. The fixture uses the observed second
Select to reach its On/Off/Numbers list before choosing an action and entering
PIN2. The shared APDU contract checks exact PIN1/PIN2 payloads, ADN INVALIDATE
or REHABILITATE targets and the enabled-session LOCI/IMSI rehabilitation.
Only ADN validity changes in warm processes; cold processes preserve the whole
card. Reviewed warm frames show On/Off. Cold option highlights are not used
as state evidence: acceptance rests on full card bytes and the protocol.

```sh
.venv/bin/python tools/nsb1_fdn_check.py tmp/nsb1-pin-request-on-cold \
  tmp/nsb1-fdn-on-qualified tmp/nsb1-fdn-on-cold \
  tmp/nsb1-fdn-off-qualified tmp/nsb1-fdn-off-cold
```

First preparation uses `nsb1_fdn_prepare.py`; later phases use the unchanged
`prepare_cold_run` copying preceding saved devices. Warm fixtures
`nsb1_fdn_input.lua`/`nsb1_fdn_disable.lua` run56s; cold
`nsb1_fdn_cold.lua` runs40s. A passive SIMI TX tap caps4096 bytes and the gate
rejects saturation. Own input hashes/native execution, physical security and
FDN contacts, complete-card effects and reviewed frames remain mandatory.
Physical PIN2-protected FDN record save and cold visible retrieval also pass.
Numbers/Add receives5678, name A and number12345; one exact absolute UPDATE
RECORD stores the32-byte ASCII/BCD object in record1. The entire card changes
only that record. A separate cold Search displays A, and a physical four-second
`#` hold displays12345; release returns to the same list. No UPDATE RECORD or
card change occurs during cold retrieval. The hold is a fixture duration, not
a claim about the firmware's minimum long-press threshold.

An independent wrong5675 Add attempt returns VERIFY9804 and Code error, sends
no UPDATE RECORD and changes only PIN2 retries3-to2. The record gate checks
exact PIN/APDU payloads, physical security/FDN contacts, hold duration,
reviewed Saved/number/list/error frames, native execution and full card/cold
links. The shared card is not prepopulated to obtain these results.

```sh
.venv/bin/python tools/nsb1_fdn_record_check.py tmp/nsb1-fdn-off-cold \
  tmp/nsb1-fdn-write tmp/nsb1-fdn-read-held tmp/nsb1-fdn-write-wrong
```

The78s save,54s cold view and60s rejection use `nsb1_fdn_write.lua`,
`nsb1_fdn_read.lua` and `nsb1_fdn_write_wrong.lua`.

In-place editing with FDN Off is independently accepted: own Search/Options/Edit,
physical PIN2, unchanged name, Clear/6/Save issues one successful UPDATE RECORD.
The whole card changes only byte3600 F5-toF6 (12345-to12346). A linked cold
process holds `#` to display12346 and returns to the same name list, with no
UPDATE RECORD and no card change. The gate requires the exact edited payload,
PIN2 authorization, saved/number/list frames, input links and native execution.

```sh
.venv/bin/python tools/nsb1_fdn_record_check.py tmp/nsb1-fdn-off-cold \
  tmp/nsb1-fdn-write tmp/nsb1-fdn-read-held tmp/nsb1-fdn-write-wrong \
  --edit tmp/nsb1-fdn-edit --edit-cold tmp/nsb1-fdn-edit-cold
```

`nsb1_fdn_edit.lua` runs74s and the standard cold view runs54s.

Erase and cancellation are separately accepted from the same saved edited
card. Physical Search/Options/Erase authenticates PIN2 and presents Erase? A.
Confirmation issues exactly one successful all-FF32-byte UPDATE RECORD to
record1; Cancel issues none and preserves the complete card. The success
caption says Phone book erased, but the APDU/card evidence establishes only
the selected FDN record's erasure, not wholesale ADN or handset storage loss.
A linked cold Search displays No phone numbers, sends no UPDATE RECORD and
preserves the erased card.

```sh
.venv/bin/python tools/nsb1_fdn_erase_check.py tmp/nsb1-fdn-edit-cold \
  tmp/nsb1-fdn-erase tmp/nsb1-fdn-cancel tmp/nsb1-fdn-erased-cold
```

Warm70s fixtures use `nsb1_fdn_erase.lua`/`nsb1_fdn_cancel.lua`; the50s cold
fixture uses `nsb1_fdn_empty.lua`. The gate checks exact confirmation/result/
empty frames, native execution, own cold-input links, physical contacts,
PIN2/update payloads and complete-card effects. Local dial restrictions remain
unaccepted on NSB-1; registration and native speech remain separate boundaries.

### SIM credential changes and recovery

An ordinary physical PIN1 change is separately accepted through own Menu4-2-5-1
(Access codes). Current1234, new4321 and confirmation produce one CHANGE CHV
`A0 24/9000`, not UNBLOCK CHV. Only the PIN1 credential changes in the complete
saved card; request state, PIN2, PUK credentials and retry counters are retained.
A separate cold process physically verifies4321, still enters phone code12345,
and preserves the entire changed card without repeating the change command.

```sh
.venv/bin/python tools/nsb1_pin_change_check.py \
  tmp/nsb1-pin-request-on-cold tmp/nsb1-pin-change tmp/nsb1-pin-changed-cold
```

The warm70s fixture uses `nsb1_pin_change.lua`; the cold28s fixture uses
`nsb1_pin_changed_reload.lua`. Existing native/own-flash gates, reviewed PIN
prompt/changed/accepted frames, physical contacts and linked cold-device hashes
remain mandatory. PIN1 change-rejection controls remain separate acceptance items.

Ordinary PIN2 change is also independently accepted through own Menu4-2-5-2.
The warm process authenticates5678, changes it to8765 and confirms; only PIN2
credential bytes change. A separate cold process authenticates8765 by changing
it back to5678. Both use CHANGE CHV/9000, never UNBLOCK CHV, and the final whole
card equals the original input. The reviewed PIN2 code changed frame, native
execution, physical contacts and linked storage are checked in each process.

```sh
.venv/bin/python tools/nsb1_pin_change_check.py \
  tmp/nsb1-pin-request-on-cold tmp/nsb1-pin2-change \
  tmp/nsb1-pin2-restore-cold --pin2-roundtrip
```

The70s phases use `nsb1_pin2_change.lua` and `nsb1_pin2_restore.lua`; these
select physical PIN2 digits in the existing own Access codes fixture. The
round-trip result proves persisted credential authentication, not cached CHV2
verification or PIN2 exhaustion/PUK2 handling.

The PIN2 round-trip gate additionally accepts `--rejected` and `--retry`.
An independent same-input run physically enters wrong current5675, new8765
and confirmation. CHANGE CHV returns9804 and Code error; the whole card changes
only PIN2 retries3-to2, with all credentials intact. A linked cold process
retains that counter, accepts current5678, changes PIN2 to8765 and restores
retries to3 organically. No retry field is reset by preparation.

```sh
.venv/bin/python tools/nsb1_pin_change_check.py \
  tmp/nsb1-pin-request-on-cold tmp/nsb1-pin2-change \
  tmp/nsb1-pin2-restore-cold --pin2-roundtrip \
  --rejected tmp/nsb1-pin2-change-wrong --retry tmp/nsb1-pin2-change-retry-cold
```

The new security gates share `check_cold_link`, which rehashes actual executable
and resident input files as well as matching the manifest's saved-device
hashes to the preceding process. A matching manifest alone is insufficient.

PIN2 exhaustion is also organic: three separately cold-seeded wrong changes
decrement retries3-to2-to1-to0, preserving every credential and all other card
bytes. The first two CHANGE CHV replies are9804; the last is9840 and renders
PIN2 code blocked. A fresh process retains the exhausted card and the own
Access codes path asks for PUK2. Physical87654321/new8765/confirmation produces
one UNBLOCK CHV9000 and restores PIN2 retries3, without changing PUK credentials
or unrelated card state. Firmware follows successful UNBLOCK with a further
VERIFY9000; the gate requires that ordering rather than silently ignoring it.

A final cold process uses recovered8765 in an ordinary change back to5678,
returning the complete card to the original healthy seed. This tests persistent
recovery, not cached authentication. The gate pins the blocked/PUK2/changed
frames, all cold-input links and native execution in every phase.

```sh
.venv/bin/python tools/nsb1_puk2_check.py tmp/nsb1-pin-request-on-cold \
  tmp/nsb1-pin2-change-wrong tmp/nsb1-pin2-exhaust-one \
  tmp/nsb1-pin2-exhaust-zero tmp/nsb1-puk2-recovery tmp/nsb1-puk2-restored-cold
```

Wrong phases and final restore run70s; `nsb1_puk2_recover.lua` runs80s from
the saved exhausted card. No counter is changed by preparation.

The gate's `--wrong-puk-cycle WRONG RECOVER COLD` branch is independently
seeded from the saved PIN2-exhausted card. Physical wrong87654322/new8765 and
confirmation returns UNBLOCK9804 and Code error, changing only PUK2 retries
10-to9. Correct PUK2 recovery in a fresh process retains that decrement as its
input, restores retries10 and PIN2 retries3, and sets PIN2 to8765. A final cold
ordinary change authenticates8765 and restores5678, returning the complete
card to the healthy seed. All three phases retain the normal own-input,
native-execution, physical-contact, frame and complete-card checks.

```sh
.venv/bin/python tools/nsb1_puk2_check.py tmp/nsb1-pin-request-on-cold \
  tmp/nsb1-pin2-change-wrong tmp/nsb1-pin2-exhaust-one \
  tmp/nsb1-pin2-exhaust-zero tmp/nsb1-puk2-recovery tmp/nsb1-puk2-restored-cold \
  --wrong-puk-cycle tmp/nsb1-puk2-wrong tmp/nsb1-puk2-after-wrong \
  tmp/nsb1-puk2-after-wrong-cold
```

Permanent PUK2 exhaustion is independently accepted without a counter fixture.
Starting at the retained nine-retry wrong-PUK2 card, nine more isolated cold
processes physically submit the same wrong PUK2. Every run changes only that
counter. Replies remain9804 down to one remaining retry; the last decrement
to zero returns9840. The final card retains PIN2=0 and PUK2=0 with credentials,
PIN1 state, file records and request settings unchanged.

Two independent cold controls start from the zero-retry card. Own Change PIN2
plus Select stays at that menu item through45s and sends no CHANGE or UNBLOCK.
The second performs the same selection followed by Back and returns to Access
codes, proving input remains responsive. Both preserve the complete exhausted
card and pass the normal own-executable/native/input gates. This is firmware
UI refusal, not a claim that a correct PUK2 APDU was submitted by the phone.

```sh
.venv/bin/python tools/nsb1_puk2_exhaust.py mame/mame \
  tmp/nsb1-pin-request-on-cold tmp/nsb1-pin2-exhaust-zero \
  tmp/nsb1-puk2-wrong tmp/nsb1-puk2-permanent --check-only \
  --permanent-cold tmp/nsb1-puk2-permanent-select tmp/nsb1-puk2-permanent-cold
```

Without `--check-only`, a fresh output directory runs the nine80s physical
attempts and records progress after each verified transition. Check-only reads
all raw run logs, card files and qualified cold links, not the progress JSON.
The two50s cold controls use `nsb1_pin2_entry.lua` and
`nsb1_pin2_entry_back.lua`. The normal silent handset profile is not promoted.

The native compatibility configuration also accepts an explicitly synthetic
PIN-blocked card. Its initial image differs from the laboratory PIN-enabled
profile only in CHV1 retries (`0`); no handset state is changed. Physical
PUK `12345678`, new PIN `4321` and confirmation produce one `A0 2C` response
`9000`, restore CHV1/PUK1 retries to `3/10`, and persist the new PIN while
leaving CHV2/PUK1/PUK2 credentials and counters intact. A separate cold
process copies qualified own persisted devices, physically verifies `4321`
with `A0 20/9000`, then independently enters phone code `12345`.

```sh
.venv/bin/python tools/nsb1_puk_roundtrip.py mame/mame roms tmp/NEW \
  --checksum-control-from tmp/QUALIFIED_A_ERASED_RUN
```

Both processes check exact reviewed UI frames, physical contact order,
durable card contents, legal native execution and unchanged flash. This
proves successful CHV1 unblock/reauthentication, not permanently exhausted
PUK handling, registration or fitted DSP compatibility.

Add `--wrong-puk-first` for a three-process rejection/persistence control.
Physical wrong PUK `87654321` followed by the new-PIN prompts produces one
`A0 2C/9804`, leaves all credentials unchanged and persists counters
`0/3/9/10`. The reviewed failure frame says `SIM card blocked`; that wording
does not mean PUK exhaustion. A cold process copies those exact devices,
successfully enters the correct PUK and new PIN, and the final cold process
verifies the new PIN. This complete variant passes without retry resets by
the harness between processes.

### Offline SMS draft and text-resource contract

PPM B with an explicitly synthetic English preference accepts physical text
entry, Save, and a separate cold Outbox read. The two-process gate checks exact
Saved/read pixels, the unsent EF_SMS record, physical contact order, phone-code
acceptance, legal native execution, inactivity coverage and unchanged flash.
This is offline storage, not registration or network SMS acceptance.

Add `--erase` to extend the two-process draft gate through physical Options /
Erase / confirmation and a fourth cold process. The erase phase first reads
the exact stored `A ` draft, then issues one successful
`A0 DC 01 04 B0` to EF_SMS record one. **Only its status changes from `07`
to `00`; old TPDU/padding bytes remain.** All other card bytes are unchanged.
The fourth process reads the persisted free record, physically selects
Outbox, renders `No messages in outbox`, and preserves the complete card
without UPDATE RECORD. Exact read/confirmation/empty frames, native
execution, receive activity, physical phone-code input and unchanged flash
pass in all four phases. Logical deletion is not secure payload erasure.

```sh
.venv/bin/python tools/nsb1_sms_draft_roundtrip.py mame/mame roms tmp/NEW \
  --checksum-control-from tmp/QUALIFIED_B_ERASED_RUN
```

Add `--restore-editor` to save at 41.5 seconds in the populated editor,
advance 0.25 seconds and reload before physical Save. The gate requires exact
emulated time, ARM registers (including banked state), a hash of the own backed
64-KiB RAM and exposed C54x architectural state to match at save/load. It then
requires the same Saved pixels/card record and cold Outbox read. This mode
passes; it covers sampled architecture and functional continuation, not every
hidden peripheral state or native network/speech behavior.

The checksum source must be a qualified own PPM B erased-storage run produced
with `prepare(..., ppm='B')` and run using `-bios 671b`. The fixture changes
only the existing checksum/retry controls plus EEPROM preference `04c7 = 01`;
it does not supply donor storage or change identity, calibration or live RAM.

Menu shortcut `2 1 3` opens Write messages, then a blank editor. Physical
digit `2` reaches decoder `2af79e` and task 5 as packed statuses `40c8/40c9`
with argument `2`. Receiver `2abd5c` extracts the argument count and normalizes
the status to `00c8/00c9`; dispatcher `2abe10` passes it through its handler
pipeline. The observed key-down remains `00c8` through `2528c0`, then
`28e4a4` returns `057c`. Within that handler, translator `2733dc` already
returns `057c` with input-mode byte `106424 == 1`. Its digit path calls
`272f40`, then `272b70` submits operation `3`, argument `2`, to shared text
API `25e20c` with object `101c48`. A fresh read-only run observes API result
`9`: the early guard at `25e222..25e22e` reads halfword `1097e2 == 0`, not
the required `1428`, and rejects this operation before processing its text.
Operations `0`, `1` and `0a` bypass this guard; this is not a claim that all
text API operations fail.

Initializer `25c8a0` clears `1097e2` through owner `1097cc+16` and writes
`1428` only on its successful tail. The observed invocation instead returns
early after `25c4bc`: the resource-header version decoded at `25c4f8` is
`0`, not required `1`. Resource reader `27388a` copies 32 bytes from address
`30` because its selected base (`10642c`) is zero. This establishes an invalid
resource selection, not missing keypad delivery.

The acquired package independently distinguishes the resources:

| PPM | Market in restore INI | LDB chunk CPU address | Declared bytes |
| --- | --- | --- | --- |
| A | USA | `2f5144` | `24` (placeholder) |
| B | USA_T9 | `2ef9c0` | `cb24` (English database) |

These addresses/lengths come from the own big-endian PPM chunk chain beginning
at `2e0030`, not a search for arbitrary text. The normalized B input SHA-1 is
`49987333223932f13dd4fb8e71a9645497894f73`. Both `noki5190` and `nsb1r4t`
offer explicit BIOS choices `671a` (unchanged default) and `671b`. B requires
its own normalized `5190f671b.fls`; it is not renamed as A. MAME stores its
NVRAM in `nsb1r4t_1`, not `nsb1r4t`. Comparison preparation pins the selected
variant and rejects checksum-source flash/manifests from another variant.

A qualified B run with checksums derived from its own erased-storage boot
accepts the physical phone code, reaches the editor, sustains legal native
execution through 40 seconds and leaves executable flash unchanged. It still
selects base zero, reads header version zero and returns text-API result `9`.
Thus adding the full database does not by itself select it. A's exact physical
Phone book menu acceptance also passes after adding the BIOS choices.
Selector `272c70` calls the PPM lookup `2ace64` with resource tag `LDB` and
language byte `10fced == ff`; the lookup returns zero, which `272ca2` stores
to `10642c`. A read-only startup watch observes that byte first cleared by
`28bbc6`, then loaded as `ff` by EEPROM reader `2ab110` at approximately
0.197 seconds. The EEPROM read starts at `04b0`, copies record `0702`'s
`30` bytes to `10fcd6`, and therefore maps `10fced` to offset `04c7`.
B's supplied LDB declares language ID `1`. With only that external preference
changed, firmware selects `2ef9d0`, reads version `1`, sets `1428` itself and
returns success for operation `3`. No live selector or validity field is forced.

Physical digit `2`, then `0`, enters `A `; Options/Save writes EF_SMS record 1
through successful `A0 DC`, and displays Message saved. The 176-byte record
at SIM-storage offset 1636 is `070281f001ff00810000024110` followed by `ff`:
status `07` (unsent), SMS-SUBMIT with GSM-7 `A `, and unused padding. Record 2
remains free. The cold process copies only qualified own flash/EEPROM/card,
opens Outbox through physical Menu `2 1 2`, displays `A`, and leaves SMS bytes
unchanged with no record update. This does not validate the placeholder SMSC
or recipient for transmission.

Physical Dictionary selection has not established a successful language choice;
A's non-dictionary path, richer editing and network SMS remain open. The own
Options order places Dictionary before Send special and Save (option 7), not
after Exit; earlier eight-Down attempts were not dictionary-selection evidence.

The separate Spell mode accepts physical letters and renders `ADA`, but the
manual describes that mode as adding a dictionary word; returning to an empty
message is not by itself a failed text-copy contract. `tools/nsb1_sms_editor_observe.lua` is a
bounded read-only debugger instrument, not an acceptance gate; its temporary
fixed-pool read watch does not establish allocator ownership. Task 6's observed
eight-byte message is not classified as text delivery.

## Native radio and ordinary-call boundary

`tools/nsb1_call_observe.lua` is a bounded physical diagnostic, not call or
registration acceptance. On qualified PPM A checksum/retry storage it types
`5550100`, presses the Call softkey, then the same softkey labelled End at
52 seconds. Dialed digits render, the attempt presents End, and End returns
the Menu softkey. No connection or speech is established; an End-labelled
screen is not proof of network signaling. The separate C/Names contact is
not the End softkey in this context.

The passive ring trace distinguishes boot search from the later user action:

| Direction | Observed boundary |
| --- | --- |
| MCU TX | Own type `51` parameter blocks finish around 2.14 s. |
| MCU TX | Type `1a`, length 68, starts `00419800` at 4.831 s; type `4a`, length 4, is `02000000` at 4.868 s. These precede dialing. |
| Native RF | Port `27` samples observed at DSP PCs `324b/3251/3257/325d` are zero. RF-port `32` writes also begin before dialing. |
| DSP RX | Native code at `3805` publishes type `8b`, length 162, at 41.052483 s, beginning `00400328ff6f0329ff6f`. Payload units/count semantics are not imported from sibling firmware. |
| MCU RX | `28f7ea` reads the header at 41.052609 s; wrapper `28f836` feeds receive loop `2afdd6`. |
| MCU route | The `83..8f` jump table sends `8b` to `2afe54` then `284222`, which rewrites the status to `13b8`; runtime confirms task `0c` post through `268944`, caller `28423b`, header `13b8a28b`. |

No later TX-ring commit is observed from the physical Call/End actions in
the 61-second interval; native execution remains legal, rings drain, the
inactivity check is nonzero and flash remains unchanged. This does not close
all other command paths or classify the call controller's internal lifecycle.
Task 12 consumes the status at `20f534`. Its own subtract/jump-table path
(`13a3` base, `13b8` index 14) selects `20f63c`. Runtime enters that branch
with controller state `106a9c == 3` and phase `106aa1 == 1`. It copies 160
bytes from message `+6` into a cached object at `20f67e..20f68a`; at `20f6b8`
state 3 then bypasses candidate processing. Candidate routine `213a28` is not
entered in the observed interval. The result is **deferred**, not measured as
an empty or usable candidate set by this controller path.

The state watch sees bootstrap clearing, followed by `20f510` explicitly
initializing state 3 at 0.997345 seconds; no further state write is observed
through 61 seconds, including physical Call/End. State 3 is therefore the
firmware's retained initial lifecycle, not a fabricated native-DSP reply.
The open software question is what ordinary input releases/changes it and
consumes the cached vector. One mapped dispatcher entrance is status `13a3`
at `20f8d6`, which passes message byte `+2` to `2116fc`; its runtime role and
the full state-writer surface remain unclosed. Statuses `13a5..13a7` instead
enter `210bd4`, which is also unobserved in this run. Do not inject these inputs
or skip state 3 to manufacture acquisition.

The local writer/activation inventory is:

| Store | Value / owner |
| --- | --- |
| `20f510` | Task-12 initialization sets 3. |
| `213942` | Initializer `2138ec` sets 1 and copies its input to byte `+15`; under further gates it can consume the cached vector at `213a02`. |
| `2143fc` | Reset/defer routine `2143e0` sets 3. |
| `214708` | Scan progression in `21460c` sets 2. |

Scope: the aligned-literal scan finds 16 pools for owner `106a88` and six
for direct state byte `106a9c`; all 13 PC-relative loads of the latter are
classified, including the two direct stores above. The `20f000..214fff`
immediate member-store scan has eight `STRB [base,+14]` candidates: two own
controller writes and six stores belonging to other message/per-cell owners.
This does not close computed aliases, generic structure copies or indirect
callers across the whole image. The fresh runtime watch remains the check
for observed changes outside the classified sites.

Initializer `2138ec` has two direct Thumb-BL callers in the aligned image
scan: `20faba` and `21179c`. The first belongs to task-12 status `043f`
at `20fa82`; when state is 3 it calls the initializer with message byte `+2`.
The independently mapped constructor is `28ed24(043f)`: case `28edce` copies
bytes `10c435/10c434` into message `+2/+3`, then explicitly posts to task 12
via `268944`. Its literal caller `24dda2` is inside handler `24dd5c` for
task-17 status `0834`. Upstream, `20eb90` supplies `0834` to constructor
`208dd4`, whose case `209184` fills two control bytes and posts to task 17
at `209236`. These are own-ROM routes, not imported task aliases.

The 61-second physical Call/End run observes neither `24dd5c`, the `043f`
constructor nor initializer `2138ec`; the probe stream is uncapped by other
events in that run. The semantics and ordinary provisioning of the two
control-record bytes remain unproved.

### Activation predecessor and current wait

Controller 16 input `0a30` selects the source call at `20b2be -> 20eb8a`.
The adjacent `MOV #a3; LSL #4` scan finds six producer calls to constructor
`21ac4c` (at `21c774/21c9d0/21d684/21f924/21fe06/21ff8e`), whose `0a30`
branch explicitly posts to task 16. This is a bounded instruction-pattern
inventory, not closure of all data-driven or indirect producers. None fires
in the measured run.

The receive traces instead establish this earlier transaction:

| Owner | Measured input / continuation |
| --- | --- |
| Controller 16 | Receives `0a35`, `0a25`, `09ec`, `09d2`, `09d4`. State at `10a688` changes `3 -> 6 -> 19` (hex); the call/end headers become helper results `0a02/0a05`. |
| Helper `21ae1c` | Receives `158b` with argument zero, saved caller `21da03`; this matches the expected result at `21da04`, not the earlier guessed initial `09f0` wait. |
| Subsystem flow | `21da10 -> 21e5d0 -> 21b3f6` returns; the comparison at `21e610` takes the `>=012b` branch `21e6c8`. `21b1e8`, `29c208` and `29c1f2` also return. Constructor `21ac4c(09ec)` at `21eeea` publishes argument 1, then `21eeee` waits in the receive helper. |
| Controller 16 | State 6 handles `09ec` at `20e464`, forwards `07d6` through `208dd4`, and retains state 6 until physical Call. |
| Controller 17 | Receives `083b`, then `07d6`; state `109fae` changes `2 -> 1d` (hex). In state `1d`, `24d9ee -> 24da90` constructs `03e9` through `28ed24`, targeting task 11. Cleanup `28ebde` returns, then `24dcc8` enters `24f044`. |

`24f044` calls receive helper `28f224` at `24f050`. It explicitly accepts
`0811/0812/0813` at `24f058..24f06a`; other inputs go through `24e30c`, whose
exception/continuation surface is not closed. Neither these replies nor the
post-call site `24dccc` is observed through 61 seconds. **The next question is
task 11's own `03e9` consumer and the legitimate producer of its completion**,
not a fabricated `0a30`, a search-controller byte or a guessed DSP packet.

The task-11 destination is runtime-confirmed: `268944` posts object `102a48`
with header `03e90100` to task `0b`. The same object reaches consumer `219c60`
and its argument-1 branch `219e52`. That branch calls `29c6b4` and returns to
`219e64`, so that call is not the stalled boundary. Follow the subsequent
consumer continuation to identify its legitimate completion; the meaning of
its returned value `5` is not yet established. The 61-second observation run
retains native receive activity, security acceptance and unchanged flash.
The continuation reaches `219ea8`, `219ec0`, `219eda`, then `21a1bc` and
the handler epilogue `21a22e`. Thus the request handler returns normally:
`21a1bc -> 21a1d4` calls `2188f0(2)`. No `0811..0813` post through `268944`
is observed in that run. The next boundary is the asynchronous work initiated
by `2188f0`, not a blocked call inside the `03e9` handler. This post-site watch
does not exclude alternate delivery primitives or unobserved later replies.

`2188f0(2)` is the measured producer of the boot-time DSP type-`1a` request:
at `218b34` it posts to task 3, object `101b4c`, header `0002441a`, body prefix
`00419800`, bitmap count `012b`. The TX ring subsequently contains the same
68-byte body at 4.831431385 seconds. Static construction at `218992..2189a4`
sets class `0002`, length `44`, type `1a`; `218b18` supplies payload byte `98`.
This connects the pending controller transaction to existing native traffic,
but does not prove that this packet requires a direct DSP reply or that the
later task-12 `8b` result completes task 11. The completion contract remains
open; neither a guessed reply nor a controller-state change is justified.

The aligned literal scan finds `0811/0812/0813` outside the controller wait
in `2516e8/251794/251798`, loaded at `251392/25149c/251574`. These select
`r6`, which the common tail `251772` passes to `27fcda` and returns in `r0`.
The reporting helper is gated by `2adc10(3400)`; it is not evidence of an
unconditional task-17 sender. Neither the common tail nor `27fcda` with these
three statuses fires in the 61-second physical probe. Two more literals at
`2803fc/280400` feed `280204/280208` in a separately gated reporting mapper.
This is an aligned-literal/reference inventory, not closure of computed
status producers.

The selector's owner is now statically bounded: `2505d0` is the enclosing
routine, and an aligned Thumb BL scan over the complete 1-MiB MCU image finds
one direct caller, `28f318`, inside controller 17's own receive helper.
At `28f2e4` the helper loads a nested buffer from message offset 8, tests the
low nibble of buffer byte 4 for `6`, and calls the selector with the outer
message byte 3 as its second argument. The selector dispatches on nested
buffer byte 5; the `24` branch reaches `2512b0`, including the `0813` selector.
Returned status goes into `r6` at `28f31c`, then the common receive tail
returns nonzero `r6` at `28f580`. These are decoded-input results, not proof
of task-11 acknowledgement messages. The next query is which outer message
and nested buffer legitimately reach this parser; do not synthesize the
completion IDs to skip the receive transaction. The outer receive cascade
at `28f48c` loads `0447` from literal `28f62c`, subtracts `019f` and then
four single steps; the final `<=1` comparison at `28f4ba` selects `05ea`
and `05eb` for `28f2e4`. Raw occurrences of those constants elsewhere also
serve unrelated return/report namespaces, so they are not producer evidence
without a destination and nested-buffer contract.

Constructor `2405e4` satisfies that contract. It allocates a 12-byte outer
message, stores its status at offset 0, and posts to task `11` (hex) at
`24075e`. For `05eb`, branch `240684` allocates the nested buffer, copies
payload from owner `+0114` and length from `+012b`, and stores its pointer at
outer `+8`. For `05ea`, branch `2406f6` copies owner `+18` using length
`+0113`, again behind a four-byte nested header. Its callers include
`240c2a` (`05eb`) and `241ccc/24206a` (`05ea`). The latter advance the
owner's byte `+b` modulo 8 before publication. These are byte-layout and
control-flow observations, not a completed protocol identification.
Neither this constructor (any status) nor the nested-buffer consumer
`28f2e4` fires during the 61-second physical Call/End run. Native execution,
security acceptance, RX activity and unchanged flash still pass. Follow
the receive-side input of this owner rather than injecting a buffer or
assuming that the cached `8b` search result is sufficient for activation.

The enclosing server starts at `241040` and initializes task id `0e` at
`241166`. Runtime confirms its entry, receive helper `240a6c`, and queue
call `240a86 -> 268b98`; the return at `240a8a` is not observed through
61 seconds. Thus this is a live server waiting for input, not an unstarted
task or a demonstrated payload rejection. Main loop `2429ba` consumes the
helper's returned event. The helper routes ordinary messages through
`240438`; its subtract cascade starts at literal `05dd` (`2407a4`), with
other special inputs handled before that path. The next producer query is
task-14 input delivery, upstream of `2405e4`, not fabrication of a task-17
completion. No receive-path closure or fitted RF compatibility is claimed.

One native ingress to that server is statically identified:

| Boundary | Own-ROM evidence |
| --- | --- |
| Native packet dispatch | `2afdea..2afdf0` selects type `80`, then `2afe94/2afe96` calls `283d1c`. An aligned Thumb BL scan finds only that direct caller. |
| MCU data decoder | Conditional path `283f26` loads `040b` from literal `2842d0`, stores it in the object, copies a discriminator to byte `+b`, then `283f56` posts to task `0e`. Decoder gates before publication remain significant. |
| Server receive | `240a98` loads special-input base `040b` from `240dac`; equality enters `240bec`, calls discriminator reader `240764`, then processes the object through `2407b8`. |

This is distinct from native `8b -> 13b8 -> task 12`. It identifies a
possible data-bearing entrance, not evidence that any fabricated type-80
body would be accepted or that the current compatibility DSP/RF model can
produce valid received radio data.
The 61-second physical probe observes neither decoder entry `283d1c`,
task-14 posts through `268944`, nor a return from its receive call. Its
native receive stream still includes `8b`; execution/security/unchanged-flash
checks pass. Next inspect the DSP type-80 publisher and its RF/codec input
requirements, rather than inserting an MCU-side completion.

The same resident image's receive prerequisites are already mapped in
[the ROM4 integration contract](djr_dsp_integration.md): selector producers
and operation routine `4c6f` ultimately depend on port-`27` sample processing
and its activation/scheduling. A passive NSB-1 count at 60 seconds measures
ring commit site `3805` 15 times (positive observation control), but `4c6f`,
decode entry `4ef7`, and type-80 publishers `4f1c/50d5` zero times. Data
`194f/1950/11fd` are all zero at that point. The byte-pinned resident contract
and native/security/receive-activity/unchanged-flash checks pass. Thus the
current failure precedes receive-operation/decode execution; it is not
evidence of a successfully decoded block lost at the MCU transport. Separate
unbound zero RF samples from activation prerequisites before designing a
software RF fixture; arbitrary samples or manufactured type-80 packets
would not demonstrate native reception.

Own uploaded scan code must be relocated independently of NSE-1: the live
`0ac7..0b20` capture places the `CALL 4701` at `0ade`, not NSE-1's `0acf`.
Return `0ae0` begins `6ff8 219b 0d20`; the subsequent delayed conditional
branch targets `0b02`, with another comparison against `198d` before the
accepted-value store. Probes at the borrowed uploaded addresses are invalid
for NSB-1; only the relocated counts below are used.

At 60 seconds own callsite `0ade` executes 4059 times, with 4058 completed
returns at `0ae0`; **all 4058 return A=0**. Four bounded operand snapshots
show threshold `219b=0574` and prior `198d=0`. Frame control `33f3` executes
34131 times; sample processor `4701` runs, while resident acquisition tail
`46e1`, channel routine `5e62`, configuration `5514` and scheduler `98b7`
do not. Channel/mode/callback cells `1835/1836/07fb` finish zero. This
reproduces the known zero-input result boundary with own uploads and fresh
storage, rather than proving a scheduler bug or a physical signal unit.
The next useful native experiment needs an independently justified RF sample
source/format and activation contract, not adjustment of threshold `0574`
or the zero selectors. The observer asserts the relocated instruction words
when those probe sites execute to catch future upload/address drift.

### Software RF source contract

The air-interface waveform and the handset sample interface are separate
contracts. [ETSI TS 145 004, section 2](https://www.etsi.org/deliver/etsi_ts/145000_145099/145004/18.00.00_60/ts_145004v180000p.pdf)
specifies GMSK at `1625/6` ksymbols/s, differential encoding, `BT=0.3`
filtering and modulation index `1/2`. This grounds waveform generation;
it does not identify COBBA sample-word packing or clocking.

| Required fixture input | Current evidence / remaining work |
| --- | --- |
| Sample arithmetic | Shared impulse/mixed-word tests establish signed 16-bit filter arithmetic, not the physical ADC resolution or alignment. |
| Component order | Four successive port-27 reads feed two filter phases. Establish their time/component interpretation before calling a buffer standard I/Q. |
| Sample cadence | Current callback advances on reads. Derive sample-time progression from the active frame/read schedule, not the calibrated DSP execution clock alone. |
| Receive window | Correlate RF control writes and queued sample processing with the requested channel/window; do not stream a waveform independently of tuning. |
| Acceptance | First require an independently predicted filter/score result, then organic acquisition/decode and MCU delivery. Merely exceeding `0574` is not proof of GSM reception. |

An own-model [NSB-1 system-manual listing](https://www.eserviceinfo.com/downloadsm/233124/NOKIA_sysa3.html)
identifies `sysa3.pdf`, System Module UR4U; the full PDF has not been
retrieved or verified. Sibling COBBA manuals are leads, not permission to
import their bus width, clocks or ADC assumptions into NSB-1. No amplitude
sweep, threshold change or synthetic decoded packet resolves these missing
sample-interface facts.

The passive 60-second read census observes four distinct port-27 consumer
groups: `324b/3251/3257/325d` each 144664 reads,
`32cf/32d2/32d5/32d8` each 95736, filter
`330c/3312/3318/331e` each 32616, and filtered-pointer path
`33cb/33d1/33d7/33dd` each 32. Reads inside a group can be separated by
roughly 115 ns of CPU execution. With an explicitly diagnostic 1-us
inactivity grouping rule, the first consumer forms 18083 processing bursts
(minimum start spacing 1.247058 ms), while the filter forms 4077 bursts
(minimum 4.615308 ms, maximum 36.337119633 s including lifecycle gaps).
These are processing schedules, not physical sample rates. In particular,
a source that samples a continuous waveform at each instantaneous callback
time would conflate block consumption with acquisition. Recover component
ordering and buffered sample-time progression before wiring such a source;
do not derive either from the minimum read spacing or these diagnostic gaps.

Live DSP disassembly confirms the shared resident reader contract on NSB-1:
`322a` can bypass mode dispatch while the `00ac` countdown is nonzero;
otherwise `00ac == 1` selects `32f4`, `== 2` selects `33b4`, and other
values take the `3236` path. Direct reader `32c5` uses BK=16, AR4=`0080`,
and eight iterations of four port reads into a circular history buffer.
The mode-1 filter reads 32 words and writes 16 intermediate words at `1a48`
through paired A/B accumulators. Its later `06fd`-selected reduction has
separate signed-add/subtract paths before the `06b7` queue publication.
This establishes software ordering and buffer ownership, not an I/Q label,
physical oversampling ratio or ADC resolution. Existing byte contracts
`receive_holdoff_before_mode_dispatch`, `receive_holdoff_history_and_commit`
and `receive_scan_filter_word_consumption` cover these exact instructions;
the real resident image and all 45 codec-contract tests pass. Do not add
duplicate assertions or reuse NSE-1 uploaded addresses to express this
shared resident behavior.

Field `10af4c == 012b` is not established as a retry limit or direct EEPROM
integer. `21b37c` counts 299 bitmap positions in a 48-byte structure; reset
routine `21b1e8` sets the field to 299. The measured storage/update path returns
before the `09ec` publication. Changing that field to bypass the branch would
not be an evidenced provisioning correction. Resolved storage/branch trace
sites are retired from the live diagnostic; the receive and activation taps
remain for the open completion contract.

The unreached candidate routine statically loops over 40 four-byte records,
examines signed message byte `+9 + 4*i`, and counts values greater than `-106`.
That is an own-ROM byte/threshold contract, not validated physical RSSI units,
nor evidence that the observed vector passed or failed it. No classification
may be inferred solely by treating payload `ff6f` as a signed 16-bit dBm value.

The handset currently leaves COBBA's RF sample callback unbound, using its
explicit zero default. There is no received laboratory cell waveform on this
native path. Reaching this result therefore does not establish a broken
mailbox or a missing HLE acknowledgement. Usable cell acquisition requires
an evidenced RF sample/timing contract or a separately labelled HLE backend;
neither may be replaced by injected MCU registration flags. The ring observer
suppresses its own header reads from the MCU-consumer watch. Detailed taps
have a retirement trigger once search-result ownership is accepted.

## Qualified executable input

Source: [archived Nokia service package](https://archive.org/download/Nokia_DCT3_firmwares/5190e671.zip).
Archive SHA-256:
`7c7167aba12342fa1ef3810c93bf518b0f8aeb79f67e9637e19d76b34e781128`.
The acquisition manifest is `roms/archive-dct3-packages/manifest.json`.

| Stream | Supplied CPU span | Decoded SHA-1 |
| --- | --- | --- |
| `nsb1___6.710` MCU | `200000..2d2cff` | `f953fd19093f17f36845cf46417f7ddd750ace80` |
| `nsb1___6.71a` PPM A | `2e0000..2fffff` | `0c55c1825cb722872c18e10a714f4be27cae3871` |
| `nsb1___6.71b` PPM B | `2e0000..2fffff` | `8c3aa2858a3bc82cda1644a81d03263983d1c491` |

All three independently contain `V  6.71`, `25-01-01`, `NSB-1`, `(c) NMP.`.
The restore INI names MCU/PPM ROM4 files and erase span `200000..2fffff`;
USA selects A, USA_T9 selects B. A ROM4 filename is not a resident DSP dump
or proof that the recovered NSE-1 mask fits this product.

`tools/nsb1_package_check.py` pins the archive, closes its six-member
inventory, validates the restore map, strictly decodes all three record
streams and checks embedded PPM identities. Normalization writes a 1 MiB
flash view with the **unsupplied** `2d2d00..2dffff` gap erased. This gap is
not acquired data or calibrated storage. No external EEPROM, identity,
security state or internal boot/DSP mask is supplied or synthesized.

```sh
.venv/bin/python tools/nsb1_package_check.py \
  roms/archive-dct3-packages/5190e671.zip \
  --ppm A --output roms/noki5190/5190f671a.fls
```

The normalized A SHA-1 is
`27b8f0cb8640e5b423960b206e9aee065f32d726`. Omitting `--output` is read-only.
The checker rejects source overwrite and never extracts archive paths.

### Executable integrity and controller mode

Own routine `294e70` reads the stored big-endian halfword at `200022` and
at most two six-byte `{24-bit start,24-bit inclusive end}` entries beginning
at `200024`; start `ffffff` terminates the list. The qualified input declares
`200024..2bffff`, followed by a sentinel. Its big-endian halfword sum modulo
65536 is `8016`, exactly the stored value. The normalizer now independently
checks this grammar/sum. This covers that declared range, **not whole-flash
authentication**, and requires no executable-byte repair.

Passive observation of byte `10c4c4` identifies initialization from zero to
2 at store `2a438e`. Own initializer `2a42c6` reads ADC selector 3 via
`2b1730`; after an earlier special-case window, a sample below ROM threshold
`3fa` takes this store. The configured selector-3 input is `280`. Getter
`2a46ac` consequently returns 2 in the startup controller. Static decode and
positive debugger observations show this continuation still executes the
flash checksum and SIM initialization. It is not evidence that mode 2 is
an erroneous boot state, nor justification for adjusting an ADC value to
select a different branch. Physical pack semantics remain unproved.

Diagnostic instruction probes must validate debugger-console errors as well
as the emulation log: accepted breakpoint installation does not validate its
action expressions. This ARM debugger exposes the link register as `r14`,
not `lr`; an invalid action must not become a false "function never ran"
conclusion. The retained normal observer uses passive RAM/MMIO taps instead.

## Own EEPROM initializer

Routine `2ab168` reads ROM byte `200005` through its literal at `2ab1dc`.
The own source byte is `f4`. It constructs the runtime descriptor at
`10c4a4`: byte 0 is `(f4 >> 6) & 1 = 1`, byte 1 is
`1 << (((f4 >> 3) & 7) - 1) = 32`, and word 4 is
`1 << ((f4 & 7) + 9) = 8192`. This establishes the firmware-declared
two-byte word addressing, 32-byte write page and 8 KiB capacity; it does
not independently identify the fitted EEPROM part.

Read helper `2ab0e6` calls address setup `2aaf58`. The latter checks the
address against descriptor word 4, selects slave byte `a0` for nonzero
descriptor byte 0, sends `address >> 8` at `2aafa8`, then sends its low byte
at `2aafb4`, and issues repeated START/read slave `a1` for reads.
Its zero-mode branch instead folds high address bits into the slave byte.
Consequently a one-byte-addressed 24C16 fixture is not faithful to this
initializer, even if its donor contents happen to satisfy a checksum.

Validator `26cc4c` reads `120` bytes, calls byte-sum routine `2a0fbc` over
the first `11c`, and compares the resulting 16-bit sum with the big-endian
32-bit field at `11c`. This confirms one external checksum clue without
supplying the absent calibration/identity contents. Routine `239da4`
independently reads `120..1fd`, sums its bytes modulo 65536, then subtracts
the **two individual bytes** returned by the `154/155` reader `295bcc`.
It compares that value with the big-endian u16 at `1fe`. This excludes the
adjustment bytes from the sum; it is not subtraction of their numeric u16.

## MCU research configuration

The own ARM entry at `200040` initializes stacks using `10fc54` and
switches to Thumb at `2000e8`. The trial maps only the minimum 64 KiB SRAM
window covering these stacks, not the generic driver's larger window;
physical fitted capacity and aliases remain unproved. Flash uses a
capacity-compatible 1 MiB trial part. The external I2C device implements
the own 8 KiB/32-byte-page protocol, with erased initial contents and an
unmeasured 5 ms write cycle. Boot/DSP masks and EEPROM dump remain separate
`NO_DUMP` entries. Shared MCU boot-transfer HLE remains explicit; no DSP
acknowledgement, service or radio peer is enabled. The own-mapped SIMI
controller is present with an empty socket; no synthetic card is inserted.

Own serial routine `2ad4b0` selects control `22` at `20028`, sends at
`2002a`, polls `20029` bit 2 and reads `2002d`. LCD clear `2ae0ac` shares
selection/status at `28/29`, command `2c`, data `2b`, six banks and 84 columns.
EEPROM byte transfer `2ac704` independently drives PUP signal `20020` bit 0
as SDA and bit 2 as SCL, releasing SDA through direction `20024` bit 0.
These port-level contracts justify the existing device composition, not
measured serial timing or fitted controller identification.

Key scanner `2af050` uses five rows, signal `31`, direction `2f`, columns
`30` and mask `33`. Decoder `2af768` selects normal table `2d2984` and
special table `2d29a0` using selector byte `10c466`. The normal table matches
the existing NSE-1 host layout; the active special table is
`3e 3e 3e 3e 0d`, selecting Power on bit 4, not bit 1. These are independently
mapped source facts; physical input/UI delivery has not yet been accepted.

### Full-scale ADC2 negative control

The observations in this subsection use the full-scale ADC2 value `3ff`,
not the compatibility instrument's current nominal 554 input. They explain
the rejected acquisition, not a still-missing event under nominal inputs.

A bounded physical Menu test on the checksum-only native compatibility
configuration holds row 2/column 1 from 3.0 to 3.25 seconds. Passive reads
of `20030..20033` show 11 accesses, all before 0.281 seconds; scanner
`2af050` is entered at startup, but neither that entry nor another keypad
register read follows the held contact through five seconds. Debugger action
expressions were checked for console errors. This does **not** demonstrate
decoded Menu delivery or application rejection: interrupt enable/routing and
scanner scheduling remain to be established for this lifecycle. A lack of
repaint alone cannot close the input path.

The subsequent routing observation identifies a firmware-owned local mask:
routine `2af7a0`, entered with return address `26f3db`, ORs `1f` into
column-mask register `20033` at store `2af7c6` (0.280282 seconds). Samples
before, during and after the Menu hold retain mask `3f`, row signal `e0`
and row direction `1f`. MAD2 pending IRQ remains zero, IRQ mask is `ce`,
global control `05`, and CPSR interrupt-disable bits are clear. Thus this
test is stopped by masked column-change detection before CPU interrupt
delivery, not evidence of a broken MAD2 IRQ route. The firmware continuation
that clears this mask remains unresolved; do not clear it diagnostically and
claim ordinary input support. Register-observation reads in this second run
are distinct from firmware reads and must not be counted as scanner activity.

An aligned Thumb literal-load scan for the exact `20033` address finds four
candidate unmask continuations: stores `26eb08`, `26f33e`, `26f4fc`, and
`26f7a2` AND the register with `e0`, clearing all five column-mask bits.
The fifth matching literal-load site belongs to the masking routine above.
This is coverage of direct PC-relative loads of that address, not a proof
that computed/indirect writers do not exist. None of the four candidate
unmask stores executes in the five-second checksum-control observation.
The startup receive call at `26f3f8` nevertheless returns repeatedly at
`26f3fc`, with observed values `37`, `c8`, `33`, `ca`, and `32`; its
receive loop is not wholly dormant. The remaining question is which
firmware transition reaches a legitimate input-enable continuation, not
whether to synthesize a key IRQ through the existing column mask.

The native checksum-control run extended to 30 seconds narrows the
`26f4aa..26f4be` continuation: byte `10ffb0` reaches `06` at store
`2aa942` around 1.249 seconds, satisfying its low-nibble comparison with 6.
The other byte, `10c3f5`, accumulates `08 -> 0a -> 0e` at `26f420` and
remains `0e` at 10, 20 and 29 seconds, short of the required low nibble `f`.
Positive instruction observations associate those accumulated bits with
logical inputs `17`, `16` and `15`. The bit-0 branch at `26f42e` requires
logical input `14`; it is not reached in the observed interval.

Own wrapper `2aaae8` calls the existing helper `2b1ee0`, then posts with
`r0=1,r1=14` through `268a94` at `2aaaf6`. An aligned classic Thumb BL scan
finds one direct-call candidate, `221f08`, after call `276fa8` and a flag
OR with `40`. This does not close indirect/data-driven callers or establish
the semantic owner of that routine. The next boundary is this report's
organic producer lifecycle, not a missing column IRQ or an arbitrary delay
before pressing Menu. No report or readiness byte has been injected.

The own image contains diagnostic state strings passed through `2afd0a`.
A bounded 30-second instruction trace of that helper records these positive
labels in the `221000..2253ff` region: `CHECK TEST MODE`, `CHECK POWER ON
REASON`, `INIT CHARGING`, `DO INIT CHARGING`, periodic connection/mode checks,
then `BOOT UP CHARGE`, `INITIALIZE`, `WAIT CHARGER VOLTAGE SETTING`, and
`CHARGER DISCONNECTED`. Their source string addresses are respectively
`2240c4`, `2241c4`, `2241ec`, `2241fc`, `223c34`/`224a74`, `224380`,
`2243cc`, `224390`, and `2243b0`. No report-14 wrapper entry is observed;
the candidate blocks `221e88`, `221efc`, and `221f08` are also unobserved.
This ties the current investigation to a positively identified charging
lifecycle, but does not prove which physical input should select a different
path. Generic ADC inputs and power-on/CCONT status semantics remain candidate
causes, not established defects. Some other diagnostic calls point at binary
or empty data rather than plain ASCII; do not assign names by treating every
helper argument as a zero-terminated string.

Getter `2a42a0` reads byte `10ff6d`. Its observed initializer is store
`2a456e`, which writes `0a` at 0.004768 seconds; no subsequent write is
observed through 30 seconds. In routine `2a4522`, the literal at `2a4734`
addresses MAD2 reset register `20001`. Its bit-0-set branch at `2a4528`
selects this `0a` store. Thus this selector is not a direct interpretation
of CCONT's charger-reset latch. The current MAD2 cold-reset value is `01`;
the observed branch is consistent with that input and does not justify
changing CCONT reset causes or clearing the MAD2 power-up indication.
Charging initialization and its report completion still need tracing from
this cold-reset lifecycle.

The subsequent boot-policy selector `2aa98e` takes its cold-reset `0a`
branch at `2aa9d2`. Positive probes observe ADC selector 3 returning `280`,
the scanner returning `ff` at `2aaa1a`, and packed serial read `90ff`
returning `03` at `2aaa24`. The selector then returns 2 at `2aaa48`.
Its shift/carry decisions test status bit 2 with `LSRS #3` and bit 1 with
`LSRS #2`; they do not test bits 3 and 2. Thus the return 2 follows the
observed bit-1-set status, not a nonzero charger ADC measurement. This
disambiguates the boot-policy result from the separately stored ADC-derived
controller mode at `10c4c4`, which also happens to be 2 in this fixture.
Neither equality alone establishes a shared semantic meaning.

Receive wrapper `220384` passes raw event `21` unchanged; the state dispatch
at `221b16..221b1a` directs it to candidate block `221e88`. Two direct
task-`14`, event-`21` publishers are `2232b0` and `2237bc`. Neither publisher
nor the report wrapper is observed in the bounded 30-second trace. The first
64 raw receive observations instead contain `b1`, `4c`, `174`, `175`, `176`
and `179`; that capped list is not exhaustive absence evidence for later
receives. It does establish live periodic activity, rather than a wholly
dormant charging task.

Reproduce the wider direct-post census with `tools/nsb1_package_check.py
roms/archive-dct3-packages/5190e671.zip --post-census`. It reuses the shared
swap16/Thumb argument-recovery machinery after source qualification: 115
direct calls to `268a94`, 84 with both task/event arguments recovered, 31
with incomplete arguments. The two event-`21` candidates above both resolve
to task `14`; no resolved event-`21` call has an unknown destination. This
does not close those 31 calls, other transport APIs, indirect calls, or
table-driven production. The JSON explicitly sets `absence_proved=false`.

Of the 31 incomplete calls, 30 have known destinations other than task `14`.
The remaining call `2a17be` routes an object from `r5`, matching its byte 6
against five eight-byte entries at `2ced48`: destination words are `2`, `1`,
`17`, callback `2b2891`, and `8`. Values below `18` select task delivery;
larger values are called as functions. None directly selects task `14`.
The same optional census additionally reports 28 direct calls to `269200`
(14 with both arguments recovered) and 376 to `268944` (83 with both).
Every destination in these additional calls is recovered; the only task-14
call among them is `2a4f20`, with scalar `30`. No additional recovered
scalar `21` candidate appears. Object pointers are not semantically treated
as event IDs, and indirect/tail-call/other-API coverage remains open.

A single host charger-contact diagnostic connects at 3 seconds and removes
at 6 seconds, using the existing configured connected raw value `3ff`
(physical voltage not validated). It still observes `{phase=06,reports=0e,
column_mask=3f}` at 10 seconds and a white 84x48 raster; neither direct
event-21 publisher nor report-14 wrapper executes through 30 seconds. This
rules out that specific contact sequence as an unblock, not the whole
charging/ADC hypothesis. The source reader `2a3686` explicitly samples ADC
selector 5 and applies calibration before returning a charger measurement;
its calibration/units require independent validation before changing inputs.

The charging state dispatcher at `224836` indexes 30 word targets at
`22484c` using halfword `10b9a2` (structure base `10b96c`, offset `36`).
A passive changed-value watch observes `0000 -> 0018` during initialization,
`0018 -> 001a` at `221aba` around 0.284 seconds, `001a -> 0004` at
`2219f2` around 3.697 seconds, then `0004 -> 0003` at `221aba` around
5.183 seconds. State 3 remains at 29 seconds. Table entries 24, 26, 4 and
3 point respectively to `223d3c`, `223fb6`, `224064`, and `224114`, matching
the positively observed initialization/boot-charge/voltage-wait/disconnected
labels. The state-3 handler tests translated events `49` and `41` before
its ordinary continuation. This proves initialization progresses to a
disconnected waiting state; it does not yet identify the missing report's
ordering dependency or establish that another ADC tuple would be correct.

The reusable observer records `nsb1_input_readiness` at all five normal
sample times. This full-scale negative control has five-second boundary
`{startup_reason=0a, phase=06, reports=0e, column_mask=3f}`. The current
nominal-input diagnostic below has a separately reviewed boundary.

### Acquisition rejection before readiness

Periodic logical event `26` reaches accumulator `2760a4`, which calls
acquisition helper `275fd0`. Its success branch alone decrements the
remaining-sample byte `10a0fe` from its initial ten and accumulates the two
samples. Both transformed halfwords must lie in inclusive `0708..157c`
(1800..5500). A checksum-control native trace observes 16 rejected attempts
at `2760b2`, all returning zero with both samples `1a2c` (6700), while
remaining stays `0a`. Debugger-console validation reports no action errors.

Own source reader `2ae342` selects table `2d29a8`, whose ten bytes are
`00 04 05 06 07 03 01 02 02 01`; source 7 uses ADC channel 2 via `2b1730`.
Helper `275fd0` applies calibration gain/offset at `10fe50/10fe54` and then
the own constants 1500 and 229. Observed gain bits are `3f800000` (1.0),
offset zero; the configured full-scale input 1023 gives integer result
`1023*1500/229 = 6700`, matching the rejected sample. The reusable passive
observer records the count and calibration at its normal sample times.
This identifies a concrete invalid acquisition under the placeholder tuple,
not by itself physical voltage units or a valid NSB-1 pack input. No timer,
report or acquisition result should be synthesized.

### Nominal voltage comparison

The original [5190 owner manual](https://altehandys.de/downloads/man-no-5190.pdf)
specifies 3.6 V DC in Technical Data. The retained PDF is
`roms/reference-docs/nsb1/nokia-5190-owner-manual.pdf`, SHA-256
`8245c8a69d31989d9261d941fd1ae9b9396fde4de10c64dfd741b761583a294b`.
The CCONT typical VBAT transfer, documented in Nokia's NSE-8/9 service manual
Table 30, is 6.5 mV/bit (see `8810_bringup.md` for the silicon reference,
not its board routing). `round(3600/6.5)=554` supplies one nominal experiment,
independent of whether the firmware boots. Only `nsb1r4t` changes ADC2;
the ordinary `noki5190` configuration and other inputs remain unchanged.
Fitted NSB-1 board routing and per-unit calibration are not promoted.

With checksum-only storage, ten positive acquisitions produce both samples
`0e2c` (3628) with pre-decrement counts `0a..01`. The real continuation
executes `221efc`, `221f08` and report wrapper `2aaae8`, with no synthetic
event `21`: the periodic event-26 acquisition lifecycle itself completes.
By two seconds readiness is `0f`, phase is `03`, and column mask is `20`.
Physical Menu from 3.0 to 3.25 seconds now produces scanner-register reads
(77 total including bounded observer snapshots), unlike the full-scale
control. Positive probes at decoder return `2af79e` additionally observe
logical key `19` six times during that hold. Application navigation remains
unaccepted. The five-second PNG's small right-edge feature is an early
transient, not the settled boot frame.

The nominal-input LCD stream contains 116 commands and 3054 data bytes
through five seconds. An independent replay of all 3170 selection-21 bus
writes, including address-mode/bank/column changes and zero bytes, reconstructs
the observed 84x48 PNG with **zero differing pixels** and 28 black pixels.
Firmware sends glyph-like nonzero data around 1.583 seconds, then issues
further full-bank clear/init traffic around 1.584 and 1.595 seconds. The
final sparse raster is therefore explained by the captured firmware traffic,
not a demonstrated dropped-pixel or addressing defect in the LCD device.
Transfer `261600` reads framebuffer `107a00` and, when selected, ANDs away
mask buffer `107808`; at five seconds the former contains seven nonzero
bytes/28 set bits and the latter is all zero. This is a genuine sparse RAM
frame, not an overlay hiding text. Rectangle renderer `260f80` clears central
text, title and softkey areas under callers `247403` and `247673` before
the later repaint. It must not be patched to preserve that transient text.

An unattended run reaches Insert SIM card by ten seconds and retains exact
identical RGB bytes at 10, 20 and 29 seconds: SHA-256
`a879c658a532b6526d3343287b42848f8a16bf10b7215086c1e215db45ee67cc`,
84x48, 250 black pixels. Run the checksum comparison for at least 12 seconds
to require the late readiness observation and exact ten-second frame:

```sh
.venv/bin/python tools/nsb1_rom4_compare.py mame/mame roms \
  tmp/nsb1-empty-socket --seconds 12 \
  --checksum-control-from tmp/nsb1-rom4-comparison
```

The six-second gate remains only native/bootstrap/readiness acceptance.
The late gate explicitly accepts graphical **empty-socket** continuation;
it does not prove inserted-card SIM security, menus, phonebook or radio.

### Inserted-card experiment

The compatibility instrument now exposes an explicit removable synthetic card;
its socket remains empty by default. `--insert-sim` selects insertion through
the MAME input configuration, not a firmware memory write. Ordinary `noki5190`
does not acquire this experimental card configuration.

With the same checksum-only EEPROM fixture, the inserted run activates the
card and issues SELECT, STATUS, GET RESPONSE, READ BINARY and READ RECORD
transactions, including the 50 records in EF_ADN (`6f3a`). At ten seconds it
displays **Security code**, not a SIM PIN prompt. Physical entry of `12345`
and Menu/OK paints five masked characters, then **Code error**, and returns
to the prompt. This demonstrates input and card traffic, not successful
phone security, persistent phonebook operation or idle/menu parity.

The inserted runner checks native execution and storage integrity, and the
optional physical-code fixture checks the own comparison/retry gate. It still
explicitly declines general card/presentation acceptance and must not be
presented as the empty-socket graphical gate.

The own static validator is `2ab9c8`: five-character input is encoded by
`2ab7ac`, compared against four bytes at `10c494`, then its Boolean result
is passed to `2ab974`. That latter routine first reads `10c498` and rejects
**every** result if the unsigned retry byte is at least five. Below five,
a failed comparison increments and persists the byte through record `0709`;
a successful comparison clears a nonzero byte through the same record.
The directory independently maps `0709` to EEPROM `04ac`, the fifth byte
of record `0701`. Thus retry ownership is established from this ROM, not
inferred from a sibling. A passive read at `2ab978` observes comparison
result one with retry `ff`: the default code matches, but the retry gate
rejects it. The positive external-storage control changes only `04ac` to
zero beyond the six integrity bytes. The same physical keys then reach the
gate with result one/count zero and paint a Menu / Names idle frame at 25 s.
Identity/encoded-code fields remain untouched; placeholder identity text is
still visible, and neither normal provisioning nor registration is claimed.

Reproduce the negative control, then repeat with `--initialized-retry`:

```sh
.venv/bin/python tools/nsb1_rom4_compare.py mame/mame roms \
  tmp/nsb1-security-negative --seconds 27 --insert-sim --phonecode-fixture \
  --checksum-control-from tmp/nsb1-rom4-comparison
```

`nsb1_security_fixture.lua` uses only physical contacts and a read-only gate
watch; it captures the entry, result and settled frame. Acceptance requires
all twelve press/release edges and the exact own match/retry observation.
It does not require an exact idle pixel oracle or prove phonebook writes,
radio service or speech.

### Menu navigation and sustained native boundary

Physical Menu at 26 seconds renders Phone book; Select at 28 seconds opens
the Search / Add entry / Edit list. Its static 84x48 RGB SHA-256 is
`1c9dac19aa382370ba315a4e99f2ea65c844f4ec099a263624b6593b0525a5bd`.
The longer `--menu-fixture` (35 s minimum) requires non-illegal native CPU
samples through the last complete second, continued bootstrap integrity,
contact edges and exact list pixels. It passes with the corrected ring
notification described below. The separate round-trip fixture below accepts
persistent SIM contacts; registration and services remain unaccepted.

The uncorrected ring-notification negative control requests reset at
31.972845538 s through
`2b1056`, with reason `68` and LR `23ce6b`; own reset caller `23ce66` reads
the stored reason and calls `2b101e`. Repeated native verifier upload follows.
The same time/reason occurs after accepted security entry with no menu input,
so this is not evidence that Add entry crashes the phone. The later security
prompt belongs to the restarted boot, not an established phonebook lock.

The passive observer now records this reset explicitly, and native acceptance
rejects it even before a second upload is visible. Do not shorten the sustained
gate or disable the watchdog to claim menu completion. The corrected native
path also permits the independent SIM UPDATE RECORD/readback test below.

In that negative control, at 31.000902077 s,
`2a46b4` writes `68` to `10ff7c` with LR `2a39c3`. Caller `2a391a` receives
selector one, reads record `0607` into its stack, and calls `231b08`.
That helper tests lifecycle byte `10b57d`: states four/five return one;
other states return two. A return of two plus record bit zero set selects
reason `68` at `2a395a`; further policy/setting checks precede publication.
Runtime locals are policy `ff`, setting `05`, lifecycle `02`. The own record
entry `2cc0a0` maps `0607` to EEPROM `04a6`, length one. The lifecycle is
initialized to two by `22fbe8` at 0.935189077 s and has no subsequent observed
write before reset in this run.

This is **not yet a proven DSP-heartbeat defect**. The observed producer
does not itself test an MDI activity counter, and a shared numeric reset
reason is insufficient to transfer that interpretation from another ROM.
The erased policy enables this branch, but its normal provisioned value and
the lifecycle state's subsystem meaning remain unresolved. No policy bit is
cleared to suppress the reset. The archived `nsb1_3.pp` contains only
`230200D0`, not an independently established value for this record.

The caller census then reaches the activity boundary: two direct Thumb BLs
call `2a391a`, `2079dc` with selector two and `2afdd0` with selector one;
no aligned raw pointer to that function was found (not indirect-call closure).
Receive loop `2afdc4` increments halfword `10c53a` at `2afdda` for ordinary
non-`dd` messages. When event `dd` arrives, `2aff04` tests that counter:
zero calls the selector-one policy routine; nonzero rearms through `267eea`
and clears the count. Thus policy selection and receive inactivity are
separate layers, rather than competing explanations. The passive read at
`2aff04` observes count zero at 31.000204692 s, before the policy publication.
The own decode now
identifies the precise receive loop whose missing traffic matters; it still
does not justify synthesizing a packet or clearing EEPROM policy bits.

### Ring notification and native stack safety

Direct CPU-state observations distinguish bootstrap success from a running
DSP. With the uncorrected backend, samples at 0.5/1/2/5/10/20/30 seconds report
`ILLEGAL=1`, `IDLE=0`, PC `ffc0`, with TX producer/consumer `0041/0033`
and RX producer/consumer `00a6/00a6`. Thus queued work is not being consumed;
the MCU's later graphical/security progress is not sustained native operation.

A bounded program-read tail catches the earlier stop at PC `1db4`, first
observed at 0.366666667 s; later interrupt acceptance changes the exposed PC
to `ffc0`, so that later PC must not be treated as the faulting instruction.
The upload writes program `1d9e=fc00` (return) through native PC `31d7`
at 0.137594269 s. Read-only program samples still see `fc00` at 0.2/0.3/0.35 s.
At 0.362311077 s, a data-space stack write at native PC `465e`, SP `1d9e`,
stores `465e` into that same overlaid word. Execution later branches from
`1caa` to `1d9d`, reads `f6bb`, then reads the overwritten return and falls
through into stack/data words, ending at `1db4` after `3798`.

The own loader initializes SP to `1ec6`, not `1db3`. A paired NSE-1 control
retains foreground SP `1ec4` through 0.5 s; NSB-1 loses stack space in steps
before the collision. The first observed imbalance returns to foreground
`408d` at 0.196091885 s with SP `1ea5` instead of `1ec4`, after HPINT vector
`ffe4` / handler `3772`. This is not evidence for enlarging the stack.

The backend incorrectly asserted HPINT on every MDISND ring commit in
addition to its INT2 ring-not-empty signal. HPINT is the explicit parameter
doorbell, owned by `doorbell_w`, not the ring notification. Removing that
extra pulse leaves explicit doorbells and the INT2 comparator intact. The
same NSB-1 input then drains TX/RX and executes without illegal instructions
through the sustained menu run. At the first inactivity check, `10c53a` is
one instead of zero, so the policy rearms rather than resetting. The 60-second
control samples legal native execution through 59 s and no baseband reset.
The 90-second acceptance run additionally observes count one at both
31/62-second inactivity checks and legal native execution through 89 s:

```sh
.venv/bin/python tools/nsb1_rom4_compare.py mame/mame roms \
  tmp/nsb1-menu-sustained --seconds 90 --insert-sim --initialized-retry \
  --menu-fixture --checksum-control-from tmp/nsb1-rom4-comparison
```

No stack pointer, return instruction, memory geometry, watchdog or policy
value is changed. NSE-1 coherent bootstrap, physical menu and 30-second RF
activation gates also pass with this shared correction.

The ordinary comparison reports **native bootstrap PASS** only and explicitly
warns when CPU execution has faulted. `--menu-fixture` additionally requires
non-illegal native samples and rejects the old execution fault rather
than waiting for the secondary 32-second reset. Neither a graphical frame nor
the bootstrap handshake is accepted as evidence of sustained native execution.

The empty-socket pixel hash and SIMI counts are unchanged. The self-test flag
at the one-second sample is now `c0` rather than `40`, clearing to `40` by
two seconds as genuine DSP traffic is processed; the structural gate is
deliberately re-banked to `00/c0/c0/40/40`, not relaxed to ignore flags.

### Persistent SIM phonebook

`nsb1_phonebook_roundtrip.py` accepts two separate MAME processes. In the
first, physical Menu / Phone book / Add entry types `ADA` and `123`; the
firmware issues `A0 DC 01 04 20` against EF_ADN `6f3a`, the card commits
record one and returns `9000`, and the LCD renders Saved. Persisted record
one contains `41 44 41`, erased name padding and number bytes `03 81 21 f3`;
the remaining 49 ADN records stay erased.

The second process loads only that qualified run's flash, EEPROM and card
storage, with input/ROM hashes and storage-copy SHA-256 provenance recorded.
It enters the phone code through physical keys again, then Phone book /
Search with an empty search name. Cold firmware reads EF_ADN record one and
renders the exact highlighted ADA result. The checker forbids an ADN update
in this phase and requires unchanged ADN bytes, not just a pre-existing file.
Both phases require native bootstrap, legal sustained CPU samples, nonzero
receive activity and physical security/menu contact evidence.

Add `--erase` to extend this to four independent processes. After cold
readback, physical Erase / One by one selects ADA and confirms `Erase?`.
Firmware performs exactly one successful record-one UPDATE RECORD; all
1600 ADN bytes become erased, while unrelated SIM files remain unchanged.
A fourth cold process physically submits an empty-name Search, renders
`No phone numbers`, reads EF_ADN without any update, and leaves the complete
SIM storage unchanged. Exact confirmation/empty-result frames, native and
physical-input checks pass in all phases. No contact is removed by the
harness.

```sh
.venv/bin/python tools/nsb1_phonebook_roundtrip.py mame/mame roms \
  tmp/nsb1-contacts --checksum-control-from tmp/nsb1-rom4-comparison
```

The card's current 3570-byte storage includes append-only PLMN, SMSS and
AoC fields after the unchanged ADN prefix. The shared phonebook checker now
recognizes each complete layout while rejecting truncated intermediate sizes.
No contact bytes are seeded, no live firmware memory is changed, and no donor
storage is loaded. This accepts physical SIM contact save/cold-read/delete,
not handset-local storage, all editing operations, registration, calls or SMS.
Fitted-mask, nominal-input and synthetic checksum/retry limitations remain.

### Startup SIM PIN and cold retry persistence

`nsb1_pin_roundtrip.py` creates the shared laboratory PIN-enabled card,
without donor contents. Physical `4321` / OK produces VERIFY CHV1
`A0 20 00 01 08`, status `9804`, and the exact Code error frame. The card
persists two remaining CHV1 attempts; CHV2/PUK counters and PIN/PUK contents
stay unchanged. A second, fresh MAME process loads only that qualified own
storage, displays Enter PIN code again, and accepts physical `1234` / OK.
VERIFY returns `9000` and restores the persistent attempt count to three.

The phone then separately displays Security code; physical `12345` / OK
produces its own Code accepted frame. This establishes startup CHV1 failure,
durable retries, successful verification and the distinct phone-code flow,
not PUK entry, PIN changes/toggling or every card-security operation.

```sh
.venv/bin/python tools/nsb1_pin_roundtrip.py mame/mame roms \
  tmp/nsb1-pin-security --checksum-control-from tmp/nsb1-rom4-comparison
```

Both processes require qualified inputs, unchanged executable flash, correct
EEPROM geometry and legal native CPU samples through the final complete
second. The successful cold run lasts 60 seconds without reset. MCU inactivity
branch coverage is separate: unlike the PIN-disabled menu control, the
PIN-enabled cold run has no observed `2aff04` callback in that interval.
A read-only watch sees timer `1d`'s record at `10029c` initialized during boot
and queued at 14.755665692 s after PIN acceptance (relative-list halfword
`02a8`, state byte two); its subsequent scheduling is unresolved. The result
explicitly records this unobserved branch rather than claiming watchdog
coverage. Menu and phonebook gates retain their nonzero activity requirements.
No timer, retry state, firmware flag or PIN result is forced at runtime.

### External reference leads, not provisioning evidence

The [independent emulator's current coverage table](https://github.com/djr-747/nokia-dct3-emulator/blob/main/docs/MODELS.md)
reports 5190 standby. Its current product profile still uses an NSE-1 donor
EEPROM and security-settings repairs, so that outcome is not a forcing-free,
own-storage comparison for this configuration. No donor bytes are admitted.

Own lookup `2a75e4` splits the record ID into high-byte group and low-byte
index, selecting eight-byte group entries at `2cc3a0`. Group 7 points to
`2cc0a8`; the reader multiplies the low index by 8 and compares a **32-bit**
record ID before returning the entry. Records are `{BE32 id, BE16 offset,
BE16 length}`. Entries `2cc0b0` and `2cc0b8` identify `0701` at EEPROM
`04a8`, length 8, and `0702` at `04b0`, length `30`. Their IDs start two
bytes before the superficially matching `0701`/`0702` halfwords. Helper
`2a78aa` reads entry offsets `+4/+6`; `2a7974` passes the resolved address,
destination and length to external read `2ab0e6`. Write wrapper `2a792e`
instead uses `2aafe2`. The package checker validates these two own records
and their storage bounds without generating contents. Security-field
semantics and valid factory contents remain unproved. In particular,
`04b0+1f=04cf` is only an address relationship until its reader/validator
is recovered, not permission to clear a purported lock field.

In the checksum-control native run, wrapper `2b245a` actually reads `0702`
to `10fcd6` (return `2b2467`) and `0701` to `10c494` (return `2b2471`).
Consequently EEPROM `04cf` corresponds to RAM `10fcf5`. A passive byte-lane
read watch observes exactly two accesses through five seconds: generic
settings-table validation at `28c040` and EEPROM serialization at `2ab072`.
The final byte is `11`, not erased `ff`. The settings table at `2cfbf4`
contains `{35,11,02,39}`; its RAM base is `10fcc0`, so offset `35` names
this byte. The validator tests table flags and maximum before assigning a
default. These observations establish generic settings ownership, **not** a
security-level interpretation or a lock decision at the current frontier.
The watch excludes its own final snapshot read; no guest state is written.
Do not import the external profile's field name or clear the byte merely
because that profile reaches standby.

Public catalogues also list a [5190 system-module document, `sysa3.pdf`](https://www.eserviceinfo.com/downloadsm/233124/NOKIA_sysa3.html).
Its contents have not been acquired or verified here; the listing alone
does not establish ADC nets or fitted component identities.

Fresh passive runs observe the following without firmware RAM/MMIO writes:

- runtime EEPROM descriptor is mode 1, page `20`, capacity `2000`;
- both own catalogues initialize at `10adc0` and `10ad50`, whose first
  pointers are `2b35a4` and `2b94e4`; family selector is 4;
- all 186 words installed at MCU `11e00` match the own descriptor at
  `2be564`, payload SHA-1 `e1e8f15a305bf32aea98f444074f30828d933fc4`;
- 246 shared-memory writes occur before the observed supervisor park;
- through five seconds sampled PC is `2a4672..2a4678`, SP `10c6dc`, with
  EEPROM geometry/catalogues stable. This is ordinary supervisor idle:
  FIQ entry `2acac4` clears flag `10c540` at `2acaca`, and the supervisor
  re-arms it at `2a4668`. A five-second run observes 39 flag writes, with
  repeated wake/re-arm pairs. Global control is `05` after startup, both
  ARM interrupt-disable bits are clear, and local FIQ mask is `e3`.
  Repeated samples in this loop therefore do not imply an interrupt deadlock;
- persisted flash equals the normalized source. The 8 KiB EEPROM contains
  firmware-created writes, which are not proof of valid calibration or identity.

Use a fresh `RUN` directory:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir RUN -- \
  noki5190 -rompath ../roms -video none -sound none -nothrottle \
  -seconds_to_run 6 -log -autoboot_delay 0 \
  -autoboot_script ../tools/nsb1_boot_observe.lua
.venv/bin/python tools/nsb1_boot_check.py roms/noki5190/5190f671a.fls RUN
```

The checker pins the source, exact installed program, five ordered geometry/
catalogue observations, stack bounds, supervisor park and unchanged executable
flash. It additionally requires repeated FIQ-owned flag clears and actual
global interrupt-enable observations. Register `0b` is labeled `irqmask`,
not global control (`0c`). It is a boundary regression, not a graphical boot
oracle.

### First missing DSP publication

MCU routine `28f934` initializes shared results to `0000/ffff/ffff/ffff`,
loads its selected program, supplies arguments
`0100/0300/0000/8000/0001/0001/0200` at `100f6..10102` and releases reset
with command `01` at `28f9c8` (0.007852154 s in the measured profile).
Its first result check at `28f9dc` reads shared `10004` (DSP data `0802`),
then retries at `28f9fc`. The own uploaded program's early MVPD reads
resident program cell `ff87` into `0802`; this precedes its block loop and
the later calls to resident helpers `8023` and `802b`.

With the explicitly silent DSP the MCU repeatedly reads `ffff`, times out,
clears success byte `10ad2d`, saves `ffff` at `10ad36` and skips the
sparse-flash transfer branch. Both ownership flags remain 1. The subsequent
supervisor wakeups are independent evidence that the RTOS still runs, not
proof of a completed verifier. Do not describe this as an unacknowledged
first block: no first sparse-flash block has been supplied yet.

The observer suppresses its own snapshot reads from the API-read tap.
Accepted read evidence must come from the firmware PCs above, not the
supervisor PC that happened to be current while Lua inspected memory.

## Native ROM4 compatibility instrument

`nsb1r4t` retains own NSB-1 flash, controller composition and erased 8 KiB
external EEPROM. It replaces the silent backend with acquired NSE-1
program/data images, explicitly named as NSE-1 inputs rather than relabeled
NSB-1 dumps. SHA-1 values are
`a05a1e96a8c36ec5a47e1ea059d15afa54ca5739` (program) and
`024c7f970f4ef754d3e90471de48a167515f930d` (data). The 52 MHz DSP clock is a
declared compatibility cadence here, not an independently measured NSB-1
timing contract. No HLE reply or donor EEPROM is attached.

Own firmware automatically tries two uploaded programs. The first,
descriptor `2b9344`, has 186 words with SHA-1
`73b0cc6e6fc602f3a2f7934eb0c35d7f31c96b3f`; the second, descriptor `2be564`,
has SHA-1 `e1e8f15a305bf32aea98f444074f30828d933fc4`. Passive release-time
readback matches both payloads exactly. Native execution publishes resident
version 4: the MCU rejects the first candidate's requested version 3 and
accepts the second candidate's requested version 4. Both attempts are also
present in the silent control; reset-edge tracing must not suppress the
second release or count a later command `03` as another reset release.

The accepted comparison performs all 63 main-loop ownership handoffs,
then the MCU's final-result poll at `28fa96` observes shared `10002`
change from `ffff` to `0004` through native execution. It subsequently
releases its own 580-word loader (`fd00/ff80/0244/0500/0078/0000`), and
executes native loader/resident code observed at `0d8f` and `8d98`.
By 0.5 seconds success byte `10ad2d` is 1, saved version `10ad36` is 4,
and the handoff count is 63; these remain observed through five seconds.
Source executable flash is unchanged and EEPROM persists at 8 KiB.
The final panel is white after the separately validated LCD self-test. This establishes compatibility of this
bootstrap path, **not fitted-mask identity, valid calibration, graphical
boot, RF operation, audio or phone-service parity**.

```sh
.venv/bin/python tools/nsb1_rom4_compare.py \
  mame/mame roms tmp/nsb1-rom4-comparison --seconds 6
```

The runner rejects existing run directories and qualifies all three input
hashes before creating its private ROM directory. It admits no EEPROM image,
records provenance, requires ordered version mismatch/acceptance, both exact
uploads, 63 handoffs, final-result publication and positive native I/O traces, and checks persisted
executable identity. `--verbose` optionally records the larger native opcode
inventory. Shorter runs record evidence without claiming the six-second
acceptance boundary. The normal `noki5190` silent-MCU gate remains independent
and unchanged in behavior.

### LCD transport and pixel self-test

Passive MCU MMIO capture observes selection `21` at `20028`, commands at
`2002c` and data at `2002b`. The own initialization stream is
`24,40,80,41,80,42,80,43,80,44,80,45,80,20,21,05,14,ce,20,0c`.
Firmware writes six 84-byte zero banks, then six full `ff` banks, then
clears them again. By 0.5/1 seconds totals are 32 commands, 1008 data bytes,
504 nonzero bytes; by 2/5 seconds totals are 68/2016/504.

The named snapshot `snap/nsb1_native_1s.png` is exactly 84x48 black;
`snap/nsb1_native_5s.png` is exactly 84x48 white. These differ from the
ordinary end-of-run auto-snapshot under `snap/nsb1r4t/`. The compatibility
checker now requires both MCU traffic and decoded pixel values, rejecting
an always-white panel or a missing nonzero transfer. No framebuffer or
firmware state is written by the observer.

This validates the exercised command/data transport and full raster under
this configuration, not fitted controller identity, contrast, analogue LCD
timing or application UI readiness. A white final frame alone must not be
used to infer a broken display: the observed firmware deliberately clears
the self-test pattern. The next consumer-side investigation is application
presentation after self-test, with the own EEPROM validity gates kept
separate from hardware transport.

### EEPROM checksum-only diagnostic control

With initially erased storage, own self-test flag byte `10fde5` reaches
`c4`, loses bit `40` at checksum-failure store `23a018`, becomes `80` after
the DSP-related check, then `00` after `29a61c` clears bit 7. This is an
observed failed integrity path, not proof of missing native execution.

An explicitly labeled control recomputes only `11c..11f` and `1fe..1ff`
in the EEPROM created by a qualified own erased-storage run. It preserves
every calibration, identity, security and other content byte; it neither
imports a donor nor makes erased contents into a valid factory profile.
The runner verifies the source run's exact input manifest and unchanged
own executable before generating the six-byte integrity control. It cannot
chain from another checksum control or silently reuse prior NVRAM.

```sh
.venv/bin/python tools/nsb1_rom4_compare.py mame/mame roms \
  tmp/nsb1-checksum-control --seconds 6 \
  --checksum-control-from tmp/nsb1-rom4-comparison
```

This control retains bit `40` (`c0` at 0.5/1 seconds, `40` at 2/5 seconds),
with the same completed native bootstrap. At full-scale ADC2, LCD traffic
and the final white frame remain unchanged. Therefore checksum consistency alone does
**not** establish application presentation or valid provisioning. The default
native acceptance and its strict LCD oracle remain unchanged; the diagnostic
deliberately makes no presentation claim. Its input/output hashes and changed
offsets are retained in the run manifest. Retire this control as a bring-up
instrument once product-matched calibrated storage supplies its replacement;
retain the decoded integrity formulas and this negative result.

### Local fixed-dialling permission

The research native-mask composition enforces the saved FDN record A/12346
through physical single-Navi input. With FDN enabled, dialling 12346 reaches
call-progress UI, while 54321 displays `Call not allowed`. After menu-driven
FDN disable and a cold restart, the same 54321 reaches the same call-progress
frame as the listed-number control. Each dial preserves the complete card;
the enable/disable cycles change only the ADN-validity byte. Cold storage,
own native execution and the SIM identity-rehabilitation handshake are gated.
This is local permission handling, not registration, connection or speech.

```sh
.venv/bin/python tools/nsb1_fdn_dial_check.py tmp/nsb1-fdn-edit-cold \
  tmp/nsb1-fdn-dial-enabled tmp/nsb1-fdn-dial-listed \
  tmp/nsb1-fdn-dial-unlisted tmp/nsb1-fdn-dial-off \
  tmp/nsb1-fdn-dial-unlisted-off
```

### Empty-socket SIMI boundary

Own routine `29cde8` resets SIMI at `20038`, configures `2003a`, writes
RX/TX FIFO controls `2003d/3e`, and writes control `32` at `20039`.
Receiver `29ce5e` drains `20037` while testing count `2003c`; the nearby
initializer manipulates FIQ6 masking. These independently recovered register
contracts justify attaching the existing controller with no inserted card.
No ATR, APDU reply, SIM-ready event or internal firmware flag is supplied.

The erased-storage native control makes no SIMI access in the first five
seconds. The checksum-only control reaches the own initializer at about
0.94 seconds: passive taps observe 18 writes, including `38=ff` at `29cdfc`,
`3a=03` at `29ce26` and the FIFO-control sequence beginning at `29ce3c`.
At full-scale ADC2 all five samples report zero SIMI reads. With nominal
ADC2, later firmware begins empty-socket transactions: sampled read/write
counts are `0/0, 0/0, 0/18, 3/21, 9/45`. The diagnostic checker now requires
that sequence plus final input readiness `0a/03/0f/20`, preserving the positive
initializer sites rather than merely accepting a source setter. These reads
are controller polling, not card responses or ATR acceptance.
Native bootstrap and LCD self-test oracles remain unchanged.

This is firmware initialization/write-side acceptance, not character RX/TX,
ATR, card security, application SIM handling or proof that SIMI was the
remaining UI prerequisite. ADC selector routing and sensible board inputs
remain separately unvalidated; do not assign VBATT to channel 0 from its
position in a generic tuple.
