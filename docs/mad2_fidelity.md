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

The implementation is not one uniform MAD2 model. Timer 0, interrupt
aggregation, keypad, GenIO/GENSIO and SIMI have derived contracts of differing
depth; timer 1, FIQ8, reset/clock fields, audio outputs and several SELECT/UIF
banks remain calibrated counters or backing latches. The address-map coverage
therefore must not be read as peripheral completeness.

## Address-space boundary

| Boundary | Implementation | Fidelity | Evidence / missing proof |
| --- | --- | --- | --- |
| ARM7TDMI, big-endian | MAME ARM7 core at 13 MHz | Observed/inferred | Core and entry point work; clock division and sleep clock are not hardware-validated. |
| Internal boot ROM | mapped at low address | Placeholder | Execution is redirected to flash entry; the real reset/boot-ROM sequence is bypassed. |
| Main RAM | 512 KiB backing store | Inferred | Firmware layout works; physical decode and model-specific sizes need cross-ROM proof. |
| DSP shared RAM | 4 KiB backing store plus special reads | Placeholder | Several ready/status offsets are synthesized; no DSP core executes. |
| DSPIF `0x30000` | four-byte peer-device register; command-4 doorbell observed | Partial | Coherent boot emits 26 command-4 strobes after initialization. Service-transport ring and shared-service timers are split, but command 4 does not accompany every ring commit and cannot replace both observed work triggers. |
| MCUIF `0x40000` | retained four-byte configuration register | Mapped latch | Boot writes `6a 0f 61 20` once and never reads it in the coherent run. Decode fields before applying window side effects. |
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
| `0f..13` timer 0 | live divider/counter/compare model with FIQ4 | Cross-ROM semantics, calibrated clock | Both 3210 ROMs program `0xf9`, observe the live divider reach `0xea`, schedule compare=`counter+2`, and acknowledge FIQ4 identically. Establish the input oscillator/divider formula and remove the frequency/catch-up knobs. |

## PUP, GPIO and serial blocks

| Block | Current behavior | Fidelity | Required next evidence |
| --- | --- | --- | --- |
| FIQ8 `15/16` | periodic timer when enabled | Placeholder | Identify source clock and acknowledgement semantics. |
| MBUS `18..1a` | byte/status state plus scheduled FIQ | Partial | Capture complete request/reply framing and collision/timing behavior. |
| Vibrator/buzzer `1b/1c/1e` | register storage only | Placeholder | Connect outputs and derive divider/volume mapping. |
| GenIO `20/24` | register storage plus open-drain 24C128 SDA/SCL | Partial | EEPROM line mapping is firmware-proven; other pins and electrical behavior remain unknown. |
| Key GPIO `28/2a/6b/a8` | 4x5 active-low matrix, ROM-derived 3210 wiring, row direction/drive, physical press/release edges on IRQ0 | Cross-ROM contract | Confirm column-mask and electrical debounce details on hardware. |
| GENSIO `2c..2e`, `6c..6f` | CCONT and LCD endpoints; status `0x03` idle/TX-ready and `0x07` CCONT RX-ready; selecting CCONT starts command phase | Partial | Confirm busy timing, remaining control bits and SELECT routing. |
| SELECT2/3 aliases `ad..af`, `ed..ef` | backing registers | Placeholder | Identify attached companion devices and alias/decode behavior. |
| SIMI `36..3f` | stateful SIM UART, 16-byte TX FIFO, RX FIFO, IIR/control registers and FIQ6 delivery | Partial hardware | ATR, PPS and validated T=0 use the organic path; TX control `0x04`/`0x00`, live fill and multi-chunk TX-empty progression are modeled. All IIR causes are decoded; WWT and framing/error causes remain unexercised fault paths. |
| UIF control pins `31..33`, `70..f3` | backing latches; `0x31.bit1` is power-duty-cycle owned | Partial | `0x31` has six exhaustive RMW sites and no external-input consumer; map physical pin nets and the remaining banks from schematics. |

## Interrupt ownership

MAD2 owns aggregation and CPU-line assertion. Component devices expose level
callbacks and do not write MAD2 pending registers directly. The extracted
CCONT and DSP peer follow this rule; MBUS and keypad remain inside the phone
state. Current line assignments are DSP service IRQ4, keypad IRQ0, CCONT IRQ6,
DSP receive FIQ0, SIMI FIQ6, timer compare FIQ4, and MBUS FIQ2/FIQ3, with line
8 represented by the extended pending bit. The keypad edge latch and CCONT
level are independent sources; acknowledging IRQ0 cannot discard an active
CCONT IRQ6. The v5.01/v6.00 keypad and CCONT firmware paths support those two
IRQ assignments. The remaining assignments still need a second runtime oracle
or hardware documentation.

## Extraction gate

Do not extract a general MAD2 device by copying the present switch statement.
Extraction is justified when each block has:

1. reset values and read/write semantics separated from phone scenarios;
2. callbacks for attached components and CPU interrupt lines;
3. no firmware-PC checks in hardware behavior;
4. a 3210 oracle plus at least one second-ROM execution trace; and
5. a focused regression for timers, interrupt masking and serial selection.

Those focused regressions do not yet exist. Current confidence comes from the
coherent integration oracle, the bounded access census and selected v5.01
alignment; this is sufficient to retain the block behavior, but not to promote
the phone-owned switch statement into a reusable MAD2 device.

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
FIQ4, and acknowledge it through status offset `0x08`. This validates the live
divider, coherent 16-bit counter read, compare and write-one-clear contract.
It does not reveal the physical input clock. The coherent profile's 20 MHz
base and catch-up behavior therefore remain explicit calibration debt rather
than being promoted into the device contract.

The widened ledger observes one MCUIF dword write (`6a 0f 61 20`) from boot and
no MCUIF reads. DSPIF receives its initial zero halfword and then 26 command-4
writes from `0x29103c` and `0x290778`. `nokia_dsp_peer_device` now owns the
register. Service-transport ring parsing and delayed shared-service completion now use
independent timers. Repeating the command-4-only experiment after that split
still did not preserve the frontier: not every service-transport producer commit is
paired with command 4. The established ring-producer and service-pending
triggers therefore remain.

## GENSIO focused trace

`NOKI3210_TRACE_GENSIO=1` logs value-level accesses to `0x2c/0x2d/0x6c/0x6d`
and is capped at 20,000 records per reset. Firmware disassembly and runtime
tracing establish:

- control `0x21` selects the LCD path;
- control `0x25` selects the CCONT path;
- status bit 0 is endpoint-write ready;
- status bit 1 is controller idle/available; and
- status bit 2 indicates CCONT read data available.

The causal status model returns `0x03` after selection, `0x07` after a CCONT
command byte and `0x03` after consuming `0x6c`. A one-second trace produced
1,978 records and exercised both values; the byte-exact 20-second oracle was
unchanged. An earlier interpretation that firmware polled status bit 3 was
incorrect: Thumb `LSRS #3` exposes original bit 2 through carry.

The v5.01 firmware repeats the same polling and transaction grammar
instruction-for-instruction. Endpoint `0x25` selection is the observable
transaction boundary in both ROMs. ADC result delivery remains synchronous in
the model because neither firmware exposes a conversion-complete interrupt or
a minimum busy duration that could be implemented without calibration.
