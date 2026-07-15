# SIM subsystem

This is the concise hardware and firmware contract for the Nokia 3210 v6.00 SIM path. The
address-heavy registration investigation remains in `sim_registration.md`; this document records
only conclusions that are implemented or directly observed.

## Hardware boundary

SIMI is the MAD2 SIM UART at `0x020036..0x02003f`:

`nokia_simi_device` owns the MAD2 UART/FIFO/IIR registers, scheduling and FIQ6.
It sends activation and transmitted bytes to `nokia_sim_card_device`; the card
returns only response bytes. The card owns ATR/PPS, T=0 state, selected files,
and the current synthetic filesystem.

| Offset | Register | Verified behavior |
| --- | --- | --- |
| `0x36` | TXD | Firmware writes PPS, T=0 headers and command bodies here. |
| `0x37` | RXD | Firmware drains ATR, procedure, data and status bytes here. |
| `0x38` | IIR | `0x10` advances/completes TX; `0x40` reports received bytes. Writing the observed bits acknowledges them. |
| `0x39` | control/status | Firmware activation writes `0x32 -> 0x33 -> 0xb3`; live readback must include ready bit `0x40` while enabled. |
| `0x3b` | TX low-water | Firmware programs `0x60` during activation. |
| `0x3c` | RX fill | Number of readable bytes consumed by the receive FIQ loop. |
| `0x3d` | RX FIFO control | Firmware programs `18,12,06` during activation and writes `06` before draining RX. Observed behavior requires a retained configuration latch but proves no destructive flush side effect. |
| `0x3e` | TX FIFO control | `0x04` opens/fills a transmit chunk; `0x00` flushes it to the serial/card boundary and produces TX-empty progression. |
| `0x3f` | TX fill | Live number of bytes waiting in the 16-byte SIMI TX FIFO. |

FIQ line 6 is the SIMI interrupt. The firmware route is:

```text
FIQ dispatcher 0x2af49c
  -> line-6 handler 0x2a054a
     -> IIR bit 0x10: TX progression 0x2a033e
     -> IIR bit 0x40: RX dispatcher 0x2a04c8
        -> byte classifier 0x2a03b4
        -> allocate/post task-21 response through 0x26aac0
```

The register/FIQ path above executes end to end; SIM replies do not use the
DSP/service message path.

The complete v6.00 IIR cascade at `0x2a054a` aligns in v5.01 at
`0x29da8a`. Its causes are finite:

| IIR bit | Firmware action |
| ---: | --- |
| `0x02` | post the static UART/error result at `0x2ccc00` |
| `0x10` | run TX-empty progression at `0x2a033e` |
| `0x20` | post static timeout result `0x06` through `0x2ca400` |
| `0x40` | run RX FIFO dispatcher at `0x2a04c8` |
| `0x80` | post the static terminal/error result at `0x2cc400` |

The ISR reads IIR once and writes the same byte back, proving write-one-clear
acknowledgement for every cause. The device currently generates only TX-empty
and RX-ready. A T=0 work-waiting timeout would legitimately generate `0x20`
only while a requested character is outstanding. The modeled card responds
before that condition, so a periodic idle `0x20` source would be a recovery
shim and is intentionally not implemented. Bits `0x02` and `0x80` remain
fault-path contracts awaiting parity/framing or card-removal evidence.

## Firmware transaction contract

Task 21 owns card activation and T=0 transport. The receive classifier produces these messages:

| Code | Meaning |
| --- | --- |
| `0x05` | ATR buffer complete. |
| `0x07` | TX descriptor accepted/completed, posted by the `0x10` IIR path. |
| `0x09` | Unclassified receive, including the PPS echo. |
| `0x0a` | Terminal SW1/SW2 result. |
| `0x0b` | T=0 procedure/data response. |

The ordering is part of the contract. `0x27e98c` sends through `0x2a02e6`, waits for code `0x07`,
and only then does its caller consume the card response. Raising RX alone leaves a valid reply
stranded behind the missing TX event. IIR `0x20` is not TX completion: it posts static code `0x06`.

Card responses must also be asynchronous. Raising FIQ from inside the final TXD write lets the
handler observe the previous firmware descriptor. TXD writes now enter a 16-byte UART FIFO rather
than the APDU parser directly. Firmware opens the FIFO through `0x3e=0x04`, fills it, and flushes
with `0x3e=0x00`; only the flush transfers the bytes to T=0 and schedules TX-empty. This permits a
command body larger than one FIFO to advance through multiple TX-empty interrupts. The device
schedules TX-ready and RX-ready on a timer, exposes RX bytes only when ready, and cancels the
trailing event when the RX FIFO empties.

## Stateful card device

`NOKI3210_MODEL_SIM_DEVICE=1` enables the SIMI/card composition. The card
implements:

- activation reset, ATR and PPS echo;
- T=0 SELECT, STATUS, GET RESPONSE, READ BINARY/RECORD and CHANGE CHV sequencing;
- selected-file state; and
- synthetic GSM 11.11 file metadata and content.

It does not inject task messages, call firmware handlers, or write SIM/registration RAM. When the
device is disabled, SIMI reads and writes retain the legacy/default behavior so the ordinary boot
profiles remain unaffected.

## Contract audit

The implemented surface is classified by ownership and evidence:

| Surface | Classification | Basis and limitation |
| --- | --- | --- |
| SIMI register window and FIQ6 route | Extracted partial hardware | `nokia_simi_device` owns offsets `0x36..0x3f`, the decoded IIR cascade, timing and FIQ6; firmware traffic executes through it in both mapped 3210 ROMs. |
| TX FIFO, live fill, and `0x3e` chunk progression | Partial hardware | The 16-byte FIFO and multi-chunk ordering are required by coherent firmware traffic. Exact FIFO-control semantics and physical UART timing remain inferred. |
| IIR write-one-clear and causes `0x10`/`0x40` | Derived contract | Firmware acknowledgement and organic TX/RX progression are observed. Timeout/error/removal causes `0x02`, `0x20`, and `0x80` are decoded but not modeled. |
| ATR/PPS and T=0 exchange | Partial card contract | The ordinary initialization conversation is coherent. Fixed 10/100 microsecond delays are calibrated scheduling choices, not measured card or UART timing. |
| SELECT/STATUS/GET RESPONSE/READ behavior | Prototype card | It satisfies organically requested initialization and presence polling. Access conditions, invalidation, record semantics, CHV state, errors, reset, and removal are incomplete. |
| Default and CPHS filesystem contents | Provisioning fixture | File sizes are ROM-informed and the data is internally coherent enough for the tested paths, but identities and service contents are synthetic test data, not 3210 hardware behavior. |
| CHANGE CHV support | Dormant prototype | The procedure/body/status sequence is implemented, but the ordinary boot does not request it and no persistent credential semantics are validated. |

The model does not force firmware state or inject RTOS messages. Controller and
card ownership are separate; remaining fidelity debt is calibrated timing,
unmodeled errors/removal, and card protocol mixed with subscriber provisioning.

The synthetic mandatory-file sizes come from the firmware table at `0x2e0c04`. Implemented content
includes ICCID `2FE2`, ECC `6FB7`, LP `6F05`, IMSI `6F07`, SST `6F38`, LOCI `6F7E`, and Phase
`6FAE`; other known files are erased (`0xff`). `EF_PHASE` reports Phase 2 (`0x02`). Returning `0x00`
prevents the validated preliminary lifecycle from composing. MF/DF STATUS data uses the GSM 11.11
directory layout, including a `0x15` GSM-specific-data length and CHV status fields; the earlier
shifted layout caused the firmware to repeat the preliminary pass.

## Current organic result

In an unforced coherent run the device now completes:

```text
ATR -> PPS -> preliminary SELECT/STATUS/READ pass
  -> LP -> AD -> SST -> LOCI -> IMSI -> ACC -> 2FE6
  -> optional CPHS ONS 6F14 absent -> remaining GSM/vendor EFs
  -> optional 6F99 absent -> timed directory-presence monitor
```

Observed reads include ICCID, ECC, Phase, LP, SST, IMSI and ACC. SIM-enable byte `[0x111c79]`
changes to 1 organically at about 1.309 s. This is the first coherent run to pass the preliminary
card-acceptance transition without firmware-state forcing.

`6F14` is the optional CPHS Operator Name String, a transparent default-alphabet field of up to
the 24 bytes accepted by parser `0x201876`. The card does not advertise CPHS and need not provide
it. The previous device nevertheless accepted every unknown SELECT, advertised a zero-byte EF,
and caused initialization to restart. SELECT now returns GSM 11.11 `94 04` (file ID not found)
without changing the current selection for unsupported files. Firmware handles that organic
optional-file result and continues through the rest of its initialization sequence.

At about 1.427 s task 20 reaches its card-presence monitor at `0x2028a4` with SIM enable 1,
no-SIM 0, and ready byte `0x10dcaf` equal to 1. The card tracks current DF separately from the
selected EF, so STATUS reports `7F40`; four consecutive checks take the monitor's steady exit at
`0x20290a`, not its changed-directory path at `0x2028f4`. The monitor issues `A0 F2` and explicitly
rearms timer `0xea` with delay `0x181`, producing a roughly 42 ms cadence. This perpetual traffic
is a normal firmware-owned presence check, not a repeated initialization pass. Task 1 subsequently
enters mode `0x0004` at about 1.435 s.

The later extended registration pass still requires a GSM/radio session which constructs callback
7 and drives:

```text
callback 7 0x05dc -> 0x0aa0 -> context attachment -> packed 0x5518
  -> task-17 0x1583 -> registration/session commit -> 0x1196/0x1199
```

Callback 7 currently receives only the global `0x05e2` sweep, not constructor
`0x05dc`. The mapped task-21 `0x120c -> A0/12 -> D0 -> 0x177x` route is GSM
11.14 SIM Toolkit: TERMINAL PROFILE arms latch `0x10dcb7`, `91xx` advertises a
proactive command, and `A0/12` FETCH retrieves it. The current EF_PHASE=2 card
correctly leaves this path dormant, and firmware's profile-download function is
part of a later session lifecycle even with a phase-3 isolation card.
This route is therefore excluded as the ordinary registration predecessor.
Validated DSP RX families do not feed this SAT path. Service-5's callback is already
registered and organically receives (`0x05f3`, `0x05e2`), while its `0x05e8`
branch remains dormant downstream. Do not replace this firmware contract
by selecting callbacks, posting task results, replaying commit keys, or setting registration state.

Later registration, MMI callbacks, report 7, and the unattended UI handoff are
outside the SIM boundary. Report 7 is a shutdown/power-lifecycle report, not a
SIM-ready event. The mapped registration descriptor and callback chains remain
in `sim_registration.md`, `mmi_settlement.md`, and normalized evidence;
they are intentionally not repeated in this concise hardware/card contract.

## Deferred CHV transaction: reply code 2

This is a mapped later card contract, not an outstanding ordinary-SIM-init
gate. The non-CPHS initialization pass now raises SIM enable without traversing
this path. Retain it for the first organic PIN/CHV lifecycle that requests it.

The downstream card contract is already mapped. Organic `0x1196` enters `0x207234`, which calls
`0x293f30`. That function constructs `A0 24` CHANGE CHV with a 16-byte body, posts it to task 21 and
waits for the result. Success is return code `2`; only that branch reaches its ENABLE setter at
`0x20733c`. The stateful card implements the required header -> TX-ready -> procedure -> body ->
TX-ready -> `9000` sequence. The full path remains unvalidated because current
ordinary boot does not organically request `0x1196`.

## Current acceptance gates

- `make verify`: exact 3210 frame and structural oracle.
- `make run-frontier`: current external-service/SIM research profile.
- `make smoke-3330e RUN_DIR=<dir> SECONDS=3`: bounded second-ROM confidence run.
- Stateful-model trace: natural ATR/PPS and the ordinary non-CPHS EF pass, with
  SIM enable rising and the timed presence monitor starting without injected
  messages or SIM-state RAM writes.

The frontier predicate protects the final organic SIM-enabled state, but there
is no focused device test for FIFO reset/fill behavior, IIR acknowledgement,
multi-chunk ordering, T=0 procedure sequencing, file metadata, timeout/error
causes, removal, or save-state resumption. Add that coverage before splitting
or materially changing the combined controller/card implementation.
