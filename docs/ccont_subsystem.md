# CCONT subsystem

CCONT is the DCT3 power-management ASIC. It monitors battery and charger
signals, provides ADC and RTC registers, controls the power/watchdog sequence,
and reports interrupt causes to MAD2. CHAPS performs the physical charging;
COBBA handles audio and RF conversion.

This document records the current hardware contract and unresolved questions.
Historical experiments have been removed except where a negative conclusion
prevents a likely wrong turn.

## Confidence

- **Proven:** firmware behavior and an independent hardware source agree.
- **Inferred:** supported by firmware disassembly/runtime behavior only.
- **Unknown:** current emulation behavior is a placeholder.

## Device boundary

`driver/nokia_ccont.{h,cpp}` implements a MAME device that owns:

- alternating GENSIO command/data framing;
- the 16-byte CCONT register file;
- sampling configured ADC source values;
- RTC reads from the emulated machine clock;
- interrupt status, mask and IRQ output;
- watchdog countdown; and
- a power output asserted by the watchdog-register power-off command.

The phone driver owns the battery/charger scenario, supplies the eight ADC
inputs, and routes the CCONT IRQ into MAD2 line 6. This is the intended split:
CCONT produces signals; MAD2 aggregates interrupts; firmware owns startup and
power policy.

## GENSIO transport

CCONT is selected through MAD2 GENSIO:

| MAD2 offset | Direction | Current meaning |
| --- | --- | --- |
| `0x2c` | write | alternating CCONT command and data bytes |
| `0x6c` | read | CCONT register data |
| `0x2d` | write | endpoint/mode selection; `0x21` is used for LCD and `0x25` for CCONT |
| `0x6d` | read | status: idle/TX-ready `0x03`, CCONT receive-ready `0x07` |

The command selects `address = (command >> 3) & 0x0f`; bit `0x04` selects a
read. Firmware helpers include `ccont_reg_read` at `0x2afb44` and the write path
at `0x2afa74`. The v5.01 equivalents are `0x2acf70` and `0x2acea0` and are
instruction-equivalent apart from relocated calls and literals. The older
`0x2b5ae4` label was incorrect; that address is an IPC message-send wrapper,
not CCONT transport.

Firmware establishes three status predicates: bit 0 is polled before endpoint
writes, bit 1 by controller-availability helper `0x2b642a`, and bit 2 after a
CCONT read command. MAD2 starts at `0x03`, resets to `0x03` on endpoint
selection, sets bit 2 after a CCONT transfer byte, and clears it when `0x6c` is
consumed. Both ROMs select endpoint `0x25` before every register helper and
send a command byte first. Endpoint selection therefore resets the device to
command phase. Transfers remain synchronous because neither ROM exposes a
conversion-complete interrupt or a firmware-visible minimum delay; no invented
busy interval is represented.

## Register map

| Register | Role | Current fidelity |
| --- | --- | --- |
| `0x0` | ADC control/channel request | Inferred; conversion currently completes immediately. |
| `0x1` | PWM/charger control | Inferred register storage. |
| `0x2` | ADC result LSB | Proven role. |
| `0x3` | ADC result MSB/status | Proven role; upper returned bits are inferred. |
| `0x4` | charger control/status | Unknown storage. |
| `0x5` | watchdog and power control | Proven role; reload command semantics inferred from firmware. |
| `0x6` | RTC enable/control | Inferred storage. |
| `0x7..0xa` | RTC second/minute/hour/day | Proven role; binary encoding is not independently confirmed. |
| `0xb..0xd` | RTC alarm/calibration | Inferred storage. |
| `0xe` | interrupt/reset status | Proven role. Cold power-key reset latches PWRONX as bit 1 (`0x02`); upper bits `0xf8` are interrupt sources. Bit 0 remains an opt-in presence overlay. |
| `0xf` | interrupt mask | Strongly inferred from firmware ISR behavior. |

Writing interrupt-status bits clears them. The IRQ output is active when
`status & ~mask & 0xf8` is nonzero; the low reset/presence bits do not assert
it. MAD2 owns the resulting CPU interrupt assertion.

The complete v6.00 CCONT helper block `0x2b061c..0x2b0a16` aligns with v5.01
`0x2ada48..0x2ade42`. This covers register reads/writes, RTC helpers, watchdog
commands, power helpers and the IRQ acknowledge loop. The ROM shadow/default
tables at v6.00 `0x2e2da8` and v5.01 `0x2d777c` are byte-identical. These are
same-product controls for the register grammar, not independent chip
documentation; command meanings that firmware never observes remain inferred.

## ADC selectors

The 3210 v6.00 firmware directly calls `0x2b52cc` with selector `0` in boot
battery reader `0x2a84b0`, while the later ADC monitor maps logical source 7
through ROM table `0x2e2d74` to selector 1. Those are distinct paths; the
electrical signal attached to each selector remains under validation.

The eight-byte route table is byte-identical in both firmware versions:
v6.00 `0x2e2d74` and v5.01 `0x2d7770` contain
`04 00 06 05 03 07 01 02`. This proves stable logical-source routing across
the two 3210 builds, but it does not name the PCB nets behind the selectors.

| Selector | Board-level interpretation |
| ---: | --- |
| 0 | unresolved; consumed by the early battery reader |
| 1 | unresolved; BSI is a corroborated hypothesis |
| 2 | unresolved |
| 3 | unresolved; battery-type/BSI is a generic DCT3 hypothesis |
| 4 | temperature-like charging input; PCB net unresolved |
| 5 | unresolved; consumed by charger-detection firmware |
| 6 | unresolved |
| 7 | unresolved |

Static audit proves that selector 1 passes
through affine calibration at `0x2a68c4`, while selector 4 independently selects
battery-init mode 4 below raw 39 and mode 1 otherwise at `0x2b4f2c`. Neither
function implements the hypothesised two-input pack-recognition table. Exact
electrical naming and scaling remain open. Environment profiles populate raw
ten-bit values; this is scenario input, not a finished physical battery model.

## Interrupt-to-firmware behavior

Firmware ISR `0x2b08c6` reads status register `0xe`, applies mask register
`0xf`, and handles the upper five bits. It posts startup events and `0x77xx`
power-management messages:

| Status bit | Firmware result |
| --- | --- |
| any active upper bit | generic startup event `0x15` and message `0x7701` |
| bit 3 | charger event `0x16` and message `0x7706` |
| bit 4 | result selector 1, message `0x7704` |
| bit 5 | result selector 2, message `0x7703` |
| bit 6 | result selector 4, message `0x7702` |
| bit 7 | message `0x7705` |

The four startup sweep events are:

| Event | Producer | Meaning |
| --- | --- | --- |
| `0x14` | `0x2abdc0/0x2abde4` | charger source-7 present/absent |
| `0x15` | `0x2b09f2` | CCONT battery initialization |
| `0x16` | `0x2b0958` | CCONT IRQ charger path |
| `0x17` | `0x2af086` | CCONT initialization complete |

Mode `0x000d` is therefore a power-on/charger sweep, not a SIM or DSP gate.

## Resolved startup-delivery defect

The earlier model reset register `0xe` to `0x08`, treating a charger-class
upper interrupt as the cold-boot indication, and asserted IRQ for every status
bit. That produced a false interrupt lifecycle and made the ROM appear to lose
the delayed `0x15`/`0x16` events. The corrected cold power-key state is PWRONX
bit `0x02`; the IRQ output considers only upper sources `0xf8`.

With those two device-boundary corrections, the provisioned boot reaches
checklist `0x08 -> 0x09 -> 0x0b -> 0x0f` and mode `0x0004` with neither the
former charged-battery RAM rewrite nor the event-15 delay-literal override.
The canonical IRQ count also falls from 51 to 10 because the low reset-cause bit no
longer repeatedly enters the interrupt cascade. This supersedes the former
claim that a routing/subscription defect prevented organic sweep completion.

PWRONX is reset/input status, not a delayed MAD2 IRQ0 event. The CCONT now
latches bit `0x02` when the phone configures the cold-boot scenario, before the
firmware's first status read. The unevidenced delayed IRQ0 timer and its
environment fixture are removed; ordinary power-button input uses the decoded
KBGPIO column/IRQ0 contract. CCONT remains on the distinct IRQ6 source. Removing
the timer preserves the contact/SIM frontier.

The power-key interrupt path is stable across the two ROMs: v6.00 handler
`0x2b3084` aligns with v5.01 `0x2b02fc`, and each calls a tiny task-1 event
`0x41` publisher (`0x2b4662`/`0x2b18ea`). This proves IRQ ownership and the
firmware transition, but no default edge timing. CCONT
watchdog-register data `0x00` enters the hardware power-off path; `0x20`,
`0x31`, and `0x3f` program, service, and disable the watchdog lifecycle in the
shared helper block. Its physical clock remains unverified.

## Important negative conclusions

These conclusions are retained because repeating the experiments would be
easy:

- Events `0x14` and `0x17` use direct scheduler paths; `0x15` and `0x16` use
  delayed scheduling. Under the corrected reset contract all four arrive.
- CCONT startup-event delivery is not controlled by service-channel
  provisioning flags. Contact-service provisioning and the CCONT sweep are
  separate subsystems.
- Rewriting message classes, forcing service enable flags, suppressing the
  `0xd5` repost, and injecting a presumed task-285 reply did not address the
  actual reset-status defect.
- The old claim that emitter `0x264f30` produced the surfaced sweep IDs was
  false: its messages occur later and use a different path.

The keypad uses MAD2 IRQ0 independently of CCONT. Any further CCONT change must
come from a separately evidenced transaction or IRQ contract, not a startup-event
hypothesis.

## Fidelity backlog

1. Establish whether real GENSIO exposes a measurable busy interval and map
   the remaining endpoint-control bits.
2. Establish ADC request-to-result latency. Firmware routine `0x2b52cc` writes
   the ADC control byte, polls GENSIO status, and reads the result registers;
   it does not wait for a CCONT interrupt. A synthetic conversion-complete IRQ
   is therefore excluded unless separate hardware evidence establishes one.
3. Confirm RTC encoding, alarm behavior, watchdog tick source and the physical
   timing of the MAD2 power-key edge.
4. Replace raw environment ADC overrides with typed battery, charger and RF
   scenario inputs while retaining deterministic tests.
5. Validate remaining register semantics against a working-phone trace or
   independent chip documentation; the v5.01 same-product control is complete.

The 3210 oracle remains the regression gate for every behavior change.
