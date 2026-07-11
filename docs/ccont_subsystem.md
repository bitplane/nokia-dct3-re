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
at `0x2afa74`. The older `0x2b5ae4` label was incorrect; that address is an IPC
message-send wrapper, not CCONT transport.

Firmware establishes three status predicates: bit 0 is polled before endpoint
writes, bit 1 by controller-availability helper `0x2b642a`, and bit 2 after a
CCONT read command. MAD2 starts at `0x03`, resets to `0x03` on endpoint
selection, sets bit 2 after a CCONT transfer byte, and clears it when `0x6c` is
consumed. Transfers remain synchronous, so no busy interval is represented.

The CCONT device still assumes command/data alternation internally. GENSIO mode
selection does not yet reset or validate that device phase.

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
| `0xe` | interrupt status | Proven role. Bit 0 is currently an opt-in presence/status overlay. |
| `0xf` | interrupt mask | Strongly inferred from firmware ISR behavior. |

Writing interrupt-status bits clears them. The IRQ output is active whenever
`status & ~mask` is nonzero. MAD2 owns the resulting CPU interrupt assertion.

## ADC channels

| Channel | Signal |
| ---: | --- |
| 0 | accessory detection |
| 1 | RSSI |
| 2 | battery voltage |
| 3 | battery type / BSI |
| 4 | battery temperature |
| 5 | charger voltage |
| 6 | VCXO temperature |
| 7 | charging current |

The channel mapping is consistent with firmware use and DCT3 service
documentation. Exact electrical scaling is not established. Environment
profiles currently populate raw ten-bit values; this is scenario input, not a
finished physical battery model.

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

## Important negative conclusions

These conclusions are retained because repeating the experiments would be
easy:

- Events `0x14` and `0x17` use direct scheduler paths; `0x15` and `0x16` use
  delayed scheduling. The observed delivery asymmetry is structural, not a
  difference in event routing records.
- CCONT startup-event delivery is not controlled by service-channel
  provisioning flags. Contact-service provisioning and the CCONT sweep are
  separate subsystems.
- Rewriting message classes, forcing service enable flags, shrinking the event
  delay, suppressing the `0xd5` repost, and injecting a presumed task-285 reply
  did not produce organic `0x15` delivery.
- The old claim that emitter `0x264f30` produced the surfaced sweep IDs was
  false: its messages occur later and use a different path.

These results mean the next CCONT change should come from new transaction/IRQ
evidence, not another firmware event injection.

## Fidelity backlog

1. Establish whether real GENSIO exposes a measurable busy interval and map
   the remaining endpoint-control bits.
2. Reset or validate CCONT command/data phase at an evidenced boundary.
3. Establish ADC request-to-result latency and which interrupt bit denotes
   conversion completion. Do not add an arbitrary timer before both are known.
4. Confirm reset values, RTC encoding, alarm behavior and watchdog tick source.
5. Replace raw environment ADC overrides with typed battery, charger and RF
   scenario inputs while retaining deterministic tests.
6. Validate register semantics against a second ROM or working-phone trace.

The 3210 oracle remains the regression gate for every behavior change.
