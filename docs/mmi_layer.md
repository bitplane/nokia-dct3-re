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

Next dives should target that handoff directly:
- find the writer/gate of the MMI idle flag `[0x1116fd]` and the UI-state `[0x11fcdc]`,
  and what event would advance them;
- find what *starts* the keypad/input task (why it's dormant) — the same activation
  that would build the resource providers;
- trace the MMI's top-level state machine at the "Insert SIM card" settle point and
  identify the one event it is waiting for.

The reusable input-injection knob is `POST_READY_KEY` (`enter`/`up`/`down`/digits;
`POST_READY_KEY_DELAY_MS`/`DURATION_MS`/`PERIOD_MS`), gated on the startup latch
(`[0x112398]` low byte → `0x0f`).
