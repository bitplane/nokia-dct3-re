# MMI context settlement

This document is the current contract for Nokia 3210 v6.00 task-5/MMI context
settlement after provisioned startup. Historical forcing experiments are
recorded in `evidence/falsifications.json`; they are not alternate boot recipes.

## Open question

Which firmware-owned task-5/MMI state or transition selects the unattended
idle window after the accepted security transaction?

The remaining boundary is firmware-internal. Current evidence does not justify
synthesizing a class-1 message, event `0x0547`, callback selection, context
state, timer result, hardware event, or peer response.

## Current result

The coherent frontier profile completes the external service session and ordinary non-CPHS
SIM initialization. Task 1 advances to startup mode `0x0004`, while a
provisioned EEPROM identity removes the phone-lock prompt and permits an
idle-like frame with the `Menu` softkey to be painted.

The frame is not yet a proved desktop, but keypad decoding is live while task 1
remains in mode `0x0004`:

```text
active-low keypad input
  -> MAD2 KBGPIO state
  -> IRQ0
  -> ISR 0x2b3084
  -> task-1 event 0x0041
  -> internal 0x41/0x42/0x43 sequence
  -> matrix scan 0x2b2f90
  -> decoded resource 0x6e02 via 0x2b4628
```

Stable multi-key delivery is now proved. After status `0x057c` presents the
security editor, a single emulation-time coroutine supplies `12345` and the left
softkey as non-overlapping physical taps. The transaction completes through
`0x0578` and callback `0x47` returns `0x05e6`. That result does not post code 7
or leave mode `0x0004`, but it is the accepted-code result: verifier `0x2ae704`
returns one only when the five-character input transforms through `0x2ae4e8`
to the four bytes stored at `0x112460`. The security editor, keypad delivery,
and generated default code are therefore complete; startup remains separate.

The accepted result has also been followed through the generic task-5 callback
dispatcher. Status `0x05e6` has an explicit case at `0x2ac54c`: it advances the
callback cursor and continues the ordinary callback sweep; it is not converted
into a display or peer request. About 0.54 seconds later task 6 receives the
editor's class-2 and class-3 window-lifecycle messages. Neither path selects the
idle window. The idle helper `0x2a255c` is entered only when task-6 control byte
`0x1116fd` is set to one by its class-1 window-selection path, and no such
selection occurs after the accepted code. No resource `0x4c22`, event `0x0547`,
or DSP TX follows. This closes the accepted editor result as a source of the
missing handoff and rules out an LCD/DSP completion at that boundary.

Producer-side tracing closes the task-6 message constructors. Initializer
`0x2b1e80`, called from the main task-5 lifecycle at `0x28c96e`, allocates the
class-1 setup message and posts it to task 6. Helpers `0x2b1f24` and `0x2b1f64`
construct class-2 and class-3 window messages respectively. In the provisioned
coherent run a class-2 lifecycle notification is called from `0x28c94a` with
arguments `0, 2, 0x0f`; this is firmware-local task-5 state, not an LCD or DSP
reply. The initial class-1 message carries zero in its selection byte, and the
later traffic does not leave task-6 control byte `0x1116fd` armed for the idle
helper.

The class-2 notification is selected by status `0x0190`, the first entry in the
`0x0190..0x019d` jump table of task-5 dispatcher `0x28bddc`. Handler `0x28c94a`
copies the current context halfwords at `0x110f22` and `0x110f1e` into the
class-2 message's selector and lifecycle-mask fields. Runtime state at
`0x29817e` proves that the active index is already `0xff`, every object pointer
is null, and selector `0x0f` deliberately leaves the four-entry table alone.
It is not a close operation and no retained window blocks idle.

Callback-table entry `0x10` at `0x292878` has a `0x05e7` case which performs a
full application/UI **reinitialization** sequence when context byte `0x110f1f`
equals one. It is not the ordinary cold-boot entrance. Like callback `0x01`,
callback `0x10` is a fixed callback-plane entry with zero records in the
complete 950-record transition table. A complete publisher census finds one
global `0x05e7(argument 1)` publisher, `0x256f68`, and only two caller
families: controller status `0x03e9`, the conditional navigation route below,
and display statuses `0x0280/81/82`, already classified as local/service-test
transactions. In an unattended coherent run callback `0x10` sees only startup
status `0x05e2`; neither `0x256f68` nor its `0x05e7` case executes.

Backward recovery closes that intermediate chain. Fixed callback-table entry
`0x01` at `0x29ea80` converts global status `0x0367` into `0x03e9` at
`0x29efe6` when that callback is the active callback-plane entry.
Exhaustive execution of controller dispatcher `0x253e20` proves `0x03e9` is the
only input that reaches `0x255ebc`; that leaf calls `0x256f68(3, 4)`, which
publishes global `0x05e7` with argument one. Callback `0x10` then takes the UI
start case described above. The resulting forcing-free predecessor chain is:

```text
global 0x0367
  -> callback 0x01 / 0x29ea80
  -> controller status 0x03e9
  -> 0x256f68 publishes global 0x05e7(argument 1)
  -> callback 0x10 / 0x292878
  -> application and UI initialization
```

The static census finds no literal/direct API publication, registration
descriptor, or fixed-sequence catalogue predecessor for `0x0367`; its nine
effective literal loads are consumers. Runtime has now proved why that absence
was not a producer proof: a physical Up-key cycle reaches firmware function
`0x2a1a80`, reads `0x0367` from the active UI-context record, and queues it
through the generic event machinery. The event is a logical
UI/navigation status produced through an indirect callback path, not a missing
hardware-ready report. The separate organic task-16-to-task-10 message `0x03e9`
is numeric reuse in a task mailbox and does not enter controller dispatcher
`0x253e20` in the coherent run.

Every queued status passes through `0x2aefba`, then the callback, descriptor,
display, and controller stages at `0x2af646..0x2af66e`. A post-frontier Up cycle
queues `0x40c8`, processes the logical input sequence, and then queues `0x0367`.
Descriptor selector `0x75` state zero maps that status to `0x051d`, with
`0x057c` also published; it does not reach controller `0x03e9`.

A bounded callback-plane audit falsifies the former missing-registration
conclusion. Callback `0x01` has a fixed entry at `0x2db728` (handler
`0x29ea81`, flags `0x01000000`) and **zero** records in the complete 950-record
transition table. The active callback byte at `0x11fcce` is initialized to
`0x01` and the engine at `0x2ac3f2` organically sweeps it through the callback
table during startup; setter `0x2ac3e0` has only three ROM callers, all in that
engine's initialization/wrap paths. The coherent trace observes `0x01`, the
sequential sweep through `0x7d`, a wrap to `0x01`, and later callback choices
`0x2f`, `0x7c`, and `0x47`. By the time Up generates `0x0367`, callback `0x01`
is no longer selected. Selector `0x75` is a separate descriptor namespace: it
has eleven transition records, including the state-zero `0x051d` mapping, and
must not be confused with the active callback byte.

There is therefore no missing hardware event or registration boundary between
`0x0367` and callback `0x01` to emulate. The `0x0367 -> 0x03e9 -> 0x05e7` chain
is a conditional key/navigation action available during callback `0x01`'s
lifecycle, and the resulting callback-`0x10` path is conditional UI
reinitialization rather than the ordinary startup trigger.

The physical-key trace does not repeat the matrix decode. A 50 ms Up tap enters
IRQ0 once on press and once on release, runs the firmware's `0x41/42/43` polling
sequence, and reaches `0x2b4628` exactly once. Later repeated `0x0367`
publications originate in persistent UI-context iterator
predicate `0x2a1a80`, not repeated MAD2 scans or a missing release edge. The
earlier `0x2a1b18 -> 0x2a1a80` attribution was an address-boundary error:
`0x2a1b18` is an instruction inside a neighboring routine. Runtime identifies
nested transition index `0x411` (`selector 0xc0`, required result one, terminal
packed value `0xffff`) as the caller-side predicate record. `0x2a1a80`
publishes the active context status and returns zero, so the record is designed
to remain unsatisfied and be polled again.

The task-6 idle selector has now been separated from the setup constructor.
`0x2b1e80` constructs class-1 mode zero. A second, tiny constructor at
`0x2b1e44` constructs class 1 with byte `+6` equal to one and posts it directly
to task 6. Task 6 receives that byte in the class-1 handler at `0x2982b4`; the
one-valued path reaches `0x298346`, arms control byte `0x1116fd`, and on the
next loop calls `display_idle_2a255c`, which requests resource `0x4c22` and
posts `0x0547`. By contrast, standard class-2 constructor `0x2b1f24` always
writes byte `+6` as zero, so the later editor/window lifecycle message cannot
arm this path.

The class-1 selector has three recovered ROM callers. Two are inside the
conditional contact/service command handler rooted at `0x236f6c`. Exhaustive
concrete execution of all 65,536 inputs to task-5 dispatcher `0x28bddc` proves
that the third leaf, `0x28c248`, is selected by exactly one status: `0x0732`.
That status is already owned by the physical-power/code-7 lifecycle, not
unattended startup. In a coherent power-key run firmware organically posts
`0x0732` and enters mode `0x000c`, but task 5 does not consume it during the
bounded run; consequently `0x2b1e44`, resource `0x4c22`, and event `0x0547` do
not execute. A 20-second receive trace and a second run after organically
accepting `12345` show why: task 1 begins the shutdown/service teardown before
task 5 performs another outer receive. The queued `0x0732` is therefore part of
shutdown ordering, not evidence of a missing idle-window producer, task-5
queue primitive, or hardware acknowledgement.

`0x2b4628` feeds the local input handlers at `0x2979d8` and `0x2a27de`, then
mirrors the one-byte key through resource `0x6e02`. The mirror uses the optional
class-availability map installed by class-`0x40` service command `0x70`; local editor
input works without it. The Up result proves the local input/event path can
produce `0x0367`; enabling class `0x6e` is unrelated.

## Ownership

Task 1 owns the startup consumer. Its state object is rooted at `0x1123ee`:

| field | address | current value/role |
| --- | --- | --- |
| pending startup event | `0x1123ee` | current scalar input |
| startup mode | `0x1123f0` | `0x0004` at the frontier |
| readiness flags | `0x112399` | `0x0f` after mode `0x000d` |
| substate | `0x11239c` | diagnostic context |

The master dispatcher is `0x270c8e`, with a mode jump table at `0x270ca8`.
Mode zero contains an interactive initialization burst at `0x270d1c`. The
coherent boot does not enter that exact block, but the mode-4 and mode-7 paths
execute their equivalent shared tail before parking in the selected mode.

Event `0x72` is not owned by a dormant keypad task. The IRQ handler posts it to
mailbox 1, and task 1 receives it. The missing behavior is therefore a startup
lifecycle transition, not IRQ routing or scheduler delivery.

## Report code 7 contract

Report code `0x07` is a conditional power/shutdown continuation, not an
ordinary boot prerequisite. Both branches leaving mode `0x000d` compare the
current mailbox item with code 7. A different report records mode `0x0004` or
`0x0007`; both branches then fall through into equivalent interactive
initialization. This is why task 6, keypad scanning, and the security editor all
operate while task 1 remains in mode 4.

The v6.00 wrapper `0x2af190` mirrors resource `0x6a01` when that optional
channel is enabled and posts code 7 to task 1. Its caller surface is closed:

| owner family | caller | classification |
| --- | --- | --- |
| power/battery | `0x21e40c` | low-voltage shutdown outcome |
| power/charger | `0x21f8de` | charging-completed outcome |
| callback/status | `0x27b3b6` | callback `0x5d` terminal completion |
| controller | `0x255c3c` | status-`0x0795` controller completion |

A physical power-key action exercises the ordinary observed path: decoded key
`0x0d` reaches controller status `0x0795`, posts code 7, moves task 1 to
mode `0x000c`, and runs service/display teardown before the power-control tail
at `0x2b4e16`. Healthy 5110 and 3310 sibling runs reach standby without posting
their equivalent report, while 3210 v5.01 preserves the same wrapper, four
caller families, and task-1 comparisons. Together these results exclude a
universal DSP-ready or boot-readiness interpretation.

Callback `0x5d` is the paired task-1 report-6/7 status dispatcher. Input
`0x0348` posts report 6/resource `0x6a00`; inputs `0x05e1`, `0x05e7`,
and `0x05dc` start timer class `0x52`; terminal `0x05eb` or recoded
completion `0x06c5` posts report 7/resource `0x6a01`. The class-`0x6a`
mirror is optional service transport, not the owning hardware subsystem.

The transition-table bound is 950 records, not the earlier 233-record estimate.
Thirty records select callback `0x5d`; therefore old claims that slot
`0x45`, chooser `0x09d0`, or record 92 uniquely owned its activation are
retired. Exact descriptor edges remain in the message census and evidence
ledgers.

After code 7, task 1 can consume report 6 and reports `0x09`, `0x0a`,
`0x0b`, `0x0c`, `0x0d`, and `0x1c` through the finite flag block at
`0x112390..0x112395`. This later phase is useful firmware topology but is not
the cold-boot entrance currently under investigation.

The sole direct `0x0732` publisher is helper `0x2a26d4`, called from the two
task-1 continuation branches. Its downstream task-6 activity was useful for
mapping the window manager, but diagnostic code-7 injection did not compose and
has been removed. No report, callback result, `0x0732`, or window event should
be synthesized from this evidence.

Detailed negative fixtures remain in `evidence/falsifications.json`. The
following families are closed as ordinary boot owners and should not dominate
other subsystem documents:

- normal battery, BSI, temperature, charger, or held-PWRONX fixtures;
- the service-only battery-characterization command-`0x8e` lifecycle;
- Advice-of-Charge and security/SIM-identity completion;
- display/local-test statuses `0x0280..0x0282`;
- generic-service paths requiring later framework modes;
- DSP heartbeat, guessed self-test replies, and contact-session completion.

The active boot frontier is task-5/MMI context settlement and unattended
idle-window selection, not report code 7.
## Active diagnostics

`NOKI3210_TRACE_HANDOFF=1` retains only the current causal seams:

- task-1 mode transitions and dispatcher state;
- task-1 mailbox publications;
- IRQ0 entry and the scan/decode seam;
- mode-0 interactive-init and task-6 idle-helper entry points.

The trace is read-only. A successful fix must work with it disabled.

The corrected release-to-key trace uses the scheduler-backed Lua input timer.
At `1.48 s` task 1 enters mode `0x0004`; a normal softkey press raises MAD2
IRQ0, enters `0x2b3084`, runs the firmware `0x41/0x42/0x43` sequence, scans at
`0x2b2f90`, and publishes decoded keycode `0x19` at `0x2b4628`. No code-7 post
or mode change occurs. The former IRQ6/event-`0x72` result was caused by
combining keypad and CCONT onto the same emulated source.

## Current MMI settlement boundary

The report-6/7 and physical-power lifecycles are closed above. The remaining
question is which firmware-owned task-5/MMI context selects the unattended
idle window after the accepted security transaction.

The same-product comparison leaves no justified software-side transition to
synthesize. Callback `0x10`/`0x05e7`, callback `0x01`/`0x0367`, and task-6
selector `0x0732 -> 0x28c248 -> 0x2b1e44` are now all retired as ordinary
startup candidates. The post-security callback lifecycle settles cleanly:
callback `0x47` returns accepted result `0x05e6`, the engine handles that result
explicitly at `0x2ac54c`, and callback `0x47` leaves the active sweep. A
negative-control run containing only `12345` and its single submission still
produces the later `0x0384`, `0x05de`/`0x05e0`, `0x0598`, and `0x0578` wave.
It is therefore the firmware-owned post-acceptance tail, not a second-softkey
transaction. Callback-plane entries `0x01` and `0x02` mostly pass those statuses
through; later pipeline stages consume or transform them, so their callback
returns are not missing acknowledgements.

The overlapping `0x00c8` activity is independently owned. Task 1 publishes
packed `0x40c8` at `0x2a2838` with an incrementing argument at approximately
one-second intervals. Its 81-record descriptor walk reads transient context
byte `0x110f1f`; in the coherent run the selected expansions emit `0x1b59`,
direct `0x01f5`, and `0x1b5b`. These are periodic context-maintenance outputs
and do not reach the task-6 class-1 constructor. The earlier attribution of
`0x00c8` to the second softkey was a timing correlation, not a producer edge.

The repeated `0x05a7` was a second address-boundary trap. Dispatcher target
`0x28c43c` belongs exclusively to status `0x05f3`; `0x05a7` instead reaches
`0x28c480` and calls `0x2b3222`, which updates three timer slots and posts task
5 timer work when required. It is normal timer activity, not a UI transaction
retry. The `0x411`/`0x2a1a80` polling, periodic `0x00c8` descriptor walk, and
shutdown-queued `0x0732` are classified behavior. None is the ordinary
cold-boot window entrance. No class-1 message, `0x0547`, callback selection,
context state, or timer result should be injected.
