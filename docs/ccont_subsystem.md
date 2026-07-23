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
- a retained power-domain latch whose output is cleared by the watchdog-register
  power-off command and restored by a charger edge.

`nokia_gensio_device` owns endpoint selection, ready/data-available status and
the serial callbacks into CCONT. The phone driver owns the battery/charger
scenario, eight raw ADC inputs, one-hertz watchdog tick, and board
routing into MAD2 IRQ2. Firmware owns startup and power policy.

## Contract audit

The device boundary is classified by evidence level:

| Surface | Classification | Basis and limitation |
| --- | --- | --- |
| CCONT selection and command grammar | Derived contract | Both 3210 ROMs select endpoint `0x25`, send the same command/address grammar, and use instruction-equivalent helpers. GENSIO status belongs to the separate serial controller, not CCONT. |
| Registers `0x2`/`0x3` ADC result | Tested partial hardware | The focused trace validates LSB and `0xb0 | high-two-bits` packing for all eight deterministic selectors; immediate completion remains inferred. |
| Registers `0xe`/`0xf` status, mask, write-one-clear, IRQ | Tested partial hardware | Both 3210 ROMs read reset status `0x03`: persistent ready bit 0 plus clearable PWRONX cause bit 1. Charger-originated restart exposes cause bit 2, while an operational charger edge uses source bit 3 and MAD2 IRQ2. Firmware reads and clears both forms through register `0x0e`. |
| ADC selector values | Product configuration | Firmware-visible routing identifies 3210 channels 0/1 as VBATT, 3 as BSI, 4 as BTEMP and 5 as VCHAR. Each product supplies named board wiring plus reviewed raw defaults; this is not a battery simulation. |
| RTC counters and alarm (`0x07..0x0c`) | Tested partial hardware | Deterministic binary counters produce second/minute sources; ROM arithmetic establishes binary encoding. Physical `0x07..0x0a` are second/minute/hour/day. A controller fixture proves the comparator and IRQ route; an organic keypad workflow programs a user alarm, receives the CCONT IRQ, and starts the ringtone. Month/calendar persistence remains outside the recovered interface. |
| Watchdog/power register `0x5` | Partial hardware | The documented eight-bit seconds counter, reload and power-off behavior are modeled. WDDISX is an explicit device input and is released in the 3210 profile; both ROMs survive beyond the maximum window through organic reloads. |
| Other register storage | Compatibility behavior | Registers `0x1`, `0x4`, `0x6` and `0xd` retain organic firmware writes so later reads compose. Five-ROM traces establish traffic but no modeled side effect; storage is not a proved hardware contract. |

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
| `0x1` | unidentified control | Compatibility storage. Both 3210 ROMs write `0x40,0x00`; 3310/3330/3410 write `0x70,0x00`. No downstream effect is established. |
| `0x2` | ADC result LSB | Proven role. |
| `0x3` | ADC result MSB/status | Proven role; upper returned bits are inferred. |
| `0x4` | unidentified control | Compatibility storage. All five boot traces write `0x20`; 3310 also writes `0x00`. |
| `0x5` | watchdog and power control | Proven role; reload command semantics inferred from firmware. |
| `0x6` | unidentified control | Compatibility storage. Every supported ROM writes the same `0x54,0x56` sequence, but no device-side effect is established. |
| `0x7..0xa` | RTC second/minute/hour/day read surface | The reader at `0x2b068c` weights these fields by 1, 60, 3,600 and 86,400 respectively. Firmware polls the seconds field and separately writes zero to the day/epoch field during its software-date lifecycle. |
| `0xb..0xc` | RTC alarm minute/hour | Partial one-shot comparison; hour bit 7 is a self-clearing disable/update strobe. An ordinary hour write arms the comparator. |
| `0xd` | unidentified control | Compatibility storage; organically read as zero during boot. Effects are not recovered. |
| `0xe` | interrupt/reset status | Cold power-key reset exposes ready bit 0 and PWRONX cause bit 1 (`0x03`); bit 0 persists while bit 1 and upper interrupt sources are write-one-to-clear. |
| `0xf` | interrupt mask | Strongly inferred from firmware ISR behavior. |

Writing PWRONX or upper interrupt-status bits clears them. The IRQ output is active when
`status & ~mask & 0xf8` is nonzero; the low reset/presence bits do not assert
it. MAD2 owns the resulting CPU interrupt assertion. The default-inactive
`CHARGER` input latches established source bit 3 on both connection and
removal. Both edges use the same firmware debounce path.

The ROM IRQ dispatcher at `0x2b08c6` independently handles status bit 4 as the
second source, bit 5 as the minute source, and bit 7 as the alarm source. The
device advances from a fixed `day 1, 12:00:00` reset state rather than host
wall-clock time, making runs and save states reproducible. Firmware helpers at
`0x2b068c..0x2b080c` multiply the returned fields directly by 60 and 3600 and
bound the seconds field at `0x3a`; this establishes binary rather than BCD
encoding. The recovered alarm helper programs the minute/hour pair without a
separate enable register. Alarm-hour bit 7 requests disable/update and clears
after the latch settles; a normal hour write arms the one-shot comparison.
Register `0xd` remains a clock-gate latch with unknown side effects.

The seconds register exposes bit 7 as the RTC-running status. Firmware checks
that bit before accepting the physical clock, masks it from the numeric seconds,
and uses bit 7 of the alarm-hour register as a polled disable/update strobe.
These two status semantics are required for task 1 to program an organic user
alarm.

The user-alarm lifecycle is distinct from the controller alarm fixture. Organic
menu input configures clock state `2` with flag byte `0x20` and commits an
absolute software deadline through `0x29b1ec`; a disabled alarm uses sentinel
`0x7fffffff`. Task 1 converts the selected deadline to seconds-of-day, programs
CCONT alarm minute/hour, and consumes the resulting bit-7 alarm IRQ. On each
minute source, `0x29b082` obtains the current-time scalar
from `0x29afe8` and publishes packed one-argument event `0x46bc` (base event
`0x06bc`) through `0x2a25c4`. This corrects the former scheduled-time reading of
that payload. Setting the clock temporarily selects a cached scalar while the
task-1 validator at `0x270550..0x2705a8` compares CCONT and cache at minute
resolution. A coherent 12:01 fixture proves that the validator clears fallback
bit `0x10` when both reach minute `721`; later minute publications then advance
from the physical counter. The keypad fixture must populate all eight date
digits before the editor accepts `OK`; with that contract respected, it
organically commits a 12:02 deadline of `0xa0124338`. With the RTC-running and
alarm update-bit contracts modeled, firmware writes alarm minute `0x02` and
hour `0x0c`; CCONT raises bit 7 at 12:02, firmware clears the deadline, and the
ringtone path drives the modeled buzzer. This is an organic keypad-to-hardware-
IRQ-to-audio acceptance chain; the direct controller fixture remains only its
focused conformance test.

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

All five supported firmware controls contain one byte-lane-correct, 18-entry
descriptor table and two literal references to it. All 90 decoded entries are
identical: logical descriptors expose physical registers `0x2`, `0x3`, `0x4`
and `0x7..0xf`; six logical slots are absent. ADC register `0x0`, watchdog
register `0x5`, and boot writes to `0x1`/`0x6` use direct special paths outside
that table. `docs/data/ccont_static_census.json` records every entry and table
address. This establishes addressability, not semantics.

Independent two-second organic traces of the same five ROMs decode 933 complete
CCONT transactions with no phase errors. They observe registers `0x0..0x7`
except no read of `0x1`, plus `0xa..0xf`; registers `0x8` and `0x9` are covered
by the longer RTC/alarm workflows rather than short boot. Counts and value sets
are in `docs/data/ccont_runtime_census.json`. Both 3210 ROMs request all eight
ADC selectors; 3310 requests 0/2/3/4/5/7, while 3330 and 3410 request
0/2/3/4/5 in this window. The latter two retain the 3310 raw tuple as an
explicit boot calibration, not a claim that their PCB nets are identical.

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
| 0 | VBATT; consumed by the early five-sample reader and task-18 raw acceptance window `0x02be..0x0314` |
| 1 | VBATT; source-7 monitor and scalar reader use voltage calibration and the 2100-unit shutdown floor |
| 2 | unresolved |
| 3 | BSI; independently consumed by the battery-size input reader `0x2a90b4` |
| 4 | BTEMP; selects battery-init mode and feeds the independent charge-fault reader |
| 5 | VCHAR/charger voltage. `0x2b084c` takes six samples, separates them at raw `0x64`, averages a stable-side set, and publishes the debounced present state. |
| 6 | unresolved |
| 7 | unresolved; observed once during early initialization, but not consumed by the organic connect/remove lifecycle. |

Static audit proves that selector 1 passes
through affine calibration at `0x2a68c4`, while selector 4 independently selects
battery-init mode 4 below raw 39 and mode 1 otherwise at `0x2b4f2c`. Neither
function implements the hypothesised two-input pack-recognition table. Exact
electrical naming and scaling remain open. Product configuration supplies raw
ten-bit defaults; these are board inputs, not a finished physical battery model.
The machine-readable coverage record is `docs/data/ccont_adc_channels.json`.

Firmware's ADC helper writes the control byte, polls GENSIO status and reads the
two result registers. It does not wait for a CCONT interrupt. Neither 3210 ROM
requires a nonzero conversion interval, and no independent source establishes
a physical duration, so completion remains immediate rather than replacing one
calibration with another. A conversion-complete IRQ remains explicitly excluded.

## Interrupt-to-firmware behavior

Firmware policy routine `0x2b08c6` reads status register `0xe`, applies mask register
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
delayed MAD2 IRQ0 event. CCONT uses MAD2 IRQ2, while physical power-button input
uses the KBGPIO column/IRQ0 path. Under this contract events `0x14` and `0x17`
use direct scheduler paths, events `0x15` and `0x16` use delayed scheduling, and
all four arrive organically.

The power-key interrupt path is stable across the two ROMs: v6.00 handler
`0x2b3084` aligns with v5.01 `0x2b02fc`, and each calls a tiny task-1 event
`0x41` publisher (`0x2b4662`/`0x2b18ea`). This proves IRQ ownership and the
firmware transition, but no default edge timing. A focused physical-input
regression distinguishes the ordinary short-press UI action from a two-second
hold: the latter reaches task-1 mode `0x000c`, terminal event `0x0074`, clears
SIM enable and tears down the display organically. CCONT
watchdog-register data `0x00` enters the hardware power-off path. CCONT's
watchdog is an eight-bit down-counter: every nonzero register write directly
loads that many seconds, so the observed `0x20` and `0x31` writes select 32- and
49-second windows. `0x3f` is therefore a 63-second load, not a disable command;
hardware watchdog disable belongs to the separate board-level `WDDISX` pin.
These semantics and the 32-second default/64-second maximum are documented in
the [Nokia NSE-8/9 System Module technical documentation](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/3210/ch2sys%20%5B103%5D.pdf).

An operational-time charger connection proves the complete interrupt path. The
input sets ADC selector 5, latches CCONT source bit 3 and raises MAD2 IRQ2.
Firmware's IRQ2 top half posts task-1 event `0x51`; task 1 calls policy routine
`0x2b08c6`, reads and write-one-clears CCONT register `0x0e`, schedules events
`0x15` and `0x16`, and enters its charging lifecycle. Removal latches a second
source-3 edge, follows the same register-owned path, and returns task 1 to mode
`0x0004`. The former IRQ6 route produced event `0x72` and only restarted the
analog-monitor sequence; it was a board-wiring error, not a missing consumer.

The charger input is a typed CCONT boundary: one physical edge atomically
changes selector-5 VCHAR and latches source bit 3. A focused connect/remove run
observes 151 selector-5 conversions, including values on both sides of the
firmware's `0x64` threshold. Selector 7 appears only once at 13 ms during early
initialization and is not re-read during either edge. Consequently the driver
does not yet synthesize charge current, battery-voltage ramping or full-battery
taper behavior; those remain hypotheses until a firmware consumer or hardware
source establishes their channel contract.

### Charger-present task-1 lifecycle

Holding the physical charger input from reset selects task-1 mode `0x0009` and
keeps the ordinary SIM lifecycle enabled. The task-1 mode table maps mode 9 to
handler `0x2711fc`. That handler accepts reports `0x0e` and `0x02`; it does not
consume shutdown report `0x07` directly. A sustained physical power-key hold
nonetheless drives the separate controller/teardown lifecycle through mode
`0x000c`, disables the SIM and lands in mode `0x0005`; its handler is
`0x27144c`. A two-second hold lies on a timing-sensitive recognition boundary,
so the focused gate uses four seconds.

`make run` truncates MAME's append-only error log before every launch, so
multi-run evidence cannot leak between fixtures. Watchdog data `0x00` now
removes the complete MAD2 digital baseband domain while CCONT remains alive. A
subsequent physical charger edge restores that domain, exposes CCONT cause bit
2 (`0x04`) and restarts the CPU, MAD2 core, GENSIO, MBUS, DSPIF/peer, SIMI/card
protocol state and LCD controller. Flash and EEPROM retain their contents;
CCONT retains its RTC and reset-cause state. Both 3210 ROMs read the charger
cause, sample selector-5 VCHAR above `0x64`, and settle organically in
acting-dead mode `0x0005`. Resetting only the ARM and MAD2 core left attached
peripherals in stale protocol states and was rejected.

CCONT watchdog expiry uses the same digital-baseband reset domain. It resets
the CPU, MAD2 peripherals, GENSIO, MBUS, DSPIF/peer, SIMI/card protocol state
and LCD controller while retaining CCONT, flash and EEPROM. MAD2 watchdog
expiry and reset-control bit 2 now use the same evidenced digital-baseband
extent while publishing their own retained reset causes.

A mapped one-second watchdog fixture closes the CCONT side separately. The last
status read before expiry and the first after reboot are both `0x13`: existing
ready/PWRONX state plus the pending RTC-second source survive exactly, while
MAD2 clock initialization restarts. CCONT watchdog expiry therefore creates no
new CCONT cause bit. Watchdog data zero remains the distinct commanded rail-off
path.

WDDISX is modeled at the CCONT device boundary rather than by suppressing the
phone's one-second tick. The NSE-8/9 documentation says an ordinary operational
phone has the watchdog enabled, so the 3210 product profile leaves WDDISX
released. The board input is typed product configuration rather than a runtime
environment choice.

The firmware service helper at `0x2b4dc0` accepts a mask: bit 0 reloads the
MAD2 watchdog and bit 1 writes `0x31` through logical CCONT descriptor 6,
which GENSIO maps to physical register 5. A focused v6.00 run observes three
CCONT-only calls from the startup/NV state machine at 12--13 ms and one
CCONT-only call from task 2 at 0.834 s. Paired 12-second clock runs observe no
MAD2-watchdog write; that service branch is conditional. The earlier claimed Thumb
pointer at `0x2dfc80` was an unaligned halfword match, not a function-table
entry. All nine direct calls are classified: six belong to one-shot boot/NV
callback `0x296428`; the steady-state CCONT call is task 2 at `0x237b2e`.

Timer 0 retains its 33,055 Hz source calibration; Timer 1 uses 1,057 Hz, with
their post-divider 8:1 relation established by paired ROM code. In the current
v6.00 lifecycle task 2 performs one combined MAD2/CCONT reload at approximately
31 seconds, loading `0x31` into both counters. Periodic non-fault MDI activity
separately prevents the firmware's reason-`0x68` DSP-liveness terminal path;
that path was previously misclassified as watchdog-counter expiry. The earlier task-17 failure was not scheduler or
CCONT behavior: the external-service prototype sent an unsupported command
`0x64`, result `5`, whose firmware handler deliberately suspends application
tasks after five scheduler ticks. Without that lifecycle request, task 17
receives `0x1587`, the task-17 -> task-20 -> task-21 SIM chain completes, and
no periodic reload or firmware wake is synthesized.

Service-channel provisioning does not control CCONT startup-event delivery.
Any further CCONT change requires a separately evidenced register, timing, or
IRQ contract.

## Completion matrix

| Goal surface | Current result | Remaining evidence |
| --- | --- | --- |
| Serial grammar | Validated across five ROMs; 90/90 descriptor entries decoded and identical | Physical GENSIO busy duration |
| Register surface | All 16 addresses classified as descriptor-backed, direct-special, or compatibility-only | Side effects of `0x1`, `0x4`, `0x6`, `0xd` |
| ADC | All eight channels exercised; NSE-8 VBATT/BSI/BTEMP/VCHAR routes classified | Electrical units and channels 2/6/7 |
| Interrupts | Bits 3/4/5/7 mapped; status bits 0/1/2 separated from IRQ latches | Physical owner and stimulus for bit 6 |
| Reset/retention | Cold, software, MAD2/CCONT watchdog, rail-off and charger wake distinguished | Physical rail transition delays |
| Watchdog | One-second eight-bit reload, WDDISX, expiry and zero-data power-off covered | Oscillator tolerance |
| RTC/alarm | Binary counters, strobes, second/minute/alarm sources and organic alarm covered | Calendar persistence and oscillator drift |
| Charger/power | Both edges, VCHAR debounce and acting-dead restart covered | CHAPS current regulation and battery dynamics |
| Save state | Register, ADC, watchdog, power, command phase and IRQ state registered; outputs reconstructed | Lua saves synchronize after the request, so exact mid-byte capture is not independently gated |

## Hardware-measurement backlog

1. Measure GENSIO select-to-ready and ADC request-to-result latency; firmware
   proves polling but no nonzero lower bound.
2. Capture registers `0x1`, `0x4`, `0x6` and `0xd` while changing charger,
   sleep and RTC states, and stimulate interrupt bit 6 if possible.
3. Correlate ADC channels 2, 6 and 7 with board nets and measure raw-to-unit
   transfer functions for VBATT, BSI, BTEMP and VCHAR.
4. Measure baseband rail transition delay and watchdog oscillator tolerance.
   Model CHAPS current and battery evolution only after both a firmware
   consumer and physical relationship are established.

The structural summary records CCONT commands and read counts, but those totals
remain diagnostic. `make verify-ccont RUN_DIR=<dir>` is the focused
transport/register gate. Its ordinary run requires endpoint selection,
command/data pairing, status values `0x03`/`0x07`, both transaction directions,
and correct ADC packing for all eight `sane` selector fixtures. Its second run
pulses the charger input and requires MAD2 IRQ2 delivery, firmware CCONT status
read, write-one-clear acknowledgement, cleared follow-up read, and final IRQ
deassertion. GENSIO endpoint/status
state and CCONT register, command and IRQ state are save-state registered
together; post-load reconstructs the IRQ output.

`make ccont-static-census` checks the common five-ROM descriptor vocabulary.
`make ccont-runtime-census RUN_DIR=<dir>` regenerates the five short organic
boots and requires the established 14-register boot surface. These censuses
separate firmware coverage from device-side semantic claims.

`make verify-ccont-mask RUN_DIR=<dir>` masks every CCONT source through mapped
register `0xf`, creates a minute/alarm transition, reads the pending alarm from
register `0xe` without acknowledging it, then unmasks only bit 7. It requires
no IRQ before unmask and an immediate routed MAD2 IRQ2 afterward.

`make verify-ccont-watchdog RUN_DIR=<dir>` runs a provisioned steady-state boot
for 55 seconds with WDDISX released and requires the task-2 combined reload,
no terminal watchdog reason, no watchdog expiry, and no soft reset. The
provisioned identity excludes the independent security-editor timeout/reset
lifecycle from this hardware gate.

`make verify-charger-lifecycle RUN_DIR=<dir>` separately proves charger-present
startup, selector-5/IRQ2 service and the physical mode-`0x0009` -> `0x000c` ->
`0x0005` acting-dead transition.

`make verify-charger-wake RUN_DIR=<dir>` proves the powered-off lifecycle:
CCONT removes power, a later charger edge raises cause `0x04`, firmware reads
that cause and VCHAR after a complete digital-domain reset, and the restarted
phone settles in mode `0x0005`. The same gate passes v6.00 and v5.01.

`make verify-mad2-reset RUN_DIR=<dir>` additionally exercises MCU software
reset, MAD2 watchdog expiry and CCONT watchdog expiry. The CCONT case requires
the complete baseband clock initializer to restart and the pre-expiry CCONT
status byte to survive unchanged.

The focused gates do not yet validate every register reset value, alarm reprogramming, the `0x06bc` consumer,
register-`0x0a` write semantics, or full save-state resumption.
RTC determinism and source assignments have source-level
regressions; the byte-exact default frame and coherent frontier protect their
integration behavior.
