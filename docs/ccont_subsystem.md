# CCONT subsystem — protocol map, hardware meaning, and component design

The living map of the Nokia 3210 **CCONT** power-management subsystem: what the chip is in
real hardware, the firmware↔chip protocol (serial register access + the interrupt→event/message
fan-out), confidence-tagged names for every signal, and the target C++ component. This is the
artifact the post-boot modelling work maps against — the boundaries and names here are meant to
outlive the boot effort, because CCONT keeps talking during normal operation (charger plug/unplug,
battery polling, RTC alarms).

See also: `hardware_atlas.md` (the whole MMIO boundary), `service_bootstrap.md` "Beyond the gate"
(the startup mode machine that consumes these signals), `driver_vision.md` (the knob→model map;
CCONT is retirement row #1).

## Confidence tiers

Every fact below is tagged. The tiers are the whole point — post-boot work needs to know what to trust.

- 🟢 **Proven** — firmware behaviour **and** an external source agree.
- 🟡 **Inferred** — consistent firmware-internal evidence only (disasm + runtime), no external confirmation.
- 🔴 **Guessed** — plausible placeholder, not yet evidenced.

## The chip (real hardware)

🟢 **CCONT = "Current Controller,"** the DCT3 power-management ASIC. The DCT3 baseband is four chips:
**MAD2** (TI, ARM7TDMI MCU + DSP), **CCONT** (power / ADC / RTC / charge-monitor / watchdog),
**CHAPS** (the charging switch/pump — does the actual charging), **COBBA-GJ** (audio + RX/TX I/Q
codec). CCONT identifies the battery by reading the **BSI** (Battery Size Indicator) line through its
ADC, monitors charger voltage, and its **watchdog block controls the power-on/off sequence**.
Sources: [DCT3 platform — LPC wiki](https://lpcwiki.miraheze.org/wiki/DCT3_platform),
[Nokia NSE-3 service manual](https://www.manualslib.com/manual/1226334/Nokia-Nse-3-Series.html).

🟡 Boundary with neighbours: **CHAPS** does the charging (CCONT only *monitors* charger voltage and
raises the charger-detect interrupt); **COBBA** does audio/RF codec (not CCONT); the **watchdog** that
DISABLE_CCONT_WATCHDOG suppresses lives in CCONT. So "battery/charger/RTC/power-rail" = CCONT;
"actually pushing charge current" = CHAPS.

## Transport — serial register access (GENSIO)

🟢 CCONT is a serial peripheral on the MAD2 **GENSIO** block: write at MMIO `0x2c`, read at `0x6c`,
transaction start/status at `0x2d`/`0x6d` (see `hardware_atlas.md`). The driver already models this
(`nokia_ccont_w` / `nokia_ccont_r`). 🟡 A command word selects a register `addr = (cmd>>3)&0xf` and
read/write direction; the firmware's `ccont_reg_read` helper is `0x2afb44`, write setup `0x2b5ae4`.

## Register file (reg 0x0–0xf)

From `nokia_ccont_r/w` + access sites. 🟡 unless tagged.

| reg | role | notes |
|---|---|---|
| `0x0` | control | |
| `0x1` | PWM / charger control (CHAPS drive) — **write-only** | 🟢 charger-control register |
| `0x2`/`0x3` | ADC result LSB / MSB | 🟢 ADC readout |
| `0x5` | watchdog (WDReg) | 🟢 the CCONT watchdog; gated by `DISABLE_CCONT_WATCHDOG` |
| `0x6` | RTC enable | |
| `0x7`–`0xa` | RTC sec/min/hour/day | 🟢 RTC, served from host clock |
| `0xb`–`0xd` | RTC alarm / calibration | |
| `0xe` | **interrupt status** (lines) | 🟢 the IRQ source the ISR reads (arg `0x90ff`); bit 0 = present-status (idx6, `MODEL_CCONT_PRESENT`) |
| `0xf` | **interrupt mask** | 🟡 read by the ISR (arg `0x11ff`) as `status & ~mask` |

## ADC channels (read via reg 0x0/0x2/0x3)

🟢 channel→signal (firmware reads + external BSI/charger confirmation); 🟡 exact scaling.

| ch | signal | hardware meaning |
|---|---|---|
| 0 | accessory detect | headset/accessory line |
| 1 | RSSI | RF signal strength (from COBBA path) |
| 2 | **battery voltage** | main Vbat |
| 3 | **battery type / BSI** | Battery Size Indicator line — 🟢 identifies the battery |
| 4 | **battery temperature** | BTEMP thermistor |
| 5 | **charger voltage** | =0 → "no charger" (the mode-`000d` cause) |
| 6 | VCXO temperature | oscillator temp |
| 7 | charging current | CHAPS current sense |

## The interrupt → event/message fan-out (the core protocol)

🟢 **`0x2b08c6` is the CCONT interrupt service routine** — the whole chip→firmware signal funnel.
It reads CCONT **interrupt-status reg `0xe`** (`& ~`mask reg `0xf`), keeps the **top 5 bits** (`0xf8`),
and per active bit posts a **startup event** (to the mode machine) and/or sends a **`0x77xx` PMM
message** (to subscriber tasks), plus sets a result-selector byte:

| int bit | → startup event | → PMM msg | handler / meaning |
|---|---|---|---|
| any set | `0x15` (🟡 "CCONT measurement IRQ") | `0x7701` (status report) | generic "a CCONT line fired" |
| bit 3 | `0x16` (🟡 charger) | `0x7706` | 🟢 **charger detect** → `charger_present_check 0x2b084c` |
| bit 4 | — | `0x7704` | 🟡 measurement result, selector=1 |
| bit 5 | — | `0x7703` | 🟡 measurement result, selector=2 |
| bit 6 | — | `0x7702` | 🟡 measurement result, selector=4 |
| bit 7 | — | `0x7705` | 🟡 → `0x29b71e` |

🟢 The `0x77xx` messages go through a generic IPC send primitive **`0x2b13a2(type, code, payload)`**
(`type` 0/1/2 = payload size; `0x2b5b24`/`0x2b5b3c` are the 1-byte / 2-byte wrappers; `0x2b12b4`
checks the channel is enabled via the flags at `0x11fee4`/`0x11ff08`, `0x2b12dc` delivers). So
**`0x77` is the power-management message family** and delivery is gated by channel-enable state — which
is exactly why some results don't reach a given subscriber on a blank phone.

🟡 **The startup-machine meaning of the gate events.** Mode `000d`'s four sub-events `0x14/0x15/0x16/0x17`
(flag byte `0x112399`, see `service_bootstrap.md`) are believed to be **CCONT measurement reports**, but
their exact source is **open** — see the Correction section below (an earlier "they come from emitter
`0x264f30`" conclusion was falsified; they appear to be scheduler-internal events from `0x26a458`).

## ⚠️ Correction — the emitter `0x264f30` is NOT the source of the surfaced sweep ids (falsified)

An earlier pass concluded the `000d` sweep events came from emitter `0x264f30` (called from
contact-service init `0x2347c6`, reading CCONT, posting task-3 messages), with dispatched id `[msg+4]`
class-gated by `[msg+2]` (classes `0x0e`/`0x1a` pass, `0x06`/`0x16` drop). **Two experiments + timing
falsified all of that — recorded here so it isn't a future bum steer:**

- 🟢 **Timing disproves the emitter as source.** The dispatched ids `0x17` (t≈0.337) and `0x14`
  (t≈0.373) are dequeued **before** emitter `0x264f30` posts its messages (t≈0.41–0.42). The emitter's
  own messages (which happen to *contain* the bytes `0x14`/`0x16`) are never dequeued as those ids — the
  `[msg+4]` match was coincidental. `0x264f30` is real (it does post CCONT-filled task-3 messages) but
  it is **not** what feeds the `000d` gate.
- 🟢 **Class-rewrite disproves the class-gating model.** Rewriting the trapped `0x15`-message's class
  `0x16 → 0x1e` (bit 3 set) *and* `0x16 → 0x1a` (a "known pass-through" class) at post time both left
  `0x15` un-surfaced and the flag at `0x08`. So neither "bit 3 of class" nor "class membership" gates
  surfacing. The whole `[msg+4]`/class model is withdrawn.
- 🟢 **The surfaced ids aren't task-3 message posts at all.** With the mode filter removed, the task-3
  posts before t≈0.38 carry classes/events like `0a 05 1e` and `50 51 22` — **none** with `[+4]=0x14`
  or `0x17`. Yet `0x14`/`0x17` *are* dequeued. So `0x26a458` surfaces them from an **internal scheduler
  source** (timer / system events), not from `0x26a204` message posts.

## What still stands (solid)

- 🟢 The `000d` flag (`0x112399`) stalls at `0x08`–`0x09`; event `0x15` **never** surfaces as a
  dispatched id → bit 2 never sets. (Reproduced every run.)
- 🟢 `0x17` reliably surfaces (bit 3); `0x14` surfaces intermittently (bit 0) — both from the scheduler
  `0x26a458`, **before** t≈0.38, not via task-3 posts.
- 🟢 The CCONT ISR `0x2b08c6` posts `0x15`/`0x16` only as `0x2697aa` *events* (a different channel from
  the mailbox the `000d` handler reads) plus `0x77xx` PMM messages. (Unchanged.)

## Corrected open question (the real lead)

**The dispatcher `0x26a458` — correct structure (Ghidra, after capstone desynced).** Decompiled locally
(`ghidra_out/.../sched_recv_26a458.c`; Ghidra output is git-ignored — read, never commit). It is the
per-task receive that returns the next event id, from **three queues** on the task record (`task*0x1c`):

1. 🟢 **Timer/delay queue** `[task+8]`: pops a delay node, returns `table[0x2d71a8 + [node+9]*8]`. That
   table maps index→id **`0xc0…0xd7`** — the timer/system family (the `0xc3` tick, the `0xd5` CCONT
   event). (My earlier `0x2d71a8` guess was right; the desync didn't reach it.)
2. 🟢 **Ring A** `[task+0x14]` (head `[+0x19]`, tail `[+0x18]`): returns `ringA[idx]`.
3. 🟢 **Ring B** `[task+0xc]` (head `[+0x11]`, tail `[+0x10]`), gated by `([task2+0xf] & 1)==0`: returns `ringB[idx]`.

The translator `0x26ff14` is a big near-identity switch on that id; the **`0xd5` case** is special — it
calls the **CCONT ISR `0x2b08c6`** then `sched_post_2697aa(0x15, …)` (so a dequeued `0xd5` re-emits
event `0x15` on the `0x2697aa` channel — the wrong one for `000d`).

## The four `000d` sweep producers — RESOLVED (Ghidra named corpus)

The ring-ref hunt found all four, already named by prior analysis, **all CCONT/charger** — which finally
nails the `000d` gate's meaning:

| event | producer (named) | addr | what it is |
|---|---|---|---|
| `0x14` | `startup_event14_source7_{absent,present}_producer` (+ `..latch_and_schedule_2a0fae`) | `0x2abdc0`/`0x2abde4` | 🟢 the **charger "source 7" present/absent** event (cf. the `7=absent->go` limp note) |
| `0x15` | `ccont_battery_init_post_event15` | `0x2b09f2` | 🟢 CCONT **battery-init** (also writes the charger latch `0x1124c9`) |
| `0x16` | `ccont_irq_charger_event16_payload6` | `0x2b0958` | 🟢 the CCONT **ISR charger** (bit-3) path |
| `0x17` | `ccont_init_post_startup_event17` | `0x2af086` | 🟢 CCONT **init** startup event |

So the `000d` gate **= "wait for the CCONT power-on/charger sweep"** (charger-source7 + battery-init +
charger-IRQ + ccont-init). That's the faithful name, and it makes the `ccont_device`'s role concrete:
own these signals.

🟢 **The surfacing asymmetry — confirmed at the producer level (per-event post channel).** The `000d`
handler *entry* calls **both** `0x2b09f2` (→`0x15`) and `0x2af086` (→`0x17`) right before the dispatch
loop (clean disasm `0x270e0e`/`0x270e18`), yet `0x17` surfaces (bit 3) and `0x15` does not (bit 2). The
difference is the **post channel**, now read directly from each producer:

| event | producer | posts via | channel | surfaces to `000d`? |
|---|---|---|---|---|
| `0x14` | `0x2abdc0`/`0x2abde4` | `startup_event14_latch_and_schedule 0x2a0fae` | direct schedule | ✅ |
| `0x15` | `0x2b09f2` | `sched_post_2697aa` (`0x2b0a12`) | **delayed/timer** | ❌ |
| `0x16` | `0x2b0958` | `sched_post_2697aa` | **delayed/timer** | ❌ |
| `0x17` | `0x2af086` | `sched_context_post_message 0x26a354` | direct message | ✅ |

So the two events that **never surface** (`0x15`/`0x16`) are exactly the two on the **delayed `0x2697aa`
channel**; the two that surface (`0x14`/`0x17`) use **direct** channels (`0x26a354` / `0x2a0fae`). Runtime
matches: the `limp2_evpost` trace shows `0x2697aa` called with `0x15`(`ev=21`)/`0x16`(`ev=22`) but never
`0x14`/`0x17`. The `0x2697aa` posts schedule through the timer mechanism and reflect as the `0xd5` timer
event (whose `0x26ff14` case re-posts `0x15` via `0x2697aa` again) — 🟡 that the reflection *never*
lands a standalone `0x15` in the ring is the one inferred step; the per-event channel split itself is 🟢.

**So the faithful question is now precise:** does a *provisioned* phone close the `000d` gate at all via
`0x15`/`0x16` (delayed) — or does the gate only need `0x14`/`0x17` plus the `CCONT_STATE==6` condition,
with `0x15`/`0x16` being a red-herring requirement on a blank unit? That is the next thing the
`ccont_device` work must answer (likely by feeding real CCONT measurement values and watching the flag),
rather than forcing events.

⚠️ Several of these producer **bodies don't decompile cleanly** even in Ghidra ("bad instruction data" —
a Thumb-decode issue), so we rely on the **names** + the clean caller-side disasm. Confirming the exact
`0x15` post path (and whether a provisioned phone reaches flag `0x0f` via a different channel) is the
remaining 🔴 — but it is now a **specific** question about `0x2b09f2`'s post mechanism, not a mystery.

## Open questions (the mapping backlog)

- 🔴 ~~emitter `0x264f30` / `[msg+4]` offset / class-gating~~ **Falsified** (see the Correction section)
  — the surfaced sweep ids come from an internal scheduler source in `0x26a458`, not task-3 posts.
- 🔴 What internal source in `0x26a458` emits the surfaced `0x14`/`0x17`, and why no `0x15`? (the real lead)
- 🟡 Exact CCONT command-word encoding (the `0x90ff`/`0x11ff`/`0x9001` arg format → reg + mask).
- 🟡 The result-selector byte at `0x1124d2`(?) and how `0x77xx` results map back to ADC channels.
- 🟡 Which task subscribes to `0x77xx`, and why `0x15`/`0x16` reach (or don't reach) the startup task (the routing records `≈0x100140`).
- 🟡 ADC conversion timing — how long after a request the completion interrupt should fire.
- 🟢→ confirm reg `0xf` is the mask (vs another role) by a runtime read at the ISR.

## Target C++ component

A MAME `ccont_device` (the PCD8544 LCD is already a real device, so this fits the grain). Staged:
a cohesive driver-internal `ccont` class first, promoted to a `device_t` once stable.

```cpp
class ccont_device : public device_t {
public:
    // GENSIO serial transport (MCU clocks command+data) — replaces nokia_ccont_r/w
    u8   reg_r(u8 addr);  void reg_w(u8 addr, u8 data);
    auto irq_cb() { return m_irq.bind(); }      // IRQ line out -> MAD2 CTSI
    void set_scenario(power_scenario s);         // no-charger / charging / charged (data, not knobs)
protected:
    void device_start() override;  void device_reset() override;
private:
    u8  m_reg[16];                                // register file 0x0-0xf
    u8  m_int_status, m_int_mask;                 // regs 0xe / 0xf  <- the IRQ the ISR reads
    struct power { u16 batt_mv, charger_mv, batt_temp, bsi, vcxo_temp, chg_current, rssi, accessory; }
        m_power;                                  // ADC sources, set by the scenario
    // THE missing piece: an ADC measurement state machine.
    //   MCU requests measurement (reg write / 0x77xx) -> schedule conversion on m_adc_timer
    //   -> on completion: write result reg 0x2/0x3, set the matching int-status bit (0xe),
    //      assert IRQ. This *produces* the 0x14-0x17 / 0x15 / 0x16 stream the startup machine waits on.
    emu_timer *m_adc_timer;
    devcb_write_line m_irq;
    // RTC from host clock (regs 0x7-0xd); watchdog (reg 0x5).
};
```

**It retires** the whole CCONT knob cluster — `ADC_PROFILE`, `ADC0/5*`, `BATTERY_PROFILE`,
`CCONT_EVENT15_DELAY`, `STARTUP_EVENT15_DELAY_CLAMP`, `CCONT_BOOT_STATUS`, `MODEL_CCONT_PRESENT`,
`CCONT_IRQ_*` — collapsing them into modelled state + the measurement timer. **Boundary:** the device
owns power/ADC/RTC/charger-monitor/watchdog and the interrupt it raises; it does **not** own the
startup mode machine (firmware), CHAPS charging current, or message routing (that's the RTOS) — it
just produces faithful interrupts and register values, and lets the firmware react.

## Why this matters past boot

The same interrupt model serves normal operation: plug a charger → CCONT charger-detect interrupt
(bit 3) → event `0x16` + `0x7706`; periodic battery poll → ADC completion interrupts; RTC alarm →
RTC interrupt. Getting the component's boundaries and signal names right now means post-boot features
(charging UI, battery meter, clock/alarm) plug into a real model instead of more forcing.
