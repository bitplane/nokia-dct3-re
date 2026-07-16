# CCONT subsystem

CCONT is the DCT3 power-management ASIC. It monitors battery and charger
signals, provides ADC and RTC registers, controls the power/watchdog sequence,
and reports interrupt causes to MAD2. CHAPS performs the physical charging;
COBBA handles audio and RF conversion.

This document records the current hardware contract and unresolved questions.

## Confidence

- **Proven:** firmware behavior and an independent hardware source agree.
- **Inferred:** supported by firmware disassembly/runtime behavior only.
- **Unknown:** current emulation behavior is a placeholder.

## Device boundary

`driver/nokia_ccont.{h,cpp}` implements a MAME device that owns:

- CCONT command/data phase after MAD2 selects the endpoint;
- the 16-byte CCONT register file;
- sampling configured ADC source values;
- the deterministic binary RTC counters and recovered periodic IRQ sources;
- interrupt status, mask and IRQ output;
- watchdog counter state; and
- a power output asserted by the watchdog-register power-off command.

`nokia_gensio_device` owns endpoint selection, ready/data-available status and
the serial callbacks into CCONT. The phone driver owns the battery/charger
scenario, eight raw ADC inputs, one-hertz watchdog tick, and board
routing into MAD2 IRQ6. Firmware owns startup and power policy.

## Contract audit

The device boundary is classified by evidence level:

| Surface | Classification | Basis and limitation |
| --- | --- | --- |
| CCONT selection and command grammar | Derived contract | Both 3210 ROMs select endpoint `0x25`, send the same command/address grammar, and use instruction-equivalent helpers. GENSIO status belongs to the separate serial controller, not CCONT. |
| Registers `0x2`/`0x3` ADC result | Tested partial hardware | The focused trace validates LSB and `0xb0 | high-two-bits` packing for all eight deterministic selectors; immediate completion remains inferred. |
| Registers `0xe`/`0xf` status, mask, write-one-clear, IRQ | Tested partial hardware | A charger-input fixture latches established source bit 3, exercises MAD2 IRQ6 and the firmware ISR, and proves write-one-clear acknowledgement and deassertion. PWRONX bit 1 is established; the opt-in bit-0 presence overlay is provisional. |
| ADC selector values | Working fixture | Firmware-visible selector routing is mapped, but raw values, electrical names, units, and physical battery relationships are not. `ADC_PROFILE` is test provisioning, not a battery model. |
| RTC registers `0x7..0xa` | Partial | A deterministic one-second clock supplies binary second/minute/hour/day counters. ROM arithmetic establishes binary encoding; month/calendar persistence remains outside the recovered interface. |
| Watchdog/power register `0x5` | Partial hardware | The documented eight-bit seconds counter, reload and power-off behavior are modeled. WDDISX is an explicit device input; the default profile grounds it pending the missing steady-state firmware service. |
| Other register storage | Compatibility behavior | Registers without mapped semantics retain written bytes so firmware-visible transactions compose. Storage must not be interpreted as a proved hardware contract. |

No direct firmware state is changed by the CCONT device. The main fidelity risk
is therefore not a forcing shim; it is that provisional timing, encoding, and
analog fixtures can be mistaken for measured hardware behavior.

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
CCONT read command. GENSIO starts at `0x03`, resets to `0x03` on endpoint
selection, sets bit 2 after a CCONT transfer byte, and clears it when `0x6c` is
consumed. Both ROMs select endpoint `0x25` before every register helper and
send a command byte first. Endpoint selection therefore resets the device to
command phase. Transfers remain synchronous because neither ROM exposes a
conversion-complete interrupt or a firmware-visible minimum delay. This proves
only that the current firmware contract tolerates immediate completion; it does
not prove that physical GENSIO or CCONT has zero latency.

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
| `0x7..0xa` | RTC second/minute/hour/day | Binary counters; roles and encoding established from the firmware helper block. |
| `0xb..0xc` | RTC alarm minute/hour | Partial one-shot comparison; programming either field arms it. |
| `0xd` | clock gates | Stored latch; effects not recovered. |
| `0xe` | interrupt/reset status | Proven role. Cold power-key reset latches PWRONX as bit 1 (`0x02`); upper bits `0xf8` are interrupt sources. Bit 0 remains an opt-in presence overlay. |
| `0xf` | interrupt mask | Strongly inferred from firmware ISR behavior. |

Writing interrupt-status bits clears them. The IRQ output is active when
`status & ~mask & 0xf8` is nonzero; the low reset/presence bits do not assert
it. MAD2 owns the resulting CPU interrupt assertion. The default-inactive
`CHARGER` input latches established source bit 3 on connection; disconnection
has no assigned status effect pending hardware evidence.

The ROM IRQ dispatcher at `0x2b08c6` independently handles status bit 4 as the
second source, bit 5 as the minute source, and bit 7 as the alarm source. The
device advances from a fixed `day 1, 12:00:00` reset state rather than host
wall-clock time, making runs and save states reproducible. Firmware helpers at
`0x2b068c..0x2b080c` multiply the returned fields directly by 60 and 3600 and
bound the seconds field at `0x3a`; this establishes binary rather than BCD
encoding. The recovered alarm helper programs the minute/hour pair without a
separate enable register, so writing either field arms a one-shot comparison;
register `0xd` remains a clock-gate latch with unknown side effects.

With periodic delivery enabled, the v5.01 structural run observes MAD2 IRQ bit
`0x10` and still reaches the same interactive menu. Its physical-key fixture is
scheduled at seven seconds rather than five because the newly represented RTC
work shifts that ROM's editor-ready time; this is a harness timing adjustment,
not a firmware-state intervention. The v6.00 menu remains valid with its
existing five-second input schedule.

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

## Startup and interrupt contract

Cold power-key state is PWRONX bit `0x02`; only upper status sources `0xf8`
contribute to the CCONT IRQ output. PWRONX is reset/input status rather than a
delayed MAD2 IRQ0 event. CCONT uses MAD2 IRQ6, while physical power-button input
uses the KBGPIO column/IRQ0 path. Under this contract events `0x14` and `0x17`
use direct scheduler paths, events `0x15` and `0x16` use delayed scheduling, and
all four arrive organically.

The power-key interrupt path is stable across the two ROMs: v6.00 handler
`0x2b3084` aligns with v5.01 `0x2b02fc`, and each calls a tiny task-1 event
`0x41` publisher (`0x2b4662`/`0x2b18ea`). This proves IRQ ownership and the
firmware transition, but no default edge timing. CCONT
watchdog-register data `0x00` enters the hardware power-off path. CCONT's
watchdog is an eight-bit down-counter: every nonzero register write directly
loads that many seconds, so the observed `0x20` and `0x31` writes select 32- and
49-second windows. `0x3f` is therefore a 63-second load, not a disable command;
hardware watchdog disable belongs to the separate board-level `WDDISX` pin.
These semantics and the 32-second default/64-second maximum are documented in
the [Nokia NSE-8/9 System Module technical documentation](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/3210/ch2sys%20%5B103%5D.pdf).

An enabled-watchdog coherent run now survives the former 32-second false expiry, but
no steady-state firmware service occurs before the corrected 49-second window
expires. The restart replays the startup `0x20`/`0x31` sequence. The retained
default WDDISX-grounded scenario therefore represents a missing
software-timing/service lifecycle, not uncertainty about the CCONT counter
arithmetic; it must not be replaced by lengthening or freezing the counter.
WDDISX is modeled at the CCONT device boundary rather than by suppressing the
phone's one-second tick. The NSE-8/9 documentation says an ordinary operational
phone has the watchdog enabled, so the grounded default is explicitly debt,
not a recovered board strap.

The firmware service helper at `0x2b4dc0` accepts a mask: bit 0 reloads the
MAD2 watchdog and bit 1 writes `0x31` through logical CCONT descriptor 6,
which GENSIO maps to physical register 5. A focused v6.00 run observes three
CCONT-only calls from the startup/NV state machine at 12--13 ms and one
combined MAD2/CCONT call from task 2 at 0.834 s. It observes no later call
before expiry. The helper also appears as Thumb pointer `0x2b4dc1` in the ROM
function table at `0x2dfc80`, but that table membership alone does not prove a
periodic owner. Direct callers cover startup/NV work and the external-service
D9 poll; the remaining question is which firmware-owned schedule should invoke
the exported helper after startup.

The working 13 MHz Timer-0 calibration plus a corrected 33,055 Hz free-running
Timer 1 preserves the interactive menu oracle. An unguarded direct run still
expires at exactly 49.000 seconds and restarts, proving that Timer-1 overflow
was not the missing watchdog-service owner. Supplying 33,055 Hz to Timer 0 as
well makes task 0 dominate and eventually reaches the task-2 reload path, but
starves the task-17 -> task-20 -> task-21 SIM startup chain and never composes
with the UI lifecycle. No periodic reload is synthesized pending evidence for
the firmware-owned schedule.

Service-channel provisioning does not control CCONT startup-event delivery.
Any further CCONT change requires a separately evidenced register, timing, or
IRQ contract.

## Fidelity backlog

1. Establish whether real GENSIO exposes a measurable busy interval and map
   the remaining endpoint-control bits.
2. Establish ADC request-to-result latency. Firmware routine `0x2b52cc` writes
   the ADC control byte, polls GENSIO status, and reads the result registers;
   it does not wait for a CCONT interrupt. A synthetic conversion-complete IRQ
   is therefore excluded unless separate hardware evidence establishes one.
3. Recover the post-startup schedule or callback owner of exported helper
   `0x2b4dc0`, then stop grounding WDDISX in the default profile. Recover the
   physical timing of the MAD2 power-key edge separately.
4. Replace raw environment ADC overrides with typed battery, charger and RF
   scenario inputs while retaining deterministic tests.
5. Validate remaining register semantics against a working-phone trace or
   independent chip documentation; the v5.01 same-product control is complete.

The structural summary records CCONT commands and read counts, but those totals
remain diagnostic. `make verify-ccont RUN_DIR=<dir>` is the focused
transport/register gate. Its ordinary run requires endpoint selection,
command/data pairing, status values `0x03`/`0x07`, both transaction directions,
and correct ADC packing for all eight `sane` selector fixtures. Its second run
pulses the charger input and requires status bit 3, MAD2 IRQ6, firmware
write-one-clear acknowledgement and final IRQ deassertion. GENSIO endpoint/status
state and CCONT register, command and IRQ state are save-state registered
together; post-load reconstructs the IRQ output.

The focused gate does not yet validate every register reset value, mask changes
while a source is pending, alarm reprogramming, watchdog expiry, or full
save-state resumption. RTC determinism and source assignments have source-level
regressions; the byte-exact default frame and coherent frontier protect their
integration behavior.
