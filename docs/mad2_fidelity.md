# MAD2 fidelity ledger

This ledger describes what the driver implements, the evidence behind it, and
the next observation needed before behavior is promoted into a reusable MAD2
device. It deliberately distinguishes a working boot approximation from a
faithful hardware contract.

Confidence labels:

- **Observed:** firmware accesses and resulting state transitions have been
  traced or established from disassembly.
- **Inferred:** behavior fits the firmware but lacks independent hardware
  documentation or a second ROM.
- **Placeholder:** a constant, shortcut or incomplete peer used to keep the
  firmware running.

## Address-space boundary

| Boundary | Implementation | Fidelity | Evidence / missing proof |
| --- | --- | --- | --- |
| ARM7TDMI, big-endian | MAME ARM7 core at 13 MHz | Observed/inferred | Core and entry point work; clock division and sleep clock are not hardware-validated. |
| Internal boot ROM | mapped at low address | Placeholder | Execution is redirected to flash entry; the real reset/boot-ROM sequence is bypassed. |
| Main RAM | 512 KiB backing store | Inferred | Firmware layout works; physical decode and model-specific sizes need cross-ROM proof. |
| DSP shared RAM | 4 KiB backing store plus special reads | Placeholder | Several ready/status offsets are synthesized; no DSP core executes. |
| DSPIF `0x30000` | reads zero, writes discarded | Placeholder | Firmware touches it; command/status semantics are unknown. |
| MCUIF `0x40000` | reads zero, writes discarded | Placeholder | Memory-window configuration semantics are unknown. |
| ROM2 window | modulo mirror of flash | Inferred | Matches current reads; decode/mirroring needs boot-ROM or second-ROM confirmation. |
| EEPROM parallel window | read-only alias of input region | Placeholder | Serial 24C128 is faithful at GenIO; relationship of the parallel window to EEPROM hardware is unproven. |

## CTSI registers

| Offsets | Current behavior | Fidelity | Required next evidence |
| --- | --- | --- | --- |
| `00` ASIC version | constant `0x40` | Inferred | Compare MAD2 revisions across phone service manuals/ROM checks. |
| `01` MCU reset | stores bits; optional firmware-specific soft-reset model | Partial | Establish reset bit effects and remove PC-oriented reset options. |
| `02` DSP reset | stored only | Placeholder | Observe DSP reset/run handshake. |
| `03` watchdog | decrement/reset loop | Inferred | Determine tick source, reload semantics and reset domain. |
| `04..07` sleep timer | software counter and synthetic destination | Placeholder | Establish 32.768 kHz counter width, latch and compare behavior. |
| `08..0b` FIQ/IRQ status and masks | latched bitfields | Partial | Verify write-one-clear behavior, priority and extended-line routing. |
| `0c` IRQ control | gates CPU lines; bit mapping inferred | Partial | Cross-check enable/mask polarity and reset value. |
| `0d` clock control | stored only | Placeholder | Map clock domains and sleep transitions. |
| `0e` interrupt trigger | backing-register read | Placeholder | Establish whether this is pending, trigger, or vector/status. |
| `0f..13` timer 0 | divider/counter/compare model | Partial | Verify input clock, divider formula, compare edge and reload behavior. |

## PUP, GPIO and serial blocks

| Block | Current behavior | Fidelity | Required next evidence |
| --- | --- | --- | --- |
| FIQ8 `15/16` | periodic timer when enabled | Placeholder | Identify source clock and acknowledgement semantics. |
| MBUS `18..1a` | byte/status state plus scheduled FIQ | Partial | Capture complete request/reply framing and collision/timing behavior. |
| Vibrator/buzzer `1b/1c/1e` | register storage only | Placeholder | Connect outputs and derive divider/volume mapping. |
| GenIO `20/24` | register storage plus open-drain 24C128 SDA/SCL | Partial | EEPROM line mapping is firmware-proven; other pins and electrical behavior remain unknown. |
| Key GPIO `28..2b` | matrix scan and IRQ approximation | Partial | Implement row/column direction and interrupt masks instead of unconditional key IRQ. |
| GENSIO `2c..2e`, `6c..6f` | CCONT and LCD endpoints; ready=`0x07` | Partial | Model transaction state/status and SELECT routing rather than a permanent-ready value. |
| SELECT2/3 aliases `ad..af`, `ed..ef` | backing registers | Placeholder | Identify attached companion devices and alias/decode behavior. |
| SIMI `36..3f` | configuration registers plus diagnostic ATR FIFO | Placeholder | Normal reception is DSP/service-message mediated; register FIFO model is not the organic path. |
| UIF control pins `31..33`, `70..f3` | mostly backing registers | Placeholder | Map pin functions from service schematics and live access sequences. |

## Interrupt ownership

MAD2 owns aggregation and CPU-line assertion. Component devices should expose
level callbacks and must not write MAD2 pending registers directly. The
extracted CCONT follows this rule; DSP, MBUS and keypad currently remain inside
the phone state. Known line assignments in the current model are DSP service 4,
SIM probe 5, CCONT/keypad 6, with line 8 represented by the extended pending
bit. Only CCONT line 6 has a component boundary; the other assignments need
hardware confirmation.

## Extraction gate

Do not extract a general MAD2 device by copying the present switch statement.
Extraction is justified when each block has:

1. reset values and read/write semantics separated from phone scenarios;
2. callbacks for attached components and CPU interrupt lines;
3. no firmware-PC checks in hardware behavior;
4. a 3210 oracle plus at least one second-ROM execution trace; and
5. a focused regression for timers, interrupt masking and serial selection.

Until the second ROM is normalized, `NOKI3210_TRACE_MAD2_LEDGER=1` provides the
curated 3210 evidence pass: at most one read and one write record per MAD2 byte
offset per reset, including value, previous value for writes, PC, time and the
register description. It intentionally excludes RAM and firmware hooks. The
old `TRACE_MMIO` documentation predates the instrumentation cleanup and is not
currently implemented.

Validation run (3210 v6.00, three emulated seconds, 2026-07-11): 86 bounded
records were emitted, comprising 34 first reads and 52 first writes. The trace
captured reset/UIF setup from `0x200068`, GENSIO/LCD initialization, CCONT
selection, MBUS status, interrupt activity, watchdog service and SIMI setup.
