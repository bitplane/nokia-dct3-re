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
(flag byte `0x112399`, see `service_bootstrap.md`) are **CCONT measurement reports** delivered to the
startup task as a one-shot sequence (see "The sweep emitter" below).

## The sweep emitter `0x264f30` — who posts `0x14/0x15/0x16/0x17` (traced)

🟢 The `000d` sweep events are posted by a single emitter **`0x264f30`**, whose **only caller is
`0x2347c6` — inside the contact-service init**. So they are emitted *once*, early, not on a repeating
CCONT-interrupt loop. The emitter **reads CCONT** (`0x2b0a74` / `0x2b0c62`, in the `0x2b0xxx` CCONT
driver) to fill the message payloads — confirming these are CCONT measurement reports — and **posts
them as task-message to task `03`** (the startup task) via `0x26a204`. Sequence built: a `0x13`
message (with a CCONT read), then `0x14`, then a `0x15`/`0x16` message, then `0x16`/`0x1a`…

🟢 **Message format and dispatched-id offset (measured).** The emitter posts task-3 messages with the
header `00 02 [class] 70 [event] [param] …`, where **`[msg+2]` = class** and **`[msg+4]` = event**.
The startup dispatcher's id (what `0x26ff14` returns to the `000d` handler) is **`[msg+4]`, gated by the
class `[msg+2]`** — only certain classes pass their `[+4]` through. Measured (post header → raw dequeued id):

| `[+2]` class | `[+4]` event | surfaced to `000d`? |
|---|---|---|
| `0x06` | `0x13` | no |
| `0x0e` | `0x14` | **yes → `0x14`** |
| `0x16` | `0x15` | no |
| `0x1a` | `0x16` | **yes → `0x16`** |

🟢 **The `0x15` gap (now exact).** `0x15` is the `[+4]` event of a message whose **class `[+2]=0x16`**,
and that class is *not* passed through to the startup dispatch — so `0x15` is structurally trapped as the
*parameter* of a `0x16`-class message and never surfaces as its own `000d` sub-event. Hence flag bit 2
(= `0x15`) never sets. (Two channels confirmed: the `000d` handler reads this **mailbox**; the CCONT ISR
`0x2b08c6` posts `0x15`/`0x16` only as `0x2697aa` *events* — a different channel — plus `0x77xx` PMM msgs.)

🟡 **Implication for the fix.** The gate is a **message-class routing** issue, not a missing measurement:
the emitter *does* produce a `0x15`, but as a class-`0x16` parameter the startup dispatch ignores. The
faithful question for `ccont_device` is narrower still — feed the emitter the CCONT values that make it
emit `0x15` as a pass-through class (or confirm real hardware reaches `0x0f` another way). 🔴 open: what
distinguishes the classes the dispatch passes (`0x0e`/`0x1a`) from those it drops (`0x06`/`0x16`).

🟢 **Naming (proven path):** the `000d` gate = "wait for the contact-service-init CCONT measurement
report sweep (task-3 messages from emitter `0x264f30`) to deliver all four sub-events." The faithful
fix is therefore *not* "make the CCONT ISR fire more" — it is to make the emitter's sweep deliver a
standalone `0x15` (or recognise the `0x15` payload), i.e. a **firmware-data/sequence** gap fed by
CCONT reads, not a missing interrupt.

## Open questions (the mapping backlog)

- 🟢 ~~Where are events `0x14`/`0x17` posted?~~ **Resolved:** emitter `0x264f30` (called once from
  contact-service init `0x2347c6`), reading CCONT, posting task-3 mailbox messages. See "The sweep emitter".
- 🟢 ~~Exact dispatched-id offset~~ **Resolved:** id = `[msg+4]` (event), class-gated by `[msg+2]`.
  Measured post-header→dequeue table above.
- 🔴 What distinguishes the message classes the startup dispatch passes through (`0x0e`/`0x1a`) from
  those it drops (`0x06`/`0x16`) — the key to why `0x15` (a class-`0x16` param) never reaches the gate.
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
