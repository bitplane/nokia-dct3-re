# Hardware atlas — the firmware↔hardware boundary (Nokia 3210, MAD2)

Boot exercises hardware, provisioning and firmware lifecycle together. This
atlas maps the actual MMIO boundary — what the firmware touches, what is
**emulated / partial / stubbed**, and where in the boot it's reached — so the *next* mystery hang
becomes "look up which stub the firmware is poking" instead of a fresh labyrinth.

Current fidelity claims are maintained in `mad2_fidelity.md`.

## The chip

MAD2WD1 contains the emulated **ARM7TDMI MCU** and an unemulated DSP core. The MCU
runs the application/UI/control firmware; the DSP owns GSM Layer 1 and audio.
`nokia_dspif_device` models firmware-visible shared memory, DSPIF and packet
rings. Separate DSP-HLE and external-service devices attach the two semantic
peers without executing DSP instructions. Companion devices are **CCONT** (power/ADC/RTC/charger), the
**PCD8544 LCD**, **24C128 EEPROM** (I2C), and the SIM card behind MAD2 SIMI.

## CPU memory map (the emulated regions)

| range | device | handler | status |
|---|---|---|---|
| `0x000000–0x00ffff` (mirror `+0x80000`) | boot ROM / low RAM | `ram_r/w` | emulated |
| `0x010000–0x010fff` (mirror `+0x8f000`) | **DSP shared RAM** | `nokia_dspif_device::shared_r/w` through `dsp_ram_r/w` | Extracted partial transport; peer-owned bootstrap reads remain an explicit DSP-HLE overlay |
| `0x020000–0x0200ff` (mirror `+0x8ff00`) | **MAD2 I/O** (all peripherals) | `mad2_io_r/w` | Mixed partial hardware, calibrated behavior and backing latches; see per-block ledger |
| `0x030000–0x030003` | **DSPIF** (DSP API control reg) | `nokia_dspif_device::dspif_r/w` | stored command-4 doorbell; HLE scheduling still partly shared-write driven |
| `0x040000–0x040003` | **MCUIF** (memory-range config) | `mad2_mcuif_r/w` | Retained four-byte configuration latch; no decoded side effects |
| `0x100000–0x17ffff` | main RAM | `ram_r/w` | emulated |
| `0x200000–0x5fffff` | flash (the firmware) | `flash_r/w` | emulated (BYO dump) |
| `0x600000–0x9fffff` | ROM2 window/mirror | `rom2_mirror_r/w` | emulated |
| `0xa00000–0xa03fff` | EEPROM parallel alias | `eeprom_r/w` | **unproven** read-only view of the input region; normal access uses serial I2C |
| `0xa04000–0xffffff` | unmapped / reserved | — | — |

## MAD2 I/O peripheral registers (`0x20000`, byte offsets)

Blocks: **CTSI** (clock/timer/IRQ/reset), **PUP** (MBUS / vibrator / buzzer / GenIO), **KBGPIO**
(keyboard), **GENSIO** (multiplexed serial: CCONT, LCD, + SELECT-muxed devices), **SIMI** (SIM UART),
**UIF** (CTRL I/O pins). Touch column: ✓ = read or written in a normalized
profile; — = not established.

### CTSI — clock, timer, interrupts, reset (mixed fidelity)
| off | reg | touch |
|---|---|---|
| `0x00` | ASIC version (r, → `0x40`) | ✓ |
| `0x01/0x02` | MCU / **DSP** reset control | ✓ |
| `0x03` | ASIC watchdog write | ✓ |
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
| `0x15/0x16` | PUP control / FIQ8 ctrl | periodic placeholder ✓ |
| `0x18/0x19/0x1a` | **MBUS control / status / RX-TX** | extracted controller with byte attachment, FIQ2 RX/TX lifecycle and calibrated timing; no bus peer ✓ |
| `0x1b` | vibrator | backing latch ✓ (read) |
| `0x1c/0x1e` | buzzer divider / volume | backing latches — |
| `0x20/0x22/0x24` | McuGenIO signal / ? / direction | partial: native EEPROM pins plus unknown stored bits ✓ |

### KBGPIO — keyboard (partial hardware ✓)
ROW `0x28` signal / `0xa8` direction, COL `0x2a` active-low input / `0x6b`
interrupt mask. Firmware drives a 4-row by 5-column matrix; physical key edges
latch MAD2 IRQ0, whose handler starts the firmware scan/decode sequence. CCONT
uses the separate MAD2 IRQ6 source. Registers `0x29/0x68/0x69/0xa9` and
`0x2b/0x6a/0xaa/0xab` remain backing storage with no established keypad role.

### GENSIO — multiplexed serial (CCONT, LCD, + SELECT-muxed)
| off | reg | status |
|---|---|---|
| `0x2c` / `0x6c` | **CCONT write / read** | emulated (serial protocol; see CCONT below) |
| `0x2d` / `0x6d` | GENSIO endpoint control / status | partial: LCD=`0x21`, CCONT=`0x25`; status tracks idle/TX-ready and synchronous CCONT RX-ready |
| `0x2e` / `0x6e` | **LCD data / command write** (PCD8544) | emulated ✓ |
| `0x6f`, `0xad/0xae/0xaf`, `0xed/0xee/0xef` | GENSIO **SELECT1/2/3** lines | partial — GENSIO multiplexes other devices on the SELECT lines; **SELECT1/2 are touched at boot** (`0x6f`, `0xad/0xae`) but what sits on them past CCONT is unmapped. A deep-dive target (RF synth? audio codec control?). |

### SIMI — SIM UART (`0x36–0x3f`)
`nokia_simi_device` owns the MAD2 UART/FIFO/IIR endpoint and FIQ6. The separate
`nokia_sim_card_device` owns ATR, PPS and the T=0 APDU lifecycle; response bytes
return through the organic SIMI register and FIQ6 path.
TXD writes enter a 16-byte hardware FIFO, `0x3e=0x00` flushes a chunk to the
T=0 parser, and `0x3f` reports its live fill. See `sim_subsystem.md` and
`sim_emulator_scope.md`.

### UIF — CTRL I/O pins (`0x32/0x33`, `0x70–0x73`, `0xb0–0xb3`, `0xf0–0xf3`)
General control I/O + directions; partly emulated, touched ✓. Register `0x31`
is CTRL-I/O signal register 1. Its six exhaustive firmware sites only
read-modify-write bit 1 from the power duty-cycle subsystem, so the driver
models it as an output latch; its physical PCB net remains unknown.

## CCONT — power / ADC / RTC / charger ASIC (serial, via GENSIO `0x2c`/`0x6c`)

Register file (`nokia_ccont_device::serial_r/w`), addressed inside the serial command (`addr = (cmd>>3)&0xf`):

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

**ADC selectors** (read via reg `0x0`/`0x2`/`0x3`): the driver deliberately exposes raw selectors
`0..7`; no validated 3210 board-level signal names are assigned.
Firmware boot reader `0x2a84b0` directly samples selector 0, whereas the later ADC-monitor source 7
maps through ROM table `0x2e2d74` to selector 1. The complete logical-source table is identical in
3210 v5.01. Values come from `nokia_adc_override` (env `NOKI3210_ADC0..7`, profiles); electrical
scaling and PCB net names remain open.

## The DSP interface

The MCU-to-DSP boundary is shared RAM at `0x10000` plus DSPIF at `0x30000`.
The DSP core remains unemulated, but the shared-memory peer is no longer a
phone-state constant shim:

- **DSP shared RAM `0x10000–0x10fff`:** device-owned backing storage; bootstrap
  ready overrides remain at `0x00..0x04`, `0xe0`, `0xfe` and conditionally
  `0x100`. The device owns MCU/DSP ring indices `0xa4/a6` and `0x1c8/0x1ca`,
  drains service pending word `0xe4`, raises IRQ4, and delivers inbound packets
  through FIQ0.
- **DSPIF `0x30000`:** written at boot (`pc 0x2001a4`) and by the reachable service
  command path (`0x290cf4`; command 4 at `0x29103c`, followed by doorbell byte 2). Stateful-SIM
  runs reach this path with service commands `0x30` and `0x32`; the peer retains the register,
  and service-transport ring/service completion now use independent timers. Their observed
  ring-producer and service-pending triggers remain distinct from command 4.

The wider firmware contains roughly 287 DSPIF references and 444 shared-RAM
base references, concentrated in the GSM-L1/audio layer at
`0x2b6xxx–0x2c8xxx`. The coherent boot now exercises more than the original
service-handshake corner: task 3 serializes DSP work into the TX ring and FIQ0
owns inbound delivery.
The request-driven peer now completes the startup D0 exchange, the organic type-`0x70/0x74`
service-session completion, and the external task-7 service session in one boot. The generic-service
`0x05e8` chain remains a mapped later radio/SAT path, not the current ordinary-SIM prerequisite.

## Current application boundary

The coherent profile exercises CTSI, MBUS, CCONT, LCD, keypad, GenIO/EEPROM,
SIMI and the modeled DSP/external-service boundary. It completes ordinary SIM file
traffic, clears no-SIM, sets SIM enable, scans the keypad, and accepts the
security transaction through normal firmware paths. Conditional power,
reinitialization, and service/test display routes are classified separately.
The remaining application boundary is unattended task-5/MMI context settlement and
idle-window selection, not a hardware acknowledgement currently justified for
synthesis. The default fault-screen oracle and coherent frontier profile
remain separate acceptance baselines; the Nokia 3330 remains the first
portability probe. See `mmi_settlement.md` and `mmi_layer.md`.

## Diagnostic

Use `NOKI3210_TRACE_MAD2_LEDGER=1` for a bounded first-read/first-write trace of
MAD2 I/O. See `mad2_fidelity.md` for the authoritative implementation ledger.
