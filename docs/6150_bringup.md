# Nokia 6150 NSM-1 bring-up

## Current boundary

The acquired v5.23 PPM C package normalizes to an exact 2 MiB CPU-order
image. `make verify-6150-static` regenerates it from a hash-pinned ZIP
wrapper without executing the installer, validates every Wintesla record,
and checks its reset and sparse DSP verification contract. Normal handset
support and graphical boot are not promoted. An explicitly separate
`nsm1r4t` executable compatibility instrument now exists.

The next prerequisite is establishing matching resident DSP behavior and
runtime validity of the explicitly derived product-local EEPROM fixture,
plus fitted flash attributes and address aliases.
The first concrete runtime compatibility question is task 2's rejection
of a type-`0x74`/`35 32` decoded storage record and explicit reason-4 restart.
The measured response agrees with the recovered ROM4 codec arithmetic;
the historical repair template's record semantics and consistency with
the modeled chip remain unproved. Resolve that storage/validation boundary
before supplying downstream SIM or battery-readiness inputs.
A 6110 profile is not a safe substitute: this image uses a larger flash
verification stream and stack addresses outside NSE-3's 64 KiB SRAM.

## Executable compatibility boundary

`nsm1r4t` uses the own normalized flash and checksum-consistent EEPROM,
128 KiB SRAM and the acquired NSE-1 ROM4 native DSP program/data. That mask
is an unproved compatibility input, not an identified NSM-1 fitted mask.
The MCU reset exit remains declared boot HLE. The base 16-Mbit flash part,
ROM4 keypad GPIO layout and other conservative peripheral defaults are
research assumptions; fitted flash identity, aliases and physical input
are not validated. DSP/service/radio HLE publications are disabled.
SIMI and the removable laboratory SIM are enabled from the own reset/RX
register contract below; their presence is not SIM initialization acceptance.

Own CCONT read routine `0x2bf082` selects control `0x28=0x22`, writes its
command through `0x2a`, polls status `0x29` bit 2, and receives at `0x2d`.
The static checker pins this span and register grammar. Selecting this
GENSIO leg alone is insufficient: the later default keypad also owns
`0x28/0x2a` and intercepts the transaction. The explicit compatibility
instrument selects the non-overlapping ROM4 GPIO layout; this is not yet
an independently decoded NSM-1 keypad contract.

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

### Current unresolved predicate

Own supervisor `0x2b6180` repeatedly calls readiness predicate `0x29ed4c`.
It requires nonzero bytes at `0x111ea0` and `0x10e6d0`, plus selector helper
`0x27bde4`: selector byte `0x10be8c==0` requires `0x10e6ce==1`; a nonzero
selector requires `0x10e6ce!=0`. Failure at `0x2b61be` skips the subsequent
readiness chain and enters the supervisor latch loop. The static checker
pins all relevant branch instructions and state literals. These are
software states, not yet identified physical input semantics.

Fresh native observation at eight seconds gives first/selector/second/third
`01/ff/00/01`: **the second input alone is false**. The remaining question
is which legitimate firmware lifecycle writes `0x10e6ce`, and what supplies
its prerequisite. The literal scan identifies references to selector
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
the full product meaning of either input `0x21` or report `0x14`.

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

Next: recover the NSM-1 record semantics and task-2 validation contract,
including the retained-marker transform and the compatibility required
between the own repair template and modeled COBBA. Keep the SIM/readiness backtrace
as a measured downstream dependency, not proof that synthesizing its
missing report would repair this earlier boundary. Keep validating NSM-1
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
