# Nokia 6150 NSM-1 bring-up

## Current boundary

The acquired v5.23 PPM C package normalizes to an exact 2 MiB CPU-order
image. `make verify-6150-static` regenerates it from a hash-pinned ZIP
wrapper without executing the installer, validates every Wintesla record,
and checks its reset and sparse DSP verification contract. Normal handset
support is not promoted; the separate diagnostic composition below now has
qualified graphical presentation. An explicitly separate
`nsm1r4t` executable compatibility instrument now exists.

The diagnostic record passes native validation/update and independent cold
persistence. Explicit nominal ADC2 and the own phone-retry initializer
fixture permit graphical boot and physical phone-code/menu acceptance,
not factory provisioning or fitted-chip proof. Negative saturated/original-
retry controls remain distinct. No readiness event or firmware flag is
synthesized.

The research clock composition retains calibrated Timer1 input1057Hz and
terminal7fff, giving an approximately31s FIQ5 cadence. The PLAY TONE fixture
below demonstrates that this materially delays a software deadline. These
software gates do not qualify physical timer rates or requested-duration
accuracy; no clock has been changed merely to pass an application fixture.

### Physical clock, date and awake alarm

The separate native compatibility composition accepts physical clock/date
initialization and an awake alarm without an injected event or firmware flag.
From the qualified PIN-enabled seed, own PIN1234 and phone code12345 lead
through Menu/Settings4. The unset-clock notice consumes a separate acknowledgement
before its time editor accepts12:34. Date01.01.2000 is then accepted, followed
by alarm12:35. Own End/Mode key0f returns to idle. The
[Nokia6150 user guide](https://josvandijken.nl/downloads/pdf/handleiding_nokia_6150.pdf)
places Alarm Clock at Settings4-1; the actual route and pixels are independently
checked against this image, not imported from another model.

Firmware publishes minutes22/hour8c through CCONT's shared time-setting
latch, then alarm minutes23/hour0c. At98s the retained counter reaches
12:35:00, statusb3/mask50 presents alarm/minute causes, and firmware acknowledges
a0. The native screen flashes Alarm! with Stop/Snooze. The MCU drives the
PUP buzzer path with divider5285 (reported2459Hz). Physical Stop at105s
returns to idle and leaves the late buzzer window quiet. A matched process
omitting only Stop continues pulsing after106s. Both checks and a shared
executable identity are required for the paired result.

The complete EEPROM endpoint is
`f8da8575410d5a83aca7a73dffd9b3e320aefae4ce935df4190238ce886323d3`
in both runs, with unchanged whole card and flash. Retained CCONT input is
exactly `1a000c010000005000`; both112s endpoints are
`0e230c00001e0e5001`. Source/cold-copy provenance, external configuration,
Phase2/read-only SIM conversation, authentication, all physical contacts,
reviewed frames and late native execution without an unsupported stop are
required. Tests reject mismatched executables, missing/changed contacts,
latch/acknowledgement errors and wrong Stop/control buzzer behavior.

```sh
.venv/bin/python tools/nsm1_clock_alarm_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-clock-alarm-stop-final --mame mame/mame
.venv/bin/python tools/nsm1_clock_alarm_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-clock-alarm-control-final --control --mame mame/mame
.venv/bin/python tools/nsm1_clock_alarm_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-clock-alarm-stop-final \
  --matched-control tmp/nsm1-clock-alarm-control-final
```

Use fresh outputs for execution and serialize processes. This remains the
declared nominal-ADC2/NSE-1-mask compatibility fixture with inherited03ca=00
timeout policy, not factory provisioning or fitted-mask proof. It establishes
awake RTC/firmware/PUP behavior, not acoustic fidelity, production timing,
calendar year interpretation, snooze, power-off alarm wake, RF or native speech.

#### Independent cold clock readback

`nsm1_clock_cold_check.py` revalidates the complete Stop/no-Stop pair, then
starts a separate process copying only that Stop endpoint's NVRAM/CFG. The
same executable and exact initial files are required; no RAM or save state
is copied. Physical login and menu input retrieve12:35 in the alarm and clock
editors, cancelling each with Back instead of confirming another setting.
No CCONT alarm/counter writes occur. At46s the retained12:35:14 counter
rolls to12:36:00; firmware acknowledges the minute cause and idle redraws
12:36. No expired-alarm alert repeats in the late observation window.

The whole cold EEPROM hash is
`4fdb494b68a913e30f92a2c48cbff9cd2e17aff00395b6e84aeb9f1f40ac6e89`.
It differs from the source only by the independently known ordinary boot
transition03e8:06->00; this is not an allowed-mutation whitelist. The whole
output is pinned. Card/flash remain unchanged. Retained CCONT ends as
`10240c00001e0e5001`, accounting for62s of emulated time, not offline host time.

```sh
.venv/bin/python tools/nsm1_clock_cold_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-clock-alarm-stop-final tmp/nsm1-clock-alarm-control-final \
  tmp/nsm1-clock-cold-qualified --mame mame/mame
```

Use a fresh output. Both awake scenarios also pass after extracting their
shared physical-key helper. This qualifies cold clock/last-alarm-value readback
and minute redraw, not independent calendar year/date interpretation, snooze,
power-off wake, physical clock accuracy or native speech.

#### Powered-off alarm and activation choices

`nsm1_alarm_power_check.py` extends the same own clock setup with physical
Power contacts at70/72s. Firmware drops the digital rails at70.7765s;
CCONT continues ticking and restores power with alarm cause80 at98s.
The observed off interval contains27 continuous RTC ticks and no checked
DSP/radio endpoint activity. This is not complete internal clock-gating proof.
Firmware acknowledges the alarm after reboot at100.2376s, rings through
native PUP control, and physical Stop at115s displays "Activate phone for calls?".

Physical No at119s drops power again at124.9796s. The140s gate checks
continued RTC progress, a blank panel and more than ten seconds of endpoint
silence after that shutdown. Physical Yes instead presents PIN authentication;
own PIN1234 and phone code12345 are accepted again after reset. Menu then
Down displays Messages followed by Call register. The accepted-editor residue
before Menu is not claimed as a clean idle screen. Activation wording is not
evidence of native RF registration or calls.
The Yes path deliberately calls restart reason0c from own2818dc/2818de;
the checker permits exactly that task1 request immediately after Yes, not an
arbitrary restart. Two boot epochs need a16384-byte passive wire-capture budget;
the original8192-byte cap truncates the late RX observation and is rejected.

Both whole EEPROM endpoints are
`1ce678a609574b51dfccdaa7c091ea520302c718f996f46cf011f1fb1c0d6660`;
whole card and flash remain unchanged. Exact CCONT endpoints are
`2a230c00001e0e5001` (No140s) and `36230c00001e0e5001` (Yes152s).
Cold provenance, physical contacts, authentication, reviewed frame hashes,
configuration and native execution are checked without firmware-state writes.
The existing nominal ADC2, compatibility-mask and diagnostic timeout-policy
limitations remain. Snooze, calendar year interpretation, physical timing,
acoustic fidelity and native speech remain unqualified.

```sh
.venv/bin/python tools/nsm1_alarm_power_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-alarm-power-no-qualified --mame mame/mame
.venv/bin/python tools/nsm1_alarm_power_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-alarm-power-yes-final --activate --mame mame/mame
```

Use fresh outputs and serialize MAME processes. Earlier awake/cold gates do
not themselves qualify power-off wake; this separate gate supplies that evidence.

#### Awake Snooze and second alarm

`nsm1_alarm_snooze_check.py` retains the same physical clock setup but presses
Snooze (Right Softkey) at105s rather than Stop. Firmware displays "Snooze
active", silences the buzzer and programs CCONT alarm minutes29/hour0c,
meaning12:41. The retained counter reaches12:41:00 at458s and presents
statusb3/mask50; firmware acknowledgesa0 at458.000247s. The flashing Alarm!
screen and native PUP divider5285 pulses return. Physical Stop at465s returns
to the idle presentation with no further alarm pulses through480s.

The gate checks both RTC deliveries, exact Snooze/Stop contacts, the quiet
interval between alarms, reviewed Snooze/second-alarm/final pixels, complete
authentication and Phase2 SIM conversation, cold provenance and late native
execution. The passive wire budget is32768 bytes for this longer observation;
truncated observations remain errors. Whole EEPROM output matches the original
awake-alarm endpoint `f8da8575410d5a83aca7a73dffd9b3e320aefae4ce935df4190238ce886323d3`,
with unchanged whole card/flash and exact CCONT endpoint`16290c00001e0e5001`.

```sh
.venv/bin/python tools/nsm1_alarm_snooze_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-alarm-snooze-qualified --mame mame/mame
```

Use a fresh output and serialize MAME. This establishes one awake Snooze/re-alarm/
Stop cycle, not repeated snoozes, powered-off snooze, independent calendar-year
interpretation, physical duration accuracy, acoustic fidelity or native speech.
The retained compatibility-mask, ADC2 and diagnostic-timeout assumptions remain.

### Card-originated Toolkit DISPLAY TEXT

The qualified `nsm1r4t` composition accepts an explicit Phase-2+ external
card without changing the complete PIN-enabled card image or handset storage.
Own firmware reads EF_PHASE03, publishes five-byte TERMINAL PROFILE
`0b41ff7700`, then polls STATUS9116 and FETCHes the22-byte card-originated
`DCT3 SAT` command. The65s LCD shows that exact text and Back softkey.
Physical Menu at68s dismisses it; only afterward does firmware send terminal
response `810301218002028281030100`, accepted with9000. The70s frame is idle.
This differs from the6130's automatic result30 refusal; neither result is
inherited from the sibling profile.

The matched Phase-2 control uses the same cold input and physical contacts,
changing only SATCFG from1 to0. It reads EF_PHASE02 and emits no proactive
profile/FETCH/response; the same Menu contact opens Messages instead of
dismissing a prompt. Both runs preserve the entire qualified16KiB EEPROM,
original3570-byte card and own executable flash. Initial storage hashes and
CFG comparison exclude unrelated provisioning changes. Authentication and
raw SIMI TX/RX, statuses, payload, response ordering and exact pixels are
checked, not just the displayed text.

```sh
.venv/bin/python tools/nsm1_toolkit_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-positive --mame mame/mame
.venv/bin/python tools/nsm1_toolkit_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-control --control --mame mame/mame
```

Omit `--mame` to recheck retained captures. Fresh outputs are required for
execution. The existing diagnostic-template policy03ca=00, nominal ADC2
and unproved NSE-1 mask remain explicit. This qualifies one Toolkit command,
not every advertised capability, factory provisioning, RF or native speech.

### Interactive Toolkit input

The same cold PIN-enabled input separately accepts external card profile3
in an84s run (`tmp/nsm1-toolkit-interactive-accepted`). After the existing
physical DISPLAY TEXT dismissal, firmware FETCHes GET INKEY `Press 5` and
GET INPUT `Enter 42`. Own physical Keypad5 and left-softkey confirmation
return `8103022200020282810301000d020435`. Keypad4, Keypad2 and a separate
left-softkey confirmation return `8103032300020282810301000d03043432`.
The card accepts the first two command responses with9115/911a and the final
response with9000. Each command's payload precedes its physical input; no
response precedes the associated physical confirmation.

Reviewed70.5s/73.5s/76s frames show Press5, the empty Enter42 editor and its
filled42 state. At79s the phone returns to its own clean Headset idle frame,
not an operator-registration screen. The complete16KiB EEPROM, original
3570-byte persistent card and executable flash remain unchanged. Only the
external SATCFG profile differs from the DISPLAY TEXT fixture. The own
five-byte terminal profile, authentication, exact APDU/payload/status sequence,
physical actions and whole input configuration are checked independently.
No firmware or controller result is supplied.

```sh
.venv/bin/python tools/nsm1_toolkit_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-interactive-accepted --interactive
```

Add `--mame mame/mame` with a fresh output directory to execute. Shared
`sim_toolkit_interactive_check.py` owns the lab command grammar; NSM-1 retains
its own profile, contacts, provisioning and pixel/storage requirements.
This qualifies these two input commands on the declared native compatibility
composition, not timer, RF, speech or fitted-mask fidelity. Installed-menu
selection has the separate contract below.

### Installed Toolkit menu selection

External profile4 appends the40-byte SET UP MENU command, installing
`DCT3 menu` with Continue/Exit after the three text/input commands. The same
own terminal profile and physical input sequence apply, but GET INPUT's
terminal response now receives9128, followed by FETCH40 and a successful
menu-installation response `810304250002028281030100` accepted with9000.

The own UI remains at idle after installation. Physical Menu80 enters the
main menu; Up81 selects DCT3 menu and Menu84 opens its two-item list. Continue
confirmation at86s sends envelope `d30702020181100101`; a separate cold run
presses Down86 and confirms Exit88, sending `d30702020181100102`. Both are
accepted with9000 and neither envelope precedes the physical selection.
Reviewed heading/list/selected-item frames and exact envelopes distinguish
these two outcomes. All complete persistent handset/card storage is unchanged.
No production firmware input or menu state is injected.

```sh
.venv/bin/python tools/nsm1_toolkit_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-menu-continue-accepted --menu-item 1
.venv/bin/python tools/nsm1_toolkit_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-menu-exit-accepted --menu-item 2
```

Both90s runs show **Wait** after the ACK-only selection. This qualifies
installation, physical navigation and selection envelopes, not automatic
return to the application menu. A real follow-up proactive command is a
separate card/application contract; do not synthesize a firmware completion
to clear this frame. Add `--mame mame/mame` with fresh outputs to execute.
All existing provisioning, native-mask, RF and speech limitations remain.

### Toolkit follow-up SELECT ITEM

External profile7 retains the same installation and Continue selection, then
answers its menu-selection envelope with9122 rather than a final9000. Firmware
FETCHes the34-byte SELECT ITEM command `Choose` with Alpha/Beta. Physical
Down89 selects Beta; left-softkey confirmation90.5 returns
`810307240002028281030100100102`, accepted with9000. Reviewed88s/89.5s/91.15s
frames show Alpha selected, Beta selected, then the original Continue/Exit
list with Continue selected. The result list matches its pre-selection frame
exactly. The item result follows physical confirmation, not card delivery alone.

```sh
.venv/bin/python tools/nsm1_toolkit_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-select-item-accepted --select-item
```

The independent92s run preserves complete EEPROM/card/flash and all unrelated
configuration. It requalifies PIN/phone authentication, own terminal profile,
all preceding text/input/menu exchanges, exact follow-up payload/status/action
ordering and all product-owned pixel oracles. Add `--mame mame/mame` with a
fresh output directory to execute. The card supplies the real next proactive
command through SIMI; no firmware completion or context change is supplied.
This qualifies clean return after this follow-up command, not automatic return
from the separate ACK-only profile, arbitrary Toolkit applications, native RF,
speech or fitted-mask fidelity.

### Toolkit REFRESH and SIM re-profiling

External profile9 retains the same own cold input and physical text/input/menu
route. Continue at86s returns pending-command status `910b`; firmware
FETCHes the11-byte REFRESH qualifier00 payload
`d009810309010082028182`, then sends its own `0b41ff7700` TERMINAL PROFILE
again before terminal response `810309010002028281030100`. Both the new
profile and REFRESH response receive9000. The88s frame reads `SIM card update
started`; the91s frame matches the qualified Headset idle exactly.

```sh
.venv/bin/python tools/nsm1_toolkit_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-refresh-accepted --refresh
```

Add `--mame mame/mame` with a fresh output directory to execute the92s gate.
Whole flash and the original PIN-enabled card remain unchanged. Firmware
changes one EEPROM byte, `03e8: 00 -> 06`; the accepted complete16KiB output
SHA256 is `5d740b1c59aea38015e374499e3e8c4aeaccd46584c5077a00a0e46d4819e34e`.
The gate requires that exact complete bank, not a changed-byte allowlist.
The own directory maps the byte to one-byte descriptor0766 nested in
44-byte record0702 at03d4. A separate read-only writer capture observes raw
serial writer2bc9ca receiving that whole44-byte record from2b8b4c, with the
byte06 after REFRESH; earlier writes contain00. The descriptor's general
semantics remain unassigned. The separate profile4 ACK-only menu control
preserves the original whole bank.

REFRESH rereads enough SIM files to produce8749 observed RX bytes and2083 TX
bytes. Its observer/parser cap is explicitly16384; ordinary Toolkit fixtures
retain8192. Saturation fails acceptance at either bound. The original
8192-byte discovery captures cannot qualify the complete exchange, even
though MAME exits successfully; they only localize the writer. The accepted
fresh run has complete wire capture, authentication, exact payload/status/
physical ordering, whole storage and reviewed frame checks. It qualifies
REFRESH re-profiling and idle return, not the later restarted application
cycle, RF registration, sustained native operation or fitted-mask fidelity.

### Toolkit PLAY TONE completion and cancellation

The independent external profile11 pair follows the same own authentication,
text/input/menu route. Physical Continue86 receives911c and FETCH28 retrieves
`d01a81030c200082028103850853415420746f6e658e011084020102`.
The87s frame reads `SAT tone` with a Quit softkey. Physical Right Softkey89
returns `81030c200002028281030111` (user termination), accepted9000, and91s
reproduces the exact installed Continue/Exit menu. The matched no-Quit process
instead retains the same prompt through91s and sends no PLAY TONE terminal
response through the92s horizon. No timeout result is supplied by the harness.

```sh
.venv/bin/python tools/nsm1_toolkit_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-tone-cancel-qualified --tone cancel
.venv/bin/python tools/nsm1_toolkit_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-tone-hold-qualified --tone hold
```

Add `--mame mame/mame` with fresh outputs to execute. Both preserve the full
qualified16KiB EEPROM, original3570-byte PIN-enabled card and own flash.
Exact APDU/payload/status/physical ordering and product-owned frames are
required; the hold control rejects a completion or extra Quit. Read-only
samples before85s, active87s and after91s show native DSP PC47a1, IDLE0 and
ILLEGAL0 in each run. The observer labels the last field `stop` so a passive
zero register value is not mistaken for an execution-error diagnostic.
These samples are bounded no-stop evidence, not complete instruction coverage.

This pair proves firmware presentation and physical cancellation, not an
indefinite wait: the extended gate below demonstrates organic completion.
Audible output and native speech remain unproved. Only the inactive reset-time HLE
tone publication is logged; no active HLE tone is synthesized. The remaining
upstream request path is bounded below; no missing behavior has been assigned
to a DSP opcode, peer or physical clock. The
piezo-buzzer path must not be substituted for this DSP-controlled tone route.

#### Organic completion on the current timer cadence

An independent no-key run extends observation to93.5s without changing any
clock, firmware state, card payload or hardware input. Own handler2737c4's
PLAY TONE branch posts internal event4190 with tone identifier0a through
2bd8de, then calls scheduler275106 with index52 and delay250 at86.064588s.
Its descriptor at100510 becomes active. The natural Timer1/FIQ5 event processes
that timer at93.002007s; the firmware acknowledges FIQ5 mask20 at2b96d8 and
consumes the expired descriptor at275f9e. Only afterward does it transmit
`81030c200002028281030100` (success), accepted9000. The93.3s frame exactly
matches the installed Continue/Exit list; the descriptor is inactive again.

```sh
.venv/bin/python tools/nsm1_toolkit_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-tone-late-qualified --tone late
```

Add `--mame mame/mame` with a fresh output to execute. The CLI94s limit is
an upper bound; the observer exits cleanly at93.5s, before the separately
mapped receive-supervision reset boundary. Complete EEPROM/card/flash remain
unchanged, and DSP PC479e/IDLE0/ILLEGAL0 is sampled at93.3s. Exact request,
250-tick scheduling, timer-state transitions, FIQ5 acknowledgement, response
after descriptor consumption, native samples and reviewed frames are required.
There is no application key after Continue and no synthetic expiry or reply.

The earlier92s hold result is horizon-limited, not evidence of a permanently
stalled timer or missing DSP completion. The late success still does not prove
the card's requested duration in physical seconds, tone generation or sound.
Current Timer1 calibration materially affects deadline service; independently
recover its source/divider and reconcile the existing shutdown contracts before
changing shared clock behavior. The ordinary tone output path remains separate.

#### PLAY TONE publication boundary

The matched hold fixture has a separate read-only publication census over
`85 <= t < 91.5` (`nsm1_toolkit_tone_boundary_check.py`). It observes **zero**
MCU shared-memory writes and zero native DSP HPI writes. The ARM tap covers
the10000..10fff aperture and declared8f000 mirror combinations; the DSP tap
covers0800..0fff. Early boot supplies70514 MCU write callbacks and seven
compiler-entry hits, with the first eight MCU writes and all seven entries
retained as positives. Native DSP execution supplies2816 I/O writes in the
late window:1408 each of port0021 values0e82/0682. No audio meaning is
assigned to those words. Caps, malformed rows and missing positives fail.

Own compiler2a4a6c has47 jump-table entries at2a4aa0. Mechanically decoded
literal loads map fields20/21 to oscillator words100ae/100b0,22/23 to
100b2/100b4, **24** to amplitude100b6, and25 to100ac. Its entry fetch is
observed seven times before5s but not during the late window. This is a
branch-entry observation with positive controls, not an all-instruction
execution census. Seven direct-BL candidates are2404f2,240e4c,240e56,
2a2bf0,2a2c24,2a2c4c and2a4d5a; indirect/interior entrants are not closed.
The first three select fixed fields12,08 and09, respectively, not the
oscillator/amplitude fields. Wrapper2a4d56's only literal Thumb pointer is
2a4d57 at2dcefc, index15 of function table2dcea8. Dispatcher2b2c9a loads
that table using input byte8 and calls the selected pointer through BX R5 at
2b2d8e. The recovered caller2b2f5e is the **D1** case of the command decoder
at2b2ef8; its result builder writes **D2** at2b2e74. These decoded command
and compiler-field namespaces are separate. No ordinary PLAY TONE producer
has been established by this table entry; do not synthesize a D1 call to
make the compiler fire. The live Toolkit handler instead posts event4190;
its output consumer is traced below. Its separate timer completion is
qualified above.

```sh
.venv/bin/python tools/nsm1_toolkit_tone_boundary_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-tone-boundary-qualified
```

Add `--mame mame/mame` with a fresh output to reproduce. It also requires
the ordinary hold fixture's exact prompts, no completion/physical Quit,
native samples, authentication and unchanged complete storage. The tone
screen therefore does not establish that a DSP tone request was published
through this aperture. Other transports, arbitrary time windows and physical
clocks remain outside this absence result.

The read-only `tmp/nsm1-toolkit-tone-policy-table` capture passes the ordinary
hold checker. Event4190 is functional deferred work: task5 drains the ring
at1112dc through2bd77c, and consumer2958e0 selects296594. Constructor2c005e
posts task6 message `002db0380a02005a`; task6 receives that exact message.
Its descriptor2db038 selects channel1, but admission byte111922 is zero and
descriptor flags are zero. Execution therefore takes2a8748 to2a88e8 before
script interpretation. This is not evidence of a missing DSP reply.

Startup explicitly clears that admission byte twice at23.846s through
2c0118. Direct ROM-data reads identify sequence interpreter2bd230 consuming
`4194, 1` at2cb400, reached through callback handler2bd2d6 with callbacka5
and sequence62b9. The sequence base2ca91c plus index2b9*4 gives2cb400;
callback record2cbe7c in table2cb954 contains62b9. This is a data-driven
producer, not an absent message-post callsite. Initialization2bffb8 also
sets admission according to cached byte11fc07==4. Routine291fb0 copies that
byte from byte5 of a selected12-byte record at111ee0+12*index. Loader2923a0
loads seven variants of NV key0762 into that bank; the own descriptor at
2da414 specifies offset3f9c, size12. Observed copies of records0 and6 contain
12 erased bytes each. Routine292550 copies the selected record then emits
0195,01a8 and01b1; callbacka5 is subsequently observed on0196. Event0196
declares no arguments: leftover argument-buffer contents are not payload.

Selector292bb8 chooses index6 when catalogue byte11fc8e equals1. Transition
1e2 sets that byte on0720; transition1e8 clears it on0721. Task1 emits0720
through2a1e64/2bd830 from callsite280c20. Header0720 also declares zero
arguments. The task1 branch tests external-status2000e bit1 and CCONT
selector0, read by2bb822/2c32f4 and compared with0312 and0390. Raw input
comparisons do not establish physical pin ownership or open-pin voltage.

| Raw selector0 | External status | Settings selection / tone admission |
| --- | --- | --- |
| 0 (default) | 0 (default) | 0720 sets state8e; record6; channel1 disabled |
| 0 | 2 | Same0720, record6 and rejected task6 descriptor |
| 1023 | 2 | No0720 observed; state8e stays0, record0; channel1 still disabled |

The default captures pass the ordinary hold checker. The bit1-high capture
`tmp/nsm1-toolkit-tone-policy-high` passes protocol, native samples, observer
status and complete-storage checks under its explicitly different CFG;
it is not a baseline configuration gate. Rebuilt default capture
`tmp/nsm1-toolkit-tone-policy-low-rebuilt` passes the full hold checker.
Neither a high bit1 alone nor an ff settings byte establishes correct
unattached/silent-mode policy. Defaults remain unchanged.
The combined-input capture `tmp/nsm1-toolkit-tone-policy-both-high` also
passes protocol/native/storage/observer checks. Its task1 trace reaches
28051e with sample03ff; callbacka5 still runs once on0196 with cachedff,
and the actual tone message is still rejected before script interpretation.
Input-driven record selection and erased record content are therefore
separate observed issues; avoiding0720 is not sufficient to produce audio.
`tools/nsm1_tone_policy_contract.py` checks the own instruction/literal
anchors, descriptor, callback record, sequence arithmetic and tone descriptor;
its mutation tests reject changed branches, sequence/channel and record data.
Channel1's physical output identity and the script semantics remain
unmapped. Recover the phone-owned settings initialization or physical settings
workflow before supplying valid record contents; donor records, guessed
enable bytes and forced admission/callback state are not justified.
The concrete initialization lead is2920b0: it constructs record bytes and
calls NV writer2b8b4a with key0762 and variant indices, including6 at292100
and0 at292260. Direct-BL candidates29d452/29dc5a lead to this routine;
recover their user-visible lifecycle before invoking or reproducing it.
The qualified `tmp/nsm1-factory-reset-qualified` navigation reaches Menu,
Settings(4), Restore factory settings(6), then the Security code prompt.
Typing12345 through the matrix produces a second successful validator result
and executes both initializer callers (returns29d457 and29dc5f), without
observer calls or firmware writes. The phone returns to the settings menu.
The saved16KiB EEPROM changes190 bytes, whole-bank SHA256
`003f7ddd0966db0a90ae7fb418819ad3bb49e0d0ef272d6868bf3084100355ba`;
card and executable flash are unchanged. Its seven0762 records are:

```text
0 0009012f010401010000ffff
1 0006042fffff00000000ffff
2 0006022f00ff00030000ffff
3 000a011b020401020000ffff
4 000a042f010401040000ffff
5 0108012f010400010000ffff
6 0009012f010401010000ffff
```

These are firmware-generated settings, not donor provisioning. Record0 and6
byte5 now equals4, satisfying the decoded initial admission comparison.
`nsm1_factory_reset.lua` logs all11 matrix contacts and both initializer
entries. Its checker requires own PIN/phone authorization, all seven frames,
exact whole-bank storage transition and cold-input/config provenance. Two
focused tests reject changed/missing/extra contacts, wrong initializer callers
and authorization ordering. Reproduce with:

```sh
.venv/bin/python tools/nsm1_factory_reset_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-factory-reset-qualified --mame mame/mame
```

Use a fresh output directory. A successful reset is not evidence of audio
playback. The reset also changes
the phone-lock lifecycle: do not replay the erased-bank phone-code keystrokes
when no phone-code prompt is present. The failed first cold probe typed those
digits into the UI, introducing unrelated SIM activity, so its publication
counts cannot establish Toolkit output.

#### Reset-bank cold tone publication

`tmp/nsm1-factory-tone-serial-qualified` cold-copies only the qualified reset's NVRAM
and CFG, then selects the same external profile11. No phone-code contact or
validator transaction occurs; PIN authentication and all Toolkit text/input/
menu/tone exchanges still pass. The actual tone0a constructor296594/2c005e
runs at86.065761s, and task6 reaches admission branch2a874a with descriptor
2db038, enabled1 and cached setting4. The original erased-bank gate remains
separate and unchanged.

The85..91.5s census records18 MCU shared writes,10 native DSP shared writes,
18 compiler entries and2854 native I/O writes, with early positive controls.
The softkey click's oscillator-control value0e10 precedes the tone constructor;
the later tone sequence publishes field20=1b58 after admission, then clears
it. These are raw control values, not an established Hz scale. Five published
commits are followed by native0866/0870 writes. Ports2c/2d reach the modeled
COBBA control-select/data interface; emitted PCM or audible sound is not yet
qualified. Native47a1/IDLE0/ILLEGAL0 samples and all reviewed Toolkit pixels
pass. The cold EEPROM changes one byte,03e8:06->00, guarded by complete-bank
SHA256 `e316e0bcef54ffc57df56d8bea63668c81aca19ca022218f1b94c087de7a0111`;
card and flash remain unchanged. No record or firmware state is patched.

```sh
.venv/bin/python tools/nsm1_factory_tone_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-factory-reset-qualified tmp/nsm1-factory-tone-serial-qualified --mame mame/mame
```

Use a fresh output. The checker composes the qualified reset, exact cold
provenance/config/storage, absence of obsolete phone-code input, protocol,
pixels, admission, compiler-to-HPI projection and native acknowledgement/I/O
census. Mutation tests reject click-only/wrong-tone evidence, disabled
admission, bad lanes, missing acknowledgements and malformed/extra rows.
The next boundary is the native DSP/COBBA waveform path, not another guessed
Toolkit reply or an injected channel enable. The acquired NSE-1 mask remains
an explicit compatibility backend, not the missing fitted6150 mask.

The same gate now distinguishes data-space serial writes from DSP I/O:
it observes20..23 and30..33, with17 boot writes and16 retained positive
records, including BSPC reset/release, BDXR0aaa and ISR-producedffd5. In the
85..91.5s window, six writes are exactly three BSPC pairs
`c00c@a4a1 -> c0cc@a4a3`; no BDXR/sample write occurs. Non-destructive final
BSPC/BSPCE readback is `c8cc/0000`. The current standard BSP model decodes
that as released RX/TX, transmit-ready, and8-bit words. It does not establish
the handset's physical word encoding, clock source or analog PCM law.
TI [SPRU131G, table9-10](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
confirms the generic FO=1/FE=0 eight-bit decode. The own service manual's
16-bit PCM-frame description below is different evidence: reconcile it with
this compatibility backend before promoting the observed width to fitted
NSM-1 hardware. Do not silently change the generic BSP decode to match it.
The paired software-format selection is recovered below; the observed8-bit
choice is not, by itself, evidence that the generic BSP word-length mask is
wrong.

This reaches the same unclocked frame-delivery boundary already mapped for
the5110, but with a different control word: do not import its16-bit `c0c8`
wire fixture unchanged. Reuse the [existing clocked native-tone work](
cobba_control_boundary.md#externally-clocked-native-dtmf) as a laboratory
comparison, preserving the explicit clock assumptions and product-owned
word format. Neither serial release nor successful control publication is
proof that microphone/earpiece samples were transferred.

#### Laboratory clocked native tone

The separate `nsm1pcm` machine composes the unchanged NSM-1 compatibility
profile with the existing laboratory8kHz completed-frame source and zero
microphone words. It does not add clocks to `nsm1r4t`, set interrupt flags,
modify guest state or substitute an HLE tone. Its explicit assumptions are
recorded in `clock_assumptions.json`; no bit-clock wiring or analog encoding
is inferred from this completed-word experiment.

`tmp/nsm1-factory-tone-clock-qualified` passes the same cold provenance,
PIN/Toolkit exchanges, pixels, complete EEPROM/card/flash and control-
publication checks. The serial window now has52000 native BDXR writes at
358b plus six reset/release writes, rather than zero samples. It captures
291047 completed words overall and51998 in85..91.5s;51997 of the latter are
compared byte-for-byte with the preceding native BDXR publication. The first
window frame has no preceding write retained inside that observation window.
All frame timestamps lie on the explicit125us grid at a62.5us phase.

The before/after windows contain constant code55. The active86.14..86.35s
window contains1680 bytes with values92/b8 and a centered raw-byte1750Hz
projection of approximately0.81318 under the supplied8kHz clock. These are
periodic native output bytes, not a decoded PCM amplitude or an analog
compliance result. The8bit frame capture agrees with the standard BSP's FO
setting; the product manual/fitted-backend caveat above remains open.
External frames also change the boot ISR's first computed output to0000
from the unclockedffd5: boot waveform timing is not claimed identical.

```sh
.venv/bin/python tools/nsm1_factory_clock_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-factory-reset-qualified tmp/nsm1-factory-tone-clock-qualified --mame mame/mame
```

Use a fresh output. The gate rejects lost/duplicate/off-grid samples, wrong
native-byte projection, missing duplex frame delivery, flat or wrong-frequency
output and changed clock assumptions. The shared validators accept only the
declared NSM-1 product and laboratory machine names; ordinary locked/unlocked
gates retain their original defaults. The existing5110 paired control/clock
gate is the regression for the shared laboratory fixture implementation.
The remaining production boundary is independently evidenced frame-clock
enable/wiring and COBBA word/analog interpretation, not MCU tone delivery.

#### Bounded native output formatting

The MCU's boot compiler field28 publishes0dca at100bc, corresponding to DSP
shared085e. Resident consumera53b/a53d copies085e to1f2b. Bit0040 of this
configuration controls both thec00c/c0cc versusc008/c0c8 BSP setup ata49a
and the output-formatting call at357f..3585. The latter reaches9435 through
hook24d5. Encoder instructions and data-mask pair `ffc5/0045` atb3d7/b3d8
are byte-pinned in the compatibility images. Why the MCU selects its complete
configuration word remains a separate question; no emulator sets bit0040.

`tmp/nsm1-factory-compand-qualified` composes the full clocked gate with
passive program-read/register observations at9435,3587 and358a during
86.14..86.15s. Each site is observed80 times, retaining the first32 triples.
These are read-time register observations, not instruction-retirement claims.
The retained inputs are positive3019 and15099. Their resulting low bytes
92/b8 match an independent FFmpeg `pcm_alaw` encoding of unscaled signed16
input byte-for-byte; the formatter result remains unchanged at the serial
write point. The reference command and executable hash are recorded, and all
underlying protocol/native/frame/storage checks remain mandatory.

TI's [SPRA163A](https://www.ti.com/lit/an/spra163a/spra163a.pdf) independently
describes EXP/NORM-based A-law companding. The structural match and observed
positive-input comparison support an A-law-compatible interpretation here,
not full-domain conformance. Negative inputs, zero conventions, other levels,
the physical COBBA law and its analog transfer remain unqualified. Do not
install a production decoder or change clocks merely from these32 samples.

```sh
.venv/bin/python tools/nsm1_factory_compand_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-factory-reset-qualified tmp/nsm1-factory-compand-qualified \
  --mame mame/mame --ffmpeg /usr/bin/ffmpeg
```

Use a fresh output and an installed FFmpeg. The gate requires the exact
configuration, code/sign-mask contract, complete80/80/80 coverage, ordered
retained triples, independent reference and unchanged serial byte. Tests reject
wrong configuration, missing/malformed observations, changed code/hooks/masks
and reference-input scaling/format drift. No HLE compander supplies DSP data.

### Broader click-format comparison

The same physical menu route supplies a wider native input domain without
changing card commands or DSP data. Passive formatter/result/serial taps
during84..84.25s retain2000 ordered triples, with unchanged serial output.
The full clocked cold/protocol/storage/frame gate remains required.

Of these accumulator inputs,1984 lie within signed16 range and cover538
distinct values. Sixteen positive accumulator values exceed32767; the
reference excludes them rather than truncating them into artificial negative
samples. The one actual negative input,-13485, matches the independent
FFmpeg A-law result3f. All833 zero inputs produce55 instead of reference d5.
Five positive inputs also differ:

| Native input | Native byte | FFmpeg byte |
|---|---|---|
| 2063 | 95 | ea |
| 2059 | 95 | ea |
| 8254 | b5 | 8a |
| 8237 | b5 | 8a |
| 8217 | b5 | 8a |

These are retained discrepancies, not a full-conformance acceptance claim.
The checker requires their exact classification, source code/sign-mask
contract and independent reference; it neither rewrites native bytes nor
changes the reference's input scale.

The bounded arithmetic audit explains all2000 native results directly from
9435..944b. The ROM selects maskffc5 only for a strictly positive input,
otherwise0045. After ABS, EXP gives30-floor(log2(magnitude)) for nonzero
values; the ROM caps it at23 and derives a shift by subtracting23.
NORM applies that shift, then SFTA shifts right3. The negated shift supplies
the segment field XORed at bit4. If that field is zero, the ROM shifts once
more and uses segment1. Zero follows this special case with mask0045,
giving55, not d5. For2063, the intermediate shift is-4, segment4 and
quantized value16; `16 XOR (4<<4) XOR ffc5` yields low byte95.
For8254, shift-6, segment6 and quantized16 yieldb5. These exactly explain
the observed boundary differences without changing the CPU or reference.

The `--steps` observer independently retains native read-time intermediate
registers for eight cases:0,-13485,37738 and the five positive differences.
EXP/T, normalization, restored magnitude, segment, sign-mask address and
unchanged return/serial results agree with the arithmetic model. Literal
code/mask bytes remain pinned. TI SPRU172C's EXP and NORM descriptions
support the used exponent/shift semantics; this is not an instruction-
retirement trace or proof of every CPU case. The arithmetic reference is
analysis-only, never a production sample source. Full-domain behavior and
the physical codec convention remain separate questions. In particular,
zero55 must not be silently replaced with d5 on an assumed codec path.

```sh
.venv/bin/python tools/nsm1_click_format_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-factory-reset-qualified tmp/nsm1-click-format-qualified \
  --mame mame/mame --ffmpeg /usr/bin/ffmpeg
```

Use a fresh output. `click_format_result.json` records the bounded
classification and reference/mask identities. Physical codec law, full
signed-domain behavior and native speech remain unproved.
Add `--steps` with a fresh output to require the eight native intermediate
cases as well as the complete2000-triple comparison and underlying boot gate.

### Active native tone restoration

`tools/nsm1_tone_replay_check.py` composes the complete clocked cold-boot
gate with an active-tone save at86.2s and load after86.25s. The54 selected
MCU/DSP register and non-destructive data fields restore exactly. Both legs
produce the same400 output words and absolute125us-grid timestamps,400 native
serial writes and22 DSP I/O writes. One frame-source observation repeats;
no shared-memory/compiler publication or physical contact occurs inside
either leg. The saved state must exist and all Lua observers must complete.

Lua observation counters survive load. The checker retains raw logs and
removes only the proved-identical replay interval in memory, subtracting
its exact counts from the two observer summaries before invoking the
unchanged ordinary waveform checks. It does not relax the291047-word
capture requirement or any storage, SIM, configuration or frame oracle.
Raw counts and the complete gate result are retained in `restore_result.json`.

Save/load can wake `emu.wait` early with false. `fixture_deadline.lua`
rechecks the deadline rather than treating that wakeup as completion or an
error. Literal relative delays retain their original first wait: converting
`.15` to `(now+.15)-now` changed earlier click samples at a scheduling
boundary. A fresh no-restore control passes the original gate unchanged;
the restore observer also executes a fake-clock false-wakeup helper test.

```sh
.venv/bin/python tools/nsm1_tone_replay_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-factory-reset-qualified tmp/nsm1-factory-tone-restore-qualified \
  --mame mame/mame
```

Use a fresh output. This proves selected active native state and laboratory
output replay, not all memory contents, physical clock wiring, analog
audio, the fitted6150 DSP mask, native RF or call speech.

### Pending Toolkit command restoration

An independent74s cold profile1 process saves at65s while the organically
fetched DISPLAY TEXT is awaiting physical dismissal. The pre-save frame
matches the reviewed `DCT3 SAT` prompt. Own ARM PC2b61fa/SP112a18 and active
DSP PC479e/IDLE0 are observed; all37 ARM registers,36 DSP fields and rolling
checksums of backed128KiB research ARM RAM, ordinary DSP data and program
restore exactly. The2.7s reference/restored windows compare1170 DSP I/O writes
including relative times, addresses, values and masks.

Only one TERMINAL PROFILE and one FETCH occur. Each branch's separate physical
left-softkey contact produces the same successful terminal response, accepted
with9000, without refetching the command. Both resulting frames match the own
clean Headset idle oracle exactly. The complete EEPROM, original persistent
card and executable flash are unchanged. Cold provenance, unrelated CFG,
PIN/phone acceptance before save, exact APDUs, payload/contact ordering and a
nonempty saved-state artifact are checked rather than inferred from pixels.

```sh
.venv/bin/python tools/nsm1_toolkit_state_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-toolkit-native-state-accepted
```

Add `--mame mame/mame` with fresh outputs to execute. Save/load uses MAME APIs;
no guest state or firmware result is forced. The replay protocol validator is
shared with the separately qualified6190 capture, but terminal profile,
authentication, backed-RAM extent, storage and pixel contracts remain own.
This covers a fetched proactive command awaiting input, not an in-flight SIM
character, every Toolkit command or exhaustive device-state restoration.
Provisioning, native-mask, sustained-operation, RF and speech limits remain.

### Native idle restoration and physical continuation

An independent54s cold process from the qualified original PIN-enabled
`tmp/nsm1-pin-menu` storage authenticates PIN1234 and separate phone12345,
without an application key before save. Save at40s records own ARM
PC2b61fc/SP112a18; load exactly restores37 ARM registers,36 DSP fields
and rolling checksums of the backed128KiB ARM research RAM, ordinary DSP
data and program. The2.7s reference/restored windows contain7771 identical
DSP I/O writes, including relative timestamps, ports, values and masks.
Physical left-softkey/Menu in both branches renders the reviewed Messages
frame `51a20a16acead610184225c78632724f9f20025565bd29a93beffec0c271d3a3`.
The own logical-key table2e0b18 supplies this contact; no sibling key mapping
is assumed.

The shared validator now accepts an explicit backed-RAM limit,120000 here;
its default remains110000 for existing64KiB products. The own stack112a18
cannot be validated under that default. Three focused tests preserve this
distinction and reject out-of-range stacks, architecture/replay/input/protocol
mutations, missing records and changed complete storage. The observer includes
all128KiB of the current research map, not a claim that fitted SRAM aliases
have been established. Register names in complete validated architecture
records are parsed as data; actual execution/observer errors remain failures.

The full16KiB EEPROM remains SHA256
`44493d3a303c99a1bc87d3320bc01bf292b7e18b581a2f17078159a12096987c`;
own flash, original complete3570-byte laboratory card and external input
configuration remain unchanged. Only persistent device files and CFG are
copied into the fresh process; the generated save artifact is nonempty and
hashed. Existing diagnostic-template policy03ca=00, nominal ADC2=554,
own retry-byte fixture and unproved NSE-1 mask compatibility are retained,
not promoted to factory provisioning or healthy RF operation. Checksums are
not bytewise memory comparison, and bounded I/O equality is not exhaustive
device-state or sustained-operation coverage. No firmware result, memory
poke, timer completion or radio response is supplied.

```sh
.venv/bin/python tools/nsm1_native_state_check.py tmp/nsm1-pin-menu \
  tmp/nsm1-native-idle-state --mame mame/mame
```

Omit `--mame` to recheck that capture; execution requires a fresh output
directory. Normal handset support and native RF/speech claims are unchanged.

### Native executable upload extent

The own v5.23 loader writes 605 distinct program words in `2800..2a5c`.
The former `2800` overlay boundary discards these writes, exposing resident
ROM in program space and zeros in data space instead. A read-only upload
observer confirms, for example, uploaded `2800=1221` versus old program
`a594` and data `0000`. Six coincidentally matching zero values do not
constitute retention of the upload.

The research `nsm1r4t` configuration selects a `3000` overlay extent;
`nsm1_native_upload_observe.lua` confirms all 605 words are visible in both
views at one second. This upload-derived bank model does not identify the
fitted DSP mask or establish the complete physical RAM geometry.
The matched 40-second old/corrected runs use only the qualified own
FDN-enabled EEPROM/card and physical listed-number dial inputs. Both pass
the existing authentication, published-record and exact Calling-frame
checks with unchanged SIM contents; only the corrected run passes
`nsm1_native_upload_check.check_upload`. Its 618 observed first opcode words
all have decoded boundaries, including observed extension variants. Neither
decoding coverage nor the local Calling frame establishes connected calls,
speech, stock provisioning or native RF acceptance.

The corrected bank also passes the 165-second native packet/queue qualification
in a 170-second cold process. `nsm1_native_upload_radio_long.lua` retains the existing
physical authentication/Menu and long radio observer, adding only passive
upload-retention observation. Ten packets include two native scan-result
cycles, identical measurement bodies and the expected intervening quiet
window; no restart is observed. The own EEPROM link and policy byte `03ca=00`
are unchanged. Thus the timeout-policy caveat below still applies: this is
bounded operation with inherited diagnostic provisioning, not stock-phone
stability, acquisition, connected calls or speech.

The corrected 45-second live-program SHA256 is
`1d3506034405fb546cb8b4c60a26f6196e2cd5c9400217fc856dd6ce3aabf107`.
`nsm1_native_radio_check.py --retained-upload` selects that exact profile
and additionally requires all 605 uploaded words retained. The historical
profile below keeps its own hash; cross-selecting either capture is rejected.
Neither profile admits arbitrary program hashes or weakened packet/queue
predicates.

```sh
.venv/bin/python tools/nsm1_native_radio_check.py \
  tmp/nsm1-native-upload-radio-long --long-run --retained-upload
```

Physical/cold software coverage includes SIM contacts, a distinct Phone-memory
contact save/read and separate erase, PIN1/PIN2 and PUK recovery, request
settings, offline SMS storage and
PIN2-protected FDN activation/storage/local dial enforcement. Registered
radio, connected calls and network SMS remain unaccepted.

The native frontier is now the RF-input/qualification contract. A bounded
105-second own run observes sustained late queue activity and a162-byte
native measurement packet, without observed restart or qualifying result.
Fitted resident-mask compatibility, flash attributes/address aliases and
physical RF/electrical calibration remain unproved.
A 6110 profile is not a safe substitute: this image uses a larger flash
verification stream and stack addresses outside NSE-3's 64 KiB SRAM.

## Executable compatibility boundary

### Native radio baseline

`nsm1_native_radio_observe.lua` combines physical PIN/phone/menu input with
an own MCU dispatcher probe at2c1b96 and passive shared-ROM4 candidate state.
The105s run receives types74 four times,8a/87 once at2.563s and8b once at
49.097797s. The latter has162 body bytes: first word0080 and40 channel/value
pairs whose signed values are-129/-130. This differs from the6130 inventory;
do not import its report timing or supervision-reset deadline.

Native counter1262/1263 stays zero through45s, then isffffcf03 at65s,
ffffe42c at90s andffffeca2 at100s. Between65 and90, each queue cursor has5417
writes; between90 and100, each has2166. Those increments equal the counter
steps, and late cursors agree. Sampled1344/1266 remain zero and219b is0574.
These are bounded queue/candidate-field observations, not validated RF units.
The separate165s capture `tmp/nsm1-native-radio-long`, using only the existing
physical PIN/phone/Menu composition, observes a second native8a/87 exchange at
122.883660923/122.883774231s and a second8b measurement at161.230732615s.
The8a payload changes from0202 to0375; the second8b reproduces the original
162-byte measurement payload. No restart request at the own2c2a02 entry is
observed through165s. The extended checker qualifies this bounded repeated-scan
result; it is not an unbounded stability or RF-acquisition claim.
Own dispatcher2c1b96 increments context112860 halfword+2; timer deliveryDC
tests it at2c1cc2, rearms timer1c/ticks09cd and clears it when nonzero, otherwise
calls policy2b4b72 with argument1. The policy includes further conditions before
reason68, so the6130 reset deadline must not be inherited from this structural
similarity. No keepalive, counter write or disabled supervision was added.

```sh
.venv/bin/python tools/nsm1_native_radio_check.py tmp/nsm1-native-radio-long --long-run
```

`nsm1_native_radio_long.lua` extends the existing own observer to120/140/160s
and observes the own restart-request entry without modifying guest state.
The gate requires all ten complete packets, identical measurement payloads,
late queue activity followed by the second quiet scan window, physical
authentication/Menu, unchanged card/flash, own live program hash and165s
horizon. Missing/malformed second-cycle packets or a restart request fail it.

An independent100s passive policy capture, `tmp/nsm1-native-liveness`, observes
packet-window counts0,1,0. Both empty windows call2b4b72 with argument1 from
2c1b95, read policy bytes0002 and return with selected reason0. The own literal
at2b4f34 identifies record0607. The first byte's bit0 is clear:2b4ba8/2b4baa
branches past reason68 selection, then2b4bc8 reaches the rearm/clear tail.
This explains the observed absence of the6130-style reset in this6150 fixture;
it does not prove the policy bytes represent a factory default or fitted phone.
The policy source is now qualified below. No policy is changed.

```sh
.venv/bin/python tools/nsm1_liveness_check.py tmp/nsm1-native-liveness
```

The gate pins own instructions/literals, the complete window/policy census and
ordering, physical authentication/Menu, native packets/program, unchanged card
and100s horizon. A configured packet-timeout bit or missing observations fails
this specific fixture gate rather than being silently treated as equivalent.

The own directory at2da48c selects group6 table2da0c4 (eight descriptors).
Descriptor2da0fc is `00000607 03ca 0001`: a one-byte EEPROM field, not a
firmware constant. Reader2b8b90 resolves it and calls raw serial reader2bcace.
The independent100s `tmp/nsm1-native-liveness-source` capture observes the
startup reader plus both empty-window requests; every raw read uses offset03ca
and length1. Both policy reads return success1/byte00 before the policy sees
0002. The second byte is supplied separately; it is not a two-byte0607 field.

Byte03ca is00 in the hash-qualified original NokiX repair template, its
checksum-repaired image, the pre-launch fixture and the saved EEPROM. Thus
packet-timeout reset suppression is inherited template policy, not evidence of
a healthy DSP link or an independently established factory setting. The165s
repeated-scan result remains valid only under that explicit policy. No enabling
or disabling fixture is introduced without an evidenced provisioning contract.

```sh
.venv/bin/python tools/nsm1_liveness_check.py tmp/nsm1-native-liveness-source \
  --policy-source \
  roms/research/nsm1-v523/original-eeprom/NokiX/scripts/scripts/misc/repair_external_eeprom/nsm-1.bin \
  roms/research/nsm1-v523/nsm1-security-checksum-valid.bin
```

The own45s live program SHA256 is
`b03957d9a60e9e3048826fa16fc407422b87407688cbc0cab75ee31a95f6a89e`.
Gateway2420 targets uploaded0a8d, not the6130's0a71. The recovered comparison
fragment `6ff8 219b 0d20` occurs at0b25, not the6130's0ae6; PORTR0027 remains
at330a. The own0b47 sequence selects1262 and increments it, independently
qualifying the counter interpretation.

The separate `nsm1_scan_metric_observe.lua` run observes eight own0b25
comparison entries (exposed PC0b26) at50.0017..50.0341s, all A=0 against
threshold219b=0574, with equal cursors advancing four words each time.
Port27 reads at330c/3312/3318/331e are zero; the first comparison snapshot
contains exactly72 zero queue words. Live4701 consumes that queue and47e6
performs its reduction. Uploaded0b25..0b4f compares the reduction, conditionally
increments1344 and advances the scan counter. The backend routes port27
to COBBA's unattached RF callback, whose default value is zero.
This locates the observed zero-score source; it does not prove arithmetic
silicon fidelity, physical RF units, component ordering, sample-clock rate
or receive-window wiring. Those contracts remain necessary before supplying
a waveform; neither guessed noise nor a threshold-crossing score is evidence.

```sh
.venv/bin/python tools/nsm1_native_radio_check.py tmp/NATIVE_RADIO
.venv/bin/python tools/nsm1_native_radio_check.py tmp/SCAN_SOURCE --metric-source
```

The checker pins the own live image, complete packet/sample inventory,
105s horizon, physical authentication, unchanged card and Menu pixels.

### Published-record diagnostic acceptance

`make_nsm1_record_profile.py` requires the exact checksum-consistent own
repair template and a fresh control whose native exchanges match the
independent ROM4 arithmetic. It encodes the repository's attributed public
numeric example with the measured compatibility-chip field00160010, applies
the own retained-record transform and updates the low-block checksum.
Only EEPROM0020..0037 and003c..003f change; identity/calibration are preserved.
This external input is an explicit diagnostic, not a donor or factory image.

In `tmp/nsm1-published-record`, the validator reaches27e798 with validity1
for3532, native3534 update and post-restart3532, without taking the prior
27e796 rejection. Firmware requests exactly one reason04 restart from242c91.
The separate `tmp/nsm1-published-record-cold` process receives only saved
EEPROM, validates one3532 with no update/restart, and leaves the entire16KiB
EEPROM byte-identical. Decoded record fields, private markers, update checksum,
retained encoded echoes and final storage checksum pass executable checks.
This does not yet qualify a copied accepted-context object or graphical boot.

```sh
.venv/bin/python tools/make_nsm1_record_profile.py tmp/nsm1-record-baseline \
  roms/research/nsm1-v523/nsm1-security-checksum-valid.bin \
  tmp/FRESH/nvram/nsm1r4t/eeprom
.venv/bin/python tools/nsm1_published_record_check.py \
  tmp/nsm1-published-record tmp/nsm1-published-record-cold
```

The earlier rejection traces below remain negative controls of the unmodified
template, not the current diagnostic frontier. The own analog comparison
below resolves the next startup dependency without promoting physical units.

### Own analog comparison and graphical presentation

The report14 call223a74 is in a shared tail, not exclusively owned by input21's
limited-fast-read setup. Other direct branches enter that setup at223a0a and
223a12. The ordinary measurement loop calls28a884/28a7b0 and decrements byte
11089a only after both converted values satisfy signed1800..5500 limits.
Source7 maps through the own table2e0b3c to CCONT ADC2; source8 may be cached.
Scale is1500/232. Live coefficients at11fe3c/11fe40 are3f8033c4/00000000,
not the sibling's identity calibration.

The saturated control returns19dc/19dc, validity0 and counter10 unchanged.
The explicit nominal554 hypothesis returns0dfd/0dfd, validity1 and a decrementing
counter, then reaches223a68 and posts the startup report organically. The
research-only input defaults to1023 and exposes554 as the existing nominal
3.6-V/typical6.5-mV-per-count comparison, not measured6150 ADC calibration.
Normal handset declarations are unchanged. No identity/calibration byte is
edited and no report, task resume, timer result or code selection is injected.

Fresh `tmp/nsm1-nominal-qualified` reproduces the full84x48 Security code frame
at8s, RGB SHA25624cbab0ad5b13376772a44f0eec7835a7ea865795c9ee229c7631fa2e0ea1fb2.
Native record validation still passes and SIM SELECT responses occur.
Presentation alone is not proof of keyboard delivery, full SIM initialization
or interactive idle; the separate physical-input acceptance is below.

```sh
.venv/bin/python tools/nsm1_nominal_input.py tmp/nsm1-published-record-cold tmp/FRESH
# Run nsm1r4t for15s with tools/nsm1_analog_observe.lua and the fresh run directory.
.venv/bin/python tools/nsm1_analog_check.py \
  tmp/nsm1-analog-coefficients tmp/nsm1-nominal-qualified
```

### Physical security code and Messages menu

Own matrix contacts submit ASCII12345 through29faf3 ->2bcde8. The comparator
matches, but the unchanged repair template's retry byte2b is rejected by
2bcd94's signed comparison against5. The later extra Left key submits an
empty string; that negative control is explicitly checked, not ignored.

The own security record loads EEPROM03cc..03d3 into112820. Nested record070a
maps112824 to03d0, one byte. Initializer29d6d4 copies the eight-byte ROM
template2dde40 before credential construction; its retry byte is0. The
explicit `--initialize-phone-retry` fixture therefore changes only03d0 from
2b to0, not the stored code, lock setting, identity or calibration. It is an
own-initializer lab fixture, not an assertion that the historical template
represents an authentic factory phone. `nsm1_security_contract.py` pins the
initializer, validator and own descriptor mapping.

Physical12345/Left reaches match1/retry0 and accepted return2bcde6=1.
A subsequent physical Left opens Messages. The full84x48 frame at18s has
RGB SHA25651a20a16acead610184225c78632724f9f20025565bd29a93beffec0c271d3a3.
A separate cold process copies the entire saved EEPROM without reinitializing
the retry byte and repeats code acceptance and exact menu pixels. The
security block remains unchanged by both accepted runs; other housekeeping
writes occur and are not declared fully understood. Own flash stays exact.
No software input event, callback result or firmware RAM byte is injected.
Full SIM/security parity, network/calls/SMS and stable native radio remain
unaccepted. Physical phonebook persistence is separately qualified below.

```sh
.venv/bin/python tools/nsm1_nominal_input.py tmp/nsm1-published-record-cold \
  tmp/FRESH --initialize-phone-retry
# Run22s with nsm1_security_input.lua and -debug -debugger none.
.venv/bin/python tools/nsm1_security_check.py tmp/nsm1-security-physical \
  tmp/nsm1-security-qualified tmp/nsm1-security-cold-qualified
```

### Physical PIN1 and cold retry persistence

`nsm1_nominal_input.py --pin-enabled` clones a current-format synthetic card
from the qualified input, requiring default PIN1234 and fresh3/3/10/10
counters. It changes only the card's PIN-request enable bit; it does not
alter handset firmware, stored phone code or retry counters. The guarded
`nsm1_sim_observe.lua` supplies one bounded UART stream when input fixtures
are composed.

Physical4321/Left transmits `A0 20 00 01 08` plus padded4321 and receives9804.
The complete3570-byte card changes only PIN1 retries3->2. An independent
cold process preserves that exact card and saved EEPROM, then physical1234
receives9000 and restores only retries2->3, reproducing the entire initial
card. Both cold inputs display Enter PIN code; rejection displays Code error
and successful authentication reaches the separate Security code prompt.
This fixture does not submit the phone code or equate SIM authentication
with phone unlock. Separate `nsm1_pin_menu.lua` composes physical1234,
phone12345 and Menu in one cold boot. Its gate requires VERIFY before phone
submission, phone acceptance before application input, an unchanged full
card, preserved cold EEPROM input and the exact Messages frame at24s.
The shared UART observer is installed once across both physical fixtures.
Two further physical wrong-PIN cold boots produce retries1 then0
(`9804`, then `9840`). From that exhausted saved card, physical
PUK12345679/new PIN4321/confirmation returns9804 and changes only PUK
retries10->9. A separate positive control from the same exhausted input
enters PUK12345678 and installs4321 with9000 and PIN retries0->3.
An independent cold boot accepts4321 without changing any card byte.
`nsm1_puk_check.py SOURCE SECOND EXHAUSTED WRONG RECOVERY COLD`
checks physical contacts, exact UNBLOCK APDU, whole-card transitions,
saved EEPROM links, executable flash and native validation dialogue.
No retry counter is initialized or changed by the harness.
Ordinary Change PIN is also accepted: physical authenticated menu4-5-6,
selection of Change PIN, old1234/new4321/confirmation produces exact
`A0 24 00 01 10` with both padded credentials and9000. Only PIN1 bytes
change in the complete card. The screen confirms PIN code changed;
independent cold4321 authentication preserves the full card.
`nsm1_pin_change_check.py CHANGED COLD` checks the physical authentication
order, APDU, whole-card transition, EEPROM link and exact confirmation.
Physical menu4-5-1 also accepts PIN-request Off and On: exact authenticated
DISABLE/ENABLE CHV commands (`A0 26/28 00 01 08`, padded1234) return9000.
The entire card changes only its enable byte. Cold Off skips SIM VERIFY,
but still requires the phone code and reaches Messages; physical On then
restores the next cold PIN challenge and successful1234 authentication.
All four runs preserve the saved card/EEPROM chain.
`nsm1_pin_request_check.py OFF OFF_COLD ON ON_COLD` checks this cycle.
The earlier no-PIN menu has its own captured icon-animation phase oracle.
Change PIN2 uses the observed menu4-5-6 third entry. Physical authenticated
5678/new8765/confirmation sends exact `A0 24 00 02 10` and9000; only PIN2
bytes change. An independent cold process accepts8765 to restore5678.
A separate wrong5675 control changes only PIN2 retries3->2 with9804;
cold correct5678 restores retries and produces the exact positive card.
`nsm1_pin2_check.py CHANGED RESTORED WRONG RETRY` checks all four runs,
whole-card/EEPROM links, credential ordering, exact APDUs and own
PIN2 code changed/Code error frames.

Two additional cold wrong-PIN2 attempts exhaust retries through9804 then
9840. Physical PUK2 entry at the same menu sends `A0 2C 00 02 10`:
wrong87654322 changes only PUK2 retries10->9; correct87654321 installs
8765, restores PIN2 retries and returns9000. Firmware then automatically
verifies the new PIN2. An independent cold process accepts8765 to restore
5678. `nsm1_puk2_check.py FIRST SECOND EXHAUSTED WRONG RECOVERED COLD`
checks physical authentication, exact command ordering, complete card and
EEPROM links, native record validity and executable flash. Retry counters
are never initialized by these fixtures. Protected application operations
remain separate targets; network services are not accepted.

Permanent PUK2 loss has a separate physical cold-chain gate. It requalifies
the three wrong PIN2 attempts and first wrong PUK2 above, then performs nine
more independent cold attempts with87654322/new8765, always using the actual
predecessor's complete saved card/EEPROM and unchanged own flash. Exact UNBLOCK
commands return9804 until terminal9840. Only PUK2 retries change, ending
PIN1/PIN2/PUK1/PUK2=3/0/10/0; no counter or credential is edited by the harness.

A further cold process authenticates PIN1 and the separate phone code, then
selects Change PIN2. It admits no CHANGE/UNBLOCK or other storage mutation,
retains every card byte and shows the reviewed Change PIN2 item at32s.
Physical Down34..34.2 moves to Change barring password at38s and47s. These
full menu frames match the independently reviewed6130 resources; this proves
bounded52s UI/security operation, not hardware/mask equivalence, radio or speech.
Every cold link checks prelaunch hashes, copied domains (NVRAM/config only),
native record validation, own executable and complete card transitions.
New cold links additionally require healthy captured console output; a zero
MAME exit status cannot hide a Lua or validity error. Older seed captures
retain their separately qualified trace-based acceptance.

```sh
.venv/bin/python tools/nsm1_puk2_exhaust.py tmp/nsm1-pin2-wrong \
  tmp/nsm1-pin2-second-wrong tmp/nsm1-pin2-exhausted tmp/nsm1-puk2-rejected \
  tmp/nsm1-puk2-exhaustion --check-only
```

Use a fresh output without`--check-only` to execute, or`--resume` to requalify
retained predecessors. Completed verbose logs are losslessly compressed;
read-only qualification accepts plain or gzip logs. Source hashes prevent
accidental input reuse, not cryptographic execution attestation. Permanent
PUK1 loss has a separate gate below; native RF remains unqualified.

The permanent PUK1 gate starts with the retained first physical wrong PIN1,
requalifies both subsequent wrong PINs and the first wrong PUK1, then performs
nine further cold12345679/new4321 attempts. Every process receives the actual
predecessor card/EEPROM/configuration without RAM or save state. UNBLOCK returns
9804 until the final9840; only PUK1 retries change, ending0/3/0/10. No blocked
counter or credential is seeded by the exhaustion harness.

The terminal24s frame is **SIM card rejected**. A further cold process renders
the exact same full frame at5s and24s, before and after the physical input
control. It transmits no VERIFY/CHANGE/UNBLOCK or card-write command and
preserves every card byte. Native record validation, own flash identity,
prelaunch hashes, complete-card transitions and execution health remain required.
The shared NSM-1 cold-copy/run helpers are reused from the PUK2 gate rather
than introducing another machine/transport implementation.

```sh
.venv/bin/python tools/nsm1_puk1_exhaust.py tmp/nsm1-pin-wrong \
  tmp/nsm1-pin-second-wrong tmp/nsm1-pin-exhausted tmp/nsm1-puk-wrong \
  tmp/nsm1-puk1-exhaustion --check-only
```

Fresh execution/resume and log compression follow the PUK2 runner's rules.
The PUK transition checker rejects exhausted counter inputs and models a
successful recovery as restoring PUK1 retries to10 as well as PIN1 to3;
the existing positive recovery/cold new-PIN control remains accepted. These
bounded52s software-security results do not promote fitted-mask/clock fidelity,
native RF, registration, calls, network SMS, speech or long-run stability.

The base card does not advertise FDN. The explicit extended laboratory
profile adds service3, EF_FDN6f3b and retained ADN validity without changing
the legacy card layout. Physical menu4-5-3 opens On/Off/Numbers; selecting
On then entering PIN25678 sends VERIFY CHV2 and INVALIDATE ADN, both9000.
The complete card changes only extension byte3575 from1 to0 and the same
session displays On. Cold initialization requires the GSM11.11 service3
session mechanism: the enabled card invalidates both IMSI and LOCI once,
no later than selecting either. Firmware observes invalid LOCI in2039b8,
sets111e9c, rehabilitates LOCI/IMSI and then reads ADN validity at203ca8.
The invalid-ADN branch203ce4 publishes FDN enabled through10c048. Without
that card lifecycle,203c3c skips the validity parser and publishes Off;
allowing reads of invalid ADN alone cannot fix that earlier gate.

`nsm1_fdn_check.py ON ON_COLD OFF OFF_COLD` verifies physical PIN1 and
phone-code authentication, PIN2-gated ADN INVALIDATE/REHABILITATE, the
two session identity rehabilitations, exact complete-card transitions and
EEPROM links. The enabled cold boot shows On; physical Off and a following
cold boot show Off. Only retained ADN validity changes, with both
phonebooks unchanged. Session identity flags are not retained card data.
FDN Numbers/Add entry requires PIN2 before opening Name/Number. Physical
5678, A/12345 transmits exact `A0 DC 01 04 20` with the32-byte FDN record
and9000. Only the first FDN record changes; ADN, credentials, retry counters
and validity remain intact. A separate cold Numbers/Search/List/Details
run displays selected A and12345 with no update or changed card byte.
The same-input wrong5675 control displays Code error, sends no UPDATE,
and changes only PIN2 retries3->2.
`nsm1_fdn_record_check.py WRITE COLD WRONG` checks physical authentication,
exact payload and outcome, complete-card changes, cold EEPROM link and
four exact save/list/detail/error frames.

The entry's Options path also authenticates PIN2 before exposing Edit/Erase.
Physical Erase reaches Erase? A; Back preserves the entire card and returns
to the selected A list without UPDATE. Confirmation sends one
`A0 DC 01 04 20` plus32FF bytes,9000 and displays Memory erased. A separate
cold search displays No phone numbers and does not rewrite the card.
`nsm1_fdn_erase_check.py SAVED CANCEL ERASE COLD` checks full EEPROM/card
links, physical PIN1/phone/PIN2 ordering, exact APDUs, confirmation/cancel/
erase/empty frames and unchanged unrelated files. The write and erase
checkers share the physical-authentication assertion.
Physical Edit authenticates PIN2 at Options, retains Name A, deletes the
last number digit and enters6 before Save. The complete card changes only
byte3600 fromF5 toF6, updating12345 to12346 in the same FDN record.
`nsm1_fdn_record_check.py WRITE COLD WRONG --edit EDIT --edit-cold EDIT_COLD`
checks the exact protected UPDATE payload, physical edit contacts, full
card/EEPROM links, save confirmation and independent cold12346 Details.
No unrelated file, credential or validity flag changes.

Local dial enforcement is accepted with a three-way physical control.
After enabling FDN on the card containing A/12346, cold physical12346/Send
reaches Calling. Same-input cold54321/Send displays Call not allowed.
Physical FDN Off changes only ADN validity; another cold54321/Send now
reaches Calling. Send is the own row2/column0 contact (logical0e), not a
volume key. All three dial attempts preserve the entire card; exact
authentication, session rehabilitation, matched EEPROM inputs, executable
flash and distinct26s frames are checked by
`nsm1_fdn_dial_check.py SOURCE LISTED UNLISTED DISABLED OFF_CALL`.
Calling here establishes passage through local FDN validation only. There
is no registered native radio link, connected call or native speech claim.

Wrong-PIN2 activation is separately controlled: from FDN Off, physical5675
returns9804, shows Code error, changes only PIN2 retries3->2 and sends no
INVALIDATE/REHABILITATE or UPDATE. An independent cold process preserves
that exact card/EEPROM, then physical5678 restores retries and enables FDN.
The complete resulting card differs from the original input only in ADN
validity. Add `--rejected WRONG --retry RETRY` to `nsm1_fdn_check.py` to
verify both controls alongside the four-run activation cycle.

```sh
.venv/bin/python tools/nsm1_nominal_input.py tmp/nsm1-security-cold-qualified \
  tmp/WRONG --pin-enabled
# Run19s with nsm1_pin_input.lua and debugger enabled.
.venv/bin/python tools/nsm1_nominal_input.py tmp/nsm1-pin-wrong tmp/COLD --preserve-sim
# Run19s with nsm1_pin_reload.lua and debugger enabled.
.venv/bin/python tools/nsm1_pin_check.py tmp/nsm1-pin-wrong tmp/nsm1-pin-cold-correct \
  --menu tmp/nsm1-pin-menu
```

### Authenticated offline SMS

After physical PIN1 and phone-code acceptance, `nsm1_sms_input.lua` opens
Messages/Write messages, enters A and selects Save. The exact unsent SMS
record is `070281f001ff008100000141` padded to176 bytes. Firmware selects
EF_SMS6f3c and issues one `A0 DC 01 04 B0` with that payload; status9000
and Message saved agree with persistent storage. Only the initially empty
first SMS slot changes in the whole3570-byte card; ADN and credentials remain
unchanged and the other nine SMS records stay free.

The independent cold run preserves the entire saved EEPROM/card, authenticates
both credentials again and reads A from Outbox. It issues no UPDATE RECORD,
leaves the complete card byte-identical and matches the established exact
editor/read frames. Shared protocol checks are parameterized only by observer
labels/capture bounds; no sibling firmware address or menu table enters the
driver. This is offline storage/UI acceptance, not submitted/delivered network SMS.
Received-message behavior and radio remain separate targets.

```sh
.venv/bin/python tools/nsm1_nominal_input.py tmp/nsm1-pin-menu tmp/WRITE --preserve-sim
# Run33s with nsm1_sms_input.lua and debugger enabled.
.venv/bin/python tools/nsm1_nominal_input.py tmp/nsm1-sms-draft tmp/COLD --preserve-sim
# Run29s with nsm1_sms_reload.lua and debugger enabled.
.venv/bin/python tools/nsm1_sms_check.py tmp/nsm1-sms-draft tmp/nsm1-sms-cold
```

The authenticated erase/cancel extension reaches the exact Erase message?
confirmation. Physical Back cancels, returns to A and preserves the entire
card with no UPDATE RECORD. A later independently cold confirmation issues
one exact176-byte update with the stored record's status07 changed to00;
all TPDU/padding and unrelated card bytes remain intact. This is a free-slot
transition, not secure deletion of message contents. The next cold process
again authenticates both credentials, displays exact No messages pixels and
performs no record write. Complete card/EEPROM cold links, own flash, native
record validity and input/protocol order pass for all three phases.

```sh
# Run32s with nsm1_sms_cancel.lua, then preserve its saved inputs for
# nsm1_sms_delete.lua (32s), then nsm1_sms_empty.lua (28s).
.venv/bin/python tools/nsm1_sms_delete_check.py tmp/nsm1-sms-draft \
  tmp/nsm1-sms-cancel-qualified tmp/nsm1-sms-erased tmp/nsm1-sms-erased-cold
```

### Physical SIM phonebook persistence

`nsm1_phonebook_input.lua` drives own Names/Add entry contacts after accepted
phone code, entering A and12345. The actual UART stream contains exactly one
`A0 DC 01 04 20` plus the expected32-byte EF_ADN record; card status is9000.
Record1 contains name byte41, length/type/BCD04812143f5 and erased padding;
records2..50 remain erased. The capture uses a4096-byte bound because the
complete write conversation exceeds1024 bytes; a reached cap fails the gate.

The independent cold process copies the complete saved EEPROM and synthetic
card, accepts the phone code again, and drives Names/Search/List/Details.
Its full84x48 Number frame renders12345 (RGB SHA256a089a5b358dd43b2f090d972dadb617c935180f95923efa43f99e9e330c055d0).
It performs no UPDATE RECORD and leaves the complete card byte-identical.
Own flash, physical input sequence, code acceptance, native record dialogue,
exact APDU payload and cold input links are checked independently.

The selected SIM name is **A**, white-on-black in rows7..16. It is not
blank or corrupted. The original-template list additionally contains two
unselected EEPROM-backed Phone rows with garbage names. Formatter259e78
runs mode2/index1 for the selected SIM contact, then mode1/index1 and index2.
For those latter rows, index helper259c9a returns0/1 and reader259ec8 selects
record0728/selector7ff0. Its first32 bytes match the original template at
physical19d4 (descriptor size37); field+16 begins2a079700. The shared buffer
111cb8 is reused between rows. That reuse is not evidence that the selected
SIM name was overwritten on-screen.

Physical Names/Erase/Erase all/Phone/confirmation asks for12345 again;
both submissions are accepted and Memory erased appears at30s. Firmware
changes the Phone record family while preserving the low encoded block,
security record and entire SIM card, with no SIM UPDATE RECORD. Independent
cold entry then shows a clean list containing selected A and Details12345.
The selected84x10 row is byte-identical before/after (RGB SHA256
8b953af1aa7385609df59fee4b01998ead9cc31ac1a044fac208b01d0a7aac7f), while
the full clean84x48 list hash is695c5291d789ebaf9c11583885e0183373313ee4e3fdd5ee16c9aee36d957c8b.
No LCD, font, SIM-alpha decoder or selected-name cache repair is required
by this case; the two formatter namespaces must be kept distinct.

Physical memory-selection acknowledgements are also observed, including
Phone selection changing settings03d6/03e8. Those settings' complete semantics
are not inferred from the mixed-list formatter. The dedicated Phone save/read
contract below qualifies one ordinary contact; general native record layout,
other local-contact operations and capacity remain incompletely mapped.

### Physical Phone-memory persistence

`nsm1_phone_local_check.py SOURCE WRITE READ CONTROL` starts from the qualified
Phone-erased cold fixture above, retaining SIM A/12345. Physical Names/Options/
Memory in use selects Phone; the writer enters D/6789. Exact choice, name,
number and selected-D list frames are required. The write changes only the
two observed settings bytes `0x03d6`/`0x03e8` and the 37-byte record at `0x19d4`.
The reviewed record contains the fixture's number prefix `04 67 89` and D at
offset `0x10`; this is not a
general Phone-record encoding specification. The complete SIM remains
byte-identical, with no UPDATE RECORD attempted.

Independent cold processes inherit the complete saved EEPROM and card through
`nsm1_nominal_input.py --preserve-sim`. The reader must move Down from the
initially selected SIM A to Phone D before Details: the mixed list does not
select the newly saved Phone contact automatically after restart. Exact D-list
and 6789 Number frames distinguish it from the existing SIM read fixture.
The code-only control runs for the same 24 seconds without contact navigation.
Cold boot writes `0x03e8` and other EEPROM regions, but reader/control outputs
must be byte-identical and retain the saved contact, identity and security
bytes. No unexplained-write allowlist is used. The semantics of those shared
boot writes remain unmapped; this contrast proves no additional read-specific
mutation in the fixture, not general nonvolatile-storage fidelity.

| Stage | Script | Emulated seconds |
| --- | --- | --- |
| Phone save from qualified erased cold input | `nsm1_phone_local_input.lua` | 45 |
| Independent writer-derived cold read | `nsm1_phone_local_read.lua` | 24 |
| Independent writer-derived code-only control | `nsm1_phone_local_control.lua` | 24 |

Use the existing `nsm1r4t` debugger/input-run recipe and prepare each fresh
directory with `nsm1_nominal_input.py PREDECESSOR DIRECTORY --preserve-sim`.
Accepted captures are `tmp/nsm1-phone-local-qualified-{save,read,control}`.
This qualifies one Phone contact with a distinct unchanged SIM control, not
name/number editing, selective erasure, capacity, normal-machine promotion or
native RF/speech.

```sh
.venv/bin/python tools/nsm1_phone_local_check.py \
  tmp/nsm1-erase-phone-cold tmp/nsm1-phone-local-qualified-save \
  tmp/nsm1-phone-local-qualified-read tmp/nsm1-phone-local-qualified-control
```

### SIM and erase reproduction

```sh
.venv/bin/python tools/nsm1_phonebook_erase_check.py \
  tmp/nsm1-erase-phone-qualified tmp/nsm1-erase-phone-cold
```

```sh
.venv/bin/python tools/nsm1_phonebook_name_check.py \
  tmp/nsm1-name-namespace-qualified/error.log \
  roms/research/nsm1-v523/nsm1-security-checksum-valid.bin \
  roms/research/nsm1-v523/6150-v523-ppm-c.fls --namespace \
  --before-list tmp/nsm1-name-namespace-qualified/snap/nsm1_phonebook_read_17.png \
  --after-list tmp/nsm1-erase-phone-cold/snap/nsm1_phonebook_read_17.png
```

`nsm1_phonebook_name_observe.lua` retains the bounded formatter/record/write
regression. Exploratory candidate-buffer reads, menu enumeration and full-RAM
dumps were removed. Ordinary cold-read acceptance retains a17s list capture.

```sh
.venv/bin/python tools/nsm1_nominal_input.py tmp/nsm1-security-qualified tmp/FRESH
# Run31s with nsm1_phonebook_input.lua and debugger enabled.
.venv/bin/python tools/nsm1_nominal_input.py tmp/nsm1-phonebook-write tmp/COLD --preserve-sim
# Run24s with nsm1_phonebook_reload.lua and debugger enabled.
.venv/bin/python tools/nsm1_phonebook_check.py tmp/nsm1-phonebook-write tmp/nsm1-phonebook-cold
```

The 60-second saturated-input control `tmp/nsm1-startup-qualified` retains
valid EEPROM; sampled report bitmap `1126c1` remains `0e` and SIM-ready
`10e6ce` remains zero. This is not producer-absence evidence: the
[own analog comparison](#own-analog-comparison-and-graphical-presentation)
checks the responsible measurement/countdown path with both saturated and
nominal inputs. Use that paired contract rather than requiring startup to
remain inactive.

The startup selector's reason0a path reads cached word90ff through2bf050.
After the analog/key checks, bit7 selects class3, bit2 selects class1,
otherwise bit1 selects class2. Thus the modeled ordinary PWRONX status03 is
consistent with class2; the omitted class1 send is not by itself evidence
of incorrect boot classification. Do not alter reset causes to select it.

`nsm1r4t` uses the own normalized flash and checksum-consistent EEPROM,
128 KiB SRAM and the acquired NSE-1 ROM4 native DSP program/data. That mask
is an unproved compatibility input, not an identified NSM-1 fitted mask.
The MCU reset exit remains declared boot HLE. The base 16-Mbit flash part and
other conservative peripheral defaults are research assumptions. The keypad
register tuple is now independently decoded below; fitted flash identity,
aliases and physical input are not validated. DSP/service/radio HLE publications are disabled.
SIMI and the removable laboratory SIM are enabled from the own reset/RX
register contract below; their presence is not SIM initialization acceptance.

Own CCONT read routine `0x2bf082` selects control `0x28=0x22`, writes its
command through `0x2a`, polls status `0x29` bit 2, and receives at `0x2d`.
The static checker pins this span and register grammar. Selecting this
GENSIO leg alone is insufficient: the later default keypad also owns
`0x28/0x2a` and intercepts the transaction. The explicit compatibility
instrument selects the non-overlapping GPIO layout independently confirmed
by the own scanner; physical input acceptance remains separate.

Own scanner `0x2c084c` uses MAD2 base `0x20000`, row output `0x31`,
column input `0x30`, direction `0x2f`, and column IRQ mask `0x33`.
Both scan loops run five iterations; normal raw positions use
`row*5+column`. Decoder `0x2c16c6` calls that scanner and selects the
normal table `0x2e0b18` or special table `0x2e0b34`, using layout byte
`0x1127f2`. The first special row is `3e 3e 3e 3e 0d`, independently
identifying column bit 4 as Power. These static anchors confirm the
currently selected register tuple and power column, not a complete
physical-key acceptance run or a validated alternate layout.

The compatibility instrument now has its own `nsm1r4t` input map:
digits occupy columns 2--4, navigation column 1, and side/call keys
column 0. The former borrowed 6110 map put digits in columns 1--3
and was inconsistent with the own normal table. A fresh physical-only
`tools/nsm1_keypad_fixture.lua` run presses 1, Left Softkey and Up,
but the observer sees only the early scan at 0.323 s and no late
scan or decoded press/release calls. Record arithmetic and both known
validator rejections remain unchanged. This corrects host input cells
without claiming runtime UI acceptance; build, MAME validity and the
default 3210 structural regression pass.

Read-only observer `tools/nsm1_native_observe.lua` records actual firmware
shared-memory writes, CPU snapshots and verifier fields; it never writes
firmware/MMIO state. The nine-second experiment progresses past the sparse
verifier into later firmware execution, captures result words `0x0000` and
`0x0004` at `0x111a9e/0x111aa0`, and records native mailbox/completion
activity. The captured 84 x 48 frame is blank. Leaving the verifier is not
proof that its result passes the product's later self-test policy. No
SIM, graphical boot, input, radio or speech acceptance is claimed.

Reproduce using an empty run directory and a ROM directory named
`nsm1r4t` containing the own flash/EEPROM and the hash-pinned NSE-1 program
and data files declared in the driver:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir run_6150_native_cold -- nsm1r4t \
  -rompath /absolute/path/to/roms -video none -sound none -nothrottle \
  -seconds_to_run 9 -log -autoboot_delay 0 \
  -autoboot_script ../tools/nsm1_native_observe.lua
```

### Downstream readiness observation

Own supervisor `0x2b6180` repeatedly calls readiness predicate `0x29ed4c`.
It requires nonzero bytes at `0x111ea0` and `0x10e6d0`, plus selector helper
`0x27bde4`: selector byte `0x10be8c==0` requires `0x10e6ce==1`; a nonzero
selector requires `0x10e6ce!=0`. Failure at `0x2b61be` skips the subsequent
readiness chain and enters the supervisor latch loop. The static checker
pins all relevant branch instructions and state literals. These are
software states, not yet identified physical input semantics.

Fresh native observation at eight seconds gives first/selector/second/third
`01/ff/00/01`: **the second input alone is false**. The remaining question
is downstream of the earlier storage rejection: which legitimate firmware
lifecycle writes `0x10e6ce`, and what supplies its prerequisite. The literal scan identifies references to selector
`0x10be8c`, including message-driven stores at `0x2009a0` and `0x2038f4`;
this does not establish an EEPROM field or justify changing provisioning.

The native service path is not simply absent. Own `0x2ab6ce` posts `0x622a`,
waits for bit 2 of `0x11fdd1` to clear, and returns success if bit 6 is set.
The observed return at `0x2b601a` is `1`, with `0x11fdd1=0x48` and DSP state
`0x111a94=1` at eight seconds. This proves that completion path ran, not
that every self-test, radio or UI prerequisite passed.

Observed LCD initializer `0x2bfaa0` writes command `0x24`, bank commands
`0x40..0x45`, column command `0x80` and six 84-byte zero banks through data
register `0x2b`, followed by command `0x20` through `0x2c`. Its status polls
use `0x29` bit 0. Clearing the display is therefore real firmware activity;
a blank frame must not be classified as a missing LCD initializer.

### SIM lifecycle ownership

The false byte is offset 6 of the object at `0x10e6c8`. Its owning routine
`0x288ec8` initializes that offset to zero at `0x288ede`, then initializes
the SIM controller through `0x2af080`. Another candidate store at
`0x288fb2` sets it to one after the counter at offset 2 reaches four
(`0x288f4a..0x288f4e`); this is a retry-exhaustion continuation, not evidence
that setting the byte means successful card initialization. These candidate
stores are not an exhaustive writer census: aliases and bulk stores remain
to be classified.

Own `0x2af080` clears the interrupt cause at `0x20038`, configures UART
registers and writes control `0x32` at `0x20039`. Own RX routine `0x2af0f6`
polls count `0x2003c` and copies bytes from `0x20037` into its receive object.
Those instruction spans are pinned by the static checker. Attaching SIMI
and the standards-shaped removable card is supported by this register
contract, but does not manufacture the task's activation event.

The write-watch observes zero initialization stores to offset 6 and no
retry-exhaustion store in the cold run. With SIMI/card attached, the same
`01/ff/00/01` readiness inputs persist at eight seconds and the supervisor
loop persists through thirty seconds. Observed control writes initialize
the UART; no bit-7 activation write solicits the card's ATR. This is not a
falsification of card protocol behavior: the firmware has not requested it.

### Task delivery frontier

The task's receive loop is `0x288114`, calling RTOS receive primitive
`0x275db4`. It consumes event byte `+4` for ordinary objects, or `+8` for
class `0x1f` objects, subject to its class filter. The receive primitive
indexes descriptors at `0x1014ac + task * 0x1c` using current task byte
`0x100022`; descriptor offsets `0x10/0x11` are compared for available input.
Sender `0x27641c` indexes the same descriptor table by its task argument.
These instructions and literals are checked by `verify-6150-static`.

Cold observation confirms task `0x16` (decimal 22) enters this loop from
`0x2893de` at approximately 2.650 seconds, descriptor `0x101714`, with
receive head/tail both zero. No return object is observed through nine
seconds, and no task-22 invocation of sender `0x27641c` is observed in that
window. The entry trace confirms that the receiver probe was reached;
this is narrower evidence than claiming every possible sender is absent.
The receiver instrument accepts both SRAM and own-flash object pointers:
the RTOS can deliver immutable objects without allocating a RAM copy.

The static gate enumerates 27 direct Thumb BL candidates targeting
`0x27641c` over CPU addresses `0x200000..0x2e0eff`. This is a linear,
even-halfword candidate scan, not a control-flow proof: embedded data,
indirect calls, descriptor interpreters and other sender primitives are
not closed. Candidate sites and scan bounds are in the generated report.
The alternate blocking sender `0x275b60` uses the same task descriptor
table; its linear inventory has 370 direct-call candidates. Wrappers
`0x29c4b4`, `0x29c90e` and `0x29ce18` send own immutable objects at
`0x2e0730`, `0x2e0728`, `0x2e0738`, carrying event bytes `0x37`, `0x38`,
`0x02`. Their respective direct callers include `0x2076d0`, `0x20768a`,
`0x207a6c`; the latter is gated by nonzero object byte `0x10e6cf`.
These wrappers are not yet classified as ordinary startup producers.

### Physical socket control

`tools/nsm1_sim_socket_fixture.lua` removes the laboratory card through
the physical `SIM_REMOVED` input at three seconds and reinserts it at four.
It writes no firmware or MMIO state. NSM-1 card-detect interrupt routing
remains a compatibility assumption, so this is a controller/firmware
experiment, not validation of fitted-board wiring or unattended boot.

The cold socket-edge run delivers own ROM object `0x2e0a50` to task 22 at
approximately 5.922 seconds: bytes `000000000c000000002a0000`, ordinary
event `0x0c`. This object is outside SRAM and must be retained by receiver
observability. Neither of the two traced sender entrances publishes it in
the observed window. Own RTOS receive supplies a descriptor-driven alternate
path: `0x275fac` loads pointer-column base `0x2d869c`, then reads the node's
index byte at `+9` and scales it by eight. Entry `0xe3` at `0x2d8db4`
points to this exact object. The SIM task schedules index `0xe3` at
`0x288f80` through `0x275106`, with delay `0xff + 0x79 = 0x178`.
The cold socket-edge trace confirms index `0xe3`, delay `0x178`, at
3.016680 seconds, retaining caller `0x2ab893` in task 0. This is a
distinct observation from the SIM-task schedule site. The retained link
register matches the return from `0x2a70dc` at `0x2ab88e`, but task-0
interrupt context means it does not prove a call chain through that helper.
The scheduling caller remains unresolved. RTOS receive reaches the
descriptor branch `0x275f9c` with node `0x100bdc`, index `0xe3`, in task
22 at 5.922080 seconds; the exact ROM object returns ten microseconds
later. The SIM-task schedule site remains static-only. Readiness
still reads `01/ff/00/01` at eight seconds. The expanded no-edge cold control
observes no receive object. The receive path can therefore carry a physical
input-triggered event, but this does not complete SIM activation.

The event reaches its own dispatch continuation `0x28830e` at 5.922103
seconds with event `0x0c`, object `0x2e0a50`, and state-object byte `+14`
zero. Firmware sets `0x10e6c9` (`+1`) and `0x10e6cf` (`+7`) to one;
the existing write watch observes both stores. This branch does not set
readiness byte `0x10e6ce` (`+6`). Its tail frees the received object and,
without a return-selecting `r7` update, loops back to RTOS receive rather
than returning the event to the outer SIM owner. Socket notification is
therefore consumed as local state maintenance, not an activation request.

The outer owner at `0x2893de` receives through the same dispatcher. At
`0x2893f0` it compares the returned event with `1`; that branch calls
reset/activation routine `0x28842c`, then enters its subsequent receive
phase. Own wrapper `0x29cb90` posts to task 22 via `0x275b60`: with
selector byte `0x10e6d6` not equal to one it uses ROM object `0x2e0720`
(event `1`); otherwise it sets `0x10e6cf` to two and uses `0x2e0718`
(event `0x20`). The bounded linear Thumb call scan finds wrapper call
candidates `0x2077e4` and `0x207bb0`, not an exhaustive indirect-call
closure. A fresh nine-second cold run with an entry tap observes no
invocation of the wrapper. Thus an activation producer exists in this
ROM; its caller lifecycle, not a manufactured SIM event, is the next
boundary to investigate.

The direct calls belong to entries `0x207718` and `0x207afc`. The former
guards the wrapper with `0x111e71 == 0`; the latter calls it after resetting
its local state. Shared application branch `0x208a7c` selects between them
via `0x2a7e44` (`0x208a90` / `0x208a96`), after a context-byte `+15`
check. Entry `0x2079b0` can also call `0x207718` at `0x2079d6`.
Read-only entry probes for both owner entries and the shared branch observe
none in a fresh nine-second cold run. This does not close indirect callers
or later activity, but places the observed failure before the SIM controller's
activation-request consumption rather than in its response timing.

The containing dispatcher begins at `0x20837c`, initializes context
`0x111e70` and secondary context `0x111e94`, and receives objects through
`0x275db4` at `0x2085c0`. It dispatches their signed halfword at `+0`.
Literal comparison at `0x2085ce` selects input `0x119a`, branching to
`0x208a58`. That handler either takes the `0x2079b0` path (context `+5 == 1`
and `+23 == 0`) or falls through `0x208a7c` toward the two activation owners,
subject to secondary-context `+15` and helper `0x2a7e44`. Neighbor inputs
`0x119b` and `0x11f8` select distinct handlers; they must not be conflated
with `0x119a`.

A fresh cold trace reaches dispatcher entry at 2.650506 seconds and its
receive-loop entrance at 2.652597 seconds, with the expected context bases.
No receive-return object is observed through nine seconds. The dispatcher
is therefore live, not an unstarted task; the bounded frontier is its absent
input rather than an observed rejection of `0x119a`.

Own producer wrapper `0x29f340` loads immutable object `0x2e0b7c`, whose
first halfword is `0x119a`, calls `0x2bced2` with that id, then posts the
object to task `0x15` through `0x275b60` at `0x29f34e`. Its bounded direct
call candidates are `0x21fb5e`, `0x21fd14`, `0x21fec6`, `0x220574`, and
`0x220b12`. A fresh nine-second cold run observes dispatcher initialization
and receive-loop entry but no producer-wrapper invocation. The static
checker retains the object/id/task and five-site candidate inventory.

The aligned halfword scan of the MCU span also finds `0x119a` at
`0x2dd28c` in the sequence `1196/3001 ... 119a/3005 ... 119b/3006`.
This is a separate descriptor-table reference, not proof of an additional
direct producer. The only recovered Thumb pool-literal load of integer
`0x119a` is the consumer comparison: a literal-only producer census would
have missed the immutable-object wrapper.

The five direct caller sites have distinct local prerequisites:

| Call site | Own-ROM prerequisite immediately upstream |
| --- | --- |
| `0x21fb5e` | Local context byte `+7 == 1`; it is cleared before posting. |
| `0x21fd14` | Receive loop at `0x21fcd4` waits for `0x09f0`, then a further literal-selected loop precedes the post. |
| `0x21fec6` | Helper `0x27d254` must return other than one at `0x21febc`. |
| `0x220574` | Receive loop at `0x22054c` waits for `0x09f0`. |
| `0x220b12` | Receive loop at `0x220afc` waits for `0x09f0`. |

These are local branch contracts, not semantic names for the owning task
or proof that every path is ordinary startup. A fresh 30-second cold run
still observes no producer-wrapper entry or application receive return.
The added probe at lifecycle receive return `0x21cf14` also observes no
object. Entry tracing resolves the owner: own task-table pointer at
`0x2d8650` selects Thumb entry `0x21e41c`, running as task `0x12` (18)
at 2.652673 seconds. It reaches receive helper `0x21cf0c` at 2.652792
seconds with link register `0x21fcf7`, the return from call `0x21fcf2`.
That loop compares against own literal `0x1587` at `0x21fcf8` before
reaching producer `0x21fd14`. The fresh queue trace shows head/tail zero
at the receive-helper entry. No return is observed through 30 seconds.
Task 18 is started; the selected cold lifecycle is waiting before its
application-start publication. This identifies the active wait, not the
semantic meaning or missing hardware origin of `0x1587`.

Own producer `0x27a4f0` loads id `0x1587`, calls `0x28fddc`, then posts
immutable object `0x2e0138` to task 18 through `0x275b60` at `0x27a4fc`.
The object begins `15870000`; this is an explicit in-ROM producer, not a
missing external-event inference. Bounded direct call candidates are
`0x281376` and `0x2bc146`. The first follows a byte store in handler block
`0x281370`; the second belongs to entry `0x2bc112`, which updates context
halfword `+10`, publishes through `0x2bf6cc`, sets context byte `+1` to
five, then posts the completion. These mechanics do not yet establish
semantic subsystem ownership.

A fresh 30-second cold run observes task-18 entry and its receive wait but
no entry into the completion wrapper, block `0x281370`, or routine
`0x2bc112`. The static checker pins the wrapper's own id/object/task and
two-site candidate inventory. Descriptor/data references to `0x1587`
remain additional candidate routes, not excluded by this direct-call result.

The two paths belong to task 1's scalar-input lifecycle. Receiver
`0x28029c` takes the RTOS return value directly (not an object pointer).
Input `0xc7` selects `0x28045c`; if `0x2bc37a() != 3`, it calls
`0x2bc112` at `0x280464`. This scalar namespace must not be confused with
task-18/object halfwords.

The other path checks low nibble of context `0x11ff14` for six at
`0x28129a`, then low nibble of `0x1126c1` for fifteen. Only then can
helper `0x2b4050` returning zero, or `0x2bf1a0` returning nonzero, select
`0x281370`. Fresh passive traces observe task 1 consuming scalar startup
inputs and periodic `0xc8`, but no `0xc7` among the first 64 returns
(trace cap reached around 21.64 seconds). Gate tracing reaches first
state six at 1.798416 seconds, while the second state progresses
`8 -> a -> e` and remains `e` at the observed 2.653150-second check.
The observed normal path therefore fails the second nibble comparison,
before either helper or completion publication. This is a runtime gate
finding, not evidence for synthesizing a missing report or changing the byte.

The write watch identifies task-1 store `0x28120c` as the observed nibble
writer. Its subtract-free comparisons map scalar reports `0x17 -> bit 3`,
`0x16 -> bit 1`, `0x15 -> bit 2`, and `0x14 -> bit 0`; all OR into
`0x1126c1`. The cold run observes the first three, matching `8/a/e`, but
not `0x14`. Own report wrapper `0x2bc4d4` posts task 1/value `0x14` via
scalar sender `0x275cb0` at `0x2bc4e2`. The bounded direct call scan finds
one candidate at `0x223a74`. This closes the missing-bit interpretation,
not the report's hardware ownership or all indirect producers.

The expanded wrapper-entry census scans460672 halfword positions in the
bounded code extent200000..2e0eff for direct conditional/unconditional
branches and calls into2bc4d4..2bc4e7, plus524288 aligned pointer words
across the full2MiB image. Only BL223a74 ->2bc4d4 is found; no interior
entry or aligned pointer names the wrapper. This rules out those simple
alias mechanisms, not computed destinations, unaligned data pointers,
argument-driven scalar posting or another report implementation. The static
report now emits coverage and transfer kinds rather than presenting the
one-call list as complete producer ownership.

The report-`0x14` caller belongs to entry `0x223520`, running as task
`0x14` (20). Its report branch calls `0x28b760` at `0x223a68`, sets bit
6 in byte `0x110894`, then calls the report wrapper at `0x223a74`.
Fresh entry traces observe task 20 starting twice (0.395867 and 0.832705
seconds, across the already-observed early reset), but never reaching
that report branch in the nine-second run.

The active lifecycle tail `0x2263fa` calls scalar receiver `0x221ee0`,
stores its transformed result at context `+50`, and dispatches on halfword
`+52` through the 30-entry pointer table at `0x226420`. Runtime receive
returns establish context `0x111c34` and state `0x18 -> 0x1a -> 4 -> 3`;
frequent scalar `0x019d` and occasional `0x019a/0x0199/0x0198` are delivered.
The first 64 returns reach the trace cap around six seconds, so absence
claims after that point must use the separate uncapped entry counters.
Own table entries select `0x2258e4` for state `0x18`, `0x225b54` for
`0x1a`, `0x225c00` for four and `0x225cb8` for three. This establishes a
live, event-driven owner; the report branch's absent lifecycle transition
is the next question, not a stalled global scheduler.

Backward decode of the report branch finds an explicit entrance: common
dispatch at `0x223688` compares its transformed input with `0x21` and
branches at `0x22368c` to `0x2239f2`, which sets up timers/state and reaches
`0x223a68`. Receiver `0x221ee0` preserves raw scalar input `0x21` at
`0x221f80..0x221f86`; it does not derive it from the observed timer inputs.
This is an input-driven initialization branch, not an automatic consequence
of the measured retry states. The cold raw-input trace contains no `0x21`
before its 64-return cap.

The state-`0x1a` handler at `0x225b54` enters a retry loop that handles
transformed input `0x49`, decrements context byte `+5`, and tests a signed
measurement against `0x01fe`. Subsequent states four and three retain
their own retry/timer branches. Their hardware meanings remain unassigned;
do not choose analog values merely to steer them toward the report branch.

The bounded direct-call inventory contains 117 Thumb candidates for scalar
sender `0x275cb0`. Recovering adjacent literal argument pairs finds task
20/input `0x21` at `0x224e34` and `0x225348`. Both are task-20 lifecycle
self-posts. The similar input at `0x26b7f4` targets task eight and is not
this contract. This argument pattern is deliberately narrow: computed,
indirect and table-driven producers are not closed.

Producer `0x224e34` belongs to block `0x224e10`, entered on transformed
input `0x42` in state ten (`0x224e06`) or fourteen (`0x22586a`). Producer
`0x225348` belongs to block `0x22530c`, selected by `0x42` in states
28/29 (`0x223bd8` / `0x223c12`). The observed retry states do not select
these branches. This establishes an additional state/input dependency;
it does not establish that the analog inputs should be changed to select it.

Own wrapper `0x2b3f12` posts raw scalar `0x42` to task 20 through
`0x275cb0`. Its sole bounded direct call candidate is `0x2bf31a`, inside
state-change helper `0x2bf2e6`. The helper compares state byte `0x1127d8`
with prior state `0x11ff10`, also honoring byte `0x1127d9 == 1` as a
notification condition. On that branch it clears the notification byte,
copies the current state into the prior state, and posts `0x42` only when
the current state is zero. Other state values take a different publication
path. Direct helper callers are task-1 sites `0x280b0c` and `0x2813a0`.
A fresh nine-second cold trace observes none of helper `0x2bf2e6`, wrapper
`0x2b3f12`, or the two task-20 self-posting blocks. This is a bounded
lifecycle observation, not a closed proof of absent hardware traffic.

Own classifier `0x2bf1a0` writes `0x1127d8` at `0x2bf214`. It reads
selector five through `0x2c32f4`, partitions readings at 100 while
accumulating five consistent samples, then derives a zero/one state using
the ROM floating-point comparison. Physical channel meaning and units are
not assigned from this code alone.

The runtime writer trace observes initialization at `0x2bf338/0x2bf33c`
setting ancillary state and notification byte `0x1127d9 = 1`; classifier
entry follows with caller `0x2bf343` in task 1, and its store sets current
state `0x1127d8 = 0`. This happens before and after the early reset, with
the latter zero publication at 0.832085 seconds. The desired current state
therefore already exists; the observed issue is that helper `0x2bf2e6`
does not consume/publish the pending notification, not an ADC value needing
adjustment.

Caller `0x280b0c` follows a stability countdown (initialized to eight,
restarted on state changes) and timer scheduling at index `0x10`, delay
six. Caller `0x2813a0` follows the earlier readiness-completion block.
The cold trace does not reach the separate countdown entry `0x280ac0`.
Do not infer that changing the selector can repair this lifecycle ordering.

Task-1 dispatch selects countdown initialization `0x280ac0` on scalar
`0xcf` (`0x280656..0x28065a`) and countdown stepping `0x280ae4` on
transformed input `0x52` (`0x28064a..0x28064e`). Receiver `0x28029c`
maps raw `0xd0` to `0x52`. Own RTOS descriptor pointer-column entries
`0x0f` and `0x10` contain scalar `0xcf` and `0xd0`, respectively: the countdown
is timer/event driven, not an undiscovered object-message handler.
The bounded adjacent-argument timer-call scan finds an index-15 schedule
at `0x281864`; a nearby-looking index-`0x0f` literal at `0x2a7b14` belongs to
a different call and the actual timer at `0x2a7b1e` schedules index `0x15`.
Do not promote a literal found near a call into its recovered argument.

The cold task-1 scalar trace contains neither `0xcf` nor `0xd0` before
its cap, consistent with the observed absence of both countdown branches.
This path cannot yet be treated as the normal startup publication: its
scheduling lifecycle remains unresolved. The pending notification alone
does not prove a missing timer implementation.

The initial task-20 state is own firmware data, not a runtime peripheral
selection: the write watch sees byte `0x111c69 = 0x18` copied by initializer
`0x200148` before task entry, on both sides of the early reset. Entry
`0x223520` loads context `0x111c34` then dispatches its existing halfword
`+52` via `0x22640a` without choosing a new mode.

State `0x18` handler `0x2258e4` validates/clamps stored parameters,
initializes local state, primes selectors four/three/two, and falls through
state-`0x19` handler `0x225a96`. The latter arms timer index `0xd8` with
delay `0x1b7`, sets retry count four, then receives. In the cold run its
first received value `0xb1` does not map to transformed timer input `0x49`,
so state `0x1a` is recorded at `0x22362c`. Repeated periodic returns retain
that state before the previously observed retry progression. The initialized
state therefore has an explicit own-ROM explanation; it is not evidence for
changing calibration or hardware identity to reach another state.

An entry tap at scalar sender `0x275cb0`, filtering destination task 20
but accepting every runtime argument value, observes zero invocations in a
fresh nine-second cold run. This extends beyond the adjacent-literal scan,
but only covers that primitive and interval. The periodic received values
are independently accounted for by timer descriptor entries: index `0xd8`
returns `0x0198`, `0xd9` returns `0x0199`, `0xda` returns `0x019a`, and
`0xdd` returns `0x019d`. They do not require a missing transport peer and
must not be counted as scalar-send activity.

The initial `0xb1` is a firmware-preloaded queue input, not an RTOS receive
sentinel or missing peer response. Queue initializer `0x27610c` iterates 24
task records at `0x2d8578`, stride 12. If record byte `+9` is nonzero,
`0x276164/0x276166` stores `0xb1` in the first queue's slot 1 and marks its
head as 1. A fresh nine-second cold trace observes task-0 writes to task
20's slot `0x1012c8` at 0.233503 s and, after the early reset, 0.693237 s.
Receive branch `0x275e40` reads that slot at 0.398276/0.835120 s, immediately
before `0x221ee6` returns `0xb1`. This closes its origin without attributing
it to a hardware event or treating the lack of scalar-send calls as a gap.

Task 20's own diagnostic strings identify the observed path as charging
initialization, not an unnamed transport-readiness state machine. Static
ADR references identify `DO INIT CHARGING` at `0x225da0`, `BOOT UP CHARGE`
at `0x225f28`, `WAIT CHARGER VOLTAGE SETTING` at `0x225f38` and
`CHARGER DISCONNECTED` at `0x225f58`. A fresh passive cold run reaches
the voltage-setting wait at 4.233769 s and disconnected state at 5.689757 s.
The observer captures bounded, unique firmware diagnostic call sites;
strings are firmware evidence, not newly invented subsystem names.

This does **not** establish an erroneous charger boot selection. The
power-reason check at `0x225a9e` distinguishes return `0x0a`; otherwise it
checks byte `0x11fe78` for an already connected charger. Both the zero-byte
path at `0x225ac0` and the reason-`0x0a` branch reach common initialization
`0x225ae0`. Thus charging initialization without a charger is explicitly
supported by this firmware. State `0x1a` eventually consumes timer input
`0x49` (`0x0198`), exhausts retries or sees a signed measurement at least
`0x01fe`, then enters the voltage-setting/disconnected path. No threshold
input should be changed merely to escape these states.

The separate input-`0x21` branch at `0x2239f2` names its operation
`Start limited fast VBAT reads` (own string `0x223ad8`). It resets local
sampling fields, arms timer `0xdd`, calls `0x28b760`, then publishes report
`0x14` through `0x2bc4d4`. Its proven behavior is therefore a fast-VBAT
sampling request followed by a report, not an independently established
generic startup-initialization command. The task-1 readiness bit depends
on that report in this run, but that consumer dependency does not establish
the full product meaning of either input `0x21` or report `0x14`. This is
one entry into shared setup/report code, not exclusive report ownership;
the independently qualified ordinary measurement path above also reaches it.

Selector 5 in state helper `0x2bf1a0` has a direct transport interpretation:
`0x2c32f4` sends CCONT command `0x00` (table byte `0x2e0c52`) followed by
`(selector << 4) | 8 | (byte[0x1126e0] & 0x87)`. It reads command bytes
`0x14` and `0x1c` from table `0x2e0c40`, combining the first response byte
with the second response's low two bits as a ten-bit sample. Thus this
helper samples CCONT ADC selector 5 through GENSIO, not a DSP-owned value.
The physical pin/source and units remain unproved for NSM-1; do not import
another product's channel name or alter its sample to trigger the report.

### Startup selection and held-key control

Task-1 selector `0x2bc37a` reads firmware reason byte `0x11fed5` through
`0x2b5e64`. Class 1 takes `0x2810c8` and sends task 20 input `0x41` via
`0x2b3f72`; class 2 takes `0x281150` without that send. Class 5 also enters
`0x281150`, explicitly through `0x2804f4/0x2804f8`. Do not treat the
absence of `0x41` as proof that an executed sender lost its message.

The released-key cold control selects class 2 for reason `0x0a` at
0.323491 s. After the early reset, reason `0x04` selects class 5 at
0.760326 s. `tools/nsm1_power_key_fixture.lua` holds only the physical PWR
input from startup through one second: it changes the first selection to
class 5, but the post-reset selection is again class 5/reason `0x04`.
Both fresh nine-second runs retain the blank frame, no observed task-20
scalar sends and no activation-wrapper calls. The model responds to the
input; it does not provide a missing ordinary-startup request. The NSE-1
keypad wiring is provisional, so this is a model-level external-input
control, not validated NSM-1 power-key decoding or a real-hardware exclusion.

### Explicit packet-triggered restart

The early reset is firmware-requested, not a watchdog timeout. A fresh
cold trace enters non-returning request `0x2c2a02` from `0x27e7f8`, task 2,
with reason 4 at 0.461303 s. The handler records the reason in `0x11fee4`
and writes MCU reset control; the next boot copies that retained reason
to `0x11fed5`. The direct-call census has eight request candidates; the
runtime observation identifies this particular caller without interpreting
the last callee's LR at the eventual MMIO write as the requester.

Task-4 converter `0x2ab7c8`, called from dispatcher `0x2c1c80`, creates the
task-2 object from the `0x70` packet family. The observed source packet at
`0x101fdc` begins `18 02 34 74 35 32 00 00 20 67 3c c1 76 0d f2 c7`.
It becomes object `0x102030`, header `000000740036010035320000`, through
the copy at `0x2ab80c` and post at `0x2ab814`. Sender `0x275cb0` can carry
object pointers as well as scalar inputs: zero observations at the other
two send primitives do not establish absence of task-2 delivery.

Own handler `0x27e618` (direct candidate caller `0x23fe4c`) rejects this
object, with local valid flag 0, class 2 and marker `0x5a` at `0x27e7cc`.
It updates the retained restart structure and checksum, sets reason 4,
then requests reset. This locates the rejection, not its root cause:
the packet's full semantics, validation alternatives and compatibility of
the NSE-1 resident ROM4 with NSM-1 remain unproved. No payload may be
replaced with guessed passing bytes. The post-reset `35 32` packet differs
in its body; do not assume a fixed template or replay the first response.

### Record exchange arithmetic

Own builder `0x27e054` sends four type-`0x70` requests to task 3 through
`0x275b60`: command `0x13` carries four flash-derived bytes; `0x14` reads
12 EEPROM bytes at `0x14`; `0x15` reads 12 bytes at zero and eight raw
identity bytes at `0x0c`; `0x16` reads 24 bytes at `0x20`. If the retained
marker `0x11fdd2` equals `0x5a`, it applies `0x27d4ac` to the final request
before sending. The different cold/restart requests therefore have an
own-firmware source; do not mistake them for nondeterministic transport.

`0x27d4ac` reads the twelve bytes at EEPROM offset zero and duplicates
them into a 24-byte work buffer. Each adjacent pair becomes its unsigned
byte product stored low byte first. It then reverses the buffer order,
bit-reverses and complements each byte, and XORs that pad into the request.
The offline `retained_record_transform` test reproduces the observed cold
request exactly from the own raw record pair and own identity record
`3000abb18f2632abcc51301a`; applying it twice restores the raw pair.
This accounts for the entire cold/restart input difference without a
DSP timing hypothesis, handset-data substitution or storage modification.

The observed `34 0e 00 82` response is a family-`0x82` MSID. The independently
recovered codec decodes it to `9a1870dd 00160010 a8a9aa27`: the first word
agrees with request `0x13`, and the chip field agrees with the modeled COBBA
serial. This is execution evidence, not a fitted NSM-1 chip identity.

`tools/nsm1_record_exchange_check.py` compares both captured `35 32` replies
against the recovered ROM4 record transform, using the measured chip field.
Both agree byte-for-byte, including the echoed 24-byte input and the removal
of each decoded block's two private marker bytes. It also checks identity
ordering, the requested flash value and complete request/response sizes.
When validator taps are present, it also checks both observed rejection
paths and their inputs against the corresponding decoded replies.
Absent taps are explicitly reported as unobserved; malformed, incomplete
or changed paths fail rather than silently preserving the old conclusion.
The first request is `ac9db72cdc5386529fa4ad4946dbdaf7c70f006dd16407d9`;
after restart it is the raw EEPROM pair
`73654ae2a7cad1f10e8752b699232739bc9657ce4047f826`. Neither decoded record
is thereby certified valid storage. No record, identity, calibration or
DSP response is changed by this comparison.

Run the observer in a fresh directory as above, then:

```sh
.venv/bin/python tools/nsm1_record_exchange_check.py /path/to/error.log
```

The earlier native NSE-1 work independently classified the same reason-4
restart for inconsistent encoded storage (`djr_dsp_integration.md`). That
is a useful mechanism cross-check, not permission to reuse its handset
identity, factory constants or complete EEPROM profile. The shared codec
arithmetic can be checked without donor provisioning.

The validator's loop at `0x27e75a..0x27e794` checks object bytes
`+0x0c..+0x12`, then the high nibble of `+0x13`. For each of the first
seven bytes, a high or low nibble in `0xc..0xe` clears validity; `0xf`
is permitted. The final byte's high nibble follows the same rule.
This is a recovered representation constraint, not yet a field-name or
lock-policy identification. If routed through that loop without the earlier
block swap, the cold decoded record fails at byte 2 (`0x3c`), and the
restart record fails at byte 3 (`0xc3`). Earlier format-specific branches
at `0x27e688..0x27e758` can reject or swap blocks first, so these are
conditional static failures, not the observed first failing instruction.
Trace those branches before attributing the rejection to this loop.

The bounded branch trace now identifies the earlier rejection in both
exchanges: `0x27e688 -> 0x27e6b0 -> 0x27e6ea -> 0x27e796`.
The format-specific path is selected by bit 6 of `0x11fdd1` (the carry
from `lsrs #7`, not bit 7). The byte at object `+0x15` is `0xec` cold
and `0x9d` after restart; neither selects the preceding accepted-format
branches. The subsequent byte at `+0x21` is `0xb8` / `0x81`, outside
the required inclusive `0x78..0x7f` range, so validity clears before
the nibble loop. The new trace retains arithmetic agreement in both
exchanges. Recover the source and meaning of the format flag and record
bytes before choosing any external-storage fixture; these tests alone
do not identify a valid phone identity or lock policy.

The format flag is firmware-owned initialization, not the DSP verdict:
task 2 sets `r5=0x11fd68`, `r6=0x69`, then unconditionally ORs bit 6
into `[r5+r6]` at `0x23fa60..0x23fa66`. The write watch observes
`0x00 -> 0x08 -> 0x48 -> 0xc8` before the first response, with further
bit-2 changes during the dialogue; restart initialization retains bit 6.
This rules out treating the format selection as a missing DSP publication
or repairing it by clearing that flag. Static anchors protect the base,
offset, unconditional store and carry-based validator selection.

The task-2 response dispatcher is `0x23fd34`, reading command at object
`+8`. Its subtract cascade maps `0x34` to `0x27e424`, `0x35` to
`0x27e618`, `0x36` to `0x27e4c2`, and `0x0d` to `0x23fda8`.
The captured stream has no separate reply to requests `0x14` / `0x15`;
after `0x35` it carries `0x0d/0x00`. In the `0x0d` handler, status byte
`+9` bits 0 and 1 select retained status values `0x10` / `0x11` and
clear format bit 6 when set (`0x23fde8..0x23fe0e`). Both are clear in
the observed reply, so this path preserves bit 6. This is a measured
status-bit contract, not proof of the semantic validity of all EEPROM
records or an identification of what the DSP tested for those bits.

The accepted-record consumer adds checks beyond the early representation
test: object halfwords `+0x16` and `+0x22` must match zero, or `0xa5a5`
when local class is 3; object byte `+0x1d` bits 4--5 must be zero.
The block-swap path rereads the own 24 EEPROM bytes, applies the retained
transform when appropriate, and compares both echoed twelve-byte blocks
at `+0x24` / `+0x30`. Later, valid records are copied from object `+0x0c`
to context `0x11232c+0x1c`. The native builder's zero marker bytes agree
with the ordinary marker case, but that does not repair the earlier
format failure. Field semantics and complete storage validity remain
unproved; the static report exposes these bounds without generating
accepted records or changing any storage.

The conditional block-swap/echo-success branch also calls reciprocal builder
`0x27d568` at `0x27e8d2`, passing object `+0x0c` as both source pointers.
This constructs a class-`0x70`, command-`0x17`, declared-length-`0x30`
message for task 3; it is not a terminal/FBUS export. The source-checked
projection below uses offsets relative to the decoded 24-byte record and
absolute offsets within the newly allocated message:

| Decoded source | Message destination | Operation |
| --- | --- | --- |
| `0..4` | `08..0c` | Five-byte copy from the first argument. |
| `0..7` | `0e..15` | Eight-byte copy from the second argument. |
| `0c..0f` | `16..19` | Four-byte copy. |
| `14..15` | `32..33` | Two-byte copy. |
| `08` | `34` | High nibble only (`& f0`). |
| `11` | `35` | Full byte. |

The builder separately rereads the encoded EEPROM blocks for message
`1a..31`, selecting its retained-marker transform when required. Thus a
semantic audit must distinguish decoded field projections from encoded
record echoes; neither the repeated first bytes nor the separate nibble
is by itself evidence of an IMEI, network identifier or lock level.
`nsm1_v523_static_check.py` now exposes this reciprocal contract and checks
24 instruction anchors plus the complete builder and accepted-call spans.
Mutation tests reject a changed command byte, intervening pointer arithmetic
and truncated source.
This describes the repair-template negative control. The separately qualified
published-record diagnostic above now reaches native update/validation;
full semantic identification of all projected fields remains unproved.

The recovered wumpus/Blacksphere
[local-command table](https://web.archive.org/web/20160119223852/http://nokix.sourceforge.net/help/blacksphere/sub_100hardware/sub_dsp/sub_mdi/sub_local.htm)
independently identifies commands `13`, `14`, `15`, `16` as MSID, FAID,
IMEI and two-block service-provider-lock operations. This corroborates the
record family, not individual decoded-field names or NSM-1 fitted-chip
compatibility. In particular, the visible command-`13` MSID response is not
a decoded IMEI record; the raw BCD identity and that response do not supply
an established plaintext/ciphertext pair for recovering an NSM-1 chip key.
The original page is retained beside the other Blacksphere references as
`blacksphere-local-20160119.html`, SHA-256
`c03783b4c845ca71559d781fca8fd0e048f6fac40afeb96bde07ae634bc265b4`.

The repair-template rejection is now a negative control: the separately
qualified published example above passes the native update/cold-validation
path. Factory record semantics and fitted COBBA compatibility remain unproved.
Continue the SIM/readiness backtrace from that validated diagnostic lifecycle,
not by synthesizing its missing report. Keep validating NSM-1
GPIO ownership independently. Do not inject an event, force readiness,
replace a verifier/packet result or infer a missing DSP message solely
from the blank display.

## Own-firmware contract

Input identity and member extents are authoritative in `roms/README.md`.
The checker pins the reset, loader, identity formatter, serial reader and
security-check instruction spans and emits exact anchor/pool coverage counts.
It independently resolves three ARM stack loads.
It does not claim a whole-image control-flow or producer census.

| Boundary | NSM-1 v5.23 evidence |
| --- | --- |
| Reset | ARM entry `0x200040`, header reorder and MCUIF write at `0x20005c`, vector copy and Thumb switch at `0x2000e8`. |
| Stack roots | Three reset pool loads resolve to `0x1160f8`; exception setup additionally adds offsets. This proves the 64 KiB NSE-3 RAM map cannot cover the selected stacks, not the fitted SRAM capacity. |
| DSP verifier | Loader `0x2a479e` initializes `0x10000=0`, `0x10002=0xffff`; stages source halfwords at 32-byte intervals from `0x200040` through `0x3fffe0`. |
| Transfer count | 127 full 512-word blocks, then 510 source words and two `0xffff` terminators; 131,072 bytes total. |
| Handshake | Alternating acknowledgements use shared `0x100fe` and `0x10100`; payload starts at `0x10200`. |
| Completion | Waits for shared `0x10002` to leave `0xffff`; captures `0x10000` and `0x10002` into state `0x111a94` at offsets `0x0a` and `0x0c`. |

Stream SHA-1 is `0fac5ba57a3504f8cb4490a97ab961f468218121`.
This is a sparse external-flash verification candidate, not a contiguous
DSP program. The DSP-side verdict algorithm and compatible resident mask
are not established. No invented completion value or donor EEPROM may
be used to promote this product.

## Primary hardware leads

Nokia's NSM-1 service manual identifies the product as dual-band GSM
900/1800. Its UE4 chapter specifies a serial GD40 display with 84 x 48
one-bit RAM and a five-by-five key matrix. Its system chapter describes
MAD2's ARM/TI Lead blocks, CCONT, COBBA-GJ and the internal MIC2/EAR route;
the PCM link is 1 MHz with an 8 kHz frame clock, with sign extension in a
16-bit frame. These are hardware facts, not proof of a working firmware
peer or native speech.

Primary references:
- [NSM-1 system module, Original 10/98](https://www.eserviceinfo.com/preview_html.php?fileid=9648&previewid=5152).
- [UE4 UI module, Original 10/98](https://www.eserviceinfo.com/preview_html.php?fileid=9648&previewid=5146).

The [indexed original system manual, page 3-45](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6150/03sys%20%5B92%5D.pdf)
establishes 16 Mbit flash, 1 Mbit SRAM (128 KiB) and 128 Kbit serial EEPROM
(16 KiB). Complete PDF retrieval remains unavailable; fitted flash attributes
and address aliases are not inferred from those capacities.

## Product-local EEPROM input

The original NokiX 2011.07.24 archive contains a 16 KiB `nsm-1.bin` repair
template. Source/member hashes and acquisition URL are in `roms/README.md`.
The static gate audits its original bytes without applying repair scripts
or changing checksums. It is a historical product-local template, not a
verified factory handset dump or proof of boot compatibility.

The own-image directory `0x2da10c..0x2da48b` contains 112 distinct group-7
descriptors; all fit the documented EEPROM, with the highest end `0x3fa8`.
Records `0x0701` and `0x0702` select `0x03cc`/8 bytes and `0x03d4`/44 bytes;
nested record `0x070b` selects the checksum at `0x03d2`/2 bytes.
Settings load `0x2c3a70` places record `0x0702` at SRAM `0x11fc16`.
Validator `0x2bccfe` reads setting state `0x11fc35`, establishing index
`0x1f` and physical EEPROM byte `0x03f3` without a value sweep.
It sums a sixteen-byte identity buffer via `0x2b1e2c`, adds the setting and
compares the truncated result with checksum state `0x112826`.

### Identity and checksum closure

Own reader `0x2b8b90` resolves record descriptors and calls the raw serial
reader `0x2bcace`. The latter deposits bytes from receiver `0x2bdece`
unchanged; no record decoder intervenes. The security record is loaded into
`0x112820`, putting the compared checksum at offset `+6`, independently
agreeing with nested record `0x070b` at physical EEPROM `0x03d2`.

Receiver `0x2bdece` independently establishes the serial wiring: it builds
PUP GenIO data address `0x20020` and direction address `0x20024`, releases
SDA by clearing direction bit 0, raises SCL with data bit 2, samples data
bit 0 while SCL is high, then lowers SCL. Its shifting `0x80` receive mask
establishes MSB-first byte reception. The static gate pins these instructions;
this is not yet runtime verification of the complete serial transaction.
An executable NSM-1 profile must select SCL bit 2, not the NSE-8 default bit 3.

Identity wrapper `0x2ab73a` invokes `0x27e254` with selector 3. This reads
eight EEPROM bytes at `0x000c`, renders the first seven bytes as high/low
packed-BCD decimal digits, computes the fifteenth decimal check digit and
null-terminates the buffer. The wrapper also sets byte 15 to zero. Assuming
successful serial reads, the original template produces
`493006102132132` followed by zero. Raw BCD bytes must not be substituted
for that sixteen-byte ASCII buffer.

The original setting remains `0x58`. Its required checksum is `0x034d`,
not the stored `0x3124`; the unchanged historical template cannot satisfy
this initial security-check relationship. `verify-6150-static` generates
ignored `roms/research/nsm1-v523/nsm1-security-checksum-valid.bin`, changing
only EEPROM bytes `0x03d2..0x03d3`. SHA-1 is
`07335e492f72ba9da9c3436890c9eb4af67fd9a9`. Identity, lock/settings byte,
calibration and all other content remain unchanged. The canonical acquired
template is not rewritten.

This is an evidenced nonvolatile-input consistency fixture, not a factory
dump or boot promotion. Remaining checksums, runtime boot, native DSP and
radio acceptance are unproved. Do not alter a lock level to seek a passing
screen, transplant NSE-3 records or synthesize a DSP completion.
