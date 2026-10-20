# SIM emulation scope

This document defines the current emulation boundary. Historical message-layer
responders, injected command scripts, state-forcing experiments, and the UI
investigation diary were useful during reverse engineering but are not part of
the intended driver. Their results are consolidated here, in
`sim_subsystem.md`, and in `sim_registration.md`; Git retains the experiments.

## Objective

Model a removable GSM SIM as a stateful device behind the MAD2 SIM interface.
Firmware must perform its own reset, ATR/PPS negotiation, T=0 exchanges, file
selection, parsing, registration state changes, and task-to-task messaging.

The implementation separates the MAD2 `nokia_simi_device` controller from the
removable `nokia_sim_card_device`. The permissions below describe the **card**
contract; FIFO registers, IIR causes, cadence and FIQ generation are controller
behavior.

The device may provide behavior owned by a physical card:

- activation and ATR;
- T=0 procedure bytes and status words;
- GSM 11.11 SELECT, STATUS, GET RESPONSE, READ BINARY/RECORD, CHV and
  SEEK, UPDATE BINARY/RECORD and cyclic-record INCREASE behavior;
- persistent current DF/EF selection and coherent file metadata/content;
- card-side serial response timing and protocol state.

It must not provide behavior owned by phone firmware or the GSM/baseband peer:

- injected RTOS messages or task events;
- writes to firmware SIM, registration, scheduler, or configuration RAM;
- forced callback selection, commit keys, or reply values;
- radio-session, PLMN-selection, network-registration, or DSP objects;
- direct UI state changes.

## Implemented hardware path

The implementation has two composed devices. `nokia_simi_device` owns the
MAD2 UART/FIFO/IIR register block and FIQ6. `nokia_sim_card_device` consumes
and returns serial bytes at that boundary and owns the removable card state.
The card currently supports:

- configurable ATR with the default `3b 10 05`;
- PPS echo;
- T=0 command header/body sequencing;
- SELECT and GET RESPONSE for MF, DF_GSM, and known EFs;
- STATUS for the current directory;
- READ BINARY and linear-fixed/cyclic READ/UPDATE RECORD with a declared GSM
  filesystem;
- persistent 50-record `EF_ADN`, ten-record `EF_SMS` and two-record `EF_SMSP`
  synthetic files, plus the three-byte cyclic `EF_ACM`;
- persistent CHV credentials, enable state and retry counters, with
  reset-scoped verification; and
- save-state coverage for transport, selection and mutable card contents.

With a PIN-enabled card, both 3210 v6.00 and v5.01 organically issue VERIFY
through SIMI. On v6.00, three wrong entries exhaust CHV1 retries and the
firmware proceeds to its PUK editor; the correct PUK plus a replacement PIN
issues UNBLOCK and persists the new credential. A separate gate saves after
one failed VERIFY and completes a successful VERIFY after restoring the
machine image. The v6.00 security-settings UI also organically issues CHANGE,
DISABLE and ENABLE. Two-launch gates prove enable-state and replacement-PIN
persistence; a rejected CHANGE proves retry consumption without mutation.

The organic 3210 conversation currently reaches:

```text
ATR -> PPS
  -> SELECT DF_GSM -> STATUS
  -> SELECT MF -> SELECT/READ ICCID
  -> SELECT DF_GSM -> SELECT/GET RESPONSE/READ ECC
  -> SELECT/READ PHASE
  -> SELECT/READ LP, AD and SST
  -> SELECT/READ LOCI, IMSI and ACC
  -> SELECT MF -> SELECT/READ 2FE6
  -> SELECT DF_GSM -> optional CPHS ONS 6F14 absent (94 04)
  -> remaining GSM/vendor EFs
  -> optional 6F99 absent (94 04)
  -> timed directory STATUS presence monitor
```

All of this travels through device registers, FIQ6, task 21, and the firmware's
normal T=0 implementation. No message responder or SIM-state write is needed.

## STATUS polling caution

Repeated `A0 F2 00 00 16` is firmware-scheduled: the response follows the
normal code-`0x0b` data path at `0x27ee94`, and the epilogue at `0x27ef0a`
explicitly handles INS `0xf2`, runs `0x27ea20`, and waits for the next message
at `0x27efb0`. Do not suppress a STATUS command ad hoc; model
its response and the directory metadata correctly. A malformed MF/DF layout
holds firmware in a preliminary polling cycle that never requests EF_IMSI
(ledger `sim_imsi_never_read_during_init` in `evidence/falsifications.json`).

## Persistent ADN contract

The base synthetic profile allocates and activates GSM 11.11 service 2 in
`EF_SST`. `EF_ADN (6F3A)` belongs to `DF_TELECOM (7F10)` and contains 50
32-byte linear-fixed records. The capacity is test-profile policy rather than
a property of the handset: real SIMs choose their own record count and length.

Firmware discovers the service, reads the EF metadata, scans the records with
absolute `READ RECORD`, and writes through `UPDATE RECORD`. Mutable records
are MAME device NVRAM, separate from the handset's 24C128. The card stores the
32-byte GSM record opaquely; alpha and BCD encoding remain firmware-owned.
`make verify-sim-phonebook` saves `ADA`/`123` through physical keypad input,
checks the resulting GSM 11.11 record, restarts with the same card NVRAM and
requires firmware Search to render the saved name.

Service 1 is also allocated and active. This advertises the CHV1-disable
function that backs the firmware's `PIN code request` setting; without it the
phone correctly presents `Not allowed` and emits no ENABLE/DISABLE command.

## Persistent SMS contract

The base profile also allocates and activates GSM 11.11 services 4 and 12.
`EF_SMS (6F3C)` has ten 176-byte linear-fixed records initialized free, while
`EF_SMSP (6F42)` has two 44-byte records and supplies the deterministic
service-centre address in record 1. Both are mutable card NVRAM appended after
the pre-existing ADN/location fields, with compatibility for older card
images. The outgoing-SMS SMSC gate edits record 1 through the physical handset
UI, checks the exact GSM 11.11 record persisted by `UPDATE RECORD`, and then
requires the next acknowledged `SMS-SUBMIT` to carry that edited address.

`make verify-radio-incoming-sms` drives those files from the radio boundary:
after paging and segmented SAPI-3 SMS-DELIVER traffic, firmware issues
`A0 DC 01 04 B0` and stores an unread `hello` message in record 1. The verifier
matches the exact persisted status, SMSC, originator, timestamp and GSM-7 user
data; no APDU or card contents are injected by the test. Physical reading
changes status `03` to `01`, while confirmed deletion changes it to free
status `00` without requiring the card to erase stale payload bytes.
Preserved-NVRAM cold boot exposes the same record without redelivery.
An organic capacity run fills all ten records through firmware APDUs and the
eleventh delivery receives RP-ERROR cause `16`; no test pre-populates the card.
The full lifecycle and cross-product evidence are in `ordinary_sms.md`.

The Smart Messaging ringtone fixture deliberately has the opposite card
result: both parts of its concatenated port-`1581` RTPL tone cross independent
SAPI-3/CP/RP/RR transactions while firmware does not issue `UPDATE RECORD` and
record 1 remains free. This proves sequential transport and application
routing, not firmware reassembly, ringtone persistence or UI behavior. The
contract is owned by the GSM fixture, not by a special SIM file or card-side
decoder.

## Remaining card work

The card filesystem covers boot, mutable linear-record paths and the generic
GSM 11.11 call-meter operation. `EF_ACM (6F39)` is a one-record, three-byte
cyclic EF. Its READ, UPDATE and INCREASE access conditions are CHV1; INCREASE
performs a checked 24-bit addition, returns `98 50` without mutation on
overflow, and exposes the new value followed by the addend only through the
immediately following six-byte GET RESPONSE. The record, pending response and
record pointer have save-state coverage, while only the record is persistent
card NVRAM. The CPHS AoC fixture advertises the service, but no handset has yet
organically issued INCREASE, so this is standards-conformance coverage rather
than a promoted product-runtime claim.

SEEK has isolated serial conformance and save-state coverage, but no retained
handset trace issues it. Its search directions, pointer updates and access
conditions are authoritative in [sim_subsystem.md](sim_subsystem.md).

The opt-in Phase-2+ application has organic handset coverage for DISPLAY TEXT,
GET INKEY, GET INPUT, SET UP MENU/menu-selection ENVELOPE, SELECT ITEM,
SEND SHORT MESSAGE, SET UP CALL, REFRESH and polling control. PLAY TONE has
playback and physical cancellation coverage, not unattended successful
completion. SET UP EVENT LIST and SEND DTMF have explicit negative capability
results on 3210 v6.00; they are not missing responses to synthesize.
The command-specific contracts and acceptance gates live in
[sim_subsystem.md](sim_subsystem.md). Toolkit call/SMS signalling does not
establish native DSP speech. All commands remain card-originated APDU/TLV
transactions with firmware-owned UI and messaging.

Extend it when an organic firmware request or focused protocol conformance test
establishes a concrete requirement. Later work includes:

- validity policies beyond the implemented CHV2-controlled ADN and
  ADM-denied FDN paths; organic REFRESH does not invalidate EFs;
- supplying coherent FPLMN and optional EFs beyond the current matched
  IMSI/SST/PLMN-selector/SPN profile;
- establishing the physical sources and timing of timeout and parity/error
  behavior. SIMI exposes the decoded `0x02`, `0x20`, and `0x80` causes through
  a typed, inactive fault ingress; ordinary traffic does not synthesize them.
  Physical removal and reinsertion
  now use the recovered SIMI status-bit-3/FIQ7 socket-detect contract and are
  covered across both 3210 ROMs, including Phase-2+ and save/load variants;
- establishing unattended PLAY TONE completion and any further Phase-2+
  commands only where terminal capabilities and an organic request or focused
  conformance requirement support them;
- deriving model-specific filesystem profiles without phone-ROM special cases
  in the transport.

### Fixed dialling implementation boundary

FDN is an opt-in partial card capability with NSM-1 activation/cold-state coverage.
[GSM 11.11 v5.3.0](https://www.etsi.org/deliver/etsi_gts/11/1111/05.03.00_60/gsmts_1111v050300p.pdf)
sections8.14-8.15,10.3.1-10.3.2 and11.5.1 establish the contract:
EF_FDN6f3b is linear-fixed, uses ADN-style records, requires CHV1 for
reading and CHV2 for updating. Its own invalidation/rehabilitation is ADM.
FDN enabling instead invalidates EF_ADN after CHV2 verification; disabling
rehabilitates EF_ADN. Capability discovery combines service3 in EF_SST with
ADN activation/validity. An invalid EF normally refuses application access;
its file status may explicitly permit reads/updates while invalidated.

File descriptors now declare explicit ALWAYS/CHV1/CHV2/ADM/NEVER access
conditions, shared by FCP and execution. Existing policies are retained;
read-only files now correctly advertise UPDATE=NEVER rather than ALWAYS.
The serial conformance suite and a fresh NSM-1 physical PIN/phone-code
Messages boot pass. CHV2 is selected by EF_FDN and exercised in the
card-level protected-write tests.
An append-only3898-byte card image now adds a versioned eight-byte header
and ten32-byte FDN records. The legacy3570-byte output is retained for base
cards. Staged loading validates the whole extension before applying it;
26 executable cold-storage cases cover valid/legacy images and malformed
lengths, version, flags and reserved bytes. FDN updates require CHV2; ADN
validity and records are saved independently. Strict invalidated ADN access
returns9810; the explicit readable-invalid profile sets the corresponding
FCP bit and allows reads/updates subject to the ordinary access conditions.
Card-only APDU tests pass both policies, including CHV2-only writes, reset
revocation, validity save/restore, parameter errors, repeated-state errors,
ADM denial and unchanged ADN contents. These are conformance results, not
handset FDN enforcement acceptance. Enabled FDN invalidates IMSI/LOCI once
per session, no later
than selecting either file (GSM11.11 10.2.7). CHV1 permits rehabilitation
of each; ADM invalidation remains unavailable. Re-selection after
rehabilitation does not re-invalidate within the same session. Activation
starts a fresh session. These validity flags are saved in machine states,
not card NVRAM. The NSM-1 now organically shows On/Off across cold boots.

Implementation status and remaining acceptance:

1. CHV2 metadata, extended persistence and selected-ADN validity commands
   have card-level executable coverage; legacy3570-byte output is retained.
2. Cold NSM-1 interpretation and physical deactivation are accepted through
   the card's identity-session handshake, not a handset-state fixture.
3. NSM-1 physical FDN save/cold-read, rejected PIN2 write and rejected
   activation with cold correct-PIN2 recovery are accepted with complete-card
   and exact-frame checks. Other handset coverage remains open.
   Physical in-place12345->12346 edit
   and independent cold read also pass, changing only the final BCD digit.
   Physical
   PIN2-protected erase/cancel and cold-empty search also pass on NSM-1,
   preserving the complete card outside the selected FDN record.
   NSM-1 local dial enforcement also passes a listed/unlisted/Off physical
   comparison: listed12346 reaches Calling, unlisted54321 is denied, and
   physical Off permits54321 to reach Calling with an unchanged card.
   This is not native network connection or speech acceptance.

`tools/run_sim_fdn.py RUN --base-card BASE --mame mame/mame` runs both
development-only `simfdnt` and `simfdnrt` after `make build`. BASE must be
a complete blank-ADN/default-credential lab card; the runner explicitly
enables PIN request in its own fixtures. No handset firmware executes.

The implementation tracks current DF separately from selected EF and declares
file parent, size, structure and record length as profile metadata. Subscriber
identity, PLMN, operator presentation, access class and A3/A8 material now come
from the reusable profile in `subscriber_profile.md`. Remaining architectural
work is to establish controller/card timing and electrical error sources.

The organically requested initialization pass is complete for a non-CPHS
synthetic card. Unsupported optional files return `94 04` (file ID not found)
and leave the current selection unchanged; firmware accepts that result and
continues its file pass. `6F14` is CPHS Operator Name String, a transparent
default-alphabet field of up to the 24 bytes accepted by parser `0x201876`; it
is intentionally absent and should only gain content in a card profile that
advertises CPHS. This file is the authoritative home for the CPHS/`94 04` card
contract.

The retained CPHS/AoC card profile is a protocol fixture, not a boot bypass. It
advertises CPHS phase 2 through `EF_INFO (6F16)`, allocates and activates the
Customer Service Profile, and serves a minimum valid 18-byte `EF_CSP (6F15)`
with only group-03 mask `0x20` enabled. Firmware requests both files through
the ordinary SIMI/FIQ6 path. This profile validates the decoded selector-0
contract at `0x287250`; it does not populate firmware task state or manufacture
an active call. Card profiles are selected through typed machine configuration,
not a process-environment override.

After initialization, task 20 deliberately polls the selected directory with
STATUS; this is the firmware card-presence monitor, not a remaining filesystem
blocker. Its contract is authoritative in `sim_subsystem.md`.

## Current boundary outside the SIM

Ordinary network registration and bounded MT SMS delivery cross the GSM/DSP
boundary and are verified separately in `network_scouting.md`. The SIM
participates only through its standards-shaped files and firmware-issued
`EF_LOCI`/`EF_SMS` updates. Callback 7 and the `0x1196` commit family are separate
lower-session machinery; the card does not own their lifecycle or objects, so
neither belongs in `nokia_sim_card_device`.

## Acceptance rules

A SIM increment is accepted only when:

1. the stimulus originates at the emulated card/register boundary;
2. firmware receives it through the normal FIQ/message path;
3. no firmware-owned message, callback, return value, or RAM state is forced;
4. the resulting APDU/state transition is reproducible in one coherent boot;
5. both 3210 regression oracles and the 3330-E smoke baseline remain healthy.

The present stop boundary is the SIMI fault source, not an unfinished CHV or
socket-detect command. Firmware decodes IIR causes `0x02`, `0x20`, and `0x80`,
but current evidence does not identify all of their electrical conditions.
Timeout and parity/error events must not be synthesized merely to exercise the
already-mapped handlers.

Primary references:

- `sim_subsystem.md` - concise hardware and transaction contract;
- `sim_registration.md` - firmware ownership of initialization and adjacent session paths;
- `dsp_interface.md` - known DSP transports and their exclusions;
- `driver_structure.md` - component boundaries and extraction status.
