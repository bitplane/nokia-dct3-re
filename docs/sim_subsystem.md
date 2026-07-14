# SIM subsystem

This is the concise hardware and firmware contract for the Nokia 3210 v6.00 SIM path. The
address-heavy registration investigation remains in `sim_registration.md`; this document records
only conclusions that are implemented or directly observed.

## Hardware boundary

SIMI is the MAD2 SIM UART at `0x020036..0x02003f`:

| Offset | Register | Verified behavior |
| --- | --- | --- |
| `0x36` | TXD | Firmware writes PPS, T=0 headers and command bodies here. |
| `0x37` | RXD | Firmware drains ATR, procedure, data and status bytes here. |
| `0x38` | IIR | `0x10` advances/completes TX; `0x40` reports received bytes. Writing the observed bits acknowledges them. |
| `0x39` | control/status | Firmware activation writes `0x32 -> 0x33 -> 0xb3`; live readback must include ready bit `0x40` while enabled. |
| `0x3b` | TX low-water | Firmware programs `0x60` during activation. |
| `0x3c` | RX fill | Number of readable bytes consumed by the receive FIQ loop. |
| `0x3d..0x3f` | FIFO flags/fill | Not yet needed by the stateful model; retain as open SIMI detail. |

FIQ line 6 is the SIMI interrupt. The firmware route is:

```text
FIQ dispatcher 0x2af49c
  -> line-6 handler 0x2a054a
     -> IIR bit 0x10: TX progression 0x2a033e
     -> IIR bit 0x40: RX dispatcher 0x2a04c8
        -> byte classifier 0x2a03b4
        -> allocate/post task-21 response through 0x26aac0
```

The former conclusion that RXD is only a reset flush and SIM replies arrive through an unrelated
DSP/service message was false. It came from confusing SIM structure offsets with MMIO references.
The register/FIQ path above has now executed end to end.

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
handler observe the previous firmware descriptor. The device schedules TX-ready and RX-ready on a
timer, exposes RX bytes only when ready, and cancels the trailing event when the FIFO empties.

## Stateful card device

`nokia_sim_card_device` is enabled by `NOKI3210_MODEL_SIM_DEVICE=1`. It owns:

- activation, ready-status and ATR state;
- SIMI IIR, RX FIFO and timed FIQ delivery;
- PPS echo;
- T=0 SELECT, STATUS, GET RESPONSE, READ BINARY/RECORD and CHANGE CHV sequencing;
- selected-file state; and
- synthetic GSM 11.11 file metadata and content.

It does not inject task messages, call firmware handlers, or write SIM/registration RAM. When the
device is disabled, SIMI reads and writes retain the legacy/default behavior so the ordinary boot
profiles remain unaffected.

The former `MODEL_SIM_CARD` message responder and `MODEL_SIM_ATR` FIFO probe have
been removed. Their useful ATR, T=0, selected-file, FCP and synthetic-EF behavior
is owned by this device. Their scratch-message injection, firmware trampolines and
receive-result rewriting are not part of the supported SIM boundary.

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
downstream of the current registration wall even with a phase-3 isolation card.
This route is therefore excluded as the ordinary registration predecessor.
Validated DSP RX families do not feed this SAT path. Service-5's callback is already
registered and organically receives (`0x05f3`, `0x05e2`), while its `0x05e8`
branch remains dormant downstream. Do not replace this firmware contract
by selecting callbacks, posting task results, replaying commit keys, or setting registration state.

The coherent contact profile also exposes the next task-1 predicate. Task 1 enters
mode `0x0004` at about 1.436 s and waits for report code `0x07`; the reporter is
`0x2af190`. Status dispatcher `0x27b370` is exercised during initialization
with `0x05e2`, and callback state slot `0x5d` is already `0x0b`. Inputs
`0x05eb` and `0x06c5` report ready directly. Inputs `0x05e1`, `0x05e7`, and
`0x05dc` arm task-local timer class `0x52`, which returns to task 5 as
`0x06c5`; none is observed. Flags `0x01a00000` intentionally select `0x032d`
instead of an automatic `0x05dc` start. A valid task-13 segmented transaction
publishes `0x05eb` directly to task 16 and does not enter this callback path.
The separately
mapped callback-`0x1a` chain (`0x1400 -> 0x08ac -> 0x0795`) is dormant:
`0x1400` is task-5 mailbox ingress, and exhaustive recovery of all 188 direct
task-5 event-helper calls found no in-ROM producer. It is therefore not the
current ordinary-boot hypothesis. A display lifecycle (`0x0280/81/82 -> 0x05e7
-> 0x0389 -> 0x157e -> 0x0396 -> 0x05eb`) is a valid service/test route, not
the ordinary predecessor. Callback `0x5d` is not yet proven to be the exclusive
ordinary owner; the independent controller-status callers remain candidates.
The unresolved code-7 predicate remains parallel to,
and is not part of, the SIM lifecycle.

The descriptor factory is now identified as `0x24f120`. Its four known callers
(`0x2996aa`, `0x2997dc`, `0x299860`, `0x2998a0`) sit immediately before the callback-7 handler and
construct the same `0x18`-byte radio-session object later attached by `0x24f25c`. All four are
branches of `0x299610`; every successful result is published through
`0x2af798(0xca8a, 0x1e, object)`. The factory remains unexecuted because the lower peer has not
delivered the object-bearing input which lets task 15 complete its local operation. Status
`0x09ee` is not that peer input: seven branches inside the task-15 state machine call
`0x208ee0(0x09ee, 0)`. Task 15 then forwards its own result to task 17, which constructs
`0x0a2e` and returns it to task 15 before `0x299610` runs.

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
- `make verify-deep`: historical bridge-profile frame and structural oracle.
- `make run-frontier`: current request-driven contact/SIM research profile.
- `make smoke-3330e RUN_DIR=<dir> SECONDS=3`: bounded second-ROM confidence run.
- Stateful-model trace: natural ATR/PPS and the ordinary non-CPHS EF pass, with
  SIM enable rising and the timed presence monitor starting without injected
  messages or SIM-state RAM writes.
