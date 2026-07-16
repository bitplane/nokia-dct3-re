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

The extracted `nokia_mad2_device` owns the CTSI core at offsets `0x00..0x16`:
reset/clock/watchdog latches, timer state, interrupt pending/masks and CPU-line
routing. Keypad, GenIO and other board peripherals remain outside that core;
GENSIO, MBUS and SIMI are separate devices. Timer-1 destination reads, FIQ8
timing, reset/clock effects, audio outputs and several SELECT/UIF banks remain
calibrated counters or backing latches. Address-map coverage therefore must not
be read as peripheral completeness.

## Address-space boundary

| Boundary | Implementation | Fidelity | Evidence / missing proof |
| --- | --- | --- | --- |
| ARM7TDMI, big-endian | MAME ARM7 core at 13 MHz | Observed/inferred | Core and entry point work; clock division and sleep clock are not hardware-validated. |
| Internal boot ROM | mapped at low address | Placeholder | Execution is redirected to flash entry; the real reset/boot-ROM sequence is bypassed. |
| Main RAM | 512 KiB backing store | Inferred | Firmware layout works; physical decode and model-specific sizes need cross-ROM proof. |
| DSP shared RAM | 4 KiB backing store plus special reads | Placeholder | Several ready/status offsets are synthesized; no DSP core executes. |
| DSPIF `0x30000` | extracted four-byte transport register; command-4 doorbell callback | Cross-ROM partial | Focused v6.00/v5.01 runs cover doorbells, service-pending completion and IRQ4; v6.00 additionally covers complete TX consumption, RX publication and FIQ0. Controller fixtures cover wrap/full/partial handling; fault reporting remains open. |
| MCUIF `0x40000` | retained four-byte configuration register | Mapped latch | Boot writes `6a 0f 61 20` once and never reads it in the coherent run. Decode fields before applying window side effects. |
| ROM2 window | modulo mirror of flash | Inferred | Matches current reads; decode/mirroring needs boot-ROM or second-ROM confirmation. |
| EEPROM parallel window | read-only alias of input region | Placeholder | Serial 24C128 is faithful at GenIO; relationship of the parallel window to EEPROM hardware is unproven. |

## CTSI registers

| Offsets | Current behavior | Fidelity | Required next evidence |
| --- | --- | --- | --- |
| `00` ASIC version | constant `0x40` | Inferred | Compare MAD2 revisions across phone service manuals/ROM checks. |
| `01` MCU reset | extracted reset-cause latch; optional phone-level soft-reset policy | Partial | Establish reset domains and remove PC-oriented reset options. |
| `02` DSP reset | stored only | Placeholder | Observe DSP reset/run handshake. |
| `03` watchdog | decrement/reset loop | Inferred | Determine tick source, reload semantics and reset domain. |
| `04..07` timer 1 | 33,055 Hz free-running counter; FIQ5 on 16-bit wrap; stored destination latch | Partial | Overflow timing composes with the menu oracle. Neither ROM accesses the destination latch during boot; establish its compare and reset-enable behavior before adding side effects. |
| `08..0b` FIQ/IRQ status and masks | latched bitfields | Focus-tested partial | Timer-0 FIQ4, simultaneous keypad IRQ0/CCONT IRQ6, masked-pending retention and acknowledgement have focused regressions. Other source assignments still need independent evidence. |
| `0c` IRQ control | gates CPU lines; bit mapping inferred | Partial | Cross-check enable/mask polarity and reset value. |
| `0d` clock control | extracted stored latch | Observed writes, unknown effects | Both ROMs write `0x0c` then `0x2c`; map clock domains and sleep transitions before adding side effects. |
| `0e` interrupt trigger | backing-register read | Placeholder | Establish whether this is pending, trigger, or vector/status. |
| `0f..13` timer 0 | live divider/counter/compare model with FIQ line 4 (`0x10`) | Focus-tested cross-ROM semantics, calibrated input clock | Both 3210 ROMs program `0xf9`, observe the live divider reach `0xea`, schedule compare=`counter+2`, and acknowledge status bit `0x10`. Recover the physical input/divider relationship without regressing the boot lifecycle. |

## PUP, GPIO and serial blocks

| Block | Current behavior | Fidelity | Required next evidence |
| --- | --- | --- | --- |
| FIQ8 `15/16` | periodic timer when enabled; extended pending/status/mask routing | Partial routing, placeholder clock | Register-level tests establish ninth-bit projection, local masking, global delivery and acknowledgement. Identify the source clock and physical timer semantics. |
| MBUS `18..1a` | extracted byte controller, status, RX/TX attachment and FIQ2/FIQ3 callbacks | Cross-ROM partial, physical character rate | Both 3210 ROMs share initialization and idle behavior; focused RX proves status-bit-5/control-bit-6/FIQ2 delivery. Recover FIQ3 phase/source, collision and error behavior. |
| Buzzer `15/1c/1d/1e` | PUP bit-5 enable and 13 MHz divider drive a MAME beeper | Partial hardware | Validate volume transfer and ringtone paths; the MZT-03C acoustic response is not modeled. |
| Vibrator `15/1b` | register storage only | Placeholder | Connect an output and recover frequency/mode semantics. |
| GenIO `20/24` | register storage plus open-drain 24C128 SDA/SCL | Partial | EEPROM line mapping is firmware-proven; other pins and electrical behavior remain unknown. |
| Key GPIO `28/2a/6b/a8` | 4x5 active-low matrix, ROM-derived 3210 wiring, row direction/drive, physical press/release edges on IRQ0 | Cross-ROM contract | Confirm column-mask and electrical debounce details on hardware. |
| GENSIO `2c..2e`, `6c..6f` | extracted CCONT/LCD controller; status `0x03` idle/TX-ready and `0x07` CCONT RX-ready; selecting CCONT starts command phase | Cross-ROM partial | Confirm physical busy timing and remaining control bits. |
| SELECT2/3 aliases `ad..af`, `ed..ef` | extracted saved latches; cross-ROM startup and RMW behavior mapped | Mapped | Identify attached companion devices and alias/decode behavior. |
| SIMI `36..3f` | stateful SIM UART, 16-byte TX FIFO, RX FIFO, IIR/control registers and FIQ6 delivery | Partial hardware | ATR, PPS and validated T=0 use the organic path; TX control `0x04`/`0x00`, live fill and multi-chunk TX-empty progression are modeled. All IIR causes are decoded; WWT and framing/error causes remain unexercised fault paths. |
| UIF control pins `31..33`, `70..f3` | backing latches; `0x31.bit1` is power-duty-cycle owned | Partial | `0x31` has six exhaustive RMW sites and no external-input consumer; map physical pin nets and the remaining banks from schematics. |

## Interrupt ownership

MAD2 owns aggregation and CPU-line assertion. Component devices expose level
callbacks and do not write MAD2 pending registers directly. The extracted
CCONT, DSP peer and MBUS devices follow this rule; keypad remains inside the
phone state. Current line assignments are DSP service IRQ4, keypad IRQ0, CCONT IRQ6,
DSP receive FIQ0, SIMI FIQ6, timer compare FIQ4, and MBUS FIQ2/FIQ3, with line
8 represented by the extended pending bit. The keypad edge latch and CCONT
level are independent sources; acknowledging IRQ0 cannot discard an active
CCONT IRQ6. A physical overlap fixture observes pending status `0x41`; firmware
acknowledges IRQ0 first and leaves IRQ6 pending. This is observed firmware
service order, not a claimed MAD2 priority encoder. A separate fixture proves
that a masked IRQ0 remains pending and is delivered immediately when its mask
and global gate permit it. The v5.01/v6.00 keypad and CCONT firmware paths
support those two IRQ assignments. The remaining assignments still need a
second runtime oracle or hardware documentation.

FIQ8 uses the ninth internal pending bit (`0x100`). Register `0x16.bit1`
projects that pending state, bit 2 masks its CPU delivery, and writing bit 1
acknowledges it. `make verify-mad2-interrupts` exercises this routing while the
periodic source is explicitly enabled through the mapped register. The source
clock remains calibrated and the extended IRQ projection at `0x0c.bit5` has no
legitimate modeled producer, so extended IRQ remains an unvalidated decode.

## Extraction gate

The CTSI core was extracted only after satisfying this gate. It does not imply
that the remaining MAD2 peripheral windows are ready to move. Each block needs:

1. reset values and read/write semantics separated from phone scenarios;
2. callbacks for attached components and CPU interrupt lines;
3. no firmware-PC checks in hardware behavior;
4. a 3210 oracle plus at least one second-ROM execution trace; and
5. a focused regression for timers, interrupt masking and serial selection.

GENSIO selection, CCONT IRQ6, timer 0, simultaneous IRQ aggregation,
masked-pending delivery and extended FIQ routing have focused regressions.
The extracted core contains no firmware addresses and exposes callbacks for
CPU routing and attached interrupt sources. Other unresolved windows remain in
the phone until their own contracts meet the same standard. GENSIO now meets
the gate through separate callbacks, two-ROM traces and focused regression.

`make verify-mad2-clocks` adds a two-ROM boot contract for the remaining core
latches. v6.00 and v5.01 each read reset cause four times, write it once from
`0x01` to `0x05`, service watchdog offset `0x03` fifteen times with `0x31`, and
write clock control `0x00 -> 0x0c -> 0x2c`. Neither accesses timer-1 offsets
`0x04..0x07`; that is a negative register-access assertion, not evidence that
the free-running Timer-1 overflow source is inactive. The sequence does not
establish reset side effects, watchdog frequency, clock-domain behavior or the
destination-latch semantics.

Until the second ROM is normalized, `NOKI3210_TRACE_MAD2_LEDGER=1` provides the
curated 3210 evidence pass: at most one read and one write record per MAD2 byte
offset per reset, including value, previous value for writes, PC, time and the
register description. It intentionally excludes RAM and firmware hooks.

Validation run (3210 v6.00 coherent profile, eight emulated seconds,
2026-07-15): 104 bounded records were emitted, comprising 44 first reads and 60
first writes across IO, DSPIF and MCUIF. `tools/mad2_access_census.py` checks the one-record-per-direction
contract and produces JSON plus Markdown through `make mad2-census
MAD2_LOG=...`. The trace captures reset/UIF setup from `0x200068`, GENSIO/LCD
initialization, CCONT selection, timer-0 setup, MBUS status, interrupt activity,
watchdog service and the complete boot SIMI setup. Offset `0x31` is classified
as CTRL-I/O signal register 1.

The `0x31` literal census is exhaustive: six loads, all followed by byte
read-modify-write operations on bit 1. Five are in the power subsystem's
duty-cycle update paths (`0x21c506`, `0x21c57a`, `0x21c5b8`, `0x21c724`, and
`0x21c738`); initialization at `0x2a6664` sets the same bit. No site consumes
an external pin level or associates an interrupt. The v5.01 initializer aligns
at `0x2a3b26`. A stored output latch is therefore the complete currently
observable contract, while the PCB signal name remains deliberately unknown.

Timer-0 initialization at v6.00 `0x2aa934` aligns uniquely and byte-for-byte
with v5.01 `0x2a75c4`. Both routines write divider `0xf9`, wait until the live
divider is at most `0xea`, program a compare two counter ticks ahead, wait for
FIQ line 4/status bit `0x10`, and acknowledge it through status offset `0x08`.
`make verify-mad2` checks that complete lifecycle from a targeted organic trace.
This validates the live divider, coherent 16-bit counter read, compare and
write-one-clear contract. The previous unused `0x04` timer constant and matching
pending guard were wrong; runtime assertion/status/acknowledgement all establish
`0x10`.
The NSE-8/9 system-module clocking scheme establishes a 13 MHz ARM system
clock at MAD2PR1, but it does not by itself establish Timer 0's input. With the
working 13 MHz timer input and divider `0xf9`, the modeled counter advances at
about 52 kHz; a focused trace shows no task-0 flash execution after second 2
and CCONT expiry at exactly 49 seconds. With a 33,055 Hz timer input before the
same divider, task 0 instead dominates steady state and the phone does not
reset during 55 seconds, but the coherent SIM/UI lifecycle does not complete.
The product profile therefore retains 13 MHz as an explicit calibrated Timer-0
value. Timer 1 now runs independently at 33,055 Hz, wraps at 16 bits and raises
FIQ5; that composition reproduces the interactive menu but does not restore the
missing periodic CCONT watchdog service. Catch-up remains disabled, and this
does not establish other MAD2 revisions' clocks.

The same target performs a scheduled save/load round trip. Main RAM, MAD2
registers, IRQ/FIQ pending state, timer counters/divider/compare latch, keypad
and CCONT aggregation, power-on state and GENSIO state are registered. Post-load
reconstructs the CPU interrupt lines without manufacturing a new source.

`NOKI3210_TRACE_MAD2_INTERRUPTS=1` records source assertions and level
recomputation, pending masks, write-one-clear acknowledgements, relevant
register accesses and only actual CPU-line transitions. It is capped at 4,096
records per reset. The paired conformance target runs three scenarios through
physical inputs or mapped MAD2 registers: overlapping keypad/charger sources,
IRQ0 held pending behind its mask/global gate, and FIQ8 held behind its local
mask/global gate. These are controller tests, not firmware-state fixtures.

The widened ledger observes one MCUIF dword write (`6a 0f 61 20`) from boot and
no MCUIF reads. DSPIF receives its initial zero halfword and then 26 command-4
writes from `0x29103c` and `0x290778`. `nokia_dspif_device` now owns the
register. Service-transport ring parsing and delayed shared-service completion now use
independent timers. Repeating the command-4-only experiment after that split
still did not preserve the frontier: not every service-transport producer commit is
paired with command 4. The established ring-producer and service-pending
triggers therefore remain.

## GENSIO focused trace

`NOKI3210_TRACE_GENSIO=1` logs value-level CCONT/LCD and SELECT-bank accesses
and is capped at 20,000 records per reset. Firmware disassembly and runtime
tracing establish:

- control `0x21` selects the LCD path;
- control `0x25` selects the CCONT path;
- status bit 0 is endpoint-write ready;
- status bit 1 is controller idle/available; and
- status bit 2 indicates CCONT read data available.

The causal status model returns `0x03` after selection, `0x07` after a CCONT
command byte and `0x03` after consuming `0x6c`. A one-second trace produced
both complete read and write transactions and exercised both values.
`make verify-ccont` validates the phase/status grammar and all eight configured
ADC selectors from this bounded organic trace. A second physical-input fixture
latches charger source bit 3 and proves firmware-visible status, MAD2 IRQ6
routing, write-one-clear acknowledgement and final deassertion. The GENSIO
endpoint/status and SELECT state is registered alongside the CCONT device
state. The byte-exact 20-second oracle was unchanged. An
earlier interpretation that firmware polled status bit 3 was
incorrect: Thumb `LSRS #3` exposes original bit 2 through carry.

The v5.01 firmware repeats the same polling and transaction grammar
instruction-for-instruction. Endpoint `0x25` selection is the observable
transaction boundary in both ROMs. ADC result delivery remains synchronous in
the model because neither firmware exposes a conversion-complete interrupt or
a minimum busy duration that could be implemented without calibration.

`make verify-gensio` checks both ROMs' endpoint traffic and SELECT latches.
They initialize `ad/ed/ae/ee` to `c4/21/20/80`, clear `af/6f/ef`, later mask
`af`, and set `6f.bit0` through instruction-equivalent routines. This does not
identify the components attached to those lines.
