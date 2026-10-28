# Native ROM4 speech activation maps

Own handset activation, codec-buffer and native runtime observations. [COBBA control](cobba_control_boundary.md) owns the admitted device boundary; [the ROM4 loader](rom4_dsp_loader.md) owns boot scope. Isolated algorithm success, mailbox delivery and call-attempt presentation do not establish a native handset speech stream.

## Handset FR activation boundary

The twelve-second atomic/staged pin-clock comparison in
`check-c54x-rom4-staged-codec` does not activate the FR scheduler. In both
captures, data-space write observations for active mode `1836` and pending
mode `1869` contain three writes each, all zero. PCM ingress state
`167c/167d` likewise has three zero writes per cell; ring cursors `06de/06df`
are initialized to `0400/0560` and have no subsequent observed data-space
writes. Each count is below the observer's 32-record per-address cap.
These are bounded data-space observations, not an exhaustive writer census
(program-overlay stores are a separate path).

The checked scheduler lists at `ee0c` and `ee18` contain encoder wrapper
`9c4f` and decoder wrapper `9cb9`, respectively. The type-2 host parser supplies
pending mode `1869`, and the pending-to-active copy includes `1836`. The MCU
constructor and parsed channel-description/mode fields are already mapped in
[the native mode producer contract](#native-speech-activation-and-pcm-buffers).
The remaining native ingress requires successful RF reception and traffic
assignment, not another guessed configuration packet. The idle/key-tone
laboratory supplies neither that lifecycle nor evidence for inventing it.
Keep clock qualification, mode selection and codec algorithm acceptance
separate: do not set the mode cells or call the wrappers from the handset
backend to bridge this gap.

### Independent RF input references

The [gr-gsm test-data collection](https://github.com/ptrkrysik/test_data)
provides raw signal captures and a separately stored burst stream. Its README
attributes the `vf_call6` signal to SRLabs. The
[grgsm_decode input path](https://github.com/osmocom/gr-gsm/blob/master/apps/grgsm_decode)
reads complex samples, adapts their declared sample rate, then feeds a
four-sample-per-symbol receiver; burst files use a distinct input path.
These are potential independent receive references, not MAD2 port words.

Before using such data as a native acceptance fixture, pin its revision/hash
and licensing, verify decoded broadcast results with the independent receiver,
and establish the transformation to the ROM's port-`27` representation:
sample rate, I/Q ordering, numerical width/sign, filtering and ready cadence.
Compare intermediate native filter results as well as final decoding. Do not
reinterpret a complex-float capture as packed integers or tune permutations
until the phone advances. No capture is installed as a production input, and
no native reception result is claimed from this source inventory.

[TS 45.002 section 5.2.4](https://www.etsi.org/deliver/etsi_TS/145000_145099/145002/13.01.00_60/ts_145002v130100p.pdf)
specifies the ordinary frequency-correction burst's positive `1625/24 kHz`
offset. The diagnostic's sixteen-complex-sample rotation would have that
frequency at `13 MHz / 12` (four samples per GSM symbol), but neither that
clock nor the physical component order is established for port `27`.
This conditional frequency identity is not grounds for selecting a sample
clock, swapping lanes or treating read-indexed stimuli as received bursts.

`check-c54x-rom4-rf-filter-mixed` adds a bounded overlapping-input check to
the isolated impulse contract. It supplies 32 words, signed values
`((n*7919+1237) mod 4096)-2048`, through port `27`, then zeros. The native
filter emits 48 captured words spanning three invocations. An independent
integer convolution accumulates the mapped coefficient contributions and
rounds each complete sum once; every native word matches, including the
cross-invocation tail. Unit tests distinguish this from summing rounded
individual impulse responses: each unit impulse rounds to zero, but the
settled unit sequence yields one because the eight coefficients sum to
65536. Both signs and the unchanged isolated-impulse expectations are checked.
The runner retains actual input/binary hashes and per-case invocation and
generated EEPROM identity before execution, on disk-backed repo `tmp/`.
This closes the numerical filter contract for this bounded mixed sequence,
not arbitrary-input overflow/saturation, physical sample format/cadence,
the historical rotating-score discrepancy or RF acquisition. No detector
threshold, input amplitude or CPU instruction semantics is changed.
Its required fixture ROM directory is `mame/roms`, not the handset `roms`
directory. Native RF/CRC evidence runners now default to ignored repo `tmp/`
on disk and accept `--output-root`; they no longer default to tmpfs.

The full-scale variant `--mixed-full-scale` supplies32 words repeating
`0000,7fff,8000,ffff,7ffe,8001,1234,edcb`; every one of48 output words
matches the same independent convolution with rounding after accumulation.
Explicit `--machine` selection now qualifies these CPU/bus compositions:

| Machine | Frontend / bus | Input | Current capture |
| --- | --- | --- | --- |
| `noki5110` | atomic / legacy | full-scale mixed | `tmp/run_rom4_rf_impulse_ir1zbl_f` |
| `nse1astd` | atomic / standard | full-scale mixed | `tmp/run_rom4_rf_impulse_mwaa4emo` |
| `nse1pipe` | staged / standard | full-scale mixed | `tmp/run_rom4_rf_impulse_rjlndfg2` |
| `nse1pipe` | staged / standard | bounded mixed | `tmp/run_rom4_rf_impulse_phbhtxr1` |

Each capture records the actual binary, ROM/input identities and profile,
and requires a unique matching machine marker and clear illegal-opcode state.
Staged `PC` is ahead of a bus access's owning instruction; using atomic
`330c` as its first-read trigger produces no admitted input. Read-only C54x
`READPC`/`EXECPC` getters instead expose the valid, non-annulled Read/Execute
slot addresses (or`ffffffff` when absent). The fixture admits input owners
`330a/3310/3316/331c` and output owners`3321/3322`, independently of the
advanced PC, while retaining exact destination/value ordering. No pipeline,
register or sample value is changed by these derived getters.

The existing `run_c54x_core.py --profile portr-block` independently checks
these owner getters at real I/O-read and data-write callbacks across1728
cases and1728 save/load replays, two program pages and three snapshot phases;
empty pipeline slots must report no owner. Capture: `tmp/run_c54x_core_g4hbuvkp`.
This is numerical/observer qualification, not all accumulator modes, physical
bus timing, arbitrary filter state or RF acquisition. It does not supersede
the separate failed rotating-score acceptance.

### Port-3 receive-window candidate

The port-3 receive-window interpretation from the reviewed external emulator
is a candidate control boundary, not an admitted universal INT0 gate. Original
resident words independently pin read/OR-`0005`/write at `44a7..44af`,
read/AND-`fffe`/write at `44b8..44c0`, and zero initialization at
`4567..456b`. The contract checker names these three sequences explicitly.
This establishes the firmware's bit operations, not the physical clock or
interrupt effect of bit 0.

The passive RF observer accounts for every port-`27` read against the last
observed port-3 bit 0, without changing cadence or the mathematical stimulus.
Fresh 60-second zero and rotating controls agree: 5,291 port-3 writes, 3,977
bit-0 transitions, 857,440 reads with bit 0 set and 133,920 with it clear.

| DSP mode `00ac` | Reads with bit 0 set | Reads with bit 0 clear |
| --- | ---: | ---: |
| `0000` | 0 | 9,760 |
| `0001` | 157,888 | 32,000 |
| `0002` | 864 | 160 |
| `0004` | 380,928 | 92,000 |
| `0009` | 320 | 0 |
| `000c` | 317,440 | 0 |

Full-control and falling-edge-age accounting separates startup from later
clear-bit reads. Both controls reproduce these ranges (rounded to microseconds):

| DSP mode | Port 3 | Clear-bit reads | Age since the most recent observed bit-0 clear |
| --- | --- | ---: | --- |
| `0000` | `0000/0100` | 9,760 | No preceding falling edge observed |
| `0001` | `0000` | 31,968 | 4,673--4,610,830 us |
| `0001` | `0100` | 32 | 4,615,442--4,615,446 us |
| `0002` | `0104` | 160 | 4,610--23,076 us |
| `0004` | `0104` | 92,000 | 1,240--155,883 us |

The observer preserves full port values, counts aged versus unaged reads, and
checks each control bucket against its mode/bit totals. These are emulated
control-to-read intervals, not measured electrical latency. In the current
model they are not all a single short post-clear tail. The native backend
presently drives INT0 from the CTSI frame timer; the physical relationship
between that frame source and an RF block-ready source remains unresolved.
Do not clear the CPU's already-latched IFR merely because a control bit falls.

The runner retains validated window totals and per-mode accounting separately
from score acceptance; `c54x_rom4_rf_score_check.py --window-only <error.log>`
checks those observations without promoting acquisition. The rotating score
still reaches only `0538`; first-response delivery remains `006d`.
Thus simply suppressing all sample reads/interrupts when bit 0 is clear
would alter multiple observed lifecycles, including startup. Which phases
use the control, which pending requests may drain after a clear, and how
sample availability is clocked remain open. Do not infer a universal gate or
import a firmware-mode-conditioned sample schedule from the external model.

### Earlier RF score and delivery captures (not current acceptance)

The following runtime counts and accepted-store/delivery observations describe
earlier captures whose matched executable/input provenance is incomplete.
They do not supersede the failed current reproducibility check above. Retain
the byte-checked consumer paths and costly negative knowledge, but do not
interpret the nine accepted stores as current acceptance or change arithmetic
to recover them. The strict delivery gate remains the promotion condition.

`c54x_rom4_rf_score.lua` supplies three explicit mathematical controls
at port `0x27`: zero; DC word pairs `(2048,0)`; and rotating word pairs
of amplitude 2048 with one turn per sixteen pairs. Pair order and
normalized frequency are diagnostic assumptions, not GSM sample-rate
or physical ADC claims. It observes the uploaded score at `0x0ad1`,
the second comparison at `0x0add`, and accepted-value store `0x0ae4`.
Fresh generated-profile 60-second runs produce:

| Input | Completed scores | Nonzero | Maximum | Second comparison | Accepted store |
| --- | ---: | ---: | ---: | ---: | ---: |
| Zero | 5158 | 0 | 0 | 0 | 0 |
| Rotating pairs | 5146 | 5146 | `0x7fff` | 500 | 9 |

A separate 60-second DC run using storage from its preceding eight-second
run has 5568 completed scores, all zero, with no second comparison or
accepted store. Its counts are not a fresh-boot oracle. Validate captures
with `tools/c54x_rom4_rf_score_check.py /path/to/error.log`; the checker
requires sustained observation, not the single completion in eight seconds.
The rotating input therefore advances the actual native score/selection
path without firmware-state or message injection. Saturation at `0x7fff`
is an observed score ceiling, not proof of correct physical RF modeling.
No control contains a GSM burst, FCCH/SCH timing or broadcast information;
these results establish neither acquisition nor registration. The following
observations identify the selection's downstream stores and exit. A supplied
RF waveform still requires explicit lane, scaling and cadence evidence.

The fresh rotating-input selection trace reproduces 5146 scores and
nine stores at `0x0ae4`. Those stores increase `0x198d` through
`060c,0d62,17e5,250e,3589,4885,5efa,77c9,7fff`; AR4=`0x1257`
and AR5=`0x1266` throughout. Live uploaded code after that store
maintains candidate-associated records, counters and flags; it does
not immediately call the MDI publisher. The traced exit `0x0ca3`
executes twice: once during setup and once at 38.52345 seconds with
score=`0x7fff`, flags at `0x1266`=`1`, and count at `0x1344`=`500`.
Its live instructions are `CALL 0x44b6; RET`. Follow-on routine
`0x0cef` executes zero times. The captured decision stream tests
`0x1266` bit 1 before branching toward the follow-on path; accepting
ever-better scores does not establish that second flag. Bit 0 is set
in the delayed-branch slot of the second comparison, including cases
that do not reach the accepted-value store; it is not an acceptance
predicate by itself.
Consequently these inputs prove candidate selection, not publication
of acquired radio data. The flag's decoded software producer is below;
its presence does not establish acquisition in this fixture.

The expanded live-code capture identifies an explicit bit-1 setter:
`0x0b78` performs a delayed branch to `0x0b42` with
`OR #2,*AR5` in its delay slot (`0x0b7a`, AR5 selected as `0x1266`).
The preceding continuation at `0x0b6a` clears A, pops AL, subtracts 8,
and branches to that setter if A is nonzero. A popped value of 8
takes the other path. This is a software continuation-value gate,
not evidence that a hardware interrupt must directly set the flag.
Its caller/return contract remains unresolved. The bounded write watch
captures setup clears and repeated bit-0 writes at advanced PC `0x0ae4`;
the first 24 writes do not contain bit 1. This bounded prefix is not
an exhaustive absence proof for all later flag writes. Trace the
continuation entry and its return-value source before attributing it
to RF acquisition, an external peer or a missing DSP instruction.
The fresh 60-second continuation census observes zero entries at
`0x0b6a`, the post-pop `0x0b6c`, and the setter branch `0x0b78`.
The unrelated/shared helper entry `0x2523` executes five times and
branches to `0x25fe`, also observed five times. Its first four captured
(A,B) pairs are `(13,1),(11,1),(12,2),(11,1)`; these are not recovered
arguments of the unexecuted continuation. The live helper bytes match
the raw ROM and are byte-pinned in the contract checker. Therefore the
current missing link is the continuation's entry/selection contract,
not an observed pop returning the wrong value. Search its code/data
references and active dispatch source before modeling an external event.
The live exact-word reference census checks 65536 words in each of the
program and data spaces. Each contains one `0x0b6a` word, at `0x0b59`
in the same uploaded sequence: `CMP #1,*(0x06fd); BC 0x0b6a,TC`.
The duplicate code/data occurrence is not two independent producers.
This recovered branch requires `0x06fd == 1`; the rotating-input run
has `0x06fd == 0`. It also follows the unexecuted `0x0cef` routine,
so mode selection is not proven to be the only missing prerequisite.
No other exact pointer word is present in the final live images, but
computed targets, earlier overwritten tables and other entry addresses
are outside this census. The next backward boundary is the upstream
candidate-validation branch reaching `0x0cef`, not an invented event
to activate the bit-1 setter directly.

The upstream rotating-input path reaches `0x0b00` 500 times and
`0x0b0b` 499 times, but `0x0b0d` and the entire captured history path
(`0x0b1e/24/2e/3a/42/4a`) execute zero times. At `0x0b0b` the live
code tests A after incrementing `0x1256` and subtracting `0x05f3`
(1523), branching back to the scan loop when A is negative. The
uncapped counter observation over all 499 checks ranges from 1 to
499; the separate `0x1344` counter exits at 500 before another check.
Thus the history-comparison and continuation gates are downstream of
an unsatisfied counter predicate in this mathematical-input run.
Do not infer a firmware bug or remove either guard: initialization,
other writers and product/request context of `0x1256` remain open.
Its initialization/write contract is recorded below; this guard does
not justify supplying an MDI reply or continuation return value.
The complete 60-second write census observes five writer PCs for
`0x1256`: loader `0x0f15` (1), reset `0x3074` (2), filtered-buffer
store at advanced PC `0x33e5` (2), uploaded reset `0x0ac7` (3), and
candidate-loop store at advanced PC `0x0b09` (499). Uploaded code
explicitly clears B and stores it to `0x198d`, `0x1256` and `0x1344`
before calling the queue helper. The scan-entry reset supersedes the
earlier filtered-buffer value `0xfc38`; it is not an input offset that
survives into this scan. The captured loop writes begin `1,2,3,4`,
and the uncapped comparison range remains 1..499. No other writer
executes during that candidate interval. This establishes the observed
lifecycle's counter constraint, not an exhaustive static writer census
or proof that all valid request/product contexts share it. Independently
audit the guard's decoding and uploaded request context before treating
the apparent 1523-versus-500 incompatibility as a firmware defect.
The independent encoding audit uses TI
[SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf),
SUB syntax 7 (pp. 4-187/188) and BC conditions (p. 4-18): `0xf010`
selects immediate subtraction from A with zero shift, `0xf843` tests
A less than zero, and `0xf842` tests A greater than or equal to zero.
Original NSE-1 v5.30 MCU flash contains the reset block at byte
`0x0959ec`, the 500-result guard at `0x095a68`, and the 1523 counter
guard at `0x095a74`. Their relative positions agree with the captured
DSP program layout. `c54x_rom4_codec_contract.py --mcu-image
roms/noki5110/5110f530.fls` verifies unique aligned source occurrences
and that common layout; mutation tests cover every checked word.
This rules out these constants being introduced by the live-code
observer or a swapped upload capture. It does not validate all core
pipeline effects or establish the request's intended physical context.
The scan-entry census observes `0x0a5c` and the reset at `0x0ac2`
three times each. At all entries active mode `0x1836`, pending mode
`0x1869`, and `0x06fd` are zero; A at `0x0a5c` is 12. The final
live program and data images each contain two exact `0x0a5c` words:
the restart branch at `0x0b69` and gateway target at `0x2421`.
The gateway is `GOTO 0x0a5c`, also present in the acquired image.
Resident sequence `0x3129..0x3133` clears `0x06fd`, calls `0x3198`,
then calls gateway `0x2420`, and sets `0x06fd=1` only after return.
Both resident sequences are byte-pinned. The runtime call/return census
confirms three entries at `0x3129`, `0x3130` and gateway `0x2420`,
with two returns to `0x3132`. Firmware writes `0x06fd=0` three times
at advanced PC `0x312c`, and `0x06fd=1` twice at advanced PC `0x3135`.
The returns occur at 1.644830615 and 38.523453018 seconds; the third
scan remains in flight at the 60-second stop. Thus the gateway-stall
hypothesis is falsified: the bounded scan can return without entering
the dormant follow-on route. Zero mode inside the scan is the selected
pre-return lifecycle, not an absent mode-setting hardware event.
Exact-word scans do not cover computed entries or overwritten tables.

The fresh 60-second rotating-input post-return capture observes both
returns through `0x3138/3a/3d/3f/42`, with `0x06e3=0` and
`0x1949=0`. Neither the host restart selection nor the operational-ready
bit is set. Both execute the fallback call at `0x314d` and continue at
`0x3151`; `0x3153/5c/6b` remain unexecuted. The fallback prepares a
two-byte type-`0x8a` body from data `0x1867` at buffer `0x1200`.
The observer at native publisher entry `0x37ce` captures type `008a`,
length 2, and bodies `006d` at 1.644835731 seconds and `0001` at
38.523456633 seconds. These are not the contemporaneous `0x1983`
cursor values (`03fe` and `047a`). Do not conflate those fields.
The byte-pinned `0x311a..0x311f` sequence loads AR4 from data
`0x1983`, then copies `*AR4` to `0x1867`. The fallback at
`0x3144..0x3152` copies `0x1867` into its two-byte publisher buffer.
A retained-storage eight-second capture confirms the selected-list
relationship: at scan entry `0x3129`, cursor `03ff` points to `006d`
and `0x1867=006d`. At fallback call `0x314d`, cursor `03fe` points
to sentinel `7fff`, while the selected word remains `006d` and is
published. The cursor changed position; this does not show the selected
entry being overwritten. These words are software search-list entries;
physical ARFCN interpretation and RF tuning must be tied to the request
producer rather than inferred from their plausible numeric values.

The native type-`0x1a` request captured at `0x3d5e` contains 33 words:
control `0010`, then `9800`, with `1000` at zero-based word index 26
and all remaining words zero. The byte-pinned receiver at `0x3d97`
copies the low byte of word 1 and words 2..32 into scratch
`0x1200..0x121f`. With control bit 0 clear, helper `0x48f7/0x4909`
walks those scratch words backwards, testing bits low-to-high. Its first
eight words select entries 1..128; the remaining 24 select 512..895.
The captured bit therefore selects entry 109 (`006d`), matching both
the live selected-list word and the native response body. This is a
static general mapping with one live single-bit corroboration. The isolated
native basis fixture below independently validates its complete bit domain;
it does not extend the live handset's RF acceptance. The later control `0011` request bypasses
bitmap expansion and must not be decoded by this rule.
`c54x_rom4_rf_delivery_check.py --search-list` checks this request-to-list
correlation; the isolated cold-control runner also requires it. Physical
band/channel interpretation still requires the MCU request producer and
RF tuning contract, independently of these numeric list entries.

The isolated original `48f7/4909` decoder now passes all512 single-bit
inputs plus empty, full and alternating controls on both atomic and staged
frontends. Its first eight descending words select list entries1..128;
the remaining24 select512..895, with low-to-high bit order in each word.
Each of515 cases checks the complete ordered output, B count, final AR2/AR3,
saved AR7, untouched input and all unwritten output words/guards. Every case
also restores its prepared input and replays all65,440 ordinary RAM words.
The optional acquired hook2428 remains RET; only a private IDLE return
sentinel is fixture code. Explicit algorithm input and calling registers do
not change a running handset or supply an RF result.

```sh
make check-c54x-rom4-rf-bitmap
make check-c54x-rom4-rf-bitmap-staged
```

This qualifies the original bitmap/list algorithm, not native request
scheduling, ARFCN band semantics, tuning, RF samples, registration or speech.

Publisher entry alone proves call arguments, not delivery. A separate
fresh eight-second capture reaches the publisher's port-1 doorbell
instruction at `0x3807` for type `008a`, length 2, at 1.644837058
seconds, with producer `00ab`, consumer `00a9` and control `0x06e1=0`.
This establishes ring publication and notification for the first response;
the device's port-1 handler invokes the transport RX notification. A fresh
eight-second MCU-side write capture then observes the consumer advance
from `00a9` to `00aa` at 1.645136923 seconds (PC `00271908`), and to
`00ab` at 1.645150077 seconds (PC `00271998`). These are low-halfword
writes at aligned MCU address `000101c8`, mask `0000ffff`, selecting
the consumer halfword at `000101ca`. Thus firmware drains both header
and body of this native response. This proves ring consumption, not
application-level interpretation or successful RF acquisition.
DSP-local data-space write taps cannot
establish absence of MCU-side shared-memory consumer updates.
For NSE-1 v5.30, the first-level switch at `0x291c34` selects table
entry `0x291c70` for type `0x8a`; its corrected pool word targets
`0x291ca0`, which calls handler `0x26be28`. This is not the adjacent
`0x26bf18` handler. The native eight-second trace reaches `0x26be28`
at 1.645216538 seconds and its non-discard branch `0x26be3c` at
1.645217923 seconds, with controller state byte `0x108427=2`.
Static decoding shows state 1 frees the message; other states clear
bit 1 at controller offset `0x7c`, rewrite its status to `0x1390`
(pool `0x26c034`), and post the same object to task 10 via `0x254c84`.
The narrow post-call tap was silent; the wider capture below establishes
the actual post. At function entry R4 still belongs to the
caller; R0 is the message pointer, and the branch observation confirms
R4 becomes that pointer (`0x101438`).
An additional repeated run with retained storage observes the actual
header store at 1.599568385 seconds: trace PC `0x26be48`, aligned
object address `0x101438`, value `13900000`, mask `ffff0000`.
This independently confirms the status rewrite, not task-10 receipt;
its timing is not a fresh-storage baseline. A narrow read tap on the post
primitive did not fire even with its argument filter removed. A wider
`0x254c80..0x254cff` fetch window captures its actual entry: PC
`0x254c84`, R0=`10`, R1=`0x101438`, R14=`0x26be5d`, at
1.599574385 seconds in the retained-storage repeat. Thus the rewritten
native response is submitted to task 10's RTOS post primitive. The entry
generates fetches at `0x254c84/88/8c` with the same PC and arguments;
the observer retains only the first address to avoid triple counting.
This resolves the silent probe as an observation-boundary issue, not a
missing firmware publication. It does not establish receiver processing.

Static consumer candidate: the subtract cascade at `0x211cf2` uses
base `0x1389`; cumulative decrements 1, 2, 2, 1 lead to a range test
covering `0x138f/0x1390`, both targeting `0x211eb0`. That case calls
`0x269266` with the incoming object and length 8, tests the shifted
controller word at offset `0x7c`, and conditionally increments halfword
`0x10fee0` (pool `0x211f58`). The carry-producing shifts must be
decoded before assigning flag semantics. A silent narrow case window is not
an absence proof; ownership, receipt and case execution are established below
with wider fetch windows and correlated objects.
The contiguous 12-byte creation records begin at `0x2a4fa0`; record
10 at `0x2a5018` contains Thumb entry `0x211a55`. Entry `0x211a54`
calls initializer `0x20cb80` before reaching receive call `0x254ed8`
at `0x211a96`. The received object is stored at `[SP+0x18]` and its
signed status halfword is read at `0x211aa0`. Initializer `0x20cb80`
independently passes task number 10 to `0x23f130` and calls `0x211f6c`
with `(2,0,0)` before initializing controller state. This strengthens
task ownership beyond address similarity. A bounded receive-window
probe was uninformative and removed; first establish initializer
completion and the scheduler's actual receive return before diagnosing
an absent task consumer or introducing any readiness response.
The wider `0x211a00..0x211bff` window resolves that observation gap:
fresh storage executes task entry at 1.082075385 seconds, initializer
entry at 1.082077462 seconds and its return to `0x211a66` at
1.082957154 seconds. A retained-storage repeat then observes receive
return PC `0x211a9a` at 1.599812000 seconds, R0=`0x101438`,
status `0x1390`: the same object submitted by `0x26be28`.
`tools/c54x_rom4_rf_delivery_check.py` validates ordered construction,
two-word publication, both consumer advances, the header rewrite,
task-10 post and receipt with object identity and monotonic timestamps.
Its negative tests reject every omitted link, mismatched objects/status,
wrong masks/cursors and reversed time; ring wrap is covered. The checker
passes this capture. It does not prove payload preservation into the task,
candidate acquisition, or the downstream case's flag/counter semantics.

A fresh generated-profile eight-second zero-input observation now reaches
case `0x211eb0` with the same object `101438`, status `1390`, at
1.816215538 seconds, after receive return at 1.816208846 seconds.
Its controller word at offset `7c` is `80000000`. The same case also receives
a separate object `101480`, status `138f`, at 1.816344538 seconds; a case
entry count alone cannot distinguish these statuses. The bounded passive
observer records object/status/control at entry rather than interpreting
the shifted flags. This establishes execution of the mapped downstream
case, not payload retention, counter mutation or acquisition success.
Evidence is `tmp/rom4-task10-consumer-final/error.log`, captured with the
reusable passive observation in `c54x_rom4_rf_score.lua`. Run
`c54x_rom4_rf_delivery_check.py ... --search-list --consumer-case` to require
both request correlation and the same object reaching that case after receipt.
Negative tests reject missing entry, wrong object/status, reversed time and
malformed controller words. No firmware or DSP state is written by this tap.

The case's `LSRS #26; BHS` tests original control bit 25, not the high
bit 31. With observed control `80000000`, bit 25 is clear and execution
reaches the counter store at `211ec4`: upper-halfword value `162f`, mask
`ffff0000`, address `10fee0`, at 1.816221769 seconds. Helper `210f5c`
then receives object `101438`/status `1390` from caller `211ed3` at
1.816223385 seconds. The separate `138f` object increments the counter to
`1630` and calls that helper later. Thus native delivery is not rejected by
this flag gate. `LSRS #27` later tests original bit 26; its runtime outcome
is not established by the silent fall-through fetch taps.
`--consumer-effects` requires ordered receipt, matching case, counter lane
and same-object helper entry; wrong objects, caller, mask, address, timestamps
and bit-25-set controls fail. Evidence is
`tmp/rom4-task10-flags-final/error.log`, a fresh eight-second zero-input run.
The next application boundary is helper `210f5c`'s status dispatch, not a
missing native response or a forced controller flag.

That dispatch's table selects `2114fc` for `1390`. It loads base `1084a4`,
increments halfword `+8` and calls getter `27e3b4`; if count exceeds the
returned limit, `LSRS #5; BHS` tests original bit 4 of byte `+1b` before
the possible call to `20fbb8`. Fresh zero-input capture reaches `2114fc`
with count 1, returns from the getter at `211508` with count 2 and limit 1,
and reaches common return `2115a2` with count 2 and flags `00`.
The same object/status is preserved at these observation boundaries. This
establishes the counted, flag-clear return path, not the flag's semantic name,
its writer closure, or successful acquisition. The neighboring `138f` object
has a different status-table case and must not substitute for this proof.
`--consumer-decision` checks the ordered same-object path, increment, bounded
limit, flag-clear predicate and preserved count/flags at return. Negative
tests cover missing entries, changed count/limit/flag, wrong object and time.
Evidence: `tmp/rom4-task10-decision/error.log`, fresh eight-second native boot;
`tmp/rom4-observer-r13/error.log` independently reproduces the same-object
consumer-decision check with the MCU stack register read through `R13`.
The remaining MCU-side question is the flag/limit provisioning and the
`20fbb8` continuation contract; it is independent of obtaining valid RF
samples. Neither should be forced merely to advance the application.

The own NSE-1 image identifies one positive bit-4 setter. In helper
`210f5c`, status `138e` selects table entry `210f94 -> 211534`, which
calls parser `210eac`. Its message byte `+6 == 0` path returns 1 at
`210f0a`; nonzero takes the separate return-2/3 path. Return 1 selects
`21154e..211556`, ORing `10` into byte `1084bf`, before calling `210e4c`.
This is a bounded writer contract, not an exhaustive flag census or an
observed successful native RF result. One upstream status-`138e` publication
is the type-`80`/channel-`40` receiver branch at `26bd98`, posting the
original object to task 10. These statuses must not be synthesized from a
scan-terminal `8a` response merely to set the flag.

Limit getter `27e3b4` reads halfword `1098c4` under synchronization calls;
its observed return 1 is software configuration, not a physical RF unit.
The possible continuation `20fbb8` sets controller byte `108427` to 1,
calls `211f6c(2,0,0)`, and its common tail cancels timer ids `65`, `5a`
and `1d` and releases pending objects at offsets `3c`, `30` and `20`.
It is not a bare readiness publication. Its other branches and full semantic
role are not closed by this decode.
The argument-2 arm of `211f6c` selects `211f92`: it calls `28f9ec(0)`
and clears bytes `+3` and `+0d` of the structure at `1066a0`.
In `28f9ec`, zero input bypasses the measurement/threshold path and selects
state 0 for byte `10b543`. Only a changed state calls notifier `28d726`;
an already-zero state returns without notification. Thus this part of the
continuation resets local history and conditionally publishes a state change,
not an unconditional acquisition-ready event.
The notifier `28d726` reads that same byte through getter `28facc`, then
requires byte `10b590 == 1`, the low nibble of `10ffac != 2`, and a
different cached value at `10b589`. It updates the cache before calling
`253492` with the new value. That consumer independently requires the low
nibble of `10ffac == 3` and byte `1073d1 != 0`; otherwise it returns.
Its admitted path processes four records through `25324a` and ends with
`2533ca(0,5)`. The record consumer iterates over two coordinates, reads
packed bitmap bits and calls `2531c0` for each point. That routine bounds
the coordinates to 84 columns and 48 rows, addresses a byte by
`84 * (y >> 3) + x`, and updates bit `1 << (y & 7)` in buffers rooted at
`107610` and `107418`. The first buffer supports set, clear and toggle
operations; the second is updated from a separate argument flag.
This establishes a framebuffer-presentation branch, not a native DSP
completion or an acquisition-ready report. No signal-strength units or
successful RF result follow from those drawing operations. The final
`2533ca` operation and other `211f6c` arguments remain open contracts.
`nse1_scan_continuation_contract.py roms/noki5110/5110f530.fls` pins 32
fragments in the hash-identified own image (dispatch, parser decisions,
setter, counter/limit roots, status producer and control tail); every pinned
byte has mutation coverage. Computed aliases, indirect writers and uploaded
replacements remain outside that bounded static contract.

The isolated cold-control runner is:

```sh
.venv/bin/python tools/run_c54x_rom4_rf_delivery.py --mame mame/mame --rompath mame/roms
```

It creates a unique evidence directory under ignored repo `tmp/`, generates matching
NSE-1 EEPROM for each case, and keeps MAME's working directory, logs and
NVRAM separate. Default coverage is two fresh 60-second runs per mode.
The runner rejects Lua callback or autoboot errors from the console as well
as the trace; the standalone delivery checker checks a sibling `console.log`
when available, or an explicit `--console` path. Partial observations emitted
before an observer failure are not acceptance evidence.
Both score and delivery checkers must pass; repeated score totals,
accepted-store counts, first response body and received object address
must agree within each mode. It is a standalone research gate, not a
physical-RF or registered-network acceptance gate.

| Port control | Fresh runs | Scores per run | Accepted stores | First native body | Task receipt |
| --- | ---: | ---: | ---: | --- | --- |
| Zero | 2 | 5158, all zero | 0 | `006d` | status `1390`, object `101438` |
| Rotating pairs | 2 | 5146, all nonzero | 9 | `006d` | status `1390`, object `101438` |

Both controls deliver the same first response despite different score
behavior. Candidate scoring therefore does not by itself establish a
successful acquisition result. The repeat checker has negative coverage
for changes to every compared observable and treats the two modes
independently. This is within-build repeatability, not a cross-host
determinism claim.

The consumer's `LSR #26; BCS` tests original controller bit 25: set
skips the counter increment. A later `LSR #27; BCS` tests bit 26.
The correlated fresh capture described above attributes the first increment
to the same `1390` object received by task 10; the subsequent `138f` object
is separate. The counter's initialized bytes were `162e036b`, so neither
a zero initial count nor a physical unit is established. Earlier uncorrelated
captures do not supersede the same-object consumer-effects acceptance check.

The MCU bitmap-to-entry path and selected-entry tuning arithmetic are now
correlated with the native capture; see the synthesizer sections above.
The mapped operational boundaries are SCU serialization/settling and
RF sample lane, scaling and cadence. Task 10's counted flag-clear return and
one flag setter are mapped above; successful acquisition remains open. The
score checker passed on that earlier capture (5146 nonzero results, 500 second
comparisons, nine accepted stores); it is not a current passing result or
evidence of acquisition.
The 1523-versus-500 constraint remains a property of the pre-return scan,
not proof that the resident lifecycle is blocked there.

## Native speech activation and PCM buffers

The codec contract checker protects seven additional resident-ROM sequences:
encoder/decoder mode gates, encoder PCM input, decoder PCM destination,
mode publication, and the ingress/egress ring-copy pointers. These are bounded
byte-checked paths, not an exhaustive closure of indirect writers/callers.

- Encoder wrapper `0x9c4f` and decoder wrapper `0x9cb9` require data
  `0x1836 == 0x00c1`. The encoder also branches on `0x6bb & 3`.
- The encoder's normal path passes `0x1536` to conditional input-mute
  helper `0xa0b8`, then preprocesses 160 PCM words and invokes the validated
  encoder. After its `0x24c1` hook, the helper tests bit 0 of data `0x1f2b`:
  clear returns without changing the PCM buffer; set repeats the zero store
  160 times through AR2. The acquired resident hook is a return, but future
  uploaded replacement behavior is outside this bounded contract. Both
  sequences are byte-pinned and word-mutation tested. The isolated
  `check-c54x-rom4-fr-encoder` gate also executes the acquired helper with
  flags `0000/0001/fffe/ffff` at two buffer bases: all 1,280 sample results,
  16 adjacent guards, return-stack balance and AR2 advancement pass.
  Its runner requires this result separately from the bit-exact encoder
  comparison and rejects absent or duplicate completion markers. This is
  isolated algorithm validation, not an organic handset fixture or proof
  that speech mode activates. Boot-time flag provenance is mapped below;
  its organic call-time value remains unclassified. Silence alone must not
  be attributed to a broken encoder or used to force the flag clear.
  A raw operand scan of the checked program image finds 35 `1f2b` words:
  31 bit-test contexts and four write contexts. The whole-word copies at
  `0xa53d` and `0x27ac` load from shared `0x085e` via AR4. The paired OR
  `0x1ca0` at `0xb885` and AND `0xe35f` at `0xb891` preserve bit 0.
  All four write contexts are byte-pinned. This bounds literal
  references in this image, not computed-address writers or future uploads.
  The next mute-specific predecessor is the MCU/DSP owner of shared
  `0x085e`, not a local encoder arithmetic correction.
  The organic 11-second physical-key gate now correlates MCU write
  `PC=272034`, bus word `100bc`, upper-halfword mask `ffff0000`, value
  `df290000`, with DSP shared read `085e=df29` and flag write
  `1f2b=df29`, both at reported PC `27ae`. The byte-pinned MVDK
  `7194,1f2b` begins at `27ac`; both bus accesses follow consumption of
  its destination extension. The observed bus PC is not the instruction
  entry. Thus the duplicate copy does
  execute; native mode remains separate from static label ownership.
  The same ordered read/copy occurs after the physical keypress. Bit 0
  stays set in this boot/tone lifecycle, while speech mode is inactive;
  this is not evidence of erroneous call-time muting. The gate rejects
  wrong MCU lanes, missing copy order and absent post-key observations.
  MCU field writer `271d70` receives selector/value in R0/R1, retains
  them in R4/R6 and dispatches through the 47-entry table at `271da4`.
  Selector `28` uses table word `271e44 -> 271e9a`, loads destination
  `100bc` from pool `272214`, and forwards R6 through the common halfword
  store at `272034`. The fresh capture reports selector `0028`, input
  and output `df29`, destination `000100bc` and merge mask zero. Thus
  this publication forwards the supplied word rather than constructing
  its mute bit. Twenty-four original-MCU byte spans protect entry, dispatch,
  selected table entry, branches, stores, pools and storage addressing;
  every byte has a negative
  mutation test. Other selectors are not semantically decoded by that
  fixture. Entry-time capture identifies original caller `275445`
  (Thumb return from `275440`), parameter zero, index 9 and source
  base `10b2b8`. The caller loads `[R5+2]`, hence value `df29` from
  `10b2ba`, compares it with the retained prior record and selects field
  `28` from byte table `2a5df8`. Live LR `271d83` at the final store is
  an internal lock-call return, not the original caller. The gate requires
  the entry observation before the shared publication and rejects that
  misattribution. A passive cold-boot RAM watch identifies the preceding
  record write: PC context `274cba` accompanies `df29` in the lower bus
  halfword (`mask=0000ffff`) at `10b2b8`, i.e. field `10b2ba`, at
  0.158787846 s before shared publication. Instruction `274cb8` loads
  `[R4+16]` and `274cba` stores it into `[R5+12]`. Instruction decoding,
  rather than a tap PC alone, establishes the writer; attributing this
  publication to the preceding `LDRH` is rejected by the checker.
  The same record refresh occurs after the physical keypress. Adjacent
  upper-halfword writes and later low-byte updates are distinct fields
  or partial updates, not full-word replacements. The next predecessor
  is the upstream structure field `[R4+16]`, rather than a missing
  DSP-originated response. Pool `27500c` sets R4 to `10a6b0`, identifying
  that field as `10a6c6`. Its observed initialization is a four-byte serial
  read through `28c496`, selector `0210`, destination `10a6c4`, caller
  `2894fd`. The byte reader `28e10e` accesses GPIO `20020`; stores at
  `28c4c0`/`28c4d0` assemble bytes `00 e0 df 29` at 0.157115692 through
  0.157406923 s. Thus the low halfword reaches the record refresh before
  publication. Routine `28c308` sends the high address bits through the
  EEPROM slave address and the low byte through the word address, so
  `0210` is a physical storage offset. Both the seed EEPROM and the fresh
  isolated NVRAM contain `00 e0 df 29` there; `make_5110_eeprom_profile.py`
  preserves this region rather than deriving it. This is provenance from
  the current seed, **not independent factory-default evidence** and not
  a donor-free speech provisioning contract. The configuration field's
  semantics remain unresolved; a matching ROM literal alone is not
  provenance. This observation does not establish
  call-time speech activation or the semantics of every flag bit.
  Descriptor-reader entry `2894c8` receives destination `10a6c4`, logical
  identifier `0301`, R2/R3 zero and caller `283689`, before the physical
  read. Caller `28367c` loads destination and identifier from pools
  `28385c`/`283878`. Adjacent conditional ROM copies at
  `28369c..2836de` target other structures at `10a6f8`/`10a72c`, not this
  four-byte field. These tables do not establish a fallback for record
  `0301` or justify replacing the seed's mute bit. Lookup `289138` uses
  group 3 in catalogue `2a6438` (pool `28950c`); group entry `2a6450`
  selects table `2a6108` with count three. Its direct-index record
  `2a6110` is `00000301 0210 0004`: identifier, physical offset and byte
  length. It supplies storage metadata, not the default payload. The
  structured decoder checks this observed path with malformed pointer,
  count, identifier and truncated-image negatives; it is not a complete
  descriptor catalogue or an absence proof for every fallback writer.
  MCU lifecycle branches `27503c..275042` and `275054..27505a` respectively
  set and clear bit 3 in record byte `[R5+13]` (the low byte of field
  `[R5+12]`). Both preserve bit 0; they cannot by themselves release the
  encoder's decoded bit-0 mute. Their full upstream branch meaning remains
  unresolved. The gate requires the selector-`0210` read before record
  initialization and rejects a changed selector or inverted event order.
- The decoder supplies destination `0x1496` before postprocessing at `0x94c9`
  and `0xab7c`; further gain/filter/mute stages precede output copying.
- `0xaa39` copies from the ring cursor in `0x6de` into the selected input
  buffer. `0xaa4f` copies 160 output words into the cursor in `0x6df`.
  Both use BK=`0x278` and save their advanced cursor. These are RAM buffers,
  not direct DSP I/O `0x21` samples.
- The checked publication at `0x579e` selects handler `0xa436` and writes
  `0x1836=0xc1` when `0x1869=0xc3` on that branch. The producer of that
  operating-mode state still needs tracing through normal MCU/DSP traffic.

Passive write taps in `c54x_rom4_codec_observe.lua` cover the mode cells and
ring cursors. A fresh four-second native NSE-1 boot initializes `0x6de=0x400`,
`0x6df=0x560` and `0x6bb=0x24`, while `0x1836`, `0x1869` and selected
input pointers remain zero. This is evidence that boot alone does not activate
the speech wrappers, not proof that the runtime activation path is absent.

The four-second fresh-profile RF observation contains thirteen ordered pairs:
two descriptor bursts of `2a04/0006`, `0041/0040`, `27a2/0030`, `0041/0020`,
`0488/0010`, `0000/0050`, followed by `2813/0030`.
The queue-owned `0041/0040` pair comes from request words
copied by resident `0xa1ba` from `0x216d` into queue `0x1920`; runtime taps
observe `0x1922=0x41` and `0x1923=0x40` at reported PC `0xa1bc`.
The slot handler reads them through cursor `0x197d` at `0x370c/0x3710`, then
advances the cursor. Both producer and consumer sequences are byte-checked.

Native TX-completion FIQ1 releases this firmware request and advances the TX
pointers to `0041/0041`; omitting it leaves them at `0038/0038`. MAX equality,
BK=0 handling and passive taps do not own this request. The short-run checker
requires the complete descriptor bursts and their queue producer, rejecting
the obsolete three- and five-pair partial fingerprints. This does not
assign physical tuning semantics to `0041/0040`.

The 30-second acceptance profile captures a scan in progress: a fresh run
receives another queue request at approximately 6.43 seconds, repeats `2a04/0006` twice,
then writes `2727/0030`, `2728/0030` and subsequent values. Its terminal
summary is checked against the exact longer-run counts in
`c54x_rom4_rf_boundary_check.py` with EF_SMSS correctly placed in DF_TELECOM.
The short-run check rejects this run by design. The explicit `--scan-30s`
profile requires those counts, all thirty-two logged initial writes, the uploaded shorter-reload
write and the later search-list selector. A fresh isolated rerun reproduces
it. Both profiles retain the producer, loop-completion and
post-scan consumption checks below; neither profile claims acquisition
or whole-boot completion.

Passive timer-port taps identify the changed cadence: uploaded code writes
I/O `0x0e=0x0546` at reported PC `0x0959`, time 6.454126 seconds. The current
controller interprets this as a 1,351-quarter-symbol period instead of 5,000.
The same write occurs at 1.583580 seconds but is then restored to `0x1387`
by resident PC `0x30fc`; the later cycle retains the shorter period.
Mode-AC writes at resident `0x442a` repeatedly select `0x0c`, and `0x32b3`
clears bit 3 to produce `0x04`. This explains the observed counter increase
within the current model, not the physical validity of the shortened period.
Decode the uploaded helper around `0x0948..0x0959` and its caller before
changing timer semantics or accepting this as normal RF operation.

A passive program snapshot confirms that the shorter reload is literal
uploaded code, not a corrupted computed operand: `0x0942` selects AR2 `0x0060`,
`0x0944` stores `0x1387`, and `0x0946` outputs it to port `0x0e`; `0x0952`
again selects AR2 `0x0060`, `0x0954` stores `0x0546`, and `0x0957` outputs
it to the same port. Calls to resident `0x407c` separate the stages, with
`0xa15f` between them. The helper then changes queue flags at `0x195c`,
updates I/O ports `0x02` and `0x04`, and branches on data `0x0284` at
`0x0975..0x0978`. Its complete exit/restore paths and the meaning of that
selector were initially unclassified. The literal write alone does not validate
the controller's reload semantics or the helper's intended duration.

The passive selector trace distinguishes the invocations: the first request
writes `0x0284=0x7c` at reported PC `0x3d8f`, then overwrites it with `1` at
`0x3dae`; the later request writes `0x7c` without that overwrite. Resident
type-`0x1a` processing sets this default at `0x3d8d` and, when its TC branch
is clear, calls `0x48f7` and replaces it at `0x3dac`. Uploaded `0x0975`
compares `0x0284` with `1`: equality branches to `0x0a1a`, whereas inequality
enters the repeated measurement body at `0x098a..0x09cc`. In the later run,
`0x0934` stores `0x7b` to `0x0285` and `0x097a` initializes `0x0e13=1`.
The repeated mode `0x0c/0x04` activity therefore corresponds to a different
search-list lifecycle, not merely a duplicate of the first single-entry
invocation. This does not yet prove the loop termination or RF acquisition.

One-second passive snapshots through 30 seconds show the helper's channel
cursor AR7 advancing through the `0x0e14`-based list and restarting, while
data `0x0e13` alternates between zero and one. Samples at idle PC `0x408d`
also expose outer-counter AR6 values `7`, `4`, and `3`; AR6 samples inside
`0x09ac` are not that counter because the nested routine has repurposed it.
The trace establishes ongoing loop progress, not a frozen wait, but a
terminal return was absent from that 30-second window. A 60-second passive
boundary run resolves it: AR6 decreases from `6` to `0` at `0x0981` in
3.712548-second steps, exits to `0x09ce` at 36.154511 seconds, and reaches
`0x0a18` at 36.156061 seconds. Resident `0x30fc` restores port `0x0e=0x1387`
at 36.156065 seconds. Thus this helper completes its eight-pass list scan
and restores the original frame period without a timer or firmware shim.
The ensuing sampled execution remains around `0x479e..0x47a1` through 60 seconds,
with mode-AC `1`, 1,991 RF writes, 30,380 frame expiries and 971,232 RF reads.
This sampling is not evidence of a stalled queue; scan completion is not RF
acquisition, native speech activation or whole-boot completion.
Resident `0x479c..0x47a2` compares data `0x06b7` with the pointer at `0x06b8`
and loops while equal; after inequality it loads AR4 from `0x06b8` and
copies ring entries with BK `0x48`. This is a queue-empty consumer boundary,
not an unfinished channel-list loop.
The annotated resident image identifies two producer tails: `0x3334..0x3345`
and `0x3359..0x335e` advance `0x06b7` after storing filtered RF results.
Both sit downstream of the `0x32f4` receive/FIR branch, whose input reads
port `0x27`. The `0x3357` producer and `0x4798` empty-wait sequences are
checked against acquired ROM words. Passive post-scan execution taps show
mode-AA reaching zero, mode-AC remaining one, and the ISR taking `0x3230`
to `0x32f4` and publishing at `0x3345`. Across consecutive frames, both
queue cursors advance together from `0x1a00` through `0x1a3c` in four-word
steps at the restored 4.615-ms cadence. The consumer therefore drains
organically produced samples; the observed polling PC is normal inter-frame
waiting, not a missing producer. Zero-valued modeled RF input does not
establish acquisition or justify synthetic queue completions.

The pending speech-mode field has an indirect host-owned writer: resident
`0x3a09` decodes type `0x02` payload words, and `0x3a21..0x3a2b` advances
AR5 from `0x1868` to store the high byte of the fifth consumed word at
`0x1869`. The annotated data dispatch table maps index `0xb00f+2` to
`0x3a09`; the acquired cold-data image independently contains big-endian
word `0x3a09` at byte offset `0x16022` (data address `0xb011`).
`0x570f..0x5714` copies eight pending words `0x1863..0x186a` to active
`0x1830..0x1837`, including this field at `0x1836`. Both instruction
sequences are byte-checked against the acquired program image. This is
a legitimate configuration path, not proof that sending an isolated
type-`0x02` packet can establish an ordinary traffic-channel lifecycle.
Pending modes are not copied indiscriminately: `0x55a7..0x55c0` selects
transition tails using the high nibble and low two bits of `0x1869`, with
the active `0x1836` family selecting additional setup. Multiple tails
converge on `0x56ef`, which performs the checked pending-to-active copy.
Resolve the MCU producer, uploaded dispatch interception and copy trigger
before any handset experiment; do not set `0x1869` or `0x1836` directly.
A fresh eight-second verbose native NSE-1 run observes no type-`0x02`
TX publication or consumption. Its type-`0x1a` payloads begin `001098`
at 1.552444 seconds and `001198` at 6.425143 seconds, both committed by
MCU PC `0x0027183e`. This bounded boot trace does not close the type-`0x02`
producer census; it establishes that ordinary initial scanning does not
activate the mapped speech mode. Traffic assignment and its channel
configuration must precede a handset speech claim.

A bounded Thumb-pattern scan of NSE-1 v5.30 finds two independently decoded
type-`0x02` construction sites, at `0x20d162` and `0x20e7be`. Both store
wire length `0x14` at object byte 2, type `2` at byte 3 and initial operation
`4` at byte 4. The first also stores object halfword `2`; the second has
the same halfword store. This matches the independently recovered NSE-3
constructor shape without borrowing its addresses or payload meanings.
The scan examines 524,272 halfword offsets and returns six length/type-store
candidates; four use dynamic type values and remain unclassified. This is
not an exhaustive type-`0x02` producer census. Recover containing functions,
all payload stores and submission callers before assigning native mode
semantics or building a traffic fixture.

The first containing function is `0x20d14c..0x20d1d8`: it allocates and
zeroes a `0x18`-byte object, writes its pointer through argument R0, and
stores argument R1 at object byte `0x0b`. If R1's high nibble is `0x10`
or `0x50`, it sets byte `0x0c` to `0x60` or `0x50`, respectively; otherwise
that byte retains the initialized zero. A halfword-wide direct-BL encoding
scan finds four candidate callers (`0x20d312`, `0x20dbb0`, `0x210bba`,
`0x210e5e`), not NSE-3's five-call topology. The first two visibly supply
R1 `0x10` and `0x1a`; the third supplies a table-derived byte. Indirect
calls remain outside this closure. The final caller at `0x210e5e` supplies
R1 `0x50`, then submits to task 3 at `0x210e96`. The table-derived caller
loads from `0x2aa404`, indexed by input byte `[R4+0x15]` plus stack byte
`[SP+0x1e]`; the first nine bytes are `10..18`. The complete reachable
index range is not proved, so adjacent ROM bytes are not table entries.
The second inlined constructor submits its object to task 3 through
`0x254c84` at `0x20e858`, while its channel selector at byte `0x0c` comes
from controller offset `0xe4`. These are distinct producer paths, not
interchangeable evidence of speech activation.

The `0x20dbb0` caller submits to task 3 at `0x20dc0c`; the table-derived
`0x210bba` caller submits at `0x210c5e`. Neither inspected constructor-to-
submission interval changes object byte `0x0c`. This bounds those paths,
not all aliases or callers of the surrounding functions.

One previously dynamic candidate has an explicit type-`0x02` setup:
`0x20ed02..0x20ed04` stores type `2` in stack slot
`SP+0x4c`, which the construction sequence at `0x20ef98..0x20efa4` copies
to the object halfword and byte 3. Its operation is held in FP (initialized
to `4` at `0x20ecfe..0x20ed00`); its selector byte `0x0c` is copied from
stack byte `SP+0x3a` at `0x20efbe..0x20efc6`. Recovering that stack byte's
writers and the intervening branch paths is the next concrete MCU-side
question. A second dynamic candidate at `0x20f3aa` copies type from `[SP]`
and selector from an indexed array at `[SP+0x28]`; its type and reachable
selector values remain unresolved. Neither path yet proves that `0xc3`
reaches the native pending speech-mode word.

The stack selector has two mapped writers: `0x20ef26..0x20ef32` stores
`0x80` when incoming request byte `[R5+6]` is zero; otherwise
`0x20ef36..0x20ef44` calls `0x20e198` with request bytes `[R5+7]` and
`[R5+0x48]`, then stores its result at `SP+0x3a`. The leaf routine's
explicit `0xc3` return at `0x20e1c0` selects first argument `0x21`,
independent of the second argument. Other explicit first-argument cases
are `0/1/3/11/15/19`, returning the `b0/b1`, `c0/c1`, `d9`, `d4/d5`,
`db`, and `d2/d3` selector families. The unmatched cascade returns
`R0-0x21`, so byte input `0xe4` also produces `0xc3` by residual arithmetic;
that is not a second evidenced speech mode.

`tools/nse1_channel_selector_check.py` executes the identified unswapped
MCU image's leaf routine with Unicorn and checks all 65,536 byte-input
pairs, including a bounded clean return. This is an isolated algorithm
fixture, not a handset-state intervention. The live writers and valid
domains of request bytes `6/7/0x48` remain necessary before constructing
any native traffic assignment fixture.

The native side of that correspondence is byte-checked: `0x387b` consumes
the ring header with `*AR2+%`; `0x387f` extracts its low-byte type while
`0x3881` extracts its high-byte length. Thus the type-2 handler starts at
the first payload word, not at the header. Its reads at `0x3a0c/15/1b/1d/26`
consume payload words 1 through 5; `0x3a27` writes word 5's high byte to
`0x1869`. The existing transport's `tx_payload_byte` independently uses
header cursor + 1 and high-byte-first packing.

The MCU copy closes the field alignment: `0x2869a6..0x2869aa` loads length
from object byte 2 and calls `0x27185a` with object + 3. That wrapper forms
the length/type header and advances the source to object + 4. Its sole
direct ring-writer call at `0x271886` reaches `0x2717de`, whose loop packs
two source bytes high-byte first and publishes the producer at `0x27183e`.
The isolated checker executes this wrapper through producer commit for
all 82 start cursors, verifies all eleven words of a 20-byte payload,
and checks wrap and producer advancement. It stops before scheduler calls;
this is not a live handset transaction. Together with the native parser,
this establishes object byte `0x0c` -> payload byte 8 -> pending word
`0x1869`. Valid live controller arguments and traffic assignment remain
unproved; packet alignment is no longer the open question.

The request object is identifiable independently of selector semantics:
constructor entry `0x20ebec` saves argument R0 in R5 at `0x20ebfa`.
Its single candidate direct-BL caller, `0x20f144`, forwards the incoming
object unchanged from dispatcher `0x20f094` (R0 saved in R5 at `0x20f096`).
That dispatcher compares the signed object halfword against literal
`0x03ed` and selects the constructor-containing handler `0x20f102` on
equality. The isolated checker verifies that route and the neighboring
`0x03ef/0x03f1/0x03f2` routes before any handler side effects. A direct-BL
scan finds one candidate caller of the dispatcher, `0x211ca0`, passing
`[SP+0x18]`; indirect calls remain unclosed. The next producer boundary is
therefore the incoming `0x03ed` object's fields `6/7/0x48`, not a guessed
global controller write. No protocol name is assigned to `0x03ed` yet.

The incoming object's constructor is `0x26c394`. Its table base is
`0x03e9` (literal `0x26c6d8`); entry 4 at `0x26c3ec` selects
`0x26c5b0` for request `0x03ed`. That branch copies `0x58` bytes from
channel record `0x109650` into object + `0x48`, explaining the selector's
second argument. At `0x26c5f8..0x26c612`, channel-record byte 0 values
`0x20/0x21/0x2f` set object byte 6 to 1 and copy mode-record
`[0x109736+3]` into object byte 7; other channel values clear byte 6 and
leave byte 7 untouched. The checker executes this fragment for every
channel byte and four representative mode bytes (1,024 cases), including
the disabled field's untouched behavior. The common tail at `0x26c6ca`
posts the object to task 10 through `0x254c84` at `0x26c6ce`.

A literal-load scan of 524,287 halfword positions finds five candidates
loading exact value `0x03ed`: two consumers (`0x20e904/0x20f09c`) and
three calls into this constructor (`0x24ff44/0x24ff88/0x250600`). This is
not a closure over computed IDs or indirect calls. The live writers of
channel record `0x109650` and mode byte `0x109739`, and the lifecycle
conditions around those constructor calls, are the remaining producer
questions. No acquired native boot trace has shown this request completing
traffic assignment.

One mode-record writer is mapped: `0x24fa28..0x24fa32` copies `[SB+1]`
to `0x109739` and `[SB+2]` to `0x109740`. The entry is shared by a
direct pointer load at `0x24f81e` and a channel-base + `0xe9` calculation
at `0x24fa24`; the following call at `0x24fa38` constructs request
`0x03f0`, not `0x03ed`. Isolated execution verifies the byte-copy
fragment for all 256 mode values. This establishes an unmodified source
byte, not its valid protocol domain or runtime lifecycle. Channel-record
writers at `0x24f7f4` and `0x24f9fa` copy `[R8+0x1a]` into `0x109650`;
the staging records' upstream population remains open. A bounded literal
reference scan finds 56 candidates for exact pointers `0x109650`,
`0x109736`, or `0x109739`; aliases, computed addresses and indirect
writes remain outside that coverage.

Dispatcher entry `0x24ea70` binds R8 to fixed record `0x10961a` and SB
to fixed record `0x10963c` at `0x24ea84/0x24ea8e`. Isolated execution
checks both bindings before any initialization calls. Consequently the
mode writer reads staged byte `0x10963d`, and the channel writers read
staged byte `0x109634`; neither source is a transient incoming pointer.
The mode-copy test now uses that actual staging record. The next unresolved
ownership boundary is population of these staging records, not recovery
of the dispatcher's register aliases. The receive/parser helper
`0x26c8cc` calls `0x23d980` from `0x26c9c0` for one accepted incoming
packet path; whether that parser populates these exact fields remains
unproved.

An additional direct mode producer is now executed: element dispatcher
`0x23ca44` routes element ID `0x63` to `0x23d050`. With at least one
source byte, it searches the ROM table at `0x2aab70`, accepting its first
eight entries `00/01/21/41/0f/03/0b/13`; index 8 is rejected. When the
presence byte at `0x109738` is zero, `0x23d08c` writes the accepted value
to `0x109739` and `0x23cc1e` marks presence. The isolated checker runs
all 256 source bytes through the full dispatcher in three configurations
(768 cases): absent field with one source byte, absent field with zero
length, and already-present field with one source byte. Accepted writes
occur only in the first configuration; rejected values, short input and
an already-present field preserve the destination. This is a firmware-derived element
contract, not yet a named GSM field or a demonstrated live packet path.
It supplies an independently accepted `0x21` mode value without using
the unmatched `0xe4` arithmetic residue. Tracing the enclosing message
parser and its physical ingress is still required for native speech.

The enclosing optional-element parser `0x23d334` is independently executed
with message selector `0x2e` and the two-byte element `63 <mode>`. Its call
at `0x23d448` reaches the same element dispatcher. All 256 byte values,
with both absent and already-present mode fields (512 cases), reproduce
the acceptance and preservation contract above. This closes the optional
element-to-record link, not the complete message parser, its live receive
route, or native speech activation. No packet is injected into a handset
run by this isolated test.

Channel-description element `0x64` independently reaches `0x23cf3a`
and writes the request constructor's channel record at `0x109650`.
For a non-hopping channel number 1, all 256 first-byte values are executed:
type bits 1 map to channel `0x2f`, 2/3 to `0x20/0x21`, 4..7 to
`0x40..0x43`, and 8..15 to `0x80..0x87`; type 0 and 16..31 preserve
the record. The low three bits become the timeslot. The mapping uses the
ROM tables at `0x2aab90/0x2aab94`. Thus parsed types 1/2/3 supply exactly
the three channel values that enable request `0x03ed`.

A further 65,536 cases cover every second/third-byte combination for type
1, timeslot 0. In the non-hopping branch, only channel numbers 1..124
write the record; 0 and 125..1023 preserve it. Bit 4 of the second byte
selects hopping, which writes the six-bit sequence number from the third
byte and the six-bit index from the second byte's low nibble plus the
third byte's high two bits, while clearing the channel-number field.
Both branches retain the second byte's high three bits as the training
sequence. This is exhaustive for those two input bytes at one accepted
type, not proof of a live assignment or physical RF behavior.

The receive-loop dispatch at `0x26caec` routes message IDs `0x05ea`
and `0x05eb` to `0x26c98c`. Isolated execution checks both IDs and
every protocol-byte value (512 cases): the payload pointer is read from
message offset 8, and payload byte 4's low nibble must equal 6 to reach
the `0x23d980` parser call at `0x26c9c0`. Its arguments are the unchanged
payload pointer and message byte 3. Other protocol values take the
non-RR branch. This verifies the internal receive boundary; the producer
of these messages, complete message parsing, and native physical ingress
remain unproved. Subtract-cascade arithmetic is checked by execution,
not inferred from neighboring message numbers.

Constructor `0x237df8` allocates a 12-byte message and writes its requested
ID. Its `0x05eb` branch at `0x237e98` marks message byte 4 as 1 and
copies receive-record byte `0x157` to message byte 3. Receive-record byte
`0x164` selects payload allocation: values `40/50/60/a0` request 26
bytes; other values request receive-record byte `0x12b` plus 4 bytes.
The isolated checker executes 1,536 metadata/allocation cases (six kinds,
all 256 length bytes), stopping before allocation. With an isolated buffer
supplied as the allocator return, it then executes the constructor and
ROM copy routine through `0x237f08`: message offset 8 retains the buffer
pointer, the payload length halfword retains the original length byte,
and receive-record offset `0x114` is copied to payload offset 4. The fixed
branch copies 22 bytes even when the length field differs; the other
branch copies exactly the original length. Payload bytes 2/3 and a trailing
guard remain untouched, and source length byte `0x12b` is cleared after
copying. This tests copying, not allocator behavior or mailbox delivery;
the receive-record's lower transport producer remains open.

The constructor's publication tail reports its ID through `0x268e48`
at `0x237f6a`, then prepares task `0x10` and the unchanged message
pointer for `0x254c84` at `0x237f72`. Those argument-preparation fragments
are executed in every constructor case; neither external primitive is
substituted or counted as executed. A whole-image aligned Thumb BL scan
finds 14 direct-call candidates for `0x237df8`: `238312/23843e/238580/`
`23858c/2385a4/238f86/238fc2/23903e/239220/23934e/23962a/2396b0/`
`239a22/239c50`. This is a direct-call candidate inventory, not exhaustive
indirect-call coverage. At `0x23843e`, the literal ID is `0x05eb`, reached
after receive-record processing at `0x237fcc`. The record's transport
ownership and live publication remain the next boundary to establish.

Receive processor `0x237fcc` supplies the constructor's record in its
fixed-block branch for input kinds `40/50/60/a0`. Full isolated routine
execution covers all 256 header bytes for each kind (1,024 cases): input
byte 13 becomes record byte `0x157`, kind byte 11 becomes `0x164`, and
header byte 14 shifted right two becomes length byte `0x12b`. The routine
clears 23 bytes at record `0x114` before validation. Only header low bits
`01` with declared length 1..22 copy 22 bytes from input offset 15; rejected
headers leave the cleared region, although the length metadata is still
stored. This verifies one record-population branch without claiming the
segmented branch, incoming-kind meanings, or native RF delivery.

A distinct unsegmented branch of the same processor is executed for kind
0, address/control bytes `03/03`, and all 256 length-header bytes. Header
low bits `01` and length 1..20 clear the 23-byte record region, copy exactly
the declared length from input offset 17, and store that length at record
`0x12b`. All other length headers preserve the payload region and length
field (the earlier metadata-byte write is separate). This verifies 256
cases of the non-fixed block path, not its sequence-number/reassembly
branches or the semantics of incoming kind 0.

The sequence-aware append branch is also executed with kind 0,
address/control `03/00`, and length-header low bits `01`: 11,264 cases
cover every prior accumulated-length byte, incoming lengths 0..21, and
matching/mismatching expected sequence values 0/1. Lengths 1..20 append
from input offset 17 to record offset `0x18 + prior_length` only when
the expected sequence is 0 and the new total is strictly below 252.
Successful appends update record length byte `0x113`; all other cases
preserve that byte and the 251-byte accumulation region. This exercises
one sequence value and the append bound, not acknowledgement, completion,
all sequence numbers, or the caller-owned record lifecycle.

Receive loop `0x238280` binds its two records to `0x108094` and
`0x107f2c`; its dispatcher routes incoming message `0x040b` to block
processing at `0x238400`. Both facts are executed in isolation. Selector
`0x237f78` is checked in 4,096 cases (eight input kinds, flag 0/1, all
address bytes). Kinds `40/50/60/a0`, or kind `80` with nonzero byte 12,
select return 0 directly. Other inputs use address bits `0x1c` from input
byte 14 (byte 16 for kind `70`): zero returns 0, `0x0c` returns 3,
and other values return `0xff`. The caller uses 0 for the first record,
3 for the second, and rejects other results. Incoming `0x040b` ownership
is now the concrete lower-producer query; these checks do not establish
its native DSP origin.

The aligned Thumb literal-load scan for `0x040b` finds four candidates:
consumer `0x2382ac` and constructors `0x26b8b8/0x26bb1c/0x26bc6e`.
The first two constructor fragments are executed in 768 cases: they
rewrite the original message ID to `0x040b`, copy original byte 4 to
byte 11, and set byte 12 to zero (`26b8b8`) or original byte 6
(`26bb1c`, tested with flag 0/1). All other 40-byte message contents
are preserved. Their publication argument fragments target task `0x0d`
with the same message pointer. The intervening `0x26923c` trace call and
the post primitive are not executed by this check. The third constructor,
computed/indirect producers, and these handlers' incoming DSP dispatch
remain open; literal-scan coverage is not an absence proof.

The containing handler for constructors `26bb1c/26bc6e` starts at
`0x26ba64`; its direct BL candidate is `0x291cda`. Dispatcher
`0x291c34` reads message byte 3 and, with dispatcher-state byte 1 zero,
routes type `0x80` to that call with the unchanged message pointer.
Isolated execution checks state zero and state one: zero reaches the
handler call; one instead branches to `0x291cea`. Handler `0x26ba64`
also calls `0x26b774` at `0x26bb6e`, whose branch contains constructor
`26b8b8`. This connects the three observed `0x040b` constructors to
the type-`0x80` handler statically; it does not yet prove which handler
branch runs, ring-to-dispatch delivery, or a native DSP type-`0x80` packet.

The dispatcher's receive loop calls `0x27191e` at `0x291d36` when
draining the shared ring. Its payload-copy fragment is executed in 1,300
cases: all 100 receive-ring cursors, with 0..12 available payload words
and a requested 12 words. Unlike the 82-word transmit ring, the receive
ring spans shared bytes `0x10100..0x101c7`; consumer word cursor
`0x101ca` ranges `0x80..0xe3`. The fragment clamps to availability,
copies big-endian words to object offset 4, wraps at the 100-word boundary,
and commits the resulting consumer cursor. Header decoding/allocation,
the receive loop's scheduler path, and native DSP publication have not
been executed as one chain by this check.

Receive-header decoder `0x2718a8` is executed for all 65,536 header
words at cursor zero and header `0x0180` at all 100 cursors (65,636
cases). It requests high-byte length plus 4 from `0x255aa0`; after an
isolated buffer is supplied as that return, it writes object bytes
`18 02 <length> <type>`, preserves the following guard bytes, advances
the consumer by one ring word with wrap, and returns `(length + 1) / 2`
rounded down as the payload word count. This verifies that a ring header
with low byte `0x80` constructs the type byte consumed by `0x291c34`.
Allocator semantics and native publication of such a ring entry remain
outside this isolated execution proof.

The acquired native DSP ROM contains type-`0x80` publication sites,
independently pinned by `c54x_rom4_codec_contract.py`: `0x4f1c..0x4f23`
sets AR5 length 14, AR4 type `0x80`, and AR3 payload `0x125b` for
publisher `0x37ce`; `0x50d5..0x50de` sets length 33, the same payload
address, and type `0x80` in the delayed call's instruction slot. The
common publisher sets BK=100, combines length/type into a ring header,
writes circularly, and conditionally commits data `0x08e4` before writing
1 to port 1 (`0x37ff..0x3809`). These are native instruction contracts,
not observed successful reception: the branches' decoder prerequisites,
payload construction at `0x125b`, capacity/commit gates and physical
input still need tracing. Native type-`0x80` publication is therefore
present in the image, but not yet demonstrated in a handset run.

The publisher's containing receive-decode engine starts at `0x4ef7`:
it stores incoming A/B in data `0x121f/0x121e`, and operation selector
`0x40` enters the short-block branch. That branch calls `0x5441` at
`0x4f18` and skips publication when B is nonzero. Byte-pinned entry
chains supply A from nonzero data `0x194f` (`0x9b40`, B=0), `0x1950`
(`0x9be2`, B=1), or `0x11fd` (`0x9dfd`, B=0 with an additional
active-mode/deferred-state guard). A fourth direct call at `0x9bd4`
also consumes `0x194f`, with an operation-`0xa0` timing guard.
These cells are operation selectors at these calls, not identified GSM
channel modes. Their producers, decoder input vectors, and successful
result predicates remain open; the existence of publication code does
not supply the physical input needed to execute it successfully.

Three known operation producers are byte-pinned to common routine `0x4c6f`:
`0x9a99` stores its result directly to `0x1950`, while `0x9acc`
stores its result in `0x194f` through AR2 at `0x9ad5` after a delayed
mask operation; `0x9b20` has the direct `0x194f` store described below.
This indirect store is why a direct-address-store scan
cannot close selector ownership. Within `0x4c6f`, `0x4c8e` waits for
data `0x00b1` to reach the selected sample-buffer start plus `0x7c`,
and `0x4ca3` waits for the final threshold (`0x1b1a` for operation
`0x40`, otherwise `0x1c24`); both loops use IDLE while waiting.
The first buffer choices are `0x1a00/0x1b72`. These are recovered
sample-progress prerequisites, not yet a named DMA register contract.
The cell's producer and actual sample contents must be established
before an operation result can be treated as a native radio decode.

Data `0x00b1` has firmware-owned pointer producers: the filtered-sample
frame branch reads it into AR3 at `0x33b6`, consumes port `0x27` at
`0x33c9/33cf/33d5/33db`, filters into successive AR3 words, and stores
the advanced AR3 back at `0x33e9`. Its loop runs eight iterations with
two destination writes each. Configuration routine `0x4414` conditionally
seeds the same pointer at `0x4424` when input A bit 1 is set, and writes
AR2 to `0x00b0`. These instruction sequences are byte-pinned; this is
not evidence for an autonomous DMA register. The native receive-decode
chain therefore bottoms out at the already-open port-`0x27` sample
boundary plus its scheduling/configuration, rather than requiring an
invented message peer. Authentic or independently generated RF sample
vectors and validation of their format remain required to demonstrate
successful native type-`0x80` reception.

An eight-second NSE-1 native cold boot with a generated EEPROM and the
passive `c54x_rom4_mode_observe.lua` observer exercises pointer seeding
twice and filtered-frame pointer commits eleven times. The observed
pointer starts at `0x121e` and advances in `0x10`-word increments; its
final value is `0x125e`. The positive-control frame site `0x33f3` executes
2617 times. In the same window, operation entry `0x4c6f`, decoder entry
`0x4ef7`, and both type-`0x80` publication sites execute zero times;
`0x194f`, `0x1950` and `0x11fd` finish zero. Thus sample-pointer movement
already occurs without receive-operation activation. This bounded run
does not prove those operations absent later or with valid RF input.
COBBA's RF receive callback is currently unbound and supplies zero, so
pointer movement is not evidence of valid received samples. The next
discriminating boundary is the operation-start configuration alongside
the RF sample contract, not a replacement pointer producer.

A further byte-pinned primary-operation writer starts at `0x9b15`:
after `0x2476`, it returns when data `0x194c` is zero. Otherwise it sets
AR3 to zero and AR4 to `0xffff`, calls `0x4c6f` at `0x9b20` with
A masked by `0x195b & 0x0100` in the delay slot, clears that flag bit,
and stores the result directly into `0x194f` at `0x9b27`. This additional
caller is not an exhaustive ownership census. Its control-word
provenance and runtime scheduling remain unresolved.

A 60-second native run using the generated EEPROM profile extends this
observation through the scan window: entries `0x9a7a`, `0x9aa8` and
`0x9b15`, their three mapped `0x4c6f` callsites, and decoder entry
`0x4ef7` all have zero observed executions. Frame-loop positive control
`0x33f3` executes 30351 times; filtered pointer commits remain eleven.
Control words `0x194c`, `0x1951` and `0x195b` are only observed being
cleared and finish zero, as do the three decode selectors. Consequently
these particular local early-return tests are not the first executed
blocker in this run: their containing entrypoints are never reached.
Investigate the scheduling/configuration that selects those routines
before adjusting a local gate or supplying a synthetic completion.
This is bounded runtime coverage, not proof of exhaustive caller absence.

The scheduling census must include indirect data-memory call lists.
Byte-pinned dispatcher `0x98b7` reads index `0x07fc`, looks up a
program-space branch target in the five entries at `0x98c1`, and jumps
indirectly. Its branch wrappers select AR5 values `0xee38`, `0xee3e`,
`0xee45`, `0xee4e` and `0xee57`. Byte-pinned interpreter `0x9903`
loads a function address from data memory through `*AR5+`, preserves
AR5 around an indirect call, and repeats until the next data word is
`0x00ff`. Those list contents are not program words at the same numeric
addresses: ROM-image indexing alone cannot recover them. Capture the
DSP data-space lists or establish their initialization before claiming
that the receive-operation callers have no scheduling producer.

Passive data-space capture after an eight-second native boot recovers
all five sentinel-terminated lists (sentinel omitted below):

| Data address | Function addresses |
| --- | --- |
| `ee38` | `990d,99d5,9aa8,9bbb,9d20` |
| `ee3e` | `990d,99d5,9aa8,9bbb,9d20,9d3d` |
| `ee45` | `990d,99d5,9a56,9a71,9aa8,9bbb,9d20,9d3d` |
| `ee4e` | `9aa8,9b4a,9b59,9bbb,9be0,9d20,9d3d,9d49` |
| `ee57` | `99aa,99d5,99b3,9a56,9a71,9a7a,9aa8,9b4a,9a34,9b59,9bbb,9be0,9d20,9d3d` |

Every list includes primary-operation wrapper `0x9aa8`; the last also
includes secondary wrapper `0x9a7a`. Thus a scheduling producer exists
in data, although dispatcher `0x98b7` and interpreter `0x9903` both
execute zero times in that bounded run. The next predecessor question
is selection of this scheduler, not construction of missing call lists.
The same five lists are independently checked against the complete
acquired data image with `c54x_rom4_codec_contract.py --data-image
mame/roms/noki5110/nse1_rom4_dsp_data.bin`; mutation tests cover all
46 table words, including their sentinels, and reject incomplete images.

Byte-pinned scheduler selections write callback cell `0x07fb`:
`0x55bb` assigns `0x98b7` unless active-mode high nibble
`0x1836 & 0xf0` equals `0xc0`; `0x5783` also assigns `0x98b7`.
The latter path can subsequently override it: channel `0x1835 == 0x2f`
selects `0xa27d`, `(0x1835 & 0xfe) == 0x20` selects `0xa2cf`, and
pending mode `0x1869 == 0xc3` selects `0xa436` while setting active
mode `0x1836` to `0xc1`. These are verified local store/branch fragments,
not an exhaustive callback-writer census or proof their enclosing
configuration routine runs during the observed scan. They connect
scheduler choice to the pending/active channel configuration already
mapped on the MCU side; do not set `0x07fb` directly to activate it.

An eight-second native observation counts zero executions of stores
`0x55bb`, `0x5783`, `0x578f`, `0x579b` and `0x57a3`. Channel
`0x1835`, active mode `0x1836`, pending mode `0x1869` and callback
`0x07fb` all finish zero; the added mode-word write observations contain
only initialization clears. Therefore this boot does not demonstrate
channel configuration arriving and being rejected by the scheduler's
local mode tests. Investigate the configuration entry/request lifecycle
upstream, retaining RF acquisition as an independent prerequisite;
do not infer that a forced type-`0x02` request would establish reception.

The byte-pinned configuration entry `0x5514` calls `0x23d0` and then
tests `0x1949 & 0x0008`, branching to `0x58a7` when clear. An
eight-second passive run observes zero executions of entry `0x5514`,
the test `0x5516`, and downstream sites `0x5536/0x5554`. Cell
`0x1949` receives only initialization clears and finishes zero; the
candidate bit-set site `0x6943` is also unexecuted. The zero flag does
not explain a local rejection in this run because the test itself is
not reached. Caller/scheduling provenance of `0x5514` remains the
next configuration-side boundary, distinct from valid RF acquisition.

Byte-pinned initialization chain `0x545d` calls `0x481a`, `0x5a34`,
`0x5514`, `0x5929` and `0x5475`. Its direct caller at `0x5e6a`
lies in routine `0x5e62`, which later loads callback `0x07fb` and
calls it indirectly at `0x5eef` (also byte-pinned). A separate
eight-second passive boot observes zero executions at `0x5e62`,
`0x5e6a`, `0x545d`, `0x5463` and `0x5eed`, with frame-loop positive
control `0x33f3` executing 3724 times. The absent configuration is
therefore already upstream of this enclosing routine in that run.
These counts are from a reused generated-profile NVRAM directory,
not a deterministic fresh-storage comparison with earlier runs.

The verified direct caller is main-loop site `0x318a`, returning to
`0x316b` after `0x5e62`. Byte-pinned gate `0x313f` selects this
lifecycle when `0x1949 & 0x0080` is set. Initialization at `0x315c`
then assigns `0x1949=0x0008`, `0x17a8=4`, `0x1868=0x30`,
`0x1869=0x40` and schedule index `0x07fc=0`. The loop calls host
processing `0x390a`; `0x06e3` bit 0 returns to `0x3000`, and bit 1
selects a separate branch rather than the configuration call. This
shows that initial configuration need not wait for a type-`0x02`
channel request: the transition through bit `0x0080` has its own
default initialization. The next predecessor is that flag's producer
and its main-loop consumption, not a forced pending-channel value.

A byte-pinned direct flag producer ends the routine that calls RF
sample-queue processor `0x4701` at `0x4695`. Its result tail at
`0x46e1` returns for signed `0x1956 < 0` or signed `0x198c <= 0`,
otherwise publishes a shifted value from `0x198f` to `0x1933` and,
for channel high nibble `0x20`, copies a double word to `0x177e`.
It then returns when `0x1f24 >= 0x01f4`; only the remaining path
ORs `0x0080` into `0x1949` at `0x46fd`. These are firmware numeric
result predicates, not yet calibrated RF units or an exhaustive
flag-writer census. They tie this lifecycle entrance to sample-processing
results rather than an invented DSP completion message. The port-`0x27`
input remains zero by default, and successful acquisition remains unproven.

A fresh-storage 60-second observation counts zero executions of
routine `0x4677`, queue-processing callsite `0x4695`, and result-tail
sites `0x46e1/0x46e6/0x46f8/0x46fd`; frame-loop positive control
`0x33f3` executes 30351 times. Thus this run cannot attribute the
missing lifecycle flag to any of the three result predicates: none
is evaluated. The observer retains bounded result snapshots for a
future run that reaches them. Static annotations identify candidate
callers at `0x9975` and `0x9995`; their selection and actual instruction
contracts require verification before interpreting the flag as an
acquisition failure. Fresh storage here means a newly created private
NVRAM directory containing only the generated NSE-1 EEPROM profile.

Both candidate callers are now byte-pinned. Entry `0x996b` calls
`0x2472`, tests B bit `0x8000` using A, and returns when clear;
otherwise it passes `0x195b & 0x0800` in A to `0x4677` and clears
flag bits `0x0c00` in the delay slot. Entry `0x9988` instead masks
B with `0x4000`; its branch at `0x9991` is **BEQ**, not AEQ,
so loading and masking `0x195b` into A does not change that gate.
When B is nonzero, it passes `0x195b & 0x0800` in A and sets
bit `0x0800` in the delayed call's slot. These fragments establish
the argument/branch contract only; the provenance of incoming B
and the runtime contents of hook `0x2472` remain separate questions.

For the mapped wrappers that first call `0x990d`, its byte-pinned
return tail `0x995a` supplies B from control word `0x194a`. It tests
bit 0 through A and either returns immediately or updates `0x195b`,
clears selectors `0x194f/0x1951`, and returns without changing B.
Thus their acquisition bit tests consume control-word flags, subject
to any modification by hook `0x2472`, rather than directly consuming
a raw RF sample. This source contract does not apply automatically
to every possible indirect caller of `0x996b/0x9988`; call ordering
and hook contents must be checked at each candidate boundary.

The data-image contract also covers the six lists selected by the
annotated `0xa27d` dispatcher: `0xedf4`, `0xee00`, `0xee0c`,
`0xee18`, `0xee26` and `0xee31`. The first five contain primary
wrapper `0x9b15`; the last contains secondary wrapper `0x9a7a`.
This supplies data-driven scheduling provenance for the additional
primary caller, without claiming that the alternative callback is
selected at runtime. All eleven known lists are checked word by word;
this is not an exhaustive census of every DSP scheduling table.

Fresh-storage 60-second flag observation finds only initialization
clears of `0x1932`, `0x194a` and `0x194b`, all finishing zero.
Entries `0x990d`, `0x996b`, `0x9988`, flag-builder candidates
`0x6318/0x659b`, and alternative callback `0xa27d` execute zero
times, against 30351 positive-control frame executions. This does
not support a local acquisition-bit rejection or a failed transfer
between the neighbouring flag words: their mapped lifecycle is
unentered. The open boundary remains selection of that lifecycle
from the active scan/request path, not a fabricated control flag.

Queue processing does not require the `0x4677` wrapper. A bounded
scan of the live program image finds direct-call candidates to
`0x4701` at uploaded addresses `0x0acf` and `0x0c16`, as well as
resident `0x4695`. An eight-second reused-profile run observes one
execution of `0x4701` and zero of `0x4677`; it does not reach
`0x4798` within that window. This reconciles the earlier post-scan
queue-consumer observations with inactive acquisition wrappers.
The opcode scan reports candidates, not an exhaustive disassembly
or proof either uploaded callsite executed. The observer suppresses
fetch counters while reading the live image at shutdown so its own
census cannot inflate execution counts. Trace the uploaded callers
before treating queue processing as acquisition-lifecycle entry.

A fresh-storage 60-second run identifies the active uploaded route:
`0x0acf` and resident `0x4701` each execute 5159 times, while
queue-empty entry `0x4798` executes 5157 times. Uploaded `0x0c16`
and wrapper `0x4677` remain uncalled; frame positive control executes
30351 times. The live-code window at `0x0ac7` contains the call
followed by `B = A - data[0x219b]` and a delayed BLEQ branch to
`0x0af3`. Its next comparison is `B = A - data[0x198d]`, with
another delayed BLEQ to `0x0af3`. Its delay slot ORs bit 0
through AR5 (set to `0x1266`); only fallthrough stores A at `0x198d`.
The flag alone is therefore not proof the second comparison passed. These
instruction bytes are captured by the passive observer, not imported
from the resident ROM image. The comparisons are an executed-path
candidate for result acceptance, not yet quantified predicate outcomes
or physical signal-strength units. Follow this uploaded result path
before extending the dormant resident acquisition-wrapper chain.

Fresh-storage 60-second operand observation quantifies the first
rejection: all 5158 completed calls reach `0x0ad1` with A zero;
the four bounded operand snapshots show threshold `0x219b=0x0574`
and prior result `0x198d=0`. Neither second comparison `0x0add`
nor accepted-value store `0x0ae4` executes. There are 5159 callsite
executions, leaving one queue-processing call unfinished at shutdown.
Thus a real executed comparison rejects the current zero-input RF
result before the accepted-result update. This does not calibrate
the threshold's physical units or prove how a valid waveform should
be represented at port `0x27`; those remain the software sample-source
contract needed before an acquisition fixture can be interpreted.

Encoder execution also requires conditional accumulator store `SACCD`.
[TI SPRU172C pages 4-152/153](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
specifies that even a false condition reads and writes back the destination,
with address modification independent of the condition. The core implements
that contract; 576 cases cover both sources, twelve conditions, three signed
test values, four address forms and two ASM settings.
Conditional delayed returns with accumulator predicates are covered by 36
additional cases: all twelve predicates against negative, zero and positive
values, preserving a two-word delay instruction and popping the stack only
when taken (SPRU172C page 4-133).

The practical software-only route is therefore:

1. preserve the recovered historical volumes and extracted inputs outside Git;
2. reproduce the native 5110 co-sim before treating DSP addresses as local
   evidence;
3. capture serial `0x2c/0x2d`, parallel MFI and PCM-port activity across boot,
   call setup, volume changes and release; and
4. translate only corroborated transactions into independent BSD-licensed
   tests against the local COBBA and MAD2 PCM devices.

The official TI
[`TMS320C54x CPU Reference Guide`](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
documents the processor and on-chip ROM protection model. It is a backend
implementation reference, not a Nokia ROM source.

If the private ROM cannot be obtained, the
[NSE-1 service manual](https://www.eserviceinfo.com/downloadsm/26879/Nokia_5110.html)
lists factory `COBBAWRX`, `COBBARDX`, `COBBACLK`, `COBBARSTX`, `VCOBBA` and
`DSPXF` test points. That is the lowest-risk known physical capture target;
the corresponding NSE-8 data/strobe pad identities have not been established.
