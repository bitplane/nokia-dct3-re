# SIM subsystem

This is the concise hardware and firmware contract for the Nokia 3210 v6.00 SIM path. The
address-heavy session map remains in `sim_registration.md`; this document records
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
handler observe the previous firmware descriptor. TXD writes enter a 16-byte UART FIFO rather
than the APDU parser directly. Firmware opens the FIFO through `0x3e=0x04`, fills it, and flushes
with `0x3e=0x00`; only the flush transfers the bytes to T=0 and schedules TX-empty. This permits a
command body larger than one FIFO to advance through multiple TX-empty interrupts. The device
schedules TX-ready and RX-ready on a timer, exposes RX bytes only when ready, and cancels the
trailing event when the RX FIFO empties. A TX-only completion does not hide an
already-readable character; newly returned card bytes are deferred before they
become visible. The card may construct a complete T=0 response synchronously,
so the controller retains a private serialization queue, but `0x39` exposes
only the single character currently transferred to its receive holding register.

Activation and T=0 traffic use the default ten-bit 9,600-bit/s character time,
approximately 1.042 ms per character. Although the synthetic ATR advertises TA1
`0x05`, both ROMs answer PPS with `ff 00 ff`, retaining the default parameters.
A bounded v6.00 trace observes consecutive ATR and PPS receive-register reads
approximately 1.065 ms apart, with RX count one for each character.

## Stateful card device

The 3210 machine profile enables the SIMI/card composition. The external
CONTACT SERVICE configuration fixture disables it through the standard MAME
`HWCFG` configuration port for the negative gate. The card
implements:

- activation reset, ATR and PPS echo;
- T=0 SELECT, STATUS, immediate GET RESPONSE, READ BINARY,
  linear-fixed/cyclic READ/UPDATE RECORD, INCREASE and complete CHV command
  sequencing, plus explicitly profiled RUN GSM ALGORITHM;
- current DF, selected EF and record-pointer state;
- declared GSM 11.11 file metadata and synthetic profile content; and
- persistent mutable `EF_ADN`, `EF_SMS`, `EF_SMSP` and cyclic `EF_ACM`
  contents through MAME device NVRAM.

It does not inject task messages, call firmware handlers, or write SIM/registration RAM. When the
device is disabled, SIMI reads and writes retain the legacy behavior used by the
missing-hardware failure baseline.

## Contract audit

The implemented surface is classified by ownership and evidence:

| Surface | Classification | Basis and limitation |
| --- | --- | --- |
| SIMI register window and FIQ6 route | Extracted partial hardware | `nokia_simi_device` owns offsets `0x36..0x3f`, the decoded IIR cascade, timing and FIQ6; firmware traffic executes through it in both mapped 3210 ROMs. |
| TX FIFO, live fill, and `0x3e` chunk progression | Partial hardware | The 16-byte FIFO and multi-chunk ordering are required by coherent firmware traffic. Exact FIFO-control semantics remain inferred. |
| IIR write-one-clear and causes `0x10`/`0x40` | Derived contract | Firmware acknowledgement and organic TX/RX progression are observed. Timeout/error/removal causes `0x02`, `0x20`, and `0x80` are decoded but not modeled. |
| ATR/PPS and T=0 exchange | Partial card contract | The ordinary initialization conversation is coherent. Both ROMs emit PPS `ff 00 ff`, so controller delivery retains the default approximately 1.042 ms character time. ATR start and card turnaround delays remain approximations. |
| SELECT/STATUS/GET RESPONSE/READ behavior | Partial card contract | It satisfies organically requested initialization, presence polling and the absolute linear-record scan. GET RESPONSE is now scoped to the immediately preceding SELECT or data-producing command. Invalidation, broader errors and removal remain incomplete. |
| UPDATE RECORD and record persistence | Partial card contract | Firmware organically writes both a standard 32-byte ADN record and a 176-byte unread SMS record. ADN is read after a card-NVRAM reload; the MT-SMS gate checks the exact persisted SMS-DELIVER bytes. Current/next/previous modes are modeled but only absolute mode has a firmware acceptance trace. |
| Cyclic EF_ACM and INCREASE | Standards-derived dormant contract | The descriptor, CHV1 access conditions, checked 24-bit addition, `98 50` overflow result, six-byte delayed response and append-only persistence follow GSM 11.11. NSE-3 constructs the exact `A0 32 00 00 03` APDU, but no product firmware acceptance trace currently exercises it. |
| RUN GSM ALGORITHM | Organic card frontier | The card accepts only `A0 88 00 00 10`, consumes the 16-byte RAND and exposes `SRES || Kc` through the immediately following twelve-byte GET RESPONSE. A3/A8 remains explicitly operator-selectable: the card defaults to no algorithm, while the synthetic laboratory subscriber selects TS 55.205 section 5's AES example and a separately provisioned test key. The AES projection has an independent FIPS-derived vector gate. With authentication explicitly enabled, both 3210 v6.00 and 3310 v6.39 organically execute the command and fetch all twelve bytes; neither yet emits MM Authentication Response, so registration is not promoted by this fixture. |
| Default and CPHS filesystem contents | Provisioning fixture | File sizes are ROM-informed and the data is internally coherent enough for the tested paths, but identities and service contents are synthetic test data, not 3210 hardware behavior. |
| CHV support | Standards-derived dormant contract | VERIFY, CHANGE, DISABLE, ENABLE and UNBLOCK have persistent credentials/counters and reset-scoped verification. Ordinary boot does not exercise the complete lifecycle, so it is not a product-runtime promotion. |

The model does not force firmware state or inject RTOS messages. Controller and
card ownership are separate; remaining fidelity debt is ATR start/turnaround timing,
unmodeled errors/removal, and card protocol mixed with subscriber provisioning.

The authentication profile follows
[3GPP TS 55.205](https://www.etsi.org/deliver/etsi_TS/155200_155299/155205/18.00.00_60/ts_155205v180000p.pdf):
A3/A8 are operator-selectable, and section 5 defines the explicit AES projection
used by this laboratory card. It is not labelled as the historical operator
algorithm of any emulated subscriber.

`make verify-radio-authentication-boundary` reproduces the 3210 side of that
frontier from a clean card: one standards-shaped MM Authentication Request,
one firmware-issued RUN GSM ALGORITHM, and one twelve-byte GET RESPONSE. The
network session has typed success and reject continuations and persists them
through its ordinary saved state, but the gate deliberately reports the
currently observed MM-response count instead of manufacturing completion.

The synthetic mandatory-file sizes come from the firmware table at `0x2e0c04`. Implemented content
includes ICCID `2FE2`, ECC `6FB7`, LP `6F05`, IMSI `6F07`, SST `6F38`,
PLMN selector `6F30`, SPN `6F46`, LOCI `6F7E`, and Phase
`6FAE`; other known files are erased (`0xff`). `EF_PHASE` reports Phase 2 (`0x02`). Returning `0x00`
prevents the validated preliminary lifecycle from composing. MF/DF STATUS data uses the GSM 11.11
directory layout, including a `0x15` GSM-specific-data length and CHV status fields; a shifted
layout causes the preliminary pass to repeat.

The base `EF_SST` allocates and activates services 2, 4, 7, 12 and 17.
`EF_SPN` contains
a standards-shaped laboratory provider name; v6.00 reads it organically but
uses its PLMN resource for the idle operator label. `EF_ADN (6F3A)` is
a synthetic 50-by-32-byte linear-fixed EF under `DF_TELECOM`; its count is card
profile policy, not handset hardware. The EF response reports structure and
record length, and the card stores firmware-produced alpha/BCD records without
interpreting their contents. Ordinary deterministic runs reset this card
NVRAM; `PRESERVE_NVRAM=1` and `run-interactive` retain it.

## Current organic result

An unforced coherent run completes:

```text
ATR -> PPS -> preliminary SELECT/STATUS/READ pass
  -> LP -> AD -> SST -> SPN -> LOCI -> IMSI -> ACC -> 2FE6
  -> optional CPHS ONS 6F14 absent -> remaining GSM/vendor EFs
  -> optional 6F99 absent -> timed directory-presence monitor
```

Observed reads include ICCID, ECC, Phase, LP, SST, SPN, IMSI and ACC. SIM-enable byte `[0x111c79]`
changes to 1 organically at about 1.309 s; the preliminary card-acceptance transition passes
without firmware-state forcing.

With service 2 advertised, firmware later selects `DF_TELECOM/EF_ADN` and
scans all 50 records through absolute `A0 B2`. Adding a contact produces
`A0 DC 01 04 20`; the validated fixture writes `ADA` and number `123` to
record 1, receives `9000`, displays `Saved`, and renders `ADA` from the same
card after restart.

The card also declares ten free 176-byte `EF_SMS (6F3C)` records and two
44-byte `EF_SMSP (6F42)` records under `DF_TELECOM`. The first SMSP record
contains the fixture service-centre address. In
`make verify-radio-incoming-sms`, firmware organically selects these files and
writes record 1 with status `03` plus the exact SMSC/SMS-DELIVER representation
of unread text `hello`; the verifier checks the resulting card NVRAM bytes.
The port-addressed ringtone fixture instead leaves all `EF_SMS` records free,
which is part of its application-routing oracle rather than a missing SIM
write.

`6F14` is the optional CPHS Operator Name String; the card does not advertise CPHS and need not
provide it. Caution: a card that accepts every unknown SELECT and advertises a zero-byte EF
causes initialization to restart. Unsupported SELECT returns GSM 11.11 `94 04` (file ID not
found) without changing the current selection; firmware handles that organic optional-file
result and continues its initialization sequence. The CPHS/`94 04` card contract is
authoritative in `sim_emulator_scope.md`.

At about 1.427 s task 20 reaches its card-presence monitor at `0x2028a4` with SIM enable 1,
no-SIM 0, and ready byte `0x10dcaf` equal to 1. The card tracks current DF separately from the
selected EF, so STATUS reports `7F40`; four consecutive checks take the monitor's steady exit at
`0x20290a`, not its changed-directory path at `0x2028f4`. The monitor issues `A0 F2` and explicitly
rearms timer `0xea` at `0x2028fc` with delay `0x181`, producing a roughly 42 ms cadence. This perpetual traffic
is a normal firmware-owned presence check, not a repeated initialization pass. Task 1 subsequently
enters mode `0x0004` at about 1.435 s.

Ordinary GSM Location Updating is now independently verified. After MM acceptance,
firmware selects `EF_LOCI` and persists location/status fields with two `UPDATE
BINARY` commands through SIMI/FIQ6. It does not require callback 7 or the
`0x1196/0x1199` lower-session commit family. Those remain mapped later-session
contracts in `sim_registration.md` and must not be forced.

The mapped task-21 `0x120c -> A0/12 -> D0 -> 0x177x` route is GSM
11.14 SIM Toolkit: TERMINAL PROFILE arms latch `0x10dcb7`, `91xx` advertises a
proactive command, and `A0/12` FETCH retrieves it. The current EF_PHASE=2 card
correctly leaves this path dormant, and firmware's profile-download function is
part of a later session lifecycle even with a phase-3 isolation card.
This route is therefore separate from ordinary registration.
Validated DSP RX families do not feed this SAT path. Service-5's callback is already
registered and organically receives (`0x05f3`, `0x05e2`), while its `0x05e8`
branch remains dormant downstream. Do not replace this firmware contract
by selecting callbacks, posting task results, replaying commit keys, or setting registration state.

MMI callbacks, report 7, and the unattended UI lifecycle are outside the SIM
boundary. Report 7 is a shutdown/power-lifecycle report, not a SIM-ready event.
The mapped session descriptor and callback chains remain
in `sim_registration.md`, `mmi_layer.md`, and normalized evidence;
they are intentionally not repeated in this concise hardware/card contract.

## Deferred CHV transaction: reply code 2

This is a mapped later card contract, not an outstanding ordinary-SIM-init
gate. The non-CPHS initialization pass raises SIM enable without traversing
this path. Retain it for the first organic PIN/CHV lifecycle that requests it.

The downstream card contract is already mapped. Organic `0x1196` enters `0x207234`, which calls
`0x293f30`. That function constructs `A0 24` CHANGE CHV with a 16-byte body, posts it to task 21 and
waits for the result. Success is return code `2`; only that branch reaches its ENABLE setter at
`0x20733c`. The stateful card implements the required header -> TX-ready -> procedure -> body ->
TX-ready -> `9000` sequence. The full path remains unvalidated because current
ordinary boot does not organically request `0x1196`.

## Current acceptance gates

- `make verify`: explicit missing-hardware structural oracle.
- `make run-frontier`: current external-service/SIM research profile.
- `make verify-sim-phonebook`: organic two-launch ADN update and persistence
  oracle.
- `make verify-radio-incoming-sms`: organic 176-byte `EF_SMS` update and exact
  persisted unread SMS record.
- `make smoke-3330e RUN_DIR=<dir> SECONDS=3`: bounded second-ROM confidence run.
- Stateful-model trace: natural ATR/PPS and the ordinary non-CPHS EF pass, with
  SIM enable rising and the timed presence monitor starting without injected
  messages or SIM-state RAM writes.

The frontier predicate protects the final organic SIM-enabled state, but there
is no focused device test for FIFO reset/fill behavior, IIR acknowledgement,
multi-chunk ordering, T=0 procedure sequencing, file metadata, timeout/error
causes, removal, or save-state resumption. Add that coverage before materially
changing the controller/card contract.

The controller currently locates the card through the sibling tag
`^sim_card`. Replace this with a configurable finder or transmit callback before
reusing SIMI in a machine whose card topology or tag differs.
