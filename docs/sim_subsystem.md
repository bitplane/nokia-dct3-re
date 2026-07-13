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
includes ICCID `2FE2`, ECC `6FB7`, IMSI `6F07`, LOCI `6F7E`, and Phase `6FAE`; other known files are
erased (`0xff`). `EF_PHASE` must report Phase 2 (`0x02`). Returning `0x00` prevents the validated
preliminary lifecycle from composing.

## Current organic result

In one unforced run the device now completes:

```text
ATR -> PPS -> SELECT 7F20 -> STATUS -> SELECT/READ mandatory EFs
```

Observed reads include ICCID, ECC and Phase, and the task-5 run reaches natural status `0x1581`
with `[0x10dcaf]=1` and `[0x10dca9]=1`. This restores the established preliminary-SIM baseline.

After PHASE, firmware deliberately polls DF_GSM with `A0 F2 00 00 16`; this is stable presence
polling, not a rejected STATUS response. The next wall is outside the card transport. The extended
IMSI pass requires a GSM/radio session which constructs callback 7 and drives:

```text
callback 7 0x05dc -> 0x0aa0 -> context attachment -> packed 0x5518
  -> task-17 0x1583 -> registration/session commit -> 0x1196/0x1199
```

Callback 7 currently receives only the global `0x05e2` sweep, not constructor
`0x05dc`. Its strongest organic ingress is now mapped as task 21 status `0x120c`
to task 20, followed by a synchronous task-21 `A0/12` request, a `0x006a`/D0
response, the D0 parser's `0x177x` event, and classifier `0x267e68`. The
coherent run never produces `0x120c`; both producer sites are gated by SIM-state
notification byte `0x10dcb7`, for which no setting write was observed.
Validated DSP RX families do not feed this path. Service-5's callback is already
registered and organically receives (`0x05f3`, `0x05e2`), while its `0x05e8`
branch remains dormant downstream. Do not replace this firmware contract
by selecting callbacks, posting task results, replaying commit keys, or setting registration state.

The descriptor factory is now identified as `0x24f120`. Its four known callers
(`0x2996aa`, `0x2997dc`, `0x299860`, `0x2998a0`) sit immediately before the callback-7 handler and
construct the same `0x18`-byte radio-session object later attached by `0x24f25c`. All four are
branches of `0x299610`; every successful result is published through
`0x2af798(0xca8a, 0x1e, object)`. The factory remains unexecuted because the lower peer has not
delivered the state which makes task 15 emit `0x09ee`.

## Reply-code 2

The downstream card contract is already mapped. Organic `0x1196` enters `0x207234`, which calls
`0x293f30`. That function constructs `A0 24` CHANGE CHV with a 16-byte body, posts it to task 21 and
waits for the result. Success is return code `2`; only that branch reaches the ENABLE setter at
`0x20733c`. The stateful card implements the required header -> TX-ready -> procedure -> body ->
TX-ready -> `9000` sequence. It still needs an organic `0x1196` run to prove the full path.

## Acceptance gates

- `make verify`: exact 3210 frame and structural oracle.
- `make verify-deep`: exact deep frame and structural oracle.
- `make smoke-3330e RUN_DIR=<dir> SECONDS=3`: bounded second-ROM confidence run.
- Stateful-model trace: natural ATR/PPS/APDUs, then organic `0x1196`, reply code `2`, and
  `0x20733c`, with no injected messages or SIM-state RAM writes.
