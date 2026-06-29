# Hardware atlas — the firmware↔hardware boundary (Nokia 3210, MAD2)

The boot is fundamentally the firmware *feeling around for hardware*. Every CONTACT-SERVICE gate
we cleared was a hardware-interaction gap (a DSP IRQ that never fired, a CCONT bit nobody set, a
charger ADC reading zero). This atlas maps that boundary — what MMIO the firmware touches, what is
**emulated / partial / stubbed**, and where in the boot it's reached — so the *next* mystery hang
becomes "look up which stub the firmware is poking" instead of a fresh labyrinth.

Built breadth-first with `NOKI3210_TRACE_MMIO=1` (one line per distinct register, with its
description + first PC), cross-referenced with the driver's address map and `nokia_mad2_reg_desc`.

## The chip

MAD2WD1 = **ARM7TDMI MCU** (emulated) **+ a DSP** (stubbed). The MCU runs the application/UI/control
firmware; the **DSP runs the GSM Layer-1 baseband and the audio codec** — so "GSM and audio" ≈ "the
DSP", and the DSP is a `return 0` shim. Companion chips reached over serial buses: **CCONT** (power /
ADC / RTC / charger), the **PCD8544 LCD**, the **24C128 EEPROM** (I²C), and (not yet touched at boot)
the **SIM** and the RF/synth path.

## CPU memory map (the emulated regions)

| range | device | handler | status |
|---|---|---|---|
| `0x000000–0x00ffff` (mirror `+0x80000`) | boot ROM / low RAM | `ram_r/w` | emulated |
| `0x010000–0x010fff` (mirror `+0x8f000`) | **DSP shared RAM** | `dsp_ram_r/w` | **STUB** ("HACK: avoid hangs"); a few offsets hand-faked |
| `0x020000–0x0200ff` (mirror `+0x8ff00`) | **MAD2 I/O** (all peripherals) | `mad2_io_r/w` | emulated (per-register, below) |
| `0x030000–0x030003` | **DSPIF** (DSP API control reg) | `mad2_dspif_r/w` | **STUB → 0** |
| `0x040000–0x040003` | **MCUIF** (memory-range config) | `mad2_mcuif_r/w` | **STUB → 0** |
| `0x100000–0x17ffff` | main RAM | `ram_r/w` | emulated |
| `0x200000–0x5fffff` | flash (the firmware) | `flash_r/w` | emulated (BYO dump) |
| `0x600000–0x9fffff` | ROM2 window/mirror | `rom2_mirror_r/w` | emulated |
| `0xa00000–0xa03fff` | EEPROM (24C128) | `eeprom_r/w` | emulated (blank; `selftest` overlay) |
| `0xa04000–0xffffff` | unmapped / reserved | — | — |

## MAD2 I/O peripheral registers (`0x20000`, byte offsets)

Blocks: **CTSI** (clock/timer/IRQ/reset), **PUP** (MBUS / vibrator / buzzer / GenIO), **KBGPIO**
(keyboard), **GENSIO** (multiplexed serial: CCONT, LCD, + SELECT-muxed devices), **SIMI** (SIM UART),
**UIF** (CTRL I/O pins). Touch column: ✓ = read/written during the boot-to-limp; — = not reached yet.

### CTSI — clock, timer, interrupts, reset (all emulated, all touched)
| off | reg | touch |
|---|---|---|
| `0x00` | ASIC version (r, → `0x40`) | ✓ |
| `0x01/0x02` | MCU / **DSP** reset control | ✓ |
| `0x03` | ASIC watchdog write | — |
| `0x04/0x05` | sleep-clock counter MSB/LSB | (timer1) |
| `0x08/0x09` | FIQ / **IRQ lines active** | ✓ |
| `0x0a/0x0b` | FIQ / IRQ mask | ✓ |
| `0x0c` | interrupt control | ✓ |
| `0x0d` | clock control | ✓ |
| `0x0e` | **interrupt trigger** (r; read-only — why `assert_irq(4)` can't be SW-triggered) | ✓ |
| `0x0f–0x13` | programmable timer (divider/counter/compare) | ✓ |

### PUP — MBUS, vibrator, buzzer, GenIO
| off | reg | status / touch |
|---|---|---|
| `0x15/0x16` | PUP control / FIQ8 ctrl | emulated ✓ |
| `0x18/0x19/0x1a` | **MBUS control / status / RX-TX** | emulated ✓ — the service bus (D0/D9 frames) |
| `0x1b` | vibrator | emulated ✓ (read) |
| `0x1c/0x1e` | buzzer divider / volume | emulated — |
| `0x20/0x22/0x24` | McuGenIO signal / ? / direction | emulated ✓ |

### KBGPIO — keyboard (emulated ✓)
ROW `0x28/0x29/0x68/0x69/0xa8/0xa9`, COL `0x2a/0x2b/0x6a/0x6b/0xaa/0xab` (signal / interrupt / direction).

### GENSIO — multiplexed serial (CCONT, LCD, + SELECT-muxed)
| off | reg | status |
|---|---|---|
| `0x2c` / `0x6c` | **CCONT write / read** | emulated (serial protocol; see CCONT below) |
| `0x2d` / `0x6d` | GENSIO start-transaction / status | emulated ✓ |
| `0x2e` / `0x6e` | **LCD data / command write** (PCD8544) | emulated ✓ |
| `0x6f`, `0xad/0xae/0xaf`, `0xed/0xee/0xef` | GENSIO **SELECT1/2/3** lines | partial — GENSIO multiplexes other devices on the SELECT lines; **SELECT1/2 are touched at boot** (`0x6f`, `0xad/0xae`) but what sits on them past CCONT is unmapped. A deep-dive target (RF synth? audio codec control?). |

### SIMI — SIM UART (`0x36–0x3f`) — **not touched at boot-to-limp**
TxD/RxD/IIR/control/clock/flags/fill. Emulation status unverified; the SIM card path is a future
frontier (GSM needs the SIM). Not reached before the limp.

### UIF — CTRL I/O pins (`0x32/0x33`, `0x70–0x73`, `0xb0–0xb3`, `0xf0–0xf3`)
General control I/O + directions; partly emulated, touched ✓. (Register `0x31` is read/written but
undocumented — `<Unknown>` in the desc table.)

## CCONT — power / ADC / RTC / charger ASIC (serial, via GENSIO `0x2c`/`0x6c`)

Register file (`nokia_ccont_r/w`), addressed inside the serial command (`addr = (cmd>>3)&0xf`):

| reg | role | notes |
|---|---|---|
| `0x0` | control | |
| `0x1` | PWM (charger) — **write-only** | (the idx6 service-channel check reads a *cached* value here; see service_bootstrap.md) |
| `0x2/0x3` | ADC read LSB / MSB | |
| `0x5` | watchdog (WDReg) | gated off by `DISABLE_CCONT_WATCHDOG` |
| `0x6` | RTC enable | |
| `0x7–0xa` | RTC sec/min/hour/day | served from host clock |
| `0xb–0xd` | RTC alarm / calibration | |
| `0xe` | **interrupt lines (status)** | bit 0 = present-status (`MODEL_CCONT_PRESENT`, idx6); bits 0–2 ignored by the IRQ dispatcher |
| `0xf` | interrupt mask | |

**ADC channels** (read via reg `0x0`/`0x2`/`0x3`): `0`=accessory, `1`=RSSI, `2`=battery V, `3`=battery
type, `4`=battery temp, **`5`=charger V** (= 0 → "no charger"; the mode-`000d` limp), `6`=VCXO temp,
`7`=charging current. Values come from `nokia_adc_override` (env `NOKI3210_ADC0..7`, profiles).

## The DSP interface (the big stub — deep-dive target)

The MCU↔DSP boundary is **DSP shared RAM `0x10000`** + the **DSPIF control register `0x30000`** —
and the DSP itself is unemulated. This is where GSM L1 and audio will land. What we know so far
(from the CONTACT-SERVICE work, `service_bootstrap.md` "DSP service-area map"):

- **DSP shared RAM `0x10000–0x10fff`** (`dsp_ram_r` HACK): `0x00..0x24` self-test echo; `0xa4/a6`
  service status/version; `0xda/e2` lower-service channel counts; `0xe0` busy flag; **`0xe4`
  lower-service pending counter** (MCU writes `0x0002` at pc `0x290c98`; `MODEL_DSP_SERVICE` drains it
  + raises IRQ 4); `0xfe/0x100` ready flags.
- **DSPIF `0x30000`** (stub → 0): written at boot (`pc 0x2001a4`) and during the service handshake
  (`pc 0x29103c`, `DSPIF[1]=0x04`). Likely the command/status side of the DSP protocol — **not yet
  reverse-engineered.** This is the next deep-dive: map the DSPIF register protocol + the shared-RAM
  mailbox so the GSM/audio interactions have a model to talk to.

## What the boot touches — and doesn't (the frontier)

To the **mode-`000d` limp** (past CONTACT SERVICE) the firmware exercises: CTSI (clock/timer/IRQ),
MBUS, CCONT (power/ADC/RTC), LCD, keypad, CTRL-I/O, and the DSP interface (stubbed). It does **not**
yet touch: the **SIM** (`SIMI`), and — necessarily, since they come after the limp — the **RF/synth**
(likely on a GENSIO SELECT line) and the **audio codec** (DSP). So the realistic phase-2 target is
**"boots to idle, no SIM / no network"**; the open strategic question is how much of the DSP/RF/SIM
the firmware *insists* on before idle vs *degrades* past. The DSP interface is both the biggest stub
and the most likely next blocker — hence the deep-dive next.

## Knob

`NOKI3210_TRACE_MMIO=1` — one deduped line per distinct MMIO register the firmware touches (MAD2 I/O,
DSPIF, MCUIF), with its description + first PC + time. Combine with `TRACE_DSP` (DSP shared RAM),
`TRACE_CCONT_READ` (CCONT), `TRACE_BUS` (scheduler) for the full boundary.
