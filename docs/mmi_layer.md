# The MMI / coherent-boot layer — "up but not alive"

The boot reaches a complete **"Insert SIM card"** MMI screen (text + scrollbar +
status-icon chrome; frames `blank → o058 → o074`). This documents the layer *above*
that screen — why the phone doesn't advance to idle, a menu, or Snake — gathered by
driving it the way a user would (keys) rather than by forcing flags.

## The MMI renders, but it does not process input

Probe: inject the **Menu/select key** (`POST_READY_KEY=enter` → keypad row 4, mask
`0x08`) repeatedly once the boot has settled (after the startup latch at t≈0.85), and
watch. On a real 3210 the Menu key works with no SIM (you can reach Settings and play
Snake). Result on our boot: **nothing changes.** Tracing the keypad path end-to-end:

- **The keypad interrupt is live.** Each injected press produces a falling edge; the
  driver asserts **IRQ 6**; and at that moment the MAD2 interrupt controller has IRQ 6
  **enabled and unmasked** (`IRQ_CTRL[0x0c]=0x05` → IRQ-enable set; `IRQ_MASK[0x0b]=0xaa`
  → bit 6 clear). So the interrupt *is* delivered to the CPU.
- **But no key is ever decoded.** The firmware reads the KBGPIO keypad registers
  (`0x28–0x2b`, `0x68–0x6b`, `0xa8–0xab`) **only during early boot** (`COL 0x2a` last
  read at t≈0.25, `COL-irq 0x6b` at t≈0.16) and **never again** — not in the settled
  state, and not in response to the IRQ. The keypress interrupt fires into a firmware
  that doesn't scan/decode it, and the display never changes.

So the input-processing subsystem (the keypad scan/decode task / MMI input handler) is
**dormant**: the low-level IRQ is wired and firing, but nothing consumes it. The phone
is *not* interactive in this state — it cannot be driven into the menu with keys.

## This is the same wall as the display-content layer

The MMI is **"up but not alive"** — it reached a display state and painted a screen, but
its runtime task/object graph isn't fully constructed:

- **Display content** (`sim_emulator_scope.md`): `display_idle` never fires (idle flag
  `[0x1116fd]` never set); and the idle window's ~18 content resource classes have no
  runtime *provider objects*, so forcing the draw blanks (the providers are built by the
  UI subsystems' init, which doesn't run).
- **Input** (this doc): the keypad IRQ is live but no task decodes keys.

Both are the **same coherent-boot incompleteness**: the MMI application subsystems get
far enough to render one screen, but the interactive layer — the input task, the window
manager's resource providers, the idle-state machine — never fully initialises. The
screen is a snapshot, not a running UI.

## Why (the open root) and where the next dives go

Everything upstream is now understood; the remaining question is the single one that
gates all of it: **what coherent top-level state activates the MMI application layer?**
On a real boot, some state transition (past the SIM read, into "ready for interactive
UI") starts the input task, brings up the window/resource providers, and drives the
idle-state machine. On our reconstructed boot that transition never fires — the same
kind of coherent-state handoff that CONTACT SERVICE, `000d`, and service-ready each
turned out to be.

The reusable input-injection knob is `POST_READY_KEY` (`enter`/`up`/`down`/digits;
`POST_READY_KEY_DELAY_MS`/`DURATION_MS`/`PERIOD_MS`), gated on the startup latch
(`[0x112398]` low byte → `0x0f`).

## Root-gate dive: the top-level UI mode is *correct* — the gate is finer

Traced the MMI's mode/activation chain. The MMI loop (`display_manager_297fc4`,
task 6) is recv-blocked on `0x26a458`; task 5 (the MMI VM) decides whether to scan
the keypad by reading the **UI-mode byte `[0x11ff41]`** (getter `0x2a8fcc`) and acting
when it is **`0xa`** (interactive). That byte is set by the **startup outcome arbiter
`0x2a922e`**, which reads **MAD2 register `0x01`** (the CTSI MCU reset-control /
reset-source register) and:
- if **bit0 set** (normal power-on) → `[0x11ff41] = 0xa` (interactive) at `0x2a9276`;
- else → a retry (`1`) or charger-reason value.

**On our boot this works correctly.** `TRACE_UIMODE`: the arbiter reads reg `0x01 =
0x01` (bit0 set) at t≈0.006 and writes `[0x11ff41] = 0x0a`, which then stays `0x0a`
for the whole run. So the phone *does* identify a normal, interactive power-on, and
the top-level UI mode is already interactive. **The UI-mode/outcome byte is not the
gate.**

**And the keypad-scan path I first found (`0x2af0ae` → `0x2b2f90`) is startup-only.**
Its callers are all in the startup region (`0x270124/34/46`, `0x27051e`, `0x270cf0`)
— it's the power-on key check (which key is held at boot), which is why the matrix is
scanned at t≈0.25 and never again. It is *not* the runtime keypad handler.

**So the runtime input is purely IRQ6-driven, and that path is the dormant one.** The
keypad interrupt fires and is delivered (previous section), the UI mode is interactive,
but the IRQ6 handler / input task that should read the latched key and post a key
*message* to the MMI never processes it. The "up but not alive" gap is therefore
*finer* than the outcome/mode: it is the **runtime input ISR / input task**, not the
top-level state.

## The IRQ6 keypad ISR chain — the ISR works, the consumer is dormant

Traced the whole keypad interrupt path (empirically via `TRACE_IRQ` on MAD2 status
reg `0x09`, then statically):

1. **IRQ dispatcher `0x2af430`** (runs in the DSP-service IRQ context, ~200 Hz):
   reads active IRQs `= status[0x09] & ~mask[0x0b]`; dispatches by line at `0x2af3d6`
   (a shift/`bhs` cascade). Line 6 (bit6 = keypad) → `0x2af40a` → **`bl 0x2b5da0`**.
   Confirmed live: when a key is injected, the dispatcher reads `reg0x09 = 0x40`
   (keypad bit) at exactly the keypress times.
2. **Keypad ISR `0x2b5da0`** does *not* scan the matrix. It:
   - **masks** further keypad IRQs — sets bit6 in IRQ mask `[0x2000b]`;
   - **enqueues event `0x72`** (from ROM `[0x2e2230]`) into a task mailbox via the
     scheduler post `0x26aac0` (TCB-indexed; `str r4,[r1,r0]`) — i.e. it *delegates*
     the scan/decode to a consumer task;
   - **acks** the IRQ (writes `0x40` to `[0x20009]`).

So the runtime design is: keypad IRQ → ISR masks + posts event `0x72` to the input
consumer → the consumer recvs `0x72`, scans/decodes the key, posts a key message to
the MMI, and **re-enables** the keypad IRQ. On our boot that consumer never completes
the cycle: keys are never decoded, the MMI never gets a key message, and — because the
ISR masked the keypad IRQ pending the consumer — **the input path self-locks after the
first press** (the mask is never cleared).

**So the true root is the event-`0x72` consumer task being dormant/stuck.** This is
the same shape as the display side (a screen painted, but the runtime consumer/provider
graph not running) and fits the coherent-boot-handoff theme: a task that should have
transitioned into interactive-app mode after startup never does, so the events it
should consume (keypad `0x72`, and by symmetry the display resource-provider setup)
pile up unhandled.

Final hop (next): identify the event-`0x72` consumer task (the mailbox `0x26aac0`
targets) and confirm it is unscheduled / blocked elsewhere — then find what should
have activated it (the post-startup → interactive-app handoff). Knobs (reverted,
git-recoverable): `TRACE_IRQ`, `TRACE_UIMODE`, `POST_READY_KEY`.

## The `0x72` consumer, pinned — and a correction: the input path is NOT dormant

Pinned the consumer and it overturns the "dormant" reading above.

- **Mailbox = task id.** recv `0x26a458` reads the current task id `[0x100022]` and
  indexes `0x108414 + id*0x1c`. The keypad ISR posts to mailbox **1**, so the consumer
  is **task 1** (`0x270170`) — the startup state machine (dispatcher `0x270c8e` → mode
  jump table `0x270ca8`).
- **Task 1 is alive** (`TRACE_RECV`: it recv's every ~1 s in the settled state) and it
  **does receive the keypad event**: `TRACE_T1` shows task 1 recv `code=0x72` at exactly
  the injected keypress time, while `mode=0x00`.
- **And it has a real handler for it.** Task 1's mode-0 path dispatches on the *message
  code* (`0x2701cc` cascade): `cmp r1,#0x72 → bl 0x270a8c`. The handler `0x270a8c →
  0x270742` runs IRQ-mask management (it **re-enables** the keypad IRQ so the next press
  is delivered — confirmed: the dispatcher sees `reg0x09=0x40` on *every* press) plus
  hardware checks and trace sends (`0x2b5b24`→`0x2b13a2`, code `0x1773`, payloads
  `0x50/0x51/…`).

**So the earlier "input consumer is dormant" conclusion is wrong.** The full path —
IRQ6 → ISR `0x2b5da0` → event `0x72` → task 1 mailbox → recv → dispatch `0x72` →
handler `0x270a8c/0x270742` → re-enable — is **functional and runs on every keypress.**
Keys are delivered and handled by task 1.

**The real conclusion is subtler:** the keypress is consumed by task 1's *startup-machine
mode-0* handler, which handles it as a low-level wake/IRQ event and does **not** translate
it into an MMI menu-navigation action. Whether that is a *bug* or *correct behaviour* is
now the open question: on a DCT3 with **no SIM**, the "Insert SIM card" state may
legitimately block menu access, in which case the phone is behaving faithfully and there
is nothing to "fix" — full keypad-driven navigation would only come with an accepted SIM
moving the MMI into an interactive state. So the input plumbing is proven correct; the
remaining question folds back into the SIM/MMI-state question (does "Insert SIM card"
grant menu access on real hardware?), not a broken input path.

This retires the "input is dormant / up-but-not-alive on the input side" framing: the
input side *is* alive. The display-content pipeline (unbacked resource providers) remains
the genuine wall for *rendering* a richer screen; the input wall was a misread.
