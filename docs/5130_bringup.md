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
Offline SMS draft save/cold read/deletion/cold-empty Outbox also pass. Registration, calls
and network SMS remain unproved.

### Physical diagnostic-display exit and cold idle

Own descriptor `2a889c` maps record0604 to EEPROM0299/length1. On the
qualified `nsk1r4t` composition, physical Menu/Up reaches the Net monitor
entry after acknowledging the Battery low warning. Selecting Test00 through
its real editor changes0299 fromff to3d and removes the diagnostic-looking
startup text. The selected screen and subsequent independent cold boot show
a clean Headset-titled idle with Menu softkey. Headset is not an operator
or proof of the physical accessory-input polarity.

The selection run changes127 EEPROM bytes in total. The other126 bytes
remain semantically unmapped and are not treated as noise or whitelisted.
The entire2048-byte resulting bank is protected by SHA256
`6d31cb5b3d495145f6d0903147aeebf98571cd59b5adab538e03bc866693206e`.
A separate48s process copies only executable inputs, NVRAM and CFG, enters
phone12345 with physical contacts and repeats no selection/application key.
It preserves the entire bank, complete original3570-byte PIN-disabled lab
card, own executable flash and hardware-input configuration. Exact36/42s
idle RGB SHA256 is
`68fbc5929acbebe923d4df2456ca487e044cdde0e122e05d4eebbf5c9ce02fc2`;
the25s title-free phase is separately checked. The54s selection gate also
requires the warning, Net monitor, Test01/Test00 editor and idle frames,
physical contact timing, own decoded keys, matching phone-code gate and
native/storage/provenance checks. No EEPROM value is directly supplied by
the selection fixture.

```sh
.venv/bin/python tools/nsk1_diagnostic_display_check.py \
  tmp/nsk1-security-positive-qualified \
  tmp/nsk1-diagnostic-display-selected tmp/nsk1-diagnostic-display-cold
```

Add `--mame mame/mame` only with two fresh outputs to execute. This remains
the declared published-record/checksum/retry and nominal ADC2 comparison
using an unproved NSE-1 mask/clock, not factory provisioning or normal-machine
promotion. The short idle result does not resolve the93.974s supervision
reset, RF acquisition, registration or native speech below.

### Native idle restoration and physical continuation

A fresh54s process from that qualified clean-idle cold input authenticates
phone12345 without an application key before save40s. ARM PC289408/SP10c100
and all37 observed ARM registers,36 DSP fields and rolling checksums of64KiB
backed research ARM RAM, ordinary DSP data and program restore exactly.
The DSP is actively executing at47a1 at the checkpoint, not merely stopped
in IDLE. Reference/restored2.7s windows each contain1170 identical DSP I/O
writes, including relative time, address, data and mask. Both physically
pressed Menu contacts decode19 and render exact Phone book pixels, RGB SHA256
`99915d05551950033d0cfbfde4d35b743a5f75729eb3e9212d665df521fda193`.

The full selected EEPROM bank, original PIN-disabled card, own flash and
external CFG remain unchanged. The cold link excludes copied RAM/save state;
the new save artifact is nonempty and hashed. The checker additionally
requalifies source idle/authentication, own loader/record/native observations,
all architecture fields, replay completion and both physical continuations.

```sh
.venv/bin/python tools/nsk1_native_state_check.py \
  tmp/nsk1-diagnostic-display-cold tmp/nsk1-native-idle-state
```

Add `--mame mame/mame` with a fresh output to execute. This observes the
current64KiB research decode, not fitted SRAM geometry; rolling checksums
and bounded I/O equality are not exhaustive memory/device-state proof.
All existing provisioning, mask/clock, RF/speech and93.974s supervision
limitations remain. No firmware result or network response is supplied.

### Toolkit refusal and EEPROM rewrite boundary

An explicit Phase-2+ external-card input reads EF_PHASE03 and produces own
four-byte TERMINAL PROFILE `03018000`, STATUS9116 and FETCH22 of `DCT3 SAT`.
The ME automatically returns `810301218002028281030130` (result30, beyond ME
capabilities), accepted with9000 before the68s physical Menu contact. It
never displays the proactive text;65s remains clean Headset idle and Menu
opens the ordinary Phone book at70s. Both native samples remain active with
no unsupported-instruction stop. This tests one request's refusal, not
successful DISPLAY TEXT or absence of every Toolkit command.

The matched74s Phase-2 control reads02 and has no proactive APDUs. Its
entire EEPROM is unchanged; the Phase-2+ run changes108 bytes while retaining
the complete card and executable flash. The differences occupy18 repeated
33-byte spans beginning0550, changing positions1..3 and5..7 in each span.
Own descriptors at2a89fc/2a8a04/2a8a0c map0728/0729/072a to0550/0658/06fd,
each with declared length33. This is record geometry, not proof of a hardware
fault. The changed executable-looking bytes are explained below as unused
padding in an empty encoded digit field; broader record semantics remain open.
The Phase-2+ output bank SHA256 is
`1bcc40818ee276be75f40554240f5f2e5a94da8a2e85af0e9537ef9a1bc2ee0d`.
The checker protects that whole observed transition but explicitly returns
`eeprom_integrity_validated=false`. It does not whitelist those regions or
classify the changes as harmless housekeeping. Storage-integrity acceptance
is unresolved, not a successful card-feature gate.

```sh
.venv/bin/python tools/nsk1_toolkit_check.py \
  tmp/nsk1-diagnostic-display-cold tmp/nsk1-toolkit-refusal
.venv/bin/python tools/nsk1_toolkit_check.py \
  tmp/nsk1-diagnostic-display-cold tmp/nsk1-toolkit-control --control
```

Add `--mame mame/mame` with fresh outputs to execute. Only SATCFG differs;
phone authentication, native execution, raw SIMI TX/RX, card statuses, payload,
response/contact ordering, complete storage, configuration and pixels are
independently checked. Shared success/refusal parsing retains the6130's
automatic refusal and the6150's physical-success contracts separately.
An additional passive74s capture (`tmp/nsk1-toolkit-nv-source`) observes the
write routine's entry at28ee2e before its prologue changes arguments. All18
complete33-byte records already match the final EEPROM at that boundary,
using RAM source10ce38 and caller28ca0f, in address order0550..0781 at
17.074376..18.716675s. Thus these differences are not introduced after that
boundary by I2C serialization or EEPROM persistence. This does not prove
the source data valid, exclude an earlier read failure, or identify the
record semantics. Static code at28c9da resolves the record address/length
through28c956 before calling28ee2e; the latter transmits bytes loaded from
the supplied buffer (28eebe/28eec0).

`tools/nsk1_nv_source.lua` composes the same physical/card fixture with this
bounded, read-only observation. Recheck both the complete experiment and
the source comparison:

```sh
.venv/bin/python tools/nsk1_toolkit_check.py \
  tmp/nsk1-diagnostic-display-cold tmp/nsk1-toolkit-nv-source
.venv/bin/python tools/nsk1_nv_source_check.py tmp/nsk1-toolkit-nv-source
```

A second74s capture (`tmp/nsk1-toolkit-nv-encode`) identifies the buffer
producer. For each of18 writes,285a98 receives an empty string at10ce88
and output10ce28, called from249c51. Its zero-input path stores only a zero
length byte then returns; it does not initialize the trailing15 bytes.
At249c82..249c8c two LDM/STM pairs copy16 bytes from SP+4 to SP+14
(10ce38). The observed249c88 stores copy00290951/0028bb2a from the stack,
not SIM payload. Record requests from249cb5 enumerate0728 indices0..7,
0729 indices0..4 and072a indices0..4.

The corresponding reader249938 copies the field and calls285a44 at249992.
That decoder tests the leading length and emits an empty string without
reading payload when it is zero. All108 changed bytes lie in that unused
payload; both initial and final lengths are zero, and all other bytes retain
the previously checked whole-bank transition. Thus this specific byte drift
is firmware-persisted empty-field padding, not demonstrated EEPROM corruption.
This is not a general validity claim for the original records or every reader.
The checker still requires the complete observed bank; no region is exempted
from the product storage guard and `eeprom_integrity_validated` remains false.

```sh
.venv/bin/python tools/nsk1_toolkit_check.py \
  tmp/nsk1-diagnostic-display-cold tmp/nsk1-toolkit-nv-encode
.venv/bin/python tools/nsk1_nv_source_check.py \
  tmp/nsk1-toolkit-nv-encode --empty-encoding
```

Paired26s lifecycle captures (`tmp/nsk1-toolkit-nv-context` and
`tmp/nsk1-toolkit-nv-context-control`) correct the apparent trigger distinction:
**both profiles execute the same record updates**. After physical code
acceptance,2713cc stores20 in the context word at10acdc. Selector270e74
receives05e7 with context stage byte10acd7=01 and flag byte10acdf=20.
Its explicit270e8a..270e96 branch calls24b420 at270ea8. That routine loops
classes3,4,5 through24b3e0, supplying the same zeroed31-byte input to both
field arguments. The selected indices are0..7,0..4,0..4 respectively; all18
calls to249c2c come from24b40c. The matched control also writes all18 complete
records, but its unused padding already equals the initial EEPROM, so the
final bank remains byte-identical. The Phase-2+ run carries different stale
padding. No unique card-triggered erase or storage fault follows from this
comparison. Why the broader firmware lifecycle chooses that context state
is not established by this bounded observation.

```sh
.venv/bin/python tools/nsk1_nv_source_check.py \
  tmp/nsk1-toolkit-nv-context --empty-encoding --lifecycle
.venv/bin/python tools/nsk1_nv_source_check.py \
  tmp/nsk1-toolkit-nv-context-control --lifecycle
```

These26s captures qualify the observed lifecycle, not the later65/70s frames;
the separate74s feature/control runs above remain the UI evidence. The whole
output banks and original card/executable remain guarded. Do not erase records
or tune timing merely to make unused padding match.
Normal-machine, fitted-mask, RF and speech claims are unchanged.

Physical manual network-search initiation also passes from independently
qualified initialized storage: Settings -> Phone settings -> Network
selection -> Manual renders **Searching / Quit**. The gate verifies all
fourteen navigation press/releases, their decoded keys, exact selection
frames and the Searching body (excluding only its animated progress strip).
It observes task-16 dynamic 05df requests for contexts 0 and 3, delivery to
the previously idle server and both 05e8 replies. SIM storage is unchanged.
The native radio census still contains only the initial nine TX packets
and one 8b reply through the qualified interval: this is UI/control
acceptance, not completed search, operator discovery or registration.

```sh
.venv/bin/python -m tools.nsk1_network_search mame/mame QUALIFIED_SOURCE NEW_RUN
```

`QUALIFIED_SOURCE` must be an own initialized 61-second compatibility capture;
the runner rechecks it and copies only inputs/NVRAM/config, never RAM or a
save state. Qualified result: `tmp/nsk1-network-manual-qualified`. The menu
sequence is an own-English-ROM harness detail, not a device configuration.

The extended `--cancel` gate is currently **failing**, not accepted:
Searching remains visible at 80/89 seconds, physical Quit at 90 seconds is
decoded and restores the selection list at 92 seconds, but firmware requests
a reboot at 93.974106 seconds. The Security code frame at 99 seconds is a
post-reset boot, not a normal menu return. The gate rejects that reset and
requires a stable selection list instead; its oracle has not been weakened.
Matched initialized cold runs with manual search/Quit and with neither
action request the same reason-68 reset through caller 22f117. Thus the
late failure is not caused by cancellation. Captures:
`tmp/nsk1-network-cancel-reset-source` and
`tmp/nsk1-radio-late-watchdog-control`.

The reset is native DSP packet-activity supervision. Dispatcher 294180
owns counter 10bf8a, incremented at 294196 for received packets. Timer dd
checks it at 2942c0: a zero count selects 29418a ->28c2b2(argument 1).
The policy writes reason 68 at 28c34c, then delayed event d5 reaches
22ef78/22effe and reset request 295136. Nonzero windows clear the counter
at 2942d0 and rearm timer id1d with ticks09cd. Qualified capture
`tmp/nsk1-native-liveness-contract` observes windows 1,1,0: clear at
31.000230s, the native8b increments to1 at38.791914s, clear at62.001171s,
then the empty window faults. The explicit failure diagnostic is:

```sh
.venv/bin/python -m tools.nsk1_radio_trace_check LOG --diagnose-liveness-reset
```

Independent timer d3 is not the cause. Its counter 10fdea only rises to1
and is cleared at282a50 from the idle loop2893b2; it never reaches its
threshold15 in the changed-write capture. Byte10fdef has multi-valued
protocol-state uses, not an established periodic heartbeat contract. The
shared reset tail22f112 must not be used to assign d3 ownership to d5.
The next boundary is the native DSP's legitimate post-RSSI packet-producing
lifecycle. No arbitrary periodic packet, disabled watchdog, counter poke or
imported heartbeat format is justified by this liveness requirement alone.

Ordinary erased storage remains a failed-startup control. The record-only
and checksum-only fixtures remain blank, with checklists `0a` and `0e`.
The combined fixture deliberately retains its firmware-owned record update
and reason-`04` restart rather than suppressing them. Identity and calibration
are not borrowed. `2915` separately stops after its first loader exchange.
Physical SIM wrong-PIN/cold-correct-PIN verification also passes using the
shared laboratory card. Physical PIN exhaustion, cold PUK recovery and cold
replacement-PIN verification also pass, including a wrong-PUK retry persisted
across cold recovery. A separate twelve-process gate also proves permanent
PUK exhaustion and cold SIM-card rejection; see SIM security below.
Fitted-mask, physical-clock and measured analog calibration evidence remain
incomplete; no independent NSK-1 DSP mask has been acquired.
Do not inherit
NSE-1/NSB-1 EEPROM contents, DSP identities, acknowledgement counts or clocks.

## Native radio frontier

`nsk1_radio_observe.lua` is an observation-only boot/physical Call/End
instrument, not registration or call acceptance. The own compatibility
composition sends seven type-51 blocks at 3.2568..3.2778 seconds, then type
1a/68 bytes at 5.923497 (`00219800` prefix) and type 4a/4 bytes at 5.956887
(`02000000`). Native code at DSP PC 3805 publishes one type-8b, 162-byte
response at 39.930208 seconds, beginning `00200374ff6c0375ff6c`.
The passive ring census is explicitly bounded to 2..59 seconds, after the
observed provisioning restart, and records nine TX packets and one RX packet
without reaching its 128-packet cap. It does not classify earlier cursor
initialization as packets or close all other command paths. Physical Call/End
does not add a ring packet within this interval; End presentation is not
connection or speech proof.

Own MCU consumption is independently mapped and observed:

| Boundary | Own route |
| --- | --- |
| RX ring header | 27333e reads a28b; wrapper 27338a returns object 1030f4/header 1802a28b to 2942b2. |
| Dispatch | 2941a6 subtracts 80 then 3; table 2941c4 index 8 selects 294210 for type 8b. |
| Status conversion | 26a440 loads literal 13b8 from 26a534, updates the header, then posts via 255268 to task 0b. |
| Task consumer | 218892 reads 13b8. The own 13a3 subtract cascade selects table 218918 index 14, target 218996. |
| Lifecycle | Runtime owner 107158 has state +14 = 3 and phase +19 = 1. At 2189d6 it copies 160 bytes from message +6 into cached object 102fd4. |
| Deferral | 218a02 tests state +14; state 3 branches to the common tail, bypassing candidate routine 215d4c. That routine is not entered in the measured run. |

The fixed-object status read tap is scoped to this observed allocator result,
not a whole-allocator ownership proof. Debugger/observer reads are excluded
from firmware-consumption observations. Do not import the sibling 5190's
task-12 alias or candidate-vector interpretation: this own ROM posts to task
11, and the cached payload's units/validity remain unresolved. The next
question is which ordinary input changes owner byte 10716c out of state 3
and consumes the cache. No state, status or ring reply should be injected.

### Controller state and activation

The aligned full-image literal inventory finds 16 pools for owner 107158
and three for direct state byte 10716c; the aligned Thumb-16 literal-load
scan finds 41 and six candidate references respectively. These are candidate
instruction encodings, not proof every occurrence is reachable code.
Within `213000..218dff`, the immediate STRB member-offset-14 scan finds seven
candidates: three address this owner (below); four belong to other objects.
The direct-byte reference at 215f3c additionally stores 2 at 215f3e.

| Store | Bounded owner classification |
| --- | --- |
| 218852 | Task-11 initialization sets 3. |
| 2157b4 | Reset/defer path sets 3. |
| 2177e6 | Initializer 2177a0 sets 1 and stores its input at owner +15. |
| 215f3e | Progression path directly sets state 2. |

The fresh runtime watch observes bootstrap clear at 20010c, clear again on
the firmware-owned provisioning restart, then initialization at 218852 sets
3 at 2.216425 seconds. No later write is observed through the 61-second
Call/End run. Neither initializer 2177a0 nor its task-11 activation handler
218d20 is entered. This does not close computed aliases, generic copies,
ARM callers or stores outside the bounded member scan.

The full aligned direct Thumb-BL candidate scan finds two calls to 2177a0:
21794c and 218d52. The first is within another controller routine. The second
belongs to task-11 input 043f: its dispatcher compares against own literal
043f at 218c1c, branches to 218d20, and passes message byte +2 to the
initializer. Thus 043f is a concrete activation predecessor, not an input to
inject. The next task is to identify its own producer/ordinary lifecycle and
the provenance of message bytes +2/+3. No sibling task numbering or control
record offsets should be substituted.

### Activation producer boundary

Own constructor 26def0 handles 043f at 26df9a: owner literal 10be77
supplies bytes +5/+4 (10be7c/10be7b) to message +2/+3, then posts to task
0b via 255268. The aligned literal inventory finds one producer-side 043f
load, at 24df8e, in handler 24df64; it shares the constructor call at
24dd2a. Dispatcher 24d45a compares against own 0834 literal 24d7dc and
calls that handler. The qualified 61-second runtime observes neither the
handler nor a 043f constructor invocation; state remains 3.

The 0834 literal inventory has three aligned pools and three Thumb-16 load
candidates: 2081fa belongs to the upstream constructor's status dispatch,
20c5e0 is a producer-side load, and 24d45a is the receiver above. The next
unresolved predecessor is the ordinary lifecycle selecting 20c5e0 and the
source of the two control bytes. These literal results do not close
table-driven, computed or indirect production.

The predecessor selector is now mapped: 20cb58 forms input 0a30 and its
equal branch at 20cb62 reaches 20c5e0. The common tail invokes constructor
2081d8 with 0834; case 208588 copies owner 10a444 bytes +1f/+1c
(10a463/10a460) into message +2/+3. Constructor 221236 posts its relevant
0a30 path to task 15. A bounded MOVS r0,a3 / LSLS r0,4 / direct-BL pattern
scan finds five calls: 222cdc, 222e5a, 224650, 22473a, 224ca8. This is not
closure of every computed or table-driven producer. All five are preceded
by a store to their owner register r5 +1d (values 2,3,1,2,3 respectively);
the owner and lifecycle prerequisites still require independent mapping.
The qualified runtime observes no 0a30 construction, 20c5e0 selector entry,
or 0834 constructor call. Thus the open software boundary is upstream of
these paths, not an unacknowledged 8b packet to bypass.

The controller's observed output constructor 221236 emits 0a35 (caller
221215), then 0a25 (2236e5), then 09ec (224bfd). Its receive helper 221378
observes object header 158b00c1 from caller 22370d before the 09ec request;
no further helper result is observed through 61 seconds. Own routine 224bdc
sends 09ec and enters that receive loop, accepting 09f0 or 0434 as exit
statuses. Constructor 221236's 09ec branch fills a mode byte from its
firmware owner and posts to task 15. This is an observed earlier transaction,
not proof that an external peer must fabricate either exit.

The request is now observed posted to task 15 as object 1014a4/header
09ec0117: mode byte +2 is 1. Task 15 reads it at 2092f6; its own subtract
cascade selects 2091b2. This stores mode 1 at owner 10a444 +1 and flag 1
at +2, clears auxiliary owner 10a464 +1, then calls 280408. The receive
helper returns 09ec to main dispatcher 20c940. Runtime 20c950 reads lifecycle
state 6 at owner +42, selecting table 20c958 index 6 -> 20a590.
That handler recognizes 09ec; mode 1 is not its special mode-4 branch, so
it takes 20a65c, stores a local flag at owner +1e via 20a83e and enters
continuation 20bbf4. This is not a lost mailbox delivery or an immediate
completion publisher. The temporary broad RAM read probe was removed once the own
consumer was found; only targeted read-only probes remain.

The matched `nsk1_radio_no_call.lua` control accepts the phone code but omits
all dialing/Call/End contacts. Both runs initialize lifecycle word 10a486
to 6 at 209dbe (2.670265 seconds), receive 09ec and enter 20bbf4. Only the
Call/End run receives 0a02 just after the physical Call at 32 seconds and
0a05 just after End at 52 seconds; writer 20cb44 sets lifecycle state 19
at 32.080274 and again at 52.063210. The no-call control observes neither
input nor that transition. These are user-action-correlated continuations,
not evidence of autonomous radio progress.

Without Call/End, no further task-15 helper return is observed through
61 seconds after 09ec. Continuation 20bbf4 reaches receive call 20bcfc;
comparison 20bd02 forms 07e0 and its equal branch advances to 20bd0c.
The next question is the own ordinary source and semantics of 07e0, not
forcing 09f0/0434 or treating the user-driven state-19 path as normal boot.
This comparison does not close every alternate exit of the surrounding
controller. The optional 20aa2e path is not observed in this matched pair.

### Nested acknowledgement route

A bounded MOVS 3f / LSLS 5 / direct-BL scan finds three 07e0 constructor
calls: 24e462, 24ebb6, 24ecee. Each calls 26dc28 with zero message pointer
and zero status argument; the own 07e0 branch posts to task 15 without a
DSP ring payload. This is an MCU controller acknowledgement surface, not
evidence that hardware directly emits 07e0. No such construction is observed
in the 61-second no-call run.

The preceding request is 07d6/mode 1, observed by receive helper 26e424 as
header 07d6018b. Dispatcher 24dff8 reads state 1d from word 109f00 and table
24e000 index 29 selects 24dd0a. That state compares against 07e5; the 07d6
fallback reaches 24d3f0, recognizes 07d6 and enters 24dd14. It constructs
03e9 through 26def0: own table entry 26df38 selects 26e20c, which copies
bytes 109ef5/109ee9 and increments counter 10be75 before posting to task 10.
Runtime confirms object 102fd4/header 03e90100/counter 01. Cleanup helper
26dd50 then releases retained objects; it is not a receive operation.

No later 26e424 receive/return is observed in this no-call interval, so
the next boundary is task 10's handling/completion of that 03e9 request and
the ordinary route back to 07e5. The 07e0 producers are downstream of this
transaction and must not be replayed to skip it. Mode/counter fields are
observations, not values to write into firmware or synthesized replies.

Task 10 accepts this request at 2132e8 with owner 108a84 flags 80000000.
The LSRS-26 test examines bit 25, which is clear, so it calls immediate
processor 212588 through 21356a rather than retaining the pending request.
Own message-mode-1 branch 2127f2 reaches builder 2112b0 with argument 2.
Runtime 2114f4 posts object 101e14/header 0002441a to transport task 3,
with bytes +5/+6 = 21/98, matching the subsequent native ring type-1a
payload prefix 00219800. This is a real MCU request, not a missing mailbox
delivery or an injected scan session.

The no-call run observes only the 03e9 message in task 10's receive loop
through 61 seconds. The separate native 8b vector still reaches task 11.
One static completion candidate is type 8a: table 2941c4 index 7 selects
294218 -> 26a2ec, whose normal branch posts status 1390 to task 10 (unless
its owner byte +3 is 1, where it frees the object). The task-10 outer
dispatcher routes 1390 through 2134dc, then, under its gates, processor
212588 selects table index 2 -> 212b28. That case increments a counter and
compares a limit before continuing; its payload meaning and relationship
to the required 07e5 completion are not yet proved. It must not be treated
as a recipe for fabricating 8a. The next questions are the actual completion
publisher and which native DSP operation can produce its required input.

The acknowledgement namespace is not one-to-one with wire messages.
Helper 26e424's input 05e6 reaches 26e56a; nonzero message byte +2 makes it
return 07e5, while zero consumes the message without that return. The base
literal 0447 at 26e898 plus the decoded subtract offset 019f establishes
05e6 independently of the output literal 07e5 at 26e880. The other explicit
07e5 literal publisher is task-15's special mode-4 path at 20a59e, which the
observed mode-1 request does not take. Neither is evidence that DSP type 8a
alone satisfies this acknowledgement.

Three checked direct calls to constructor 23a728 select 05e6 versus 05ed:
23bb80, 23c010 and 23c5b0 each test owner byte +13a. Zero selects 05e6;
nonzero selects 05ed. The constructor copies owner byte +0 to outer
message byte +2, so selecting 05e6 alone does not prove the nonzero
success recode. The receive helper owns context 10858c and calls the RTOS
receiver at 23abca. Qualified initialized cold capture
`tmp/nsk1-radio-completion-server` passes the 61-second boot/storage check
and native radio trace checker; the server enters 23abca but has no
observed return at 23abce, and no 05e6/05ed/05ea/05eb construction occurs.
This bounds this boot and these direct sites, not all producers. The next
boundary is the server's legitimate ingress; changing +13a would not make
an absent receive occur.

One own-ROM transport ingress is established: type 80 selects 294250,
which calls decoder 269f28. Its conditional 040b construction at 26a132
reaches 26a15e/26a162, posting through 255268 to task 13. Another 040b
construction at 269d84 also selects task 13; a separate branch selects
task 12 and must not be conflated with this destination. Server dispatch
23abdc uses base 040b at pool 23aef0, then recognizes 040c/040d and
05fa/05fb through its subtract cascade. The qualified cold packet census
contains only type 8b, not 80; its server receive remains unreturned.
This identifies an absent lower transport family, not the meaning of a
valid type-80 body, a complete producer census, or permission to synthesize
one. Native RF sampling/decoding and alternate server inputs remain open.

The server is not DSP-only. A full aligned direct-BL candidate inventory
finds 346 calls to post primitive 255268. A deliberately narrow argument
pattern (MOVS r0,13 in the final two halfwords) selects eight:

| Post | Own input source |
| --- | --- |
| 212044, 212d8a, 212e58, 212f20 | Task-10 control paths post template 2ac8e0, header 040c0000. |
| 2697d8, 2699e0 | Select template 2ac2f0/2ac2f4, headers 040d0000/040d0300. |
| 26a162 | Conditional type-80 decoder delivery described above. |
| 26e3ee | Separate constructor 26e25c sends dynamic inputs 05dd..05e3 or 060a/060b. This is not constructor 26def0. |

Shared-tail, indirect and data-derived destinations are outside this narrow
pattern; it is not a closed producer inventory. The constructor preserves
the requested header, so these firmware IPC inputs must not be renamed
DSP type-80 packets. The qualified empty receive applies to all inputs
at the observed server, but their legitimate activation prerequisites
remain independent questions. In particular, 05fa/05fb dispatch recognition
alone does not establish an additional producer: the helper also returns
05fb after its local context/timer cleanup at 23ac4e/23ac6c.

Constructor 26e25c has 33 aligned direct Thumb-BL candidates. The explicit
05dd call at 24d9a8 is reached from input 07ea at selector 24d70e via
24d714 ->24d99c, provided owner byte +0a differs from 4. It supplies
null object/field2=3, then stores owner +0a=2. Status 07ea is itself virtual:
helper 26e424, in receive mode 2 with no prior result, checks queued pointer
`[109f24+4]` at 26e62e and returns 07ea through 26e786 when nonzero.
It must not be requested as a DSP wire message. Qualified cold run
`tmp/nsk1-radio-dynamic-server-request` passes boot/storage and native-radio
checks, with zero calls to 26e25c. The queued-work producer, helper mode-2
selection and other dynamic call arguments remain open; the callsite census
does not close indirect callers.

Queue storage is explicit: helper dispatch 26e6f8..26e6fe selects
07d3/07d4, then 26e47c tests message byte +2. Nonzero replaces/frees the
old queued object and stores the new message at 109f28 through 26e4b4.
Zero instead forwards the message through constructor 26e25c as 05e0
(07d3) or 05e1 (07d4). The explicit mode-2 receive call is 24ee1a:
24ee0e selects mode 2 only when owner byte +0a is neither 2 nor 3;
otherwise 24ee3e selects mode 1. Cold capture `tmp/nsk1-radio-queued-work`
passes boot/storage and native-radio checks, observes only zero writes
to 109f28 (startup clear and 24dcd4), and never enters receive mode 2.
This is a dormant queued-work route, not a demonstrated missing startup
packet. Upstream request eligibility and computed aliases remain unclosed.

There is also a nested-buffer decoder, not an equivalent completion code:
05ea/05eb select 26e4e4, which reads the buffer pointer at message +8.
Buffer byte +4 low nibble 6 selects 23daac, with outer message byte +3
passed separately. Its command-byte +5 value 24 selects 23e784; the success
return at 23e86e is 0813 (literal 23ebbc), not 07e5. Constructor 23a728
allocates the outer message and posts through 255268 to task 16. These
own-ROM paths corroborate the sibling decoder structure but do not establish
that this command completes task-16 state 1d. Other parser commands and
upstream buffer producers remain to be classified.

The qualified unattended capture `tmp/nsk1-radio-buffer-source` retains the
61-second native run with a read-only breakpoint at constructor 23a728 for
inputs 05ea/05eb: zero hits. Its transport census remains nine TX packets
and one RX packet through 59 seconds, and `check_phase(..., through=59)`
passes. This excludes observed use of this constructor in that boot only;
it is not an absence proof for every producer or a test of received RF data.

Shared scan-state byte 108a8d belongs to builder base 108a00 +8d;
task-10 catalogue base 10885c is a distinct owner. Runtime initialization sets 16;
builder 21151a sets 12 at 5.923710 seconds. Native 8b consumption at 21899e
then changes it to 10 at 39.930843 seconds. Thus the vector updates shared
scan state even though its controller retains state 3 and caches the data.
No counter case 212b28, scan finalizer 2111d4 or helper recode 26e56a is
observed through 61 seconds. Static finalizer 2111d4's ordinary branch
posts template 2ac904/status 042f to task 15; it does not directly post 07e5.
The 05e6 producer/transport route and the actual finalizer-triggering DSP
contract remain open and must be mapped without conflating these statuses.

Finalizer delivery is also not a direct acknowledgement recode. Task-15
helper dispatch 20931c selects 042f by 0410 +1f, reaches 209480, copies
message byte +2 to r7 +18 and returns the unchanged input in r6 through
20948a. It does not return 07e0. Any claim that scan completion releases
the 20bd02 wait still requires the downstream lifecycle path, not just
the presence of a finalizer call.

The acquired NSE-1 resident ROM4 program contains a native type-8a publisher:
313f tests data 1949 bit 0080 and branches to 3153 when set. Otherwise it
copies data 1867 to 1200, sets AR5 to 2 and AR3 to 1200, then CALLD 37ce
at 314d sets packet type AR4=008a in the delay slot at 314f. The optional
`--dsp-program` contract checks the acquired image hash and this instruction
sequence; this does not establish the fitted NSK-1 mask.

CPU-qualified breakpoints with explicitly DSP-scoped data expressions are
required here: debugger `focus` controls observation, not the CPU used by an
unqualified breakpoint or expression. Earlier unqualified DSP breakpoint
negatives are not evidence. Capture `tmp/nsk1-radio-native-operation-scoped`
passes the 61-second boot/storage checks and positively observes native
entries 3000, 3076 and search handler 3d5e, but no entry at 30bf, 30f5,
313f, 314d or 3153. Thus the operation initializes but does not reach the
local 1949 gate. Its bit value alone cannot explain the missing 8a.
The selector at 3078 calls 31ae with AR2=4. That helper computes T=15-AR2
and returns BITT of data 06d1; C54x bit numbering therefore selects physical
bit 4 (mask 0010), not bit 11. Clear TC enters the service loop at 307e;
the set path reaches 3096 and subsequently requires 06e3 bit 0 before 30bf.
The qualified final capture `tmp/nsk1-radio-native-operation-final` repeats
the positive entries and samples 06d1=010e, 06e3=0000, 1949=0000 at
30.5, 33, 36, 50 and 59 seconds, with the boot/storage checks passing.
The sampled control word leaves bit 4 clear. Its legitimate setter and the
operation that requests it are the next boundary; neither a guessed packet
nor a changed DSP control word is justified.

The setter is generic, not a missing literal store. After the mailbox-0872
comparison with 4 succeeds, 31e7..31ed loads the pending index from 06d0,
forms `1 << index`, and ORs it into 06d1. Request helper 318e tests the
corresponding bit, stores its index to 06d0 at 3191, and calls publisher
382f with pointer 0871. The qualified `tmp/nsk1-radio-native-control-writers`
run passes the 61-second boot/storage checks and observes post-restart
pending indexes 1, 2, 8 and 3, with completion writes 0002, 0006, 0106 and
010e. No pending index 4 is observed; neither 32-write cap is reached.
An aligned resident CALL/CALLD candidate scan finds six calls to 318e.
Exactly one has the immediate delay-slot assignment AR2=4: CALLD at 4a49,
followed by STM at 4a4b. This is the next static caller to classify, not
proof of its ordinary execution or the payload semantics of mailbox 0872.

The caller is selected by host control type 70: dispatch 397e..3982 calls
4951 for that type. Handler 4951 extracts the upper byte of its first
payload word and uses table B0a6 for values through 18; companion acquired
data-ROM entry B0b7 is 4a3c, identifying subcommand 11. Routine 4a3c
services the mailbox, optionally waits on its previous index, requests
index 4 at 4a49 and waits at 4a4d before branching to 3aac.
Capture `tmp/nsk1-radio-native-request-dispatch` passes the boot/storage
checks and observes 4a3c/4a49 before the provisioning restart: pending
06d0=0004 at 0.603154 seconds, completion 06d1=011e at 0.603515 seconds.
The later reset clears both words at 1.299708 seconds; subsequent observed
requests are 1, 2, 8 and 3, not 4. This is not an operation that never
executes, but its first-boot occurrence does not establish normal acquisition
ownership.

The restart-only explanation is disproven by independent cold continuation
`tmp/nsk1-radio-native-cold-control`: only initialized inputs/NVRAM/config
are copied, not RAM or save states. Its 61-second boot/storage check passes
with minimum one record. Requests 1, 2, 8 and 3 again complete to 010e,
without index 4. Post-two-second traffic remains nine TX/one native type-8b
RX (the latter at 38.791418 seconds). The first-boot clear is a native
loader write at PC 0f10, not an observed device-side RAM discard. Thus
the absence survives an ordinary initialized cold boot. The type-70/11
control path remains a candidate completion family, not an established
ordinary acquisition prerequisite; its first-boot use alone does not justify
replaying it or changing reset retention.

The observed RSSI vector has its own positively identified native route.
Publisher 47f6 sets AR4=008b at 47ff and CALLD 37ce at 4810 sets AR5=00a2
in its delay slot. Qualified cold capture `tmp/nsk1-radio-native-rssi-caller`
passes the 61-second boot/storage checks. At RX producer write PC 3805,
SP=1e82 contains `0e13,008b,00a2,4814,0a2e,30ed,38ca,3976`.
Live DSP disassembly confirms CALL 47f6 at 0a2c, returning to 0a2e;
the acquired resident image instead has that call at 0a17. The uploaded
program, not the resident-only caller census, is authoritative for this
execution. Live 0a2e clears TC (ST0 bit12) and conditionally returns on NTC; the
alternate tail writes sentinel 7fff and updates 11f9/11fa. No type-8a
publication is implied by this RSSI return. The existing candidate 8a
mapping must not be promoted to a required scan completion without its
own operation/request evidence.

Qualified cold capture `tmp/nsk1-rssi-return-qualified` follows publication
with exact DSP breakpoints at 0a2e ->0a2f ->30ed ->30f0 ->30f2. ST0=081f
has TC clear throughout; 06be=0020, 06e1=0000, 06e3=0000 and both list
pointers11f9/11fa=03ff. The conditional return is taken, not stalled at the
publisher; the 06be bit0400 test is clear and execution falls to30f2.
The marker-writing tail0a30 is not observed. The bounded return trace is
armed by actual type8b publication and has separate debugger bits from the
candidate-entry count, so it does not suppress that observation. Native
boot/storage and packet/mailbox/stack checks pass. TI SPRU172C's RSBX
definition and SPRU131G table4-1 establish that F4BC clears ST0 bit12 (TC),
not OVM. Neither this return nor the pointer values prove completion of
the wider native scan lifecycle; its subsequent work/list progression
remains the next question.

The next branch is qualified in `tmp/nsk1-rssi-gateway-compatibility`.
After publication, data1983 and AR3 select03ff, whose value0374 is a channel
word, not end marker7fff. Execution takes3103 ->311a ->312c ->3130 and
enters0a71; neither immediate-completion3112 nor subsequent313a is observed
within61 seconds. Crucially, live program2420/2421 contains `f073 0a71`,
not the acquired resident image's `f073 0a5c`. The upload updates this
gateway to the relocated routine, whose entry word76f8 matches the scan
initializer. Live0a5c is891a inside another routine. Thus a stale borrowed
gateway does not explain this boundary. Boot/storage and executable native
trace checks pass, including gateway, branch and register/cursor values.
The next target is the wait/return condition inside live routine0a71;
the resident-only0a5c function address must not be used for that trace.

A further qualified cold capture, `tmp/nsk1-live-scan-wait`, includes live
disassembly of0a00..0dff and positive execution markers at0a94,407c,408c,
4097,0a99,0aca and0ae4 after RSSI publication. Thus the initializer's
407c interrupt/IDLE wait does return; it is not the terminal boundary.
Live0ae4 calls4701, whose disassembly includes sample-buffer cursor waits
at4717 and4742 comparing data06b7 against the cursor selected from06b8.
These are candidates for the next runtime trace, not proven stalls or a
license to fabricate sample data. The capture passes independent boot,
storage and native-radio checks; registration remains unproved.

`tmp/nsk1-scan-progress-qualified` rules out a permanent empty-sample-queue
stall at this boundary. Positive markers reach4701,4717,3360,4742,0ae6 and
479e. The producer advances1a00 ->1a04 and the consumer catches up;4701
returns to0ae6. Five passive samples at40,45,50,55,60 seconds show the
double-word counter1262/1263 advancingffff:c296 ->ffff:d383 (4333 steps),
with matched queue cursors at each sample. Data1344 and1266 stay zero.
Thus the scan measurement loop runs but has no observed qualifying result;
sampling activity is not evidence of valid RF data. Live0b08 increments
the double-word counter and0b0e exits if it becomes positive;0b11/0b13
continues the loop while1266 bit0 is clear. The next question is the
qualification/termination contract, including how its duration relates to
the independently proved packet-liveness reset. Do not disable that reset
or invent sample/heartbeat traffic to make the two timelines compose.

The live qualification contract is narrower than a missing-completion claim:
4701 returns a measurement in A;0ae6 subtracts data219b into B. Only a
positive difference increments1344, and a new maximum over198d sets1266
bit0. The initializer seeds1262/1263 withffff/c1bf;0b08..0b10 increments
that double word and exits at0cb8 when it becomes positive. With bit0
clear,0b13 returns directly to the next measurement, bypassing the later
500-qualifying-observation and subsequent candidate-processing exits.
In `tmp/nsk1-scan-qualification-final`, eight passive observations at39s
read A=0,219b=0574 at fetched PC0ae7. Late samples65..90s retain zero1344
and1266, while the timeout counter advancesffff:d7be ->ffff:ece7 and is
still negative. Thus this no-result scan has not timed out before the
separately observed93.974s reboot. This identifies a timeline mismatch,
not its cause: sample cadence, other native packet producers and the
borrowed resident-mask compatibility remain distinct questions. It does
not justify changing thresholds, shortening the timeout, supplying guessed
RF samples or disabling packet supervision.

`tools/nsk1_scan_qualification.lua` reproduces the passive extension of the
initialized cold fixture for92 seconds. In addition to the ordinary native
boot/storage checks, run `python -m tools.nsk1_radio_trace_check LOG
--scan-qualification` to require its measurement and late-counter evidence.
The ordinary61-second radio gate alone does not cover this longer horizon.

The qualification fixture also captures the live page-zero program at45s as
`native_program_45s.bin` (65,536 big-endian words). The qualified capture
`tmp/nsk1-scan-publishers-qualified` has3,146 changed words relative to the
65,535 overlapping acquired resident words, but the same19 raw CALLD
candidates targeting37ce and no immediate B/BD candidates targeting it.
`tools/nsk1_radio_contract.py --live-dsp-program` reports all candidate
addresses and coverage; it does not infer packet arguments or reachability
from nearby literals. Indirect transfers, other program pages and direct
ring writes remain outside that census. No autonomous heartbeat contract
follows from the unchanged candidate set.

The source audit still maps sample production to the frame ISR (vectorffc0
to3204; mode1 at32f4); the separate transfer vector goes to3598. Existing
CTSI frame timing is not falsified by slow scan completion alone. Exact
zero unattached RF input is also not evidence for the noise/offset encoding
of a powered physical receiver. The outstanding boundary is therefore the
RF-input and packet-production contract, including the data-driven serial
publisher at9404 and eligibility of the existing control/result producers,
not a justified clock multiplier or synthetic periodic packet.

The live serial path is now bounded:8dd0 first checks queued count06b6,
then control flags06f3 (0400 bypass,0100 selects8df5,0200 selects8ded).
The only two raw direct caller candidates of938b are8ded and8df5; their
CALLD delay slots load AR4 with009b and009a respectively. Helper938b itself
returns when signed count06b6 is nonpositive. Its packet publication at9404
therefore serves queued serial data on these paths, not an unconditional
heartbeat. `serial_publisher_contract` checks the direct caller set and
the count/argument/publication sequences in the captured live image;
indirect entry and queue-producer ownership remain unclosed.

The qualified passive probe in `tmp/nsk1-serial-contract-final` observes
four3f50 dispatcher entries near2s with count06b6=0, no9404 or8e1c publisher
entry, and the positive4810 RSSI entry near38.791s. Its AR5=03ff snapshot
precedes CALLD's delay slot setting packet length00a2; it is not a changed
wire length. The integrated scan fixture retains probe handles, bounds each
site to four entries and checks the positive control before accepting the
negative. These four count samples are not an all-writer census of06b6.
The independent cold/storage/radio and90-second scan checks still pass.

The queue producer is distinct from the RF sample queue. Handler8ed3 reads
I/O35 at8ed8 and I/O36 status at8ee6, then runs the parser-state dispatch
8f39..8f40. The checked positive count updates are8fb5 and8fc8. A full
page-zero raw-word scan finds nine06b6 operands, not a proof against indirect
or alias writers. Port27's frame-driven RF reduction instead supplies the
06b7/06b8 queue; these must not be conflated with serial count06b6.

The61-second cold probe `tools/nsk1_serial_queue.lua` reuses the existing
ROM4 serial observer and records two CPU-bus writes to06b6, both zero.
Its complete port summaries have no35 reads/writes;36 has four reads and
two writes, ending0080. RSSI4810 remains the positive execution control.
Qualified capture: `tmp/nsk1-serial-queue-final`. Check with
`python -m tools.nsk1_radio_trace_check LOG --serial-queue` alongside the
independent storage qualification. CPU-bus taps do not cover backend upload
copies. The shadow-state model has no independently evidenced35 receive
source or serial request-bit0200 producer; which physical interface owns
these ports remains open. An absent optional serial input is not proof of
the missing ordinary-radio completion and must not be populated with guessed
bytes to satisfy the packet watchdog.

The native tuning parameters are product-owned, not inherited NSE-1 addends.
Own descriptor29821c begins216a/1e80/011c/0130/0078/0000; payload298228
begins0004/17bf/1797. The61-second cold capture
`tmp/nsk1-rf-tuning-provenance` observes MCU296104 reading000417bf and
17971d00 at298228/29822c. Native overlay copy31d7 subsequently writes
216a=0004,216b=17bf,216c=1797, after zero initialization. With selected
word0374 and control06be=0020, packing channel+receive-addend gives
`pack_tuning_nonnegative(0374+17bf)=00303633`; the actual4023/4028 port
callbacks write3633 to31 then0030 to32. All source/upload/port events are
ordered and the ordinary native boot/storage/radio checks pass.

Reproduce with `tools/nsk1_rf_tuning.lua` on the initialized cold fixture;
`python -m tools.nsk1_radio_trace_check LOG --tuning-provenance` checks the
correlation. The static own-image contract also pins the descriptor/payload.
This excludes simply transplanting the NSE-1 GSM900 addend13a6 as a repair.
It does not by itself identify the fitted synthesizer or establish frequency units,
prove SCU wire serialization, or supply a physical RF sample representation.
The first two unrelated programming pairs (1d00/0004 and0190/0010) are not
interpreted as accumulator halves merely because the selected-entry pair is.

Independent frequency-plan corroboration is available from Nokia's
[NSK-3 System Module, issue3, pp36/39/42](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6130/03sys%20%5B106%5D.pdf):
64/65 UHF division,200kHz reference,487MHz receive IF and400MHz transmit IF.
The [NSK-1/3 Repair Hints v2, p38](https://files.elektroda.pl/44719,nokia%2B5110_5130_6110_6130_v20.html)
also identifies487/87/13MHz receive filters. These are indexed primary
documents, not locally archived PDFs. [TS45.005 table2-2](https://www.etsi.org/deliver/etsi_ts/145000_145099/145005/17.00.00_60/ts_145005v170000p.pdf)
provides the fixed DCS1800 channel formula.

Using the own addends and the checked packing inversion gives:

| Direction / channel884 | Divider | LO | RF | Difference |
| --- | ---: | ---: | ---: | ---: |
| Receive (observed port pair) | 6963 | 1392.6MHz | 1879.6MHz | 487MHz |
| Transmit (calculated only) | 6923 | 1384.6MHz | 1784.6MHz | 400MHz |

`nsk1_dcs1800_tuning_consistency` checks this numerical relation over all374
fixed DCS channels in each direction. Direction must be explicit: a packed
value can describe different valid channels under the two addends. This is
frequency-plan consistency, not SCU wire validation, observed transmission,
sample timing/encoding or native reception. No machine configuration changes.

`tools/nsk1_radio_trace_check.py` qualifies this observed boundary from the
captured log: ordered 9-TX/1-RX traffic, complete packet bodies, positive
DSP entry controls, uncapped post-reset pending-index/completion progression
and the live RSSI stack. A changed boundary fails for review rather than
silently becoming a new baseline. This supplements, not replaces, the
independent boot/storage qualification and proves no RF validity or registration.

```sh
.venv/bin/python -m tools.nsk1_radio_trace_check path/to/error.log
```

For the mapped mode-1 continuation, 042f fails the explicit success comparison
at 20bd02 and the subsequent 09ef, 0a02, 0410, 09ec and 0a03 selectors.
It reaches 20bb8e; because it is not 07d5, 20bb96 calls 20cb42 with state
19 and 20bb9e sets owner byte +15 to 1 before receiving again at 20bcfc.
This is a static route for that continuation, not an observed delivery or
closure of finalizer handling in every lifecycle state. The scan-finalizer
path therefore cannot be substituted for the transaction's success report.

`tools/nsk1_radio_contract.py` checks the own image hash, critical table
entries/instruction fragments and emits machine-readable bounded references:

```sh
.venv/bin/python -m tools.nsk1_radio_contract path/to/5130f561a.fls
```

`nsk1_radio_observe.lua` is the corresponding read-only instrument for a
fresh qualified startup fixture. Its breakpoints are own-standard-specific;
do not apply them to 2915 or another ROM. The current radio traces are
observations, not a network-registration acceptance gate.

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
Add `--wrong-puk-first` for a fourth process between blocking and recovery.
The physical incorrect PUK `87654321` plus new-PIN/confirmation produces
UNBLOCK `9804`; exactly the PUK1 retry byte changes from 10 to 9. Credentials
and PIN retries remain unchanged. Its frame says SIM card blocked, which is
not evidence of permanent blocking: the persisted PUK counter remains 9.
Cold correct-PUK recovery must restore both counters and change only the new
PIN, then a further cold verification preserves the whole card. This recovery
mode does not exhaust PUK; the separate mode below does.

### Permanent card blocking

```sh
.venv/bin/python tools/nsk1_puk_roundtrip.py mame/mame roms \
  tmp/nsk1-profile-source tmp/nsk1-puk-permanent-final --exhaust-puk
```

This twelve-process gate starts with a complete current-format3570-byte
laboratory card, normal CHV1 retries3 and PUK1 retries10. It supplies no
blocked state. Three physical wrong PINs block CHV1, changing only that
retry byte. Ten subsequent independent cold processes each enter the wrong
PUK87654321, new PIN4321 and confirmation through physical contacts. The
first nine UNBLOCK commands return9804 and decrement exactly one PUK retry;
the tenth returns9840 with both counters zero. Every other card byte,
including credentials, unrelated files and CHV2/PUK2 counters, is preserved.

The last attempt first renders Code error, then the exact SIM card rejected
frame at35/40 seconds. A final cold process renders that same full frame
at10/20/30/40 seconds, sends no VERIFY or UNBLOCK command and leaves the
entire exhausted card unchanged. No frame mask is used. All phases retain
the native boot/storage/execution qualification and no donor provisioning,
firmware-state write or injected APDU is used. This is permanent card-block
presentation/persistence on the native compatibility instrument, not fitted
DSP proof, stable radio or network acceptance. Qualified capture:
`tmp/nsk1-puk-permanent-final`.

### PIN-request setting

```sh
.venv/bin/python tools/nsk1_pin_request_roundtrip.py mame/mame \
  tmp/nsk1-pin-roundtrip-qualified tmp/nsk1-pin-request-final
```

On the authenticated PIN-enabled card, physical navigation through Settings
→ Security settings → PIN code request, entry of PIN1234, and selection of
Off produces DISABLE CHV (`A0 26`) with status9000 and the Done frame.
Exactly serialized card byte3520 changes from1 to0; credentials, retry
counters and all files remain unchanged. Native execution, own flash and
phone identity/code storage checks pass through59 seconds.

A separate41-second cold process with that persisted card and no physical
input issues no VERIFY command, preserves the whole card, and displays the
distinct phone Security code prompt at10/20/30/40 seconds. Disabling SIM PIN
does not disable the phone lock.

Starting from the persisted disabled card, physical phone-code authentication
and the same Security settings menu produce ENABLE CHV (`A0 28`), status9000.
Exactly byte3520 changes from0 to1. A further no-input cold process displays
the exact PIN prompt at10/20/30/40 seconds, sends no VERIFY, and leaves the
whole card unchanged. The authenticated UI requires one initial
Menu press in this disabled-card lifecycle, versus two in the PIN-enabled
fixture; do not reuse the input schedule without its startup authentication.
The four-process gate requalifies the wrong/correct-PIN source capture before
copying persistent inputs only. It checks exact ordered menu contacts,
command statuses, whole-card differences, Done frames and cold prompt frames.
Qualified output: `tmp/nsk1-pin-request-final`.

### Ordinary PIN2 change

The same physical access-code menu independently changes PIN2 from5678 to8765.
Only card bytes3492..3495 change, CHANGE CHV returns9000, and the firmware
renders `PIN2 code changed`. A separate cold process authenticates startup
PIN1, accepts the persisted8765 as current PIN2, and restores5678 through
CHANGE CHV; the complete card then equals the original input. No retry,
PUK, file or PIN-request state changes. Both75-second phases retain the
own native/storage checks and do not establish radio or fitted-mask fidelity.

```sh
.venv/bin/python tools/nsk1_pin2_change_check.py \
  tmp/nsk1-pin-roundtrip-qualified tmp/nsk1-pin2-change tmp/nsk1-pin2-restore
```

Physical fixtures are `nsk1_pin2_change.lua` and `nsk1_pin2_restore.lua`,
composed over the existing PIN1 input harness. The PIN2 route adds one Down
contact before selecting the change-code entry; it is not inherited from
another product's menu layout.

Append `--rejection-cycle tmp/nsk1-pin2-wrong-complete
tmp/nsk1-pin2-retry-recovery` to qualify the rejection control. Physical
current PIN2 `5675`, followed by replacement8765 and confirmation, returns
CHANGE CHV9804 and the exact Code error frame. Only PIN2 retry byte3517
changes3→2. The editor does not authenticate the old-code field on its own:
no card command is sent until the replacement and confirmation are submitted.
A separate cold process accepts the unchanged current5678, applies8765,
and restores retries to3; its complete card matches the successful change
control. Neither the wrong code nor the retry count is supplied through a
card-state edit.

### PIN2 exhaustion and PUK2 recovery

```sh
.venv/bin/python tools/nsk1_puk2_check.py tmp/nsk1-pin-roundtrip-qualified \
  tmp/nsk1-pin2-wrong-complete tmp/nsk1-pin2-retries-one \
  tmp/nsk1-pin2-retries-zero --recovery tmp/nsk1-puk2-recovery \
  tmp/nsk1-puk2-cold-auth
```

Three independently cold physical wrong-current5675 submissions decrement
only PIN2 retry byte3517 from3→2→1→0. CHANGE CHV returns9804,9804,9840;
the final exact frame says `PIN2 code blocked`. No blocked-card fixture is
supplied. The firmware then changes the same access-code menu to a PUK2
editor. Physical87654321 plus replacement8765 and confirmation produces
UNBLOCK9000, automatic verification of the new PIN2, and `Code accepted`.
Only the four PIN2 digits and its retry byte change; the PUK2 retry count stays10.
The recovery script allows the eight-digit PUK entry to finish before the
new-code phase, rather than overlapping contacts on the PIN-change schedule.

A further cold process accepts8765 and changes it back to5678. The entire
card equals the original healthy seed. The checker requalifies that seed,
native execution, own flash/storage, every cold link, physical contacts,
command ordering, complete card differences and exact reviewed frames.
Add `--wrong-puk-cycle tmp/nsk1-puk2-wrong tmp/nsk1-puk2-after-wrong
tmp/nsk1-puk2-after-wrong-cold` for an independent rejection branch from the
same organically blocked card. Physical PUK87654322 plus replacement and
confirmation returns UNBLOCK9804 with Code error. Only PUK2 retry byte3519
changes10→9; PIN2 remains blocked and both credentials remain unchanged.
A cold correct-PUK2 recovery resets both retry counters and applies8765;
a further cold authentication/restoration returns the whole card to the
original seed. Permanent PUK2 exhaustion is qualified separately below;
none of these gates establishes fitted-mask, network or long-run stability.

### Permanent PIN2 rejection

```sh
.venv/bin/python tools/nsk1_puk2_exhaust.py mame/mame \
  tmp/nsk1-pin-roundtrip-qualified tmp/nsk1-pin2-wrong-complete \
  tmp/nsk1-pin2-retries-one tmp/nsk1-pin2-retries-zero tmp/nsk1-puk2-wrong \
  tmp/nsk1-puk2-permanent --check-only --permanent-cold \
  tmp/nsk1-puk2-permanent-select tmp/nsk1-puk2-permanent-navigation
```

The runner first requalifies the healthy seed, all three physical PIN2
failures and the first wrong-PUK2 attempt. Nine further independent cold
processes physically enter87654322 and replacement/confirmation. Each changes
only PUK2 retry byte3519, from9 down to0. UNBLOCK returns9804 until the last
attempt, which returns9840 and renders the exact `PIN2 code rejected` frame.
`--check-only` rechecks raw inputs/logs/storage/frames without running MAME;
omit it with a fresh output directory to generate the sequence. `--resume`
requalifies existing attempts before continuing missing ones.

Two separate cold controls preserve the fully exhausted card unchanged.
Selecting Change PIN2 shows `PIN2 code rejected` and returns to the menu,
without issuing CHANGE or UNBLOCK. In the navigation control, a subsequent
physical Down decodes as18 and moves to Change security code. Full decoded
startup/menu/navigation input, exact rejection/menu frames and unchanged
storage distinguish credential refusal from dead input. This is firmware UI
refusal at zero PUK2 retries, not a claim that a correct-PUK2 APDU was submitted
at zero, nor that the entire SIM is rejected: startup PIN1 still authenticates.

### Fixed-dialling activation and cold state

```sh
.venv/bin/python tools/nsk1_fdn_check.py tmp/nsk1-pin-roundtrip-qualified \
  tmp/nsk1-fdn-on-qualified tmp/nsk1-fdn-on-cold \
  tmp/nsk1-fdn-off tmp/nsk1-fdn-off-cold
```

`nsk1_fdn_prepare.py SOURCE OUTPUT` appends the explicit laboratory FDN
extension to a requalified healthy card and records base/derived hashes;
it does not alter handset storage. Physical Settings4 → Security3 →
Fixed dialling2, On and PIN25678 issue VERIFY CHV2 and INVALIDATE ADN.
Only serialized ADN-validity byte3575 changes1→0. Firmware rehabilitates
LOCI and IMSI through the actual SIMI transport. The settled frame shows
Fixed dialling On; a separate cold process authenticates PIN1, repeats the
identity rehabilitation and preserves the complete card and On frame.

Physical Off with PIN25678 rehabilitates ADN in addition to the startup
identity exchange, changing only3575 from0→1. The final cold process
preserves the complete card, shows Off and needs no validity operations.
All four phases qualify native execution, own storage/flash, source and
cold links, ordered physical/decoded keys, complete APDU payloads/statuses,
whole-card differences and exact settled frames. The warm fixtures are
`nsk1_fdn_input.lua`/`nsk1_fdn_disable.lua`; `nsk1_fdn_cold.lua` only browses
the stored state. Phone-code input is delayed to24 seconds; menu entry
starts34 seconds after the acceptance presentation has expired. Intermediate
LCD captures may lag a selection; the gate uses settled frames and protocol
results, not those images alone. The common expected-card validity transition
now lives in `sim_fdn_profile.py`, shared with the NSB-1 gate; handset menus
and native acceptance remain product-owned.

Protected record save/edit/read/rejection is independently qualified below;
selected-record erase/cancel and local dial permission are qualified below.
The compatibility-mask
and radio boundaries are unchanged.

### Protected FDN record save/edit and cold retrieval

```sh
.venv/bin/python tools/nsk1_fdn_record_check.py tmp/nsk1-fdn-off-cold \
  tmp/nsk1-fdn-write tmp/nsk1-fdn-read tmp/nsk1-fdn-write-wrong
```

Own Fixed dialling → Numbers → Add accepts physical PIN25678, name A and
number12345. Firmware issues exactly one PIN2-protected UPDATE RECORD to
EF_FDN6f3b, record1, length32, with the expected ADN-format bytes. The
complete card changes only that record; the settled frame lists A. No FDN
record is preloaded into the card input. `nsk1_fdn_write.lua` uses two-second
menu spacing and half-second digit entry, keeping the transaction below the
known late native packet-supervision restart.

A separate cold process uses Numbers → Search, submits an empty name, and
holds physical # for four seconds to display12345. It reads record1,
preserves the whole card and needs no PIN2 verification. An independent
wrong-current5675 Add attempt from the same empty-card seed returns VERIFY
9804 and Code error; only PIN2 retry3517 changes3→2, with no file write.
The gate requalifies the cold Off seed, own native/storage, each cold link,
physical and decoded input, credential/update payloads and ordering, selected
file/record, complete card differences and exact list/number/error frames.
Fixtures are `nsk1_fdn_write.lua`, `nsk1_fdn_read.lua` and
`nsk1_fdn_write_wrong.lua`.

Append `--edit tmp/nsk1-fdn-edit tmp/nsk1-fdn-edit-cold` to qualify in-place
editing. Physical Search → Options → Edit, PIN25678, unchanged name, C/6 and
Save issue one protected update to the same record1. Only byte3600 changes
F5→F6: A/12345 becomes A/12346, with no new record or unrelated mutation.
The separate cold process displays12346 using the same held-# Search route
and leaves the complete card unchanged. `nsk1_fdn_edit.lua` keeps menu
spacing separate from credential/number typing. These are file-operation
checks, not dial-permission, network or fitted-mask acceptance.

### Selected FDN erase and cancellation

```sh
.venv/bin/python tools/nsk1_fdn_erase_check.py tmp/nsk1-fdn-edit-cold \
  tmp/nsk1-fdn-erase tmp/nsk1-fdn-cancel tmp/nsk1-fdn-erased-cold
```

Independent cold copies of A/12346 enter Search → Options → Erase and
authenticate PIN25678. Both render the exact `Erase? A` confirmation.
Confirm issues one UPDATE RECORD to EF_FDN record1 with32 ff bytes; only
that record is cleared. Physical C instead cancels with no update command
(including unacknowledged ones) and preserves the complete card. The final
frames distinguish return to Numbers from return to the existing A list.

A further cold Search on the erased result displays `No phone numbers`,
then returns to Numbers after the message expires. The gate checks both
frames, PIN1-only cold authentication, absence of file mutation and exact
whole-card preservation. Native/storage/source links, decoded physical
actions, PIN2/update payloads and confirmation frames remain required.
Fixtures are `nsk1_fdn_erase.lua`, `nsk1_fdn_cancel.lua` and
`nsk1_fdn_empty.lua`. This proves selected-record erasure, not a command
to erase every FDN/ADN record or any network/call support.

### Local FDN dial permission

```sh
.venv/bin/python tools/nsk1_fdn_dial_check.py tmp/nsk1-fdn-edit-cold \
  tmp/nsk1-fdn-dial-enabled tmp/nsk1-fdn-dial-listed \
  tmp/nsk1-fdn-dial-unlisted tmp/nsk1-fdn-dial-disabled \
  tmp/nsk1-fdn-dial-unlisted-off
```

With the physically saved A/12346 record, a menu-driven FDN enable changes
only ADN validity. Independent cold copies type12346 and54321 followed by
the single-Navi Call action. The listed number reaches call-progress UI;
the unlisted number displays `Call not allowed`. After physical FDN disable,
a further cold process types the same54321 and reaches the identical progress
frame as the listed control. Each dial preserves the complete card; toggles
change only3575. Exact digit previews, physical/decoded contacts, source/cold
links, native/storage checks, SIM identity handshakes and reviewed outcomes
are required. Fixtures are `nsk1_fdn_dial.lua` and
`nsk1_fdn_dial_unlisted.lua` over the same physical-input harness.

This is local permission handling only. There is no laboratory registration,
connected call, native speech or fitted-mask promotion in these results.

### Ordinary PIN change

```sh
.venv/bin/python tools/nsk1_pin_change_roundtrip.py mame/mame \
  tmp/nsk1-pin-roundtrip-qualified tmp/nsk1-pin-change-final
```

Physical Settings → Security settings → Change access codes → Change PIN
code accepts current PIN1234, replacement4321 and matching confirmation.
The card receives CHANGE CHV (`A0 24`), returns9000 and changes only the four
PIN digits. The exact PIN code changed frame guards presentation; CHV2,
PUK credentials, retry counters, enabled flag and every file remain intact.
A separate cold process physically verifies4321, returns9000, reaches the
distinct phone-code prompt and preserves the entire card. No UNBLOCK command
is involved: this verifies ordinary PIN change separately from PUK recovery.
The gate requalifies the existing PIN source and guards native execution,
own flash and phone identity/code fields. Its75-second change phase remains
below the known late DSP-liveness reboot; it does not prove long stability.

## Offline SMS storage

```sh
.venv/bin/python tools/nsk1_sms_draft_roundtrip.py mame/mame roms \
  tmp/nsk1-profile-source tmp/nsk1-sms-roundtrip --erase
```

The own Messages menu directly exposes Inbox/Outbox/Write messages; the sibling
5190's extra Text messages submenu is not inherited. The first isolated process
physically enters the phone code, composes `A `, selects Save and requires one
successful `A0 DC 01 04 B0` update to EF_SMS record 1. Its exact 176-byte record
is `070281f001ff00810000024110` followed by FF padding: status 07 is an unsent
SMS-SUBMIT, not a received/network-delivered message. All other nine SMS records
remain free and all ADN bytes remain erased. Message saved pixels match
`d4562fa5c9fbc8bb0180f9361ffebbecf39ad0bcda7708ab15e45b196b4144eb`.

Cold preparation revalidates raw save evidence before copying only persistent
inputs/nvram/cfg. Physical phone-code acknowledgement and menu navigation open
Outbox, select the stored message and read its `A ` content. The read frame
matches `f2320fef70b570e75b15c83bcefdcbb8beaf7c80c29b184ca7ddac24e9b9de57`.
The whole serialized SIM must remain unchanged, with a real READ RECORD and no
SMS UPDATE. Both phases require ordered physical contacts, decoded key values,
successful own phone-code comparison, native record arithmetic, sampled native
execution and unchanged executable/identity/code fields. Save samples through
59 seconds and cold read through 40 seconds. This proves offline persistence,
not SMS transport, registration, fitted DSP or speech.

With `--erase`, a third process opens the saved draft and physically selects
Options/Erase/confirmation. One successful absolute record-one UPDATE changes
only the status byte from 07 to 00; all TPDU bytes and unrelated SIM files are
preserved. This is logical deletion, not payload scrubbing. Exact confirmation
and No more messages in outbox frames are required. A fourth process copies
only the verified persistent files, physically opens Outbox and displays
No messages in outbox (RGB SHA256
`cdff157a2a10f5c1528e9085b1981401a9714ae37cded35e3eddd43fd8a83fb0`).
It reads EF_SMS without UPDATE and preserves the entire deleted card. Native,
physical-input, phone-code and executable/identity checks apply to all stages.

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
