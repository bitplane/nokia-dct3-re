# Interactive startup handoff

This document is the current contract for the Nokia 3210 v6.00 transition from
provisioned startup to decoded keypad input. Historical forcing experiments are
recorded in `evidence/falsifications.json`; they
are not alternate boot recipes.

## Current result

The coherent frontier profile completes contact service and ordinary non-CPHS
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
class-availability map installed by contact-service command `0x70`; local editor
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

## Report code 7

Report code `0x07` selects a later startup continuation; it is not a prerequisite
for initializing task 6, scanning the keypad, or completing the security editor.
Both branches leaving mode `0x000d` compare their current message with code 7.
When it differs, they record mode `0x0004` or `0x0007`; both outcomes then fall
through into the same branch-local interactive initialization tail.

Supplying code 7 diagnostically proves the later continuation: task 1 posts
`0x0732`, starts task 6, and task 6 posts display/window event `0x0547`. The
experiment does not compose. It leaves an earlier task-5 lifecycle active, and
`0x0547` remains queued while repeated `0x0d16` timer traffic wins the receive
path. No downstream result should be injected from that experiment.

The ordinary-entrance census closes an apparent alternative to code 7. There
are four effective `0x0732` literal loads in the ROM. Sites `0x2762cc` and
`0x2763ee` populate event fields in allocated structures, and `0x2921c4`
populates a registration/event set. The sole direct publisher is helper
`0x2a26d4`, which acquires resource `0x4c0b` and posts `0x0732`; its only two
callers are `0x270f7a` and `0x271292`, the two task-1 branches immediately after
their explicit code-7 receive. Task 6's idle-window helper `0x2a255c` is called
only from its own receive loop at `0x298000` after that task has been started.
No independent ordinary `0x0732` or task-6 window-manager entrance is present
in the statically enumerable surface.

The v6.00 report wrapper is `0x2af190`. It publishes resource `0x6a01` and posts
code 7 to task 1. It has exactly four callers:

| owner family | caller | classification |
| --- | --- | --- |
| power/battery | `0x21e40c` | low-voltage shutdown outcome |
| power/charger | `0x21f8de` | charging-completed outcome |
| callback/status | `0x27b3b6` | callback `0x5d` completion |
| controller | `0x255c3c` | status-`0x0795` controller completion |

This report namespace is separate from the application-readiness selectors at
`0x2520ec`. In particular, task 18's selector `0x12` marks checklist byte
`0x112288`; it is not a neighboring task-1 report and says nothing directly
about report code 7.

Within the task-1 report family, code 7 has a genuine paired status.
Wrapper `0x2af17a` publishes resource `0x6a00` and posts code 6. Callback
`0x5d` emits that pair's code 6 on input `0x0348`, starts timer class `0x52` on
`0x05e1` or `0x05e7`, and emits code 7/resource `0x6a01` only on terminal
`0x05eb` or recoded timer completion `0x06c5`. This constrains code 7 to the
terminal side of one startup resource lifecycle; it is not a generic per-task
readiness ordinal. Code 6 is not necessarily delivered first: task 1 uses code
7 to enter the following startup phase, then accepts code 6 together with codes
`0x09`-`0x0d`, `0x0b`, and `0x1b`-`0x1c` while filling the six flags at
`0x112390..0x112395`.

### Task-1 report phase

The task-1 consumer is now fully decoded across mode `0x0004`, the alternate
mode-`0x0007` entry, and the phase selected by code 7. Both entry routes perform
an explicit mailbox comparison with code 7 before their shared tail. Code 7
avoids recording the parked mode; a different message records mode 4 or 7 and
then reaches the same initialization and following report phase.

That phase has a finite event-to-state map:

| Report | State effect |
| --- | --- |
| `0x09` | set `0x112390 = 1` |
| `0x0a` | set `0x112394 = 1` |
| `0x0b` | set `0x112393 = 1` |
| `0x0c` | set `0x112392 = 1` |
| `0x0d` | set `0x112391 = 1` |
| `0x1c` | set `0x112395 = 1` |
| `0x06` | set exit selector `0x112398 = 1` and leave the phase |
| `0x1b` | set exit selector `0x112398 = 2` |
| `0x04` | re-evaluate the six flags and exit selector |

Once all six one-byte flags are set, selector 1 exits through the common
mode-`0x000c` tail; selector 2 uses byte `0x11ff50`; selector 0 continues into
the later event-`0x74` and display/keypad gates. Thus code 6/resource `0x6a00`
is a decisive post-code-7 outcome, not a chronological prerequisite for code
7. Byte `0x112396` is a later halfword state set at `0x271426`; `0x112397` and
`0x11239b` have no recovered direct references. Byte `0x11239c` stores the
startup classification selected by `0x2af0ae` and is also read by the startup
supervisor. Byte `0x11239d` is cleared on code-7 entry and set only by
`0x2b4652` when the previous keymap-decoded value is `0x0d` while display state
is 1; the later event-`0x74` path uses it to permit the keypad scan fallback.
This store is downstream of `0x2b46da -> 0x2b2f90` matrix scanning and cannot
be the missing pre-code-7 transition.

Callback `0x5d` is organically active in state `0x0b`. Direct inputs `0x05eb`
and `0x06c5` report code 7. Inputs `0x05e1`, `0x05e7`, and `0x05dc` start a
task-local class-`0x52` timer whose recoded completion is `0x06c5`.

The callback's narrow semantic ownership is established even though its
product feature is not. It adapts a paired task-1 report lifecycle: input
`0x0348` posts report `0x06`, while terminal input `0x05eb` or timer completion
`0x06c5` posts report `0x07`. Each wrapper also mirrors the result to the
optional external resource channel as `0x6a00` or `0x6a01` respectively.

Class `0x6a` is not the owning hardware subsystem. The adjacent wrapper farm
maps its resource ids onto task-1 reports as follows:

| Resource | Task-1 report |
| --- | --- |
| `0x6a00`, `0x6a01`, `0x6a02`, `0x6a03` | `0x06`, `0x07`, `0x0b`, `0x0a` |
| `0x6a04`, `0x6a05`, `0x6a06`, `0x6a07` | `0x0c`, `0x0d`, `0x1c`, `0x09` |
| `0x6a08`, `0x6a09`, `0x6a0a`, `0x6a0b` | `0x03`, `0x02`, `0x0e`, `0x0f` |
| `0x6a0c`, `0x6a0d` | `0x01`, `0x11` |

The wrappers call the generic enabled-resource sender `0x2b5ae4`. Its
availability check indexes the class bitmap installed by contact-service
command `0x70`, and its request is queued through the external service
transport. A standalone phone can therefore post the task-1 report while the
optional `0x6a` mirror is disabled. Reports `0x14`-`0x19` use a different
resource/channel family, further disproving `0x6a` as one subsystem's device
contract.

Callback-table neighbour `0x5c` shares flags `0x01a00000` but is a much larger,
unrelated state machine, so table adjacency supplies no stronger identity.
Naming callback `0x5d` as battery, display, DSP, or security remains
speculative; its defensible name is the task-1 report-6/7 status dispatcher.

### Callback-0x5d ownership boundary

The callback selection and state-transition surfaces are now separately
bounded. `0x00dc` is the generic terminator of a variable-length catalogue
sequence, not a callback-specific action token, so it carries no subsystem
ownership. The fixed callback table contains the handler and flags; calls reach
it indirectly through the generic task-5 callback dispatcher.

The old 233-record bound for this table was wrong: 233 was derived from the
descriptor count, not the transition extent. The 234 status descriptors at
`0x2cb218` reference records through exclusive index 950, so the transition
table at `0x2cc7f0` contains 950 records. Thirty records select callback `0x5d`,
owned by descriptor inputs including `0x00c9`, `0x09d0..0x09d2`, `0x0af0`,
`0x1391`, `0x13ba`, and `0x1527..0x1532`. Record 92 remains:

```text
record 92 at 0x2ccad0
selector 0x5d, required state 3, new state 4
packed input 0x551c = event 0x151c with one argument
```

There is one direct MCU producer of `0x151c`, at `0x24d934`. The containing
context routine emits `0x09c9`, `0x09cd`, then `0x151c` only when its active
context matches and callback-state slot `0x5d` is already 1 or 2. The `0x09cd`
handler constructs the later callback-7 object lifecycle; this is the same
mapped context/session cycle already shown to depend on the absent
`0x05e8 -> 0x05ea -> 0x07dd` service-object chain. It cannot bootstrap
callback `0x5d` from the coherently observed state `0x0b`.

Thus record 92 describes one dormant context/session cycle, not the sole
non-initialization owner of callback `0x5d`. The wider table invalidates that
ownership closure and must be used for any future static callback census. This
correction does not reopen code 7 as the boot blocker: runtime still shows that
a physical power action produces code 7 and enters the shutdown lifecycle.

All four callers are mapped, but no ordinary coherent producer has yet been
proved. This is stronger than an unbounded “missing event” search: the open
question is which valid external condition or transaction makes one of these
already-known owners complete during a real 3210 boot.

The alternate mode-0d exit no longer forms a static impossibility. Full decode
of `0x2a6942` proves monitor state 0 returns 2 and is accepted; only initialized
states 1 and 2 return zero. Battery initialization clears the state to 0 before
the sole classifier writer publishes 1, 2, or 3. The coherent run reaches the
gate after state 1 is published. A real boot may therefore avoid mode 4 through
different ordering between the readiness checklist and the first monitor sample,
without requiring the unsafe state-3 ADC region. That observation motivated the
bounded timing audit below; it did not establish that the alternate branch would
remove the report-code-7 dependency.

That contract is now resolved for the emulated boot. Battery initialization
holds state 0 from `0.200664 s`; the first classifier publication changes it to
state 1 at `0.364121 s`. The eleven application-readiness writers do not begin
until the modeled contact peer acknowledges the final service-empty `0x622a`
transaction at `1.285269 s`. Firmware then resumes the complete second task
group in a fixed order; task 18's immediate code-`0x12` call at `0x285c5e` is
last at `1.297865 s`, and task 1 evaluates the gate at `1.298045 s`.

This is a contact-peer timing dependency, not a uniquely late battery, CCONT,
or task-18 response. It also is not the code-7 breakthrough: static comparison
of the two mode-`0x000d` tails shows that the accepted state-0 branch at
`0x270eee` performs its own mailbox receive and explicit comparison with code 7
before the shared interactive initialization. The observed state-1 branch at
`0x270fa4` waits for the same report in mode 4. Changing the peer's calibrated
delay solely to win the state-0 window would select a different dead wait, so
the delay is ledgered as fidelity debt and the frontier returns to the organic
code-7 owner.

### Mode-transition cause

A RAM write watch on the task-1 mode word closes the transition surface without
relying on a linear-PC flash hook. The coherent v6.00 boot has only these
firmware-owned changes after RAM initialization:

```text
mode 0001 -> 000d  store 270184, caller 270e3c, current message 00c9
mode 000d -> 0004  store 270184, caller 271266, current message 00d5
```

The message shown by the watch is the current mailbox item, not a command that
directly writes the mode. Mode `0x000d` waits until controller state
`[0x11ff6c]&0x0f == 6` and readiness bits `[0x112399]&0x0f == 0x0f`, then
selects one of two continuation families. `0x2a6942` supplies the battery-monitor
classification described above. `0x2b084c` is the charger-presence classifier:
it repeatedly samples analog selector 5, classifies six readings, converts the
aggregate through the software-float helpers, compares it with the firmware
threshold, and caches the boolean at `0x1124c8`.

The accepted battery/no-charger family at `0x270eee` receives a message and, if
it is not code 7, calls the mode-7 setter at `0x270f52`. The coherently selected
state-1 family reaches the corresponding receive at `0x27124e` and, if the
message is not code 7, calls the mode-4 setter at `0x271262`. Both calls return
directly into their shared interactive-initialization tail; entering mode 4 or
7 does **not** block that initialization. It records which later continuation
is waiting for code 7. This explains why the security editor and keypad work
while task 1 remains in mode 4.

In the coherent run the first message at the mode-4 comparison is `0x00d5`.
That is the scheduler's delayed form of report `0x15`, one of the four normal
readiness reports just consumed by mode `0x000d`. Its arrival before code 7 is
therefore ordinary queue ordering, not evidence that an ADC response explicitly
requested a low-battery wait. The unresolved contract is narrower: what later
power/charger lifecycle condition posts code 7 and resumes the already-running
interactive system.

## Excluded owners

The following candidates were tested or closed statically and must not be
reintroduced without new evidence:

- normal battery voltage, BSI, temperature, charger, or held-PWRONX values;
- the mode-4 battery-characterisation route: its event `0x43` is selected only
  by external contact-service command `0x8e` payload 3, not ordinary boot;
- Advice-of-Charge initialization and exhausted-account completion;
- display/service events `0x0280`-`0x0282` and the state-7 `0x06ca` route;
- callback slot `0x45` and the `0x09d0`/`0x09d1` chooser;
- callback `0x31`, status `0x00ca`, and resource result `0x0348`;
- task-13 direct-to-task-16 `0x05eb` delivery;
- the security editor and EEPROM/SIM identity comparison;
- task-17 startup object `0x1587` and its `0x0a35`, `0x0a25`, `0x09ec`,
  `0x043c` local transaction;
- DSP heartbeat traffic, a guessed type-`0x74` `0a 09` echo, and ROM-4 class
  `0x74` payload `13 00`;
- contact/DSP self-test completion, which now clears organically before the
  mode-4 wall;
- generic-service registration paths that require later framework modes.

The detailed evidence and exact fixtures live in `falsifications.json`,
`resource_providers.md`, `battery_classifier_analysis.md`, and
`sim_registration.md`.

## Cross-ROM controls

Report code 7 is not a universal DCT3 “DSP ready” report:

- Nokia 5110 v5.30 has a structurally equivalent wrapper and four callers. A
  traced forcing-free boot, physical-code unlock, and transition to standby
  posts zero code-7 reports across 53 task-1 messages. Its complete report-stub
  farm contains fifteen codes, while healthy startup fires only `0x14`,
  `0x15`, `0x16`, and `0x17`. The fired `0x14` block and dormant code-7 block
  share one battery/charger dispatcher; code 7 is skipped when predicate
  `0x260cea` finds its selected measurement above threshold+`0x15e`. This is
  protocol-family evidence that code 7 is conditional power lifecycle, not a
  universal DSP-ready report. The 5110 task-1 state machine and serial keypad
  remain product-specific and are not replay sources for the 3210.
- Nokia 3310 v6.39 reaches an interactive idle-like frame in an independent
  message-level emulator without executing its equivalent wrapper.
- Nokia 3330 v4.50 retains the same four-owner reporter topology statically.
- Nokia 3210 v5.01 contains the same resource-`0x6a01`/code-7 wrapper at
  `0x2ac5bc`, with exactly four callers at `0x21e22c`, `0x21f772`, `0x252a4a`,
  and `0x277d06`. Its task-1 state machine at `0x26dc20..0x26df14` is
  instruction-for-instruction equivalent to v6.00's
  `0x27120e..0x271502`: both mode entries compare against code 7, record their
  parked mode only on a different message, then use the same
  code-6/report-flag/event-`0x74` control flow. The state bytes are
  relocated from v6.00 `0x112390..0x11239d` to v5.01
  `0x1121bc..0x1121c9`. This proves the contract is stable across two 3210
  firmware releases; it is not a v6.00-only lifecycle artifact. The four
  caller bodies also retain their local predicates and outcomes. Callback
  `0x5d` retains the same input cascade and terminal statuses, with only its
  local timer class changing from `0x51` to `0x52`. No product-version branch
  or additional NV/hardware predicate is exposed by the comparison.

These controls reject generic peer traffic as justification for a synthetic
report. The faithful correction must be observable by the 3210 firmware at a
real hardware, transport, or nonvolatile-data boundary.

## Retired bridge

`MODEL_STARTUP_REPORTS` formerly returned code 7 and the later startup
checklist directly from the task-1 getter. It established downstream control
flow but was a firmware-result force and did not compose. It and the associated
historical oracle have been removed.

## Active diagnostics

`NOKI3210_TRACE_HANDOFF=1` retains only the current causal seams:

- task-1 mode transitions and dispatcher state;
- task-1 mailbox publications;
- report-7 wrapper/caller and callback-`0x5d` activity;
- IRQ0 entry and the scan/decode seam;
- the small set of controller, power, and identity observations still needed
  to reject accidental regressions in the classified surface.

The trace is read-only. A successful fix must work with it disabled.

The corrected release-to-key trace uses the scheduler-backed Lua input timer.
At `1.48 s` task 1 enters mode `0x0004`; a normal softkey press raises MAD2
IRQ0, enters `0x2b3084`, runs the firmware `0x41/0x42/0x43` sequence, scans at
`0x2b2f90`, and publishes decoded keycode `0x19` at `0x2b4628`. No code-7 post
or mode change occurs. The former IRQ6/event-`0x72` result was caused by
combining keypad and CCONT onto the same emulated source.

## Closed ownership and remaining lifecycle

The independent code-6 caller at `0x28c22c` is selected by status `0x0794`.
The census finds no direct producer: its sole numeric predecessor is fixed
catalogue input `0x32b4`/`0x72b4` (status index `0x12b4`), for which there is no
in-ROM initiating producer. The adjacent code-7 status `0x0795` is a real
terminal pair, but both of its effective producers are already classified as
later conditional paths. One requires display state 7; the other requires
framework mode 11 reached through external status `0x03ab`. Status `0x03ab`
itself has one consumer literal and no recovered in-ROM producer. This closes
the independent `0x0794`/`0x0795` family without turning current-state absence
into an invented transition.

A fresh coherent trace corrects the runtime interpretation: the security/text
transaction receives multi-key input and completes through `0x0578` while task
1 remains in mode 4. Code 7 is not the input owner. The observed `0x05e6` is a
successful security-code comparison, not evidence for synthesizing code 7.

A physical power-key control now reaches code 7 organically and closes its
ordinary ownership. Pressing power after the mode-4 frontier decodes key
`0x0d`, later reaches controller status `0x0795`, posts report `0x07`, and moves
task 1 to mode `0x000c`. That mode consumes report `0x74`, runs the service and
display teardown sequence, calls the power-control tail at `0x2b4e16`, and
waits. This is the shutdown lifecycle, not application startup. Synthetic
cold-boot holds released at 0.5, 1.0, 1.5, and 2.0 seconds all traversed IRQ0
without producing `0x0367`; no calibrated hold timer was retained.

No faithful correction is proved yet. All four code-7 callers and both code-6
callers are classified, and runtime closes code 7 as a physical-power/shutdown
result. The corrected 950-record table expands callback `0x5d`'s static
contract, so earlier claims that slot `0x45`/`0x09d0` was its only
non-initialization selector are retired. That static correction does not make
callback `0x5d` the current ordinary-hardware frontier.

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
