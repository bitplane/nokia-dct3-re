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

FIQ line 6 is the SIMI UART interrupt. The firmware route is:

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
fault-path contracts awaiting parity/framing evidence.

Socket presence is a separate MAD2 input. Both mapped ROMs route FIQ7 to the
same handler (`0x2a06f0` in v6.00, `0x29dc30` in v5.01). The handler samples
SIMI control/status bit 3: set means absent and posts the static task-21 removal
result; clear means inserted and puts the SIM transport in state 5 before
arming its ordinary retry timer. The controller therefore overlays bit 3 from
the physical socket level and pulses FIQ7 on live insertion/removal edges.
Cold configuration establishes the initial level without manufacturing a
hot-plug edge.

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
so the controller retains a private serialization queue, but `0x3c` exposes
only the single character currently transferred to its receive holding register.

Activation and T=0 traffic use the default ten-bit 9,600-bit/s character time,
approximately 1.042 ms per character. Although the synthetic ATR advertises TA1
`0x05`, both ROMs answer PPS with `ff 00 ff`, retaining the default parameters.
A bounded v6.00 trace observes consecutive ATR and PPS receive-register reads
approximately 1.065 ms apart, with RX count one for each character.

The card accepts a checksum-valid T=0 PTS request only before its first APDU.
`ff 00 ff` retains default F/D; explicit default `ff 10 11 fe` is echoed.
Unsupported F/D requests receive the default-parameter response, not an echo
claiming an unimplemented speed. Bad checksums, reserved fields, other protocols
and unsupported PTS2/PTS3 fields receive no response. Deactivation aborts a
partial negotiation. This is byte-protocol coverage, not enhanced SIM clock or
terminal waiting-time implementation. The default-speed fallback follows
[GSM 11.11 v5.3.0, sections 5.8.2--5.8.3](https://www.etsi.org/deliver/etsi_gts/11/1111/05.03.00_60/gsmts_1111v050300p.pdf).
`python3 tools/run_sim_pts.py RUN --mame mame/mame` checks ten request families,
partial-request save/restore, reset recovery and a following SELECT MF; it
claims neither a handset boot nor physical baud-divider validation.

`verify-simi-restore` runs the development-only `simitest` controller/card
composition without a handset CPU or ROM. It checkpoints SELECT MF with one
body byte still in TX and the card awaiting that byte, then compares the replayed
`9f 16` status and TX-empty/RX-ready sequence. A second checkpoint during GET
RESPONSE replays the procedure byte, 22-byte MF metadata and `90 00` status,
with identical RX-prefix, interrupt ordering and pending timer deadlines.
A third checkpoint saves a failed VERIFY CHV2 after its attempt has been
consumed but before its status is received. A second wrong PIN2 and STATUS
prove one attempt remains before restore; replaying the saved status and
issuing STATUS then prove two attempts remain, without repeating VERIFY.
The security counter and queued response therefore restore together; the
unchanged CHV1 counter is checked independently.
Omitting the saved retry-counter array makes this final STATUS comparison fail.
Work started after restore is compared by elapsed time; already-pending RX
events retain their saved absolute deadlines. Omitting the card's saved
body-reception latch makes the first replay fail; the unmodified devices pass.
This is executable save-state conformance for the selected transactions,
not physical UART timing, every APDU boundary or handset service acceptance.

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

The synthetic card implements [GSM 09.91 section 5.1.1 option 1](https://www.etsi.org/deliver/etsi_etr/100_199/174/01_60/etr_174e01p.pdf)
for legacy DCS1800 MEs: selecting `7f21` uses the same backing application
as `7f20`, while SELECT/GET RESPONSE and STATUS retain visible identifier
`7f21`. EF parent/access checks and RUN GSM ALGORITHM use the canonical
application; Telecom remains a separate directory. There is no duplicate
mutable application store or NVRAM-format change. The controller/card gate
checks visible FID, selection save/restore, EF_PHASE access, authentication,
and failure to select that EF from Telecom. This is an issuer-selected
compatibility policy, not a claim that every real card aliases the directories.

SIM NVRAM loading validates the complete serialized record before installing
card fields. The supported append-only layouts retain base files, then SMS/SMSP,
then security state, then EF_ACM, then preferred/forbidden PLMN lists, then
EF_SMSS, then EF_ACMmax/EF_PUCT; missing later fields receive their declared
defaults. Truncated records, trailing bytes and non-boolean CHV1-enable flags
are rejected atomically. Device startup initializes defaults because MAME
reports load failure without resetting NVRAM. `verify-sim-nvram-atomic` exercises
eighteen cases through actual isolated MAME processes, including all six legacy
layouts and an unchanged full record. Its short-ADN negative detects the former
partial-load defect. This is storage conformance, not a handset-service claim;
physical phonebook and security gates remain separate.
The base record is 3570 bytes; its original 3562-byte prefix and all existing
file/security offsets are unchanged. `run_sim_nvram_atomic.py --card-seed`
additionally cold-loads an independently produced current record and requires
byte-exact preservation, rejecting silent reinitialization.

It does not inject task messages, call firmware handlers, or write SIM/registration RAM. When the
device is disabled, SIMI reads and writes retain the legacy behavior used by the
missing-hardware failure baseline.

## Contract audit

The card implements the GSM application class `A0`. Other APDU classes return
`6E 00` without executing a command or accepting its body. Incoming `P3=0`
means no body, whereas outgoing `P3=0` requests 256 bytes; an empty VERIFY
therefore returns `67 00` without consuming a retry. These rules follow
[GSM 11.11 sections 9.1, 9.2 and 9.4.5](https://www.etsi.org/deliver/etsi_i_ets/300600_300699/300608/08_60/ets_300608e08p.pdf).
The isolated `verify-simi-restore` fixture checks three rejected classes,
empty VERIFY, unchanged selection/retries and a subsequent valid APDU. This
is card/controller conformance, not additional handset-service coverage.
SELECT, STATUS and GET RESPONSE reject nonzero reserved `P1/P2` with `6B 00`;
SELECT rejects a file-ID length other than two with `67 00`. The same fixture
checks all six reserved-parameter cases and SELECT lengths zero, one and three
before a valid STATUS confirms unchanged file selection and PIN retries.

GET RESPONSE also accepts the implicitly selected MF immediately after card
activation. That allowance and pending command responses are consumed once;
an intervening command or repeated GET RESPONSE returns `6F 00`, following
[GSM 11.11 sections 9.2.18 and 9.4.5](https://www.etsi.org/deliver/etsi_i_ets/300600_300699/300608/08_60/ets_300608e08p.pdf).
The isolated fixture checks initial MF access and its save/replay, ten further
transactions covering authentication/MF/DF/EF responses, and STATUS interrupting
a SELECT/GET RESPONSE pair. These are card-conformance checks, not proof that
a handset uses first-command GET RESPONSE.

Record addressing follows [GSM 11.11 sections 6.4 and 8.5/8.6](https://www.etsi.org/deliver/etsi_i_ets/300600_300699/300608/08_60/ets_300608e08p.pdf): linear EFs do
not wrap at their boundaries; cyclic EFs do. Current access is `P1=0/P2=04`,
absolute access leaves the pointer unchanged, and successful next/previous
access alone advances it. A failed access preserves the pointer. The isolated
fixture exercises eleven READ RECORD cases on virgin EF_ADN, including the
last-record boundary, current access, absolute-pointer preservation and invalid
mode bits. Thirty additional APDU transactions use distinct ADN contents to
validate current/absolute/relative writes, failed-write data preservation and
restoration of the record contents and pointer after an overwrite. They also
check cyclic EF_ACM selection, all three disallowed update modes, successful
PREVIOUS update and wrapping reads. Only the existing single-record cyclic
EF is covered; multi-record cyclic rotation is not implemented or validated.

READ/UPDATE BINARY use the full `P1:P2` offset as specified in GSM 11.11
sections 9.2.3/9.2.4; the high bit is not a short-file selector. Out-of-range
access returns `94 02` without modifying file contents. Fourteen isolated
binary transactions on EF_LOCI check high-byte offsets `80` and `FF`, the
last valid byte, the first invalid byte and readback after rejected writes.
These are serial-path card conformance checks, not new handset capabilities.

SEEK implements both types and all four linear-fixed search directions under
[GSM 11.11 sections 8.7 and 9.2.7](https://www.etsi.org/deliver/etsi_i_ets/300600_300699/300608/08_60/ets_300608e08p.pdf).
It matches record prefixes without wrapping, commits the pointer only on
success, and exposes type-2 record numbers through GET RESPONSE. The serial
fixture checks both undefined-pointer starts, one- and sixteen-byte patterns,
interior-only mismatch, six parameter errors, incompatible cyclic/transparent
files, and pending-result save/replay. Directory-only selection reports `94 00`
for SEEK and both read commands; incompatible EFs report `94 08`. Current
ADN, SMS, SMSP and ACM require CHV1 for READ; SEEK applies the selected
file's READ condition. Disabled, unblocked CHV1 satisfies that condition; an enabled,
unverified card returns `98 04`, including after restoration of an earlier
unverified checkpoint. FCP advertises the same policy. The isolated serial
fixture covers denial, wrong PIN, successful verification and restoration.
Other transparent/vendor-file access conditions remain a separate audit.
EF_IMSI (`6F07`) independently enforces and advertises CHV1 READ, as required
by GSM 11.11 section 10.2.2. READ BINARY applies the descriptor's access
condition before returning data. The serial fixture proves pre-PIN denial,
successful verification and readback, then denial after restoring the earlier
unverified session. Separate controller-driven activation checks restart an
authenticated card, then disconnect/reinsert/reactivate it; both produce ATR
and require fresh VERIFY before IMSI readback. These are isolated socket/card
contracts, not handset hot-plug acceptance. They do not close the remaining
transparent/vendor-file policy audit.
EF_PUCT (`6F41`) also requires CHV1 READ (GSM 11.11 10.2.12). Its FCP and
READ BINARY now agree; the serial fixture verifies denial before PIN,
successful five-byte readback, and denial after unverified-state restoration.
The laboratory profile fixes ACMmax (`6F37`) and PUCT UPDATE to CHV1, as for
its existing ACM profile. GSM 11.11 10.2.6/10.2.12 permits this administratively
selected CHV1/CHV2 policy; this does not claim all physical cards use CHV1.
Both files now have mutable card-owned bytes with matching FCP, save-state
and append-only NVRAM ownership. Serial tests check blocked-PIN denial,
successful whole-file writes, exact readback, out-of-range write preservation
and restoration of changed contents. Reset/reinsertion authorization replays
retain their separate unverified baseline rather than an AoC mutation snapshot.
The atomic loader gate cold-loads non-default values and rejects partial AoC
extensions before installing fields.
This is card conformance; handset tariff/ceiling editing remains unvalidated.

EF_SMSS (`6F43`, SMS status) belongs to DF_TELECOM and enforces CHV1
READ/UPDATE, with matching FCP (GSM 11.11 10.3.7). Its two card-owned bytes
hold last-used TP-MR and the capacity-exceeded notification flag; the initial
profile is `FF FF` (flag unset/capacity available). The serial fixture checks
wrong-parent selection, pre-PIN denial, mutation, failed bounds preservation
and exact restoration. Both bytes are saved and appended to NVRAM; complete
legacy PLMN records receive defaults, while partial extensions are rejected
before any retained state is installed. A serial-written record is independently
cold-loaded byte-for-byte by the atomic-storage gate.
Physical two-process SMS gates additionally correlate retained SMSS with
actual submitted TP-MR and its cold increment on the 6210 and 8210. Their
first references differ (1 and 0 respectively), as do relative TP-VP bytes
(`FF` and `A7`); the shared checker does not normalize those product choices.
The standard GSM transparent set Kc (`6F20`), PLMNsel (`6F30`), HPLMN
(`6F31`), ACMmax (`6F37`), BCCH (`6F74`), ACC (`6F78`), FPLMN (`6F7B`),
LOCI (`6F7E`) and SST (`6F38`) also enforces CHV1 READ. Serial tests check
each FCP, denial before verification and exact one-byte readback afterward,
including after reset and reinsertion. Other unclassified file policies
remain separate work. IMSI, HPLMN, ACC and SST are not CHV1-writable: GSM 11.11
10.2.2/10.2.5/10.2.15/10.2.7 assigns UPDATE to ADM. Their FCP reports READ CHV1
and UPDATE ADM (laboratory ADM level 4), while the card implements no ADM
credential or administrative update command. The serial fixture checks that
a verified PIN still receives `98 04` for UPDATE and each file's contents
remain unchanged, including on reset/reinsertion replay. This corrects
advertised policy; it does not claim administrator provisioning support.
The normative reference is
[GSM 11.11 v4.20.1](https://www.etsi.org/deliver/etsi_i_ets/300600_300699/300608/08_60/ets_300608e08p.pdf),
whose access-level table permits ADM levels 4 through 14.
The bounded serial runner permits 16 seconds
for the expanded command sequences and still requires every completion marker.
PLMNsel and FPLMN now also support CHV1-protected UPDATE BINARY, as specified
in GSM 11.11 10.2.4/10.2.16. Their 24-/12-byte bodies are mutable card-owned
arrays with exact FCP READ/UPDATE conditions, bounds checking, save/load and
append-only NVRAM persistence. The serial fixture verifies both updates and
restoration, then checks the exact serialized bodies; the storage gate cold-loads
that same record. The 6210 forbidden-PLMN persistence gate additionally proves
firmware-originated cause-11 handling for visited test PLMN 001-02: an actual
FPLMN update survives a cold boot and prevents automatic access. Clearing only
that list allows the same accepting cell to register and persist its LAI.
This is a bounded 30-second cold-selection comparison, not a claim about every
roaming PLMN. The 6210 home-PLMN rejection control proves the complementary rule:
cause 11 updates EF_LOCI to roaming-not-allowed but leaves EF_FPLMN erased.
The HPLMN must not be forbidden (GSM 03.22). International visited selection
remains separate; see `6210_bringup.md` for the exact coverage.
These record-file conditions follow GSM 11.11 sections 10.2.8 and 10.3
([ETSI specification](https://www.etsi.org/deliver/etsi_i_ets/300600_300699/300608/08_60/ets_300608e08p.pdf)).
No retained handset trace uses SEEK;
this is standards-derived card conformance, not new UI capability.

RUN GSM ALGORITHM also requires current directory DFGSM and CHV1
authorization (satisfied while CHV1 is disabled and unblocked), following GSM 11.11 8.16.
The serial fixture proves `98 04` before PIN verification, `94 08` under
DF_TELECOM, no pending authentication result after either rejection, and
independently computed SRES/Kc after valid verification and directory selection.
These checks are repeated following controller reset and reinsertion.
CHV1 authorization also requires a nonzero retry count. Three wrong ENABLE
attempts can block a still-disabled CHV1 (GSM 11.11 8.12); disabled mode must
not bypass that state. One shared authorization predicate covers protected
READ, UPDATE and authentication. Serial tests block this disabled card, deny
all three operations, retain ALWAYS ICCID access, and recover through UNBLOCK
without changing the ADN record rejected by the failed update.
With an authentication algorithm selected, RUN GSM ALGORITHM rejects nonzero
`P1/P2` with `6B 00` and a RAND length other than sixteen with `67 00`
(GSM 11.11 sections 9.2.16 and 9.4.5). With no algorithm selected it remains
unsupported (`6D 00`). The isolated fixture rejects two parameter cases and
lengths zero, fifteen and seventeen, then checks a valid laboratory RAND and
the twelve-byte SRES/Kc response. This checks parser recovery and framing;
the independent algorithm vector gate remains the cryptographic oracle.

The implemented surface is classified by ownership and evidence:

| Surface | Classification | Basis and limitation |
| --- | --- | --- |
| SIMI register window and FIQ6 route | Extracted partial hardware | `nokia_simi_device` owns offsets `0x36..0x3f`, the decoded IIR cascade, timing and FIQ6; firmware traffic executes through it in both mapped 3210 ROMs. |
| TX FIFO, live fill, and `0x3e` chunk progression | Partial hardware | The 16-byte FIFO and multi-chunk ordering are required by coherent firmware traffic. Exact FIFO-control semantics remain inferred. |
| IIR write-one-clear and causes `0x10`/`0x40` | Derived contract | Firmware acknowledgement and organic TX/RX progression are observed. Decoded causes `0x02`, `0x20`, and `0x80` have a typed, activity-gated ingress for a future electrical/card backend, but normal traffic never synthesizes them because their physical trigger conditions and timing remain unknown. |
| Socket detect status bit 3 and FIQ7 | Validated hardware contract | Physical removal deactivates transport/card state and posts the firmware removal result; reinsertion organically reactivates SIMI, ATR and file initialization in v6.00 and v5.01. Phase 2+, removal and save/load variants are independently gated. |
| ATR/PPS and T=0 exchange | Partial card contract | The ordinary initialization conversation is coherent. Both ROMs emit PPS `ff 00 ff`, so controller delivery retains the default approximately 1.042 ms character time. ATR start and card turnaround delays remain approximations. |
| SELECT/STATUS/GET RESPONSE/READ behavior | Partial card contract | It satisfies organically requested initialization, presence polling and the absolute linear-record scan. GET RESPONSE is scoped to the preceding SELECT/data-producing command or implicit MF selection after activation. Opt-in FDN images add CHV2-controlled ADN INVALIDATE/REHABILITATE and persistent validity, with strict/readable-invalid card conformance. NSM-1 physical activation, cold On/Off interpretation, deactivation and protected-record operations are qualified through the identity-session handshake; per-product limits remain in sim_emulator_scope.md and model_coverage.md. Organic REFRESH does not issue validity commands. |
| SEEK record search | Standards-derived dormant contract | Both types, all four linear-fixed directions, prefix matching, failure-preserved pointers and pending-result restoration have isolated serial conformance coverage. No handset acceptance trace currently issues SEEK. |
| UPDATE RECORD and record persistence | Partial card contract | Firmware organically writes both a standard 32-byte ADN record and a 176-byte unread SMS record. ADN is read after a card-NVRAM reload; the MT-SMS gate checks the exact persisted SMS-DELIVER bytes. Relative/current record reads and writes, write-failure preservation and single-record cyclic updates have isolated conformance coverage; only absolute mode has a handset acceptance trace. Multi-record cyclic UPDATE RECORD ordering remains unsupported. |
| Cyclic EF_ACM and INCREASE | Standards-derived dormant contract | The descriptor, CHV1 access conditions, checked 24-bit addition, `98 50` overflow result, six-byte delayed response and append-only persistence follow GSM 11.11. NSE-3 constructs the exact `A0 32 00 00 03` APDU, but no product firmware acceptance trace currently exercises it. |
| RUN GSM ALGORITHM | Organic authenticated registration | The card accepts only `A0 88 00 00 10`, consumes the 16-byte RAND and exposes `SRES || Kc` through the immediately following twelve-byte GET RESPONSE. A3/A8 remains explicitly operator-selectable: the card defaults to no algorithm, while the synthetic laboratory subscriber selects TS 55.205 section 5's AES example and a separately provisioned test key. The AES projection has an independent FIPS-derived vector gate. With authentication explicitly enabled, both 3210 v6.00 and 3310 v6.39 organically execute the command, fetch all twelve bytes, require typed result `0x0066`, copy Kc, publish SRES and emit MM Authentication Response. Both then accept Location Updating Accept, persist EF_LOCI, release the dedicated channel and return to camp. |
| Default and CPHS filesystem contents | Provisioning fixture | File sizes are ROM-informed and the data is internally coherent enough for the tested paths, but identities and service contents are synthetic test data, not 3210 hardware behavior. |
| CHV support | Validated card contract | PIN-enabled 3210 startup organically issues VERIFY. Three wrong PINs produce `98 04`, `98 04`, then blocked `98 40`; the firmware presents its PUK editor and organically issues UNBLOCK with a replacement PIN. The security-settings UI organically issues DISABLE, ENABLE and CHANGE, including a rejected CHANGE that consumes one retry without modifying the credential. Credentials, enable state and retry counters persist in card NVRAM, authorization is reset/card-removal scoped, and a live retry survives MAME save/load. A second boot verifies both toggle persistence and the replacement PIN. The same startup VERIFY path is reproduced by v5.01. |

The model does not force firmware state or inject RTOS messages. Controller and
card ownership are separate; remaining fidelity debt is ATR start/turnaround
timing and the physical sources/timing of controller timeout and serial-error
signals. Subscriber identity and subscription data now come from the shared
typed profile documented in `subscriber_profile.md`. Physical socket edges use SIMI status bit 3 and
MAD2 FIQ7; firmware owns removal handling, reactivation and the repeated file
initialization after reinsertion.

The authentication profile follows
[3GPP TS 55.205](https://www.etsi.org/deliver/etsi_TS/155200_155299/155205/18.00.00_60/ts_155205v180000p.pdf):
A3/A8 are operator-selectable, and section 5 defines the explicit AES projection
used by this laboratory card. It is not labelled as the historical operator
algorithm of any emulated subscriber.

`make verify-radio-authentication-boundary` reproduces the 3210 lifecycle from
a clean card: one standards-shaped MM Authentication Request,
one firmware-issued RUN GSM ALGORITHM, one twelve-byte GET RESPONSE and the
firmware's accepted-result branch, queued outbound radio primitive and MM
Authentication Response. The
v6.00 consumer is at `0x2068bc`;
NHM-5 v6.39 has the same instruction-for-instruction contract relocated to
`0x21bca4`. Both require command result `2`, GET RESPONSE result `0x0066`,
copy the eight-byte Kc, and call their SRES publisher. The products then diverge
in internal routing: NSE-8 posts status `0x0a01` to task 15 and NHM-5 posts
`0x0a65` to task 12. Their relocated consumers nevertheless converge on the
same radio-state contract: copy the SRES primitive and set pending bit `0x10`.
Runtime writes prove that bit is set at `0x10d12c` and `0x10ab90`
respectively. Continuing independent downlink opportunities on the assigned
SDCCH lets each asynchronous firmware result reach the next uplink request.
The gate requires authenticated Location Updating Accept, EF_LOCI persistence,
dedicated-channel release and return to idle camp.

The synthetic mandatory-file sizes come from the firmware table at `0x2e0c04`. Implemented content
includes ICCID `2FE2`, ECC `6FB7`, LP `6F05`, IMSI `6F07`, SST `6F38`,
PLMN selector `6F30`, SPN `6F46`, LOCI `6F7E`, and Phase
`6FAE`; other known files are erased (`0xff`). `EF_PHASE` reports Phase 2 (`0x02`). Returning `0x00`
prevents the validated preliminary lifecycle from composing. MF/DF STATUS data uses the GSM 11.11
directory layout, including a `0x15` GSM-specific-data length and CHV status fields; a shifted
layout causes the preliminary pass to repeat.

The base `EF_SST` allocates and activates services 1, 2, 4, 7, 12 and 17.
Service 1 advertises the CHV1-disable function implemented by the card; omitting
it makes the firmware correctly reject `PIN code request` as unavailable before
issuing an APDU.
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
contains the fixture service-centre address. `make verify-radio-outgoing-sms-smsc`
navigates the physical settings UI, replaces that number, observes firmware
write the complete record through `UPDATE RECORD`, and proves the following
`SMS-SUBMIT` uses the persisted value. In
`make verify-radio-incoming-sms`, firmware organically selects these files and
writes record 1 with status `03` plus the exact SMSC/SMS-DELIVER representation
of unread text `hello`; the verifier checks the resulting card NVRAM bytes.
The ordinary inbox gates additionally prove firmware-owned `03` → `01` read
and `01` → `00` erase transitions, preserved-NVRAM discovery, selective
two-message deletion and organic ten-record capacity exhaustion. The card
does not decode GSM 7-bit text or maintain a parallel inbox; see
`ordinary_sms.md`.
The port-addressed ringtone fixture instead leaves all `EF_SMS` records free,
which is part of its application-routing oracle rather than a missing SIM
write. Matched concatenation and physical RTPL playback occur while card NVRAM
remains unchanged; wrong reference, total and destination port also leave it
free. Thus the card owns neither temporary multipart retention nor the
received-ringtone parser state at this frontier.

The opt-in Phase-2+ application also exercises the opposite direction through
the handset rather than implementing SMS inside the card. A physical menu
selection makes the card advertise and serve a `SEND SHORT MESSAGE` proactive
command. The firmware converts its TPDU into the ordinary SAPI-3
`CP-DATA/RP-DATA/SMS-SUBMIT` path; the laboratory network acknowledges CP and
RP, the handset returns a successful TERMINAL RESPONSE, and RR releases the
dedicated channel. `make verify-sim-toolkit-sms` protects that complete
SIMI/FIQ6-to-radio composition.
The adjacent `SET UP CALL` profile supplies the same destination as a proactive
address TLV. Firmware presents its own confirmation UI, originates the normal
CC SETUP, completes assignment/alerting/connect, and returns the command result
to the card. `make verify-sim-toolkit-call` then physically ends the call and
requires CC and RR release. Speech media remains covered by the ordinary call
gates rather than being implied by this signalling-focused toolkit fixture.

`SELECT ITEM` is also supported: the card supplies two item TLVs, physical Down
and Select choose the second firmware-rendered entry, and the ME returns item
identifier `02` in TERMINAL RESPONSE. Conversely, NSE-8 v6.00 returns general
result `31` (command type not understood by ME) for `SET UP EVENT LIST` requesting
User Activity and Idle Screen Available. The card accepts that result and does
not expect or synthesize an Event Download envelope. These are separate
positive and negative gates because event download is not an applicable 3210
capability under this terminal profile.

Qualifier-0 `REFRESH` takes a distinct but successful path. NSE-8 re-downloads
TERMINAL PROFILE before returning the REFRESH TERMINAL RESPONSE. The card must
therefore preserve that in-flight command across re-profiling, accept its one
delayed completion, and only then schedule the new application session.
`make verify-sim-toolkit-refresh` protects this ordering and catches both lost
completion state and overlapping proactive commands. The command-ready timer
must publish DISPLAY TEXT's command ID and number before setting pending;
REFRESH completion cleared the old ID, so merely logging readiness previously
advertised9100 and caused zero-length FETCH attempts. The NSB-7 REFRESH gate
now proves a real post-refresh FETCH and a complete second interactive/menu
session, not only the timer's readiness log.

The NSE-8 `POLL INTERVAL`/`POLLING OFF` fixture returns result `00` plus a
five-second effective-duration TLV, then accepts the immediately chained off
command. No later STATUS occurs in that bounded run; the existing gate
preserves this measured silence as a fixture-specific regression.
It is not a universal prohibition: [GSM 11.14 section 6.4.14](https://www.etsi.org/deliver/etsi_TS/101200_101299/101267/08.02.01_60/ts_101267v080201p.pdf)
disables proactive polling without affecting SIM presence detection.
The NSB-7 fixture accepts the same commands while ordinary STATUS continues
at an observed roughly31-second cadence. The shared protocol checker is
separate from those product observations. Neither immediately chained fixture
proves that a five-second polling schedule ran before OFF; reported duration
alone is not elapsed-time evidence.

`PLAY TONE` reaches the modeled DSP audio boundary: general-beep tone `10`
produces an organic 1750 Hz start and stop. With an alpha identifier, physical
Select clears the presentation and the ME returns result `11` (user cleared
down before end). The bounded unattended variant stops audio but has not
produced a successful terminal response, so the gate claims playback and
cancellation only, not automatic command completion.

`SEND DTMF` is not supported by this 3210 firmware's terminal profile. The
negative fixture first establishes and connects the same proactive call, then
supplies `12#`; the ME returns result `31` while the call is demonstrably active
and subsequently releases it normally. This rules out “no active call” as the
cause without inventing a DTMF transport the handset does not expose.

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
11.14 SIM Toolkit. The default EF_PHASE=2 card correctly leaves it dormant.
The opt-in Phase-2+ profile returns EF_PHASE `03`; task 20 then organically
sends TERMINAL PROFILE (`A0 10`) and accepts `90 00`. The card application
later marks one DISPLAY TEXT command ready. A subsequent ordinary command
completion returns `91 16`, task 21 posts `0x120c`, task 20 sends FETCH
(`A0 12`), and the returned D0 BER-TLV reaches the firmware's `0x177x`
router. The MMI renders `DCT3 SAT`; Left Softkey dismissal produces TERMINAL
RESPONSE (`A0 14`) with general result `00`. v5.01 and v6.00 both complete the
same transaction through SIMI/FIQ6.

The same card contract is corroborated across later DCT3 products. NHM-5
v6.39 (3310) reads EF_PHASE 3, submits TERMINAL PROFILE, FETCHes the same
DISPLAY TEXT command and returns result `00` after a physical Navi press.
NHM-6 v4.50 (3330) reaches the same FETCH boundary but returns `20 01`
(terminal currently unable, screen busy) in its current presentation
lifecycle. That result is preserved as a product limitation, not rewritten as
card or transport failure.

The separate NSM-1 v5.23 native-compatibility composition (6150) independently
advertises `0b41ff7700`, displays the same card payload and returns result00
only after physical Menu. Matched Phase-2 input has no proactive traffic;
complete handset/card storage is unchanged. The NSK-3 v5.61 composition
(6130) instead advertises `03018000` and automatically returns result30
(beyond ME capabilities), without displaying the text. These observed
product-specific outcomes must not be unified into a fabricated success.
Own fixture scope and gates are in [6150 bring-up](6150_bringup.md) and
[6130 bring-up](6130_bringup.md).

The5190's own clean-idle composition independently passes physical GET INKEY5
and GET INPUT42 after DISPLAY TEXT, returning its own `0f41ff7700` terminal
profile and exact input responses with unchanged whole EEPROM/card/flash.
Its physical Navi contact differs from the6150 despite sharing the lab schedule.
`nsb1_toolkit_check.py --interactive` guards this independently of its older
Field test01 DISPLAY TEXT fixture; native RF and speech remain unqualified.
The5190 also installs DCT3 menu and returns distinct physical Continue/Exit
envelopes. Both ACK-only selections return to the clean list with the selected
item highlighted, unlike the separate6150/6190 Wait observations. Product
wrappers retain these different pixel contracts (`--menu-item 1` / `2`).
The5190 also independently accepts a card-issued SELECT ITEM after Continue;
physical Beta returns the exact item-2 result and restores the clean installed
list with unchanged complete storage (`--select-item`).
The5190 pending-display restoration gate also proves one FETCH followed by
two physical successful responses, observed native architecture/checksum/I/O
restoration and identical returned-idle pixels with unchanged complete storage
(`nsb1_toolkit_state_check.py`). This is not mid-character SIMI coverage.

Separate5190 profile9 additionally completes REFRESH through own terminal
re-profiling and successful response, displaying SIM update started before
exact idle return. Its product-specific settings write has a pinned complete
EEPROM output; card/flash remain unchanged (`nsb1_toolkit_check.py --refresh`).
This is not inferred from the6150 result or a later application-cycle claim.

The6150 also independently handles a three-command external card: physical
DISPLAY TEXT dismissal, GET INKEY5 and GET INPUT42 produce exact successful
responses and return to its own clean idle frame. Full persistent card,
EEPROM and flash are unchanged. This is not inferred from the6190's support;
the common lab command bytes/validator are shared while product inputs and
acceptance remain separate (`nsm1_toolkit_check.py --interactive`).
Separate6150 profile4 runs also install DCT3 menu and physically select
Continue/Exit, returning distinct item-1/item-2 envelopes with9000 and unchanged
complete storage. Both ACK-only results show Wait; automatic menu return is
not promoted (`--menu-item 1` / `--menu-item 2`).
A separate6150 profile7 follows Continue with SELECT ITEM. Physical Beta
selection emits the exact item-2 terminal response, accepted with9000, and
returns to the clean installed list with unchanged complete storage
(`--select-item`). This does not change the ACK-only return boundary.
Separate6150 profile9 accepts REFRESH qualifier00, re-downloads its own
terminal profile before returning success, displays SIM card update started
and returns to exact idle. Its complete EEPROM has a separately pinned
firmware-owned settings update; card/flash stay unchanged. Full re-read wire
capture uses an explicit higher observation cap, not a relaxed storage gate
(`--refresh`; details in6150_bringup.md). A later application restart is not
qualified by this92s window.
The6150 independently presents PLAY TONE and physically cancels it with
result11, returning to the installed menu with unchanged whole storage.
The matched no-key control remains pending through92s despite three nonstopped
native samples. This is a cancellation contract, not audible playback
(`nsm1_toolkit_check.py --tone cancel` / `--tone hold`).
The separate `--tone late` window reaches organic Timer1/FIQ5 expiry near93s,
successful response and exact menu return without a key. The shorter hold is
horizon-limited; current clock calibration does not establish duration accuracy.
The separate publication census observes no late compiler entry or MCU/DSP
HPI write, with early compiler/aperture positives and continuing native I/O;
the output boundary is MCU-side, not an established unanswered DSP tone request
(`nsm1_toolkit_tone_boundary_check.py`; scope in6150_bringup.md).
The6150 pending DISPLAY TEXT also survives native save/load with one FETCH,
two physical successful completions and exact observed architecture/I/O/pixel
replay, preserving complete persistent storage (`nsm1_toolkit_state_check.py`).
This is proactive-command state, not a claim about saving mid-character.

NSK-1 v5.61 (5130) independently produces that same four-byte profile and
automatic result30 refusal. Its matched Phase-2 control has no proactive
exchange. Unlike the6130 fixture, this Phase-2+ trial also rewrites108 EEPROM
bytes while the Phase-2 control preserves them; [5130 bring-up](5130_bringup.md)
localizes the differences to firmware-copied stack padding in empty encoded
digit fields, not bytes damaged in transport. Both card profiles execute the
same accepted-code record updates; the control's unused padding already
matches storage. General record validity remains unclaimed; the whole bank
is checked, not region-whitelisted.
This is not a successful feature. The shared wire validator distinguishes automatic
capability refusal from success after a physical contact; product wrappers
retain their own storage, native-execution and pixel requirements.

NSM-5 v5.40 (5210) independently submits a nine-byte TERMINAL PROFILE, FETCHes
the same DISPLAY TEXT command, renders `DCT3 SAT`, and returns result `00`
after a physical left-softkey press. The longer profile is an observed
product-owned capability declaration; the cross-ROM checker therefore accepts
an explicit expected profile length rather than normalizing it to five bytes.

A second opt-in card application proves that proactive commands can be
sequenced rather than merely demonstrated in isolation. The successful DISPLAY
TEXT response returns `91 15`, the firmware immediately FETCHes a GET INKEY
command (`0x22`), renders `Press 5`, and accepts a physical `5` followed by the
OK softkey. Its TERMINAL RESPONSE contains success and Text String
`0d 02 04 35` (8-bit DCS plus the entered digit). The card validates the fetched
command number and type before accepting either response. This route remains
separate from ordinary registration.

The third opt-in profile continues with GET INPUT (`0x23`). Its command asks
for one or two digits; the firmware renders `Enter 42`, edits the value through
the ordinary keypad path, and returns success with Text String
`0d 03 04 34 32`. This validates variable-length input and a three-command
`91xx`/FETCH/TERMINAL RESPONSE sequence without card-side knowledge of the MMI.

The fourth profile appends SET UP MENU (`0x25`). The firmware installs `DCT3
menu` as top-level item 10 and renders the supplied `Continue` and `Exit`
items. Selecting `Continue` organically sends ENVELOPE (`A0 C2`) body
`d3 07 02 02 01 81 10 01 01`; the card validates the menu-selection BER-TLV,
records item 1 and returns `90 00`. Invalid, unknown, or out-of-sequence menu
selections return `6a 80`.

The NSE-8 v6.00 capability boundary is also measured for later proactive
commands. During an established call, SEND DTMF (`0x14`) receives terminal
result `31`; LAUNCH BROWSER (`0x15`) with an alpha identifier and URL receives
the same result. Both commands travel through `91xx`, FETCH and TERMINAL
RESPONSE normally. Result `31` is therefore firmware capability rejection,
TIMER MANAGEMENT (`0x27`, start timer 1) receives the same result. All three
commands travel through `91xx`, FETCH and TERMINAL RESPONSE normally. Result
`31` is therefore firmware capability rejection, not a missing card
transaction and not a reason to synthesize DTMF, browser, or SAT-timer behavior
for this product.

An unknown comprehension-required TLV (`7e`) appended to an otherwise valid
DISPLAY TEXT command returns general result `32` (command data not understood),
distinct from the result-`31` capability responses above. This validates the
firmware parser's mandatory-TLV failure path through the normal card transport.

DISPLAY TEXT qualifier `80` also has a measured unattended lifecycle. With no
key input, v6.00 returns result `12` (no response from user) about 58.38 seconds
after FETCH. The timer is firmware-owned; the card retains the fetched command
until that ordinary TERMINAL RESPONSE arrives.
Validated DSP RX families do not feed this SAT path. Service-5's callback is already
registered and organically receives (`0x05f3`, `0x05e2`), while its `0x05e8`
branch remains dormant downstream. Do not replace this firmware contract
by selecting callbacks, posting task results, replaying commit keys, or setting registration state.

MMI callbacks, report 7, and the unattended UI lifecycle are outside the SIM
boundary. Report 7 is a shutdown/power-lifecycle report, not a SIM-ready event.
The mapped session descriptor and callback chains remain
in `sim_registration.md`, `mmi_layer.md`, and normalized evidence;
they are intentionally not repeated in this concise hardware/card contract.

## CHV transaction boundary

PIN-required startup, PUK recovery and the settings lifecycle are
runtime-validated. `Menu 4-3-1` drives DISABLE (`A0 26`) and ENABLE (`A0 28`)
through the firmware's PIN editor and On/Off selector. `Menu 4-3-5` drives
CHANGE (`A0 24`) with the old and replacement PINs. The gates restart with the
same card NVRAM and prove that disabled cards omit startup VERIFY, re-enabled
cards require it again, and changed credentials are used by the next boot.
A wrong old PIN returns `98 04`, persists one consumed retry and leaves the
credential unchanged.

## Current acceptance gates

- `make verify`: explicit missing-hardware structural oracle.
- `make run-frontier`: current external-service/SIM research profile.
- `make verify-sim-phonebook`: organic two-launch ADN update and persistence
  oracle.
- `make verify-sim-pin`: PIN-required boot and successful VERIFY on v6.00.
- `make verify-sim-pin-unblock`: wrong-PIN retry exhaustion followed by
  firmware-driven PUK recovery and replacement-PIN persistence.
- `make verify-sim-pin-state-roundtrip`: retry decrement, MAME save/load and
  successful VERIFY from the restored branch.
- `make verify-sim-pin-removal`: authenticate, remove the card through its
  physical input and require reset-scoped authorization to clear.
- `make verify-sim-pin-toggle`: disable CHV1 through the settings UI, restart
  with the same card, then re-enable it and require persistent state at each
  boundary.
- `make verify-sim-pin-change`: change CHV1 through the settings UI and verify
  the replacement credential on the following boot.
- `make verify-sim-pin-change-reject`: reject a wrong old PIN, consume exactly
  one retry and preserve the stored credential.
- `make verify-sim-pin-v501`: same-product v5.01 VERIFY corroboration.
- `make verify-radio-incoming-sms`: organic 176-byte `EF_SMS` update and exact
  persisted unread SMS record.
- `make smoke-3330e RUN_DIR=<dir> SECONDS=3`: bounded second-ROM confidence run.
- Stateful-model trace: natural ATR/PPS and the ordinary non-CPHS EF pass, with
  SIM enable rising and the timed presence monitor starting without injected
  messages or SIM-state RAM writes.

The frontier and focused gates protect organic SIM-enabled state, persistent
security state, save-state resumption, and physical removal/reinsertion through
FIQ7. FIFO reset/fill behavior is covered. Timeout and parity/error cause bits
are decoded and exposed through an inactive typed ingress, but remain the
controller fidelity frontier until a physical source establishes when they fire.

CHV command/status semantics follow ETSI GSM 11.11 v5.1.0, sections 9.4 and
9.5: <https://www.etsi.org/deliver/etsi_gts/11/1111/05.01.00_60/gsmts_1111v050100p.pdf>.

The controller holds no card finder. Activation level and transmitted bytes
leave through `card_activate_cb` and `card_tx_cb`, and the card's responses
return through its own `response_cb`; the phone wires the pair in its machine
configuration. A machine with a different card topology or tag rebinds the
callbacks without touching the controller.
