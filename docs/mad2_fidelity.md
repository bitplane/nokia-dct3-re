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
routing. Keypad, PUP, UIF and other board peripherals remain outside that core;
GENSIO, MBUS and SIMI are separate devices. Timer-1 terminal-count behavior,
MCU-reset extent, the SIMI clock gate and the ARM clock-stop/wake contract are
modeled. The timer divider tree, FIQ8 timing, clock-gate bits without an attached
consumer and several SELECT/UIF banks remain calibrated or mapped-only.
Address-map coverage therefore must not be read as peripheral completeness.

## Address-space boundary

| Boundary | Implementation | Fidelity | Evidence / missing proof |
| --- | --- | --- | --- |
| ARM7TDMI, big-endian | MAME ARM7 core at 13 MHz, suspended by MAD2's clock-stop request and resumed by a routed interrupt | Cross-ROM partial | The task-0 and shutdown callers, auto-clearing request bit, timer wake, external-key wake and save-state behavior are established. The present boot profile does not organically reach the task-0 idle request, and physical clock-transition latency is unknown. |
| Internal boot ROM | one-instruction reset-vector HLE | Undumped HLE boundary | The available `boot_rom` files are uniform erased fill, not dumps. Supported MAD2 profiles execute an ARM branch from reset vector 0 to the established flash entry at `0x200040`; CPU state is not forced. Clock/power/flashing-mode work performed by real mask ROM remains absent. NSE-3 retains `NO_DUMP` ROM3/ROM4 frontiers rather than borrowing this contract. |
| Main RAM | 512 KiB backing store | Inferred | Firmware layout works; physical decode and model-specific sizes need cross-ROM proof. |
| DSP shared RAM | 4 KiB backing store plus peer-owned writes | Partial HLE | MCU RAM tests use ordinary storage; the peer publishes the cross-ROM bootstrap handshake and busy/ready status, but no DSP core executes and physical response latency is unknown. |
| DSPIF `0x30000` | extracted four-byte transport register; command-4 doorbell callback | Cross-ROM partial | Focused v6.00/v5.01 runs cover doorbells, service-pending completion and IRQ4; v6.00 additionally covers complete TX consumption, RX publication and FIQ0. Controller fixtures cover wrap/full/partial handling; fault reporting remains open. |
| MCUIF `0x40000` | retained four-byte configuration register | Mapped latch | Boot writes `6a 0f 61 20` once and never reads it in the coherent run. Decode fields before applying window side effects. |
| ROM2 window | modulo mirror of flash | Inferred | Matches current reads; decode/mirroring needs boot-ROM or second-ROM confirmation. |
| EEPROMSelX | unmapped | Mapped absence | Five supported ROMs contain no executable direct literal-derived consumer of the removed immutable-input alias. Dynamic accesses and future products remain outside coverage; serial EEPROMs attach through PUP. |

## CTSI registers

| Offsets | Current behavior | Fidelity | Required next evidence |
| --- | --- | --- | --- |
| `00` ASIC version | constant `0x40` | Inferred | Compare MAD2 revisions across phone service manuals/ROM checks. |
| `01` MCU reset | bit 2 requests a deferred digital-baseband reset; power bit 0 and retained cause bits are readable | Cross-ROM partial | Both ROMs contain three bit-2 reset sites. A mapped-MMIO fixture proves the reset extent and post-reset value `0x05`; exact rail timing remains unknown. |
| `02` DSP reset | ordinary latch by default; an atomic product wiring contract may select a release mask and running-status readback | Product-specific partial | NHM-2 releases through bit 2 and then observes `0x53`; reset reassertion clears ready bit 4. Recover the physical clock/ready timing and validate another MAD2/DSP combination before generalizing the values. |
| `03` watchdog | nonzero writes reload an eight-bit seconds counter; expiry resets the digital baseband and retains cause `0x03` | Focus-tested partial | The mapped-MMIO fixture proves reload, expiry and reset extent. Determine the physical tick source and exact rail timing. |
| `04..07` timer 1 | current counter wrapping after fixed terminal count `0x7fff`; FIQ5 at the terminal; continues while ARM clock is stopped | Cross-ROM partial | Both ROMs use identical stable-read and FIQ5 race handling, and convert the unsigned terminal-minus-current interval by rounded division by 8 before comparing it with Timer 0. A 16-bit free-running model breaks organic shutdown timing. Absolute input/divider provenance remains calibrated. |
| `08..0b` FIQ/IRQ status and masks | latched bitfields | Focus-tested partial | Timer-0 FIQ4, simultaneous keypad IRQ0/CCONT IRQ2, masked-pending retention and acknowledgement have focused regressions. The overlap fixture briefly gates CPU delivery through MAD2 MMIO so two physical input callbacks compose deterministically. Other source assignments still need independent evidence. |
| `0c` IRQ control | gates CPU lines; bit 5 projects the ninth IRQ pending state and write-only bit 6 acknowledges it | Cross-ROM partial | Both 3210 dispatchers implement the same bit-5/read and `0x40`/write contract. The ninth IRQ source and owner remain unidentified. |
| `0d` clock control | stored gates; bit 1 is an auto-clearing ARM clock-stop request; bit 5 drives the extracted SIMI clock input | Cross-ROM partial | Both ROMs have instruction-equivalent RMW sites: boot sets bits 2--3 once, SIMI code exclusively manages bits 5--6, and the task-0/shutdown helper writes `(old | 0x02) & 0xfe`. Bit 6 is SIMI-owned but its hardware effect is not established; bit 4 has no recovered writer. |
| `0e` external status | read-only three-bit input bank | Five-ROM mapped surface | The conservative census finds reads and zero writes in all five ROMs; the 3210 power/control state machine independently tests bits 0, 1 and 2. Exact PCB pin ownership remains unknown, so the device exposes inputs but synthesizes none. |
| `0f..13` timer 0 | live source divider/counter/compare model with FIQ line 4 (`0x10`) | Focus-tested cross-ROM semantics, calibrated input clock | Both 3210 ROMs program `0xf9`, observe the live divider reach `0xea`, schedule compare=`counter+2`, and acknowledge status bit `0x10`. The register divides source ticks by `data+1`; paired timeout arithmetic proves the resulting Timer-0 interval is compared with `round(Timer1_remaining/8)`. Which documented MAD2 clock feeds the divider remains unproven. |

## PUP, GPIO and serial blocks

| Block | Current behavior | Fidelity | Required next evidence |
| --- | --- | --- | --- |
| FIQ8 `16` | periodic timer when enabled; extended pending/status/mask routing; routed wake from ARM clock-stop | Partial routing/wake, placeholder clock | Register-level tests establish ninth-bit projection, local masking, global delivery, acknowledgement and wake. Identify the source clock and observe an organic countdown overlapping idle sleep. |
| MBUS `18..1a` | extracted byte controller, status, RX/TX attachment and FIQ2/FIQ3 callbacks | Cross-ROM partial, physical character rate | Both 3210 ROMs share initialization and idle behavior; focused RX proves status-bit-5/control-bit-6/FIQ2 delivery. Recover FIQ3 phase/source, collision and error behavior. |
| Buzzer `15/1c/1d/1e` | extracted PUP bit-5 enable and 13 MHz divider drive a MAME beeper; `make verify-buzzer` validates mapped MMIO and `make verify-alarm` validates the organic keypad-to-CCONT-to-ringtone path | Partial hardware | Recover volume/acoustic transfer; keypad tones are DSP/COBBA-owned and the MZT-03C acoustic response is not modeled. |
| Vibrator `15/1b` | extracted PUP bit-4 enable drives the named MAME `vibration` output; `0x1b.bits6..5` select modes `00/20/40/60` and bits `4..0` carry the mode parameter | Partial hardware | Exercise the optional vibra battery pack organically and determine the electrical waveform represented by each mode/parameter combination. |
| GenIO `20/22/24` | extracted register family plus open-drain 24C128 SDA/SCL; `0x22` is an uninterpreted latch | Partial | EEPROM line mapping is firmware-proven. Neither v5.01 nor v6.00 corroborates the proposed backlight bit 6. |
| Key GPIO `28..2b/68..6b/a8..ab` | extracted sparse block with 4x5/5x5 active-low matrices, row direction/drive, mask, cold-boot latch and unmasked press/release transitions on IRQ0; firmware masks all five columns during row changes; unknown neighbors remain plain latches | Cross-ROM contract | Confirm electrical debounce and transition timing on hardware. |
| GENSIO `2c..2e`, `6c..6f` | extracted CCONT/LCD controller; status `0x03` idle/TX-ready and `0x07` CCONT RX-ready; selecting CCONT starts command phase | Cross-ROM partial | Confirm physical busy timing and remaining control bits. |
| SELECT aliases `6f/ad..af/ed..ef` | extracted saved latches; direct static accesses are product-specific: 3210 uses `6f/af`, 3410 writes `ad/ae/ed/ee`, and none resolve in 3310/3330 | Mapped | Identify attached companion devices and alias/decode behavior. |
| SIMI `36..3f` | stateful SIM UART, 16-byte TX FIFO, one-character RX holding state, IIR/control registers and FIQ6 delivery | Partial hardware | ATR, PPS and validated T=0 use the organic 9,600-bit/s path; larger card replies remain private serialization state. TX control `0x04`/`0x00`, live fill and multi-chunk TX-empty progression are modeled. All IIR causes are decoded; WWT and framing/error causes remain unexercised fault paths. |
| UIF control pins `30..33/70..73/b0..b3/f0..f3` | extracted neutral saved-latch device; `0x31.bit1` is power-duty-cycle owned | Mapped | `0x31` has six exhaustive RMW sites and no external-input consumer; map physical pin nets before adding callbacks or side effects. |

## Interrupt ownership

MAD2 owns aggregation and CPU-line assertion. Component devices expose level
callbacks and do not write MAD2 pending registers directly. The extracted
CCONT, DSP peer, MBUS and KBGPIO devices follow this rule. Current line assignments are DSP service IRQ4, keypad IRQ0, CCONT IRQ2,
DSP receive FIQ0, SIMI FIQ6, timer compare FIQ4, and MBUS FIQ2/FIQ3, with line
8 represented by the extended pending bit. The keypad edge latch and CCONT
level are independent sources; acknowledging IRQ0 cannot discard an active
CCONT IRQ2. A physical overlap fixture observes pending status `0x05`; firmware
acknowledges IRQ0 first and leaves IRQ2 pending. This is observed firmware
service order, not a claimed MAD2 priority encoder. A separate fixture proves
that a masked IRQ0 remains pending and is delivered immediately when its mask
and global gate permit it. The v5.01/v6.00 keypad and CCONT firmware paths
support those two IRQ assignments. The remaining assignments still need a
second runtime oracle or hardware documentation.

FIQ8 uses the ninth internal pending bit (`0x100`). Register `0x16.bit1`
projects that pending state, bit 2 masks its CPU delivery, and writing bit 1
acknowledges it. `make verify-mad2-interrupts` exercises this routing while the
periodic source is explicitly enabled through the mapped register. Firmware
helper `0x2a1388` loads a 16-bit software countdown and enables the source;
the FIQ dispatcher decrements it and calls expiry helper `0x2a13e0` at zero.
The three callers supply either `0xffff` or application-owned countdown data,
so they establish programmable-tick semantics but no seconds conversion. The
clock-stop helper does not mask or disable FIQ8. A phase-separated mapped-MMIO
fixture enters sleep with FIQ8 enabled and unmasked, then observes extended
pending bit `0x100` wake the ARM on the next tick. This validates the current
routed-wake behavior, but ordinary boot does not organically overlap an active
countdown with task-0 sleep, so it does not establish whether the physical
source oscillator continues in every low-power state. The source clock remains
calibrated.

The separate ninth IRQ uses the same internal bit but a different register
contract. Both v6.00 (`0x2af3f8..0x2af400`) and v5.01
(`0x2ac824..0x2ac82c`) read `0x0c.bit5` as pending and write `0x40` to
acknowledge it. The old model incorrectly treated bit 6 as a local mask and
mapped acknowledgement to bit 5; the cross-ROM dispatcher decode corrects
both errors. No recovered component asserts IRQ line 8, and no ordinary or
focused run observes it, so this establishes register semantics but not the
physical source, ownership, or delivery timing.

## Extraction gate

The CTSI core was extracted only after satisfying this gate. It does not imply
that the remaining MAD2 peripheral windows are ready to move. Each block needs:

1. reset values and read/write semantics separated from phone scenarios;
2. callbacks for attached components and CPU interrupt lines;
3. no firmware-PC checks in hardware behavior;
4. a 3210 oracle plus at least one second-ROM execution trace; and
5. a focused regression for timers, interrupt masking and serial selection.

GENSIO selection, CCONT IRQ2, timer 0, simultaneous IRQ aggregation,
masked-pending delivery and extended FIQ routing have focused regressions.
The extracted core contains no firmware addresses and exposes callbacks for
CPU routing and attached interrupt sources. Other unresolved windows remain in
the phone until their own contracts meet the same standard. GENSIO now meets
the gate through separate callbacks, two-ROM traces and focused regression.

`make verify-mad2-clocks` provides the paired-ROM boot contract. v6.00 and
v5.01 read reset cause `0x01` and complete the peripheral clock-gate lifecycle
`0x00 -> 0x0c -> 0x2c -> 0x0c`. Bit 5 is the SIMI clock gate, not an ARM sleep
selector; MAD2 now exposes it as a device callback, and the gated controller
freezes delivery while retaining protocol and FIFO state. The callback is
replayed after save-state restore so the attached clock line cannot diverge
from the retained MAD2 latch. Bits 2--3 are enabled together by the common
peripheral initializer and are never cleared by either ROM. Bit 6 occurs only
inside a SIMI variant-control helper; this establishes ownership but not an
effect worth synthesizing. Bit 4 has no writer in the complete direct census.
Ordinary boot does not execute the Timer-1 readers, so their
contract is established by paired static decode and the focused accelerated
controller test rather than the boot trace. It also performs no MAD2-watchdog
write in the bounded run; recovered `0x31` service sites are conditional, not a
periodic startup requirement.

The same paired-ROM census resolves the separate ARM clock-stop path. The
helper at v6.00 `0x2b4e7a` / v5.01 `0x2b222e` clears bit 0 and, for a nonzero
argument, pulses bit 1. Its callers are the task-0 idle-supervisor loop and the
masked shutdown path. The device therefore stores `0x0d` without bit 1 and
suspends the ARM only when the request arrives with no already-routed IRQ/FIQ.
Any subsequently routed IRQ or FIQ resumes it; pending-but-masked sources do
not. This is a clock-stop model, not a complete power-rail or oscillator-startup
model.

`make verify-mad2-sleep` validates this contract on both ROMs with an MMIO
controller fixture: Timer 1 continues while the ARM is suspended, reaches
destination `0x7fff`, raises FIQ5 and wakes the CPU. Each run saves and reloads
while asleep and reproduces the same wake. Separate v6.00 cases prove external
IRQ0 wake from a physical Up-key edge and extended-FIQ wake from an enabled
FIQ8 tick. These fixtures manipulate only mapped controller registers and
physical inputs; they do not write firmware RAM or messages. The current
normalized boot does not naturally execute task 0's stop request, so this gate
proves controller semantics rather than claiming an observed idle duty cycle.

`make verify-mad2-timer1` proves destination `0x7fff`, FIQ5/status `0x020` and
firmware acknowledgement. `make verify-mad2-reset` runs two mapped-MMIO
fixtures. Reset-control bit 2 causes a complete digital-baseband restart and
retains reset cause `0x05`; MAD2-watchdog expiry resets the same domain and
retains cause `0x03`. Flash, EEPROM and CCONT remain outside that domain.
CCONT watchdog expiry uses the same reset extent with its own retained cause.
Exact rail sequencing, oscillator start latency, the SIMI bit-6 effect and the
peripherals fed by the boot-only bit-2/bit-3 pair remain unresolved.

The CCONT power-domain gate establishes one reset boundary separately from the
reset-control register. Watchdog-register data `0x00` holds the digital
baseband off; a later charger edge resets the CPU, MAD2, GENSIO, MBUS, DSPIF,
SIMI and LCD domains together while CCONT and nonvolatile storage persist.
Both 3210 ROMs restart through MAD2 reset value `0x01`, consume CCONT charger
cause `0x04` and reach acting-dead mode. With the scripted four-second power
hold, v6.00 removes the rail at about 14.03 emulated seconds; the charger-wake
fixture therefore applies its external edge at 16 seconds. That is a bounded
firmware-lifecycle observation, not a claimed physical rail-settling time.
Exact electrical sequencing remains unknown.

The MAD2-ledger MAME category (`RUN_VERBOSE=1`) provides a curated register-access pass: at most
one read and one write record per MAD2 byte
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
write-one-clear contract. Runtime assertion, status and acknowledgement all
establish FIQ bit `0x10`; the earlier `0x04` assignment is retained only as a
falsification in the evidence ledger.
The NSE-8/9 system-module documentation establishes a 13 MHz ARM clock and a
32.768 kHz sleep-clock input to MAD2PR1, and states that MCU/DSP clocks stop
in sleep while the sleep clock remains active. It does not document the
internal timer divider tree. The paired ROMs prove the functional relation
instead:
Timer 0 uses programmed divider `0xf9` (`250` source ticks per counter tick),
while timeout code compares its remaining interval with
`round(Timer1_remaining / 8)`. The current `33,055 Hz` Timer-0 source and
`1,057 Hz` Timer-1 rate preserve that observed relation within calibration
error and keep the boot lifecycle coherent. Applying 32.768 kHz directly to
Timer 1 and the ratio-derived 1.024 MHz to Timer 0 passes the isolated timer and
sleep gates but drives coherent firmware into repeated terminal reset reason
`0x6c` at about four seconds. This disproves the direct-input assignment, not
the external crystal: at least one undocumented internal divider remains.
Using the 13 MHz ARM clock directly as Timer 0's input is independently
disproven because it starves task 2 and expires CCONT at 49 seconds. No
scheduler wake or firmware state is synthesized;
The former process-environment clock overrides have been removed; conformance
fixtures now exercise the configured product clocks through mapped registers.
Timer 1 runs independently at the retained `1,057 Hz` calibration, reaches
terminal count `0x7fff`, raises FIQ5 and wraps to zero on the next tick.
Both timers remain active while the ARM clock is stopped, consistent with the
sleep-clock domain; this establishes domain ownership but not the exact divider
ratios. Catch-up remains disabled, and none of this establishes other MAD2
revisions' clocks.

No persistent 13 MHz/32.768 kHz selector is visible in the paired ROMs: bit 1
is a one-shot stop request, not a retained domain-select bit. The remaining
clock work is therefore limited to the exact divider tree and transition
latency, the physical source of FIQ8, and attached consumers for the other gate
bits. Additional wake-source claims require either an exercised routed source
or board-level evidence; the implementation deliberately treats MAD2's routed
IRQ/FIQ output as the wake boundary rather than maintaining an invented source
allow-list.

The same target performs a scheduled save/load round trip. Main RAM, MAD2
registers, IRQ/FIQ pending state, timer counters/divider/compare latch, keypad
and CCONT aggregation, power-on state and GENSIO state are registered. Post-load
reconstructs the CPU interrupt lines without manufacturing a new source.

The MAD2-interrupt MAME category (`RUN_VERBOSE=1`) records source assertions and level
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

The GENSIO MAME category (`RUN_VERBOSE=1`) logs value-level CCONT/LCD and SELECT-bank accesses
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
latches charger source bit 3 and proves MAD2 IRQ2 routing plus firmware-owned
CCONT status read, write-one-clear acknowledgement, cleared follow-up read and
final deassertion. The GENSIO
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
