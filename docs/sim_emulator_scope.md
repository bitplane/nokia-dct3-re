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
- GSM 11.11 SELECT, STATUS, GET RESPONSE, READ BINARY/RECORD and CHV behavior;
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
- READ BINARY and linear-fixed READ/UPDATE RECORD with a declared GSM
  filesystem;
- persistent 50-record `EF_ADN`, ten-record `EF_SMS` and two-record `EF_SMSP`
  synthetic files; and
- save-state coverage for transport, selection and mutable card contents.

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

## Persistent SMS contract

The base profile also allocates and activates GSM 11.11 services 4 and 12.
`EF_SMS (6F3C)` has ten 176-byte linear-fixed records initialized free, while
`EF_SMSP (6F42)` has two 44-byte records and supplies the deterministic
service-centre address in record 1. Both are mutable card NVRAM appended after
the pre-existing ADN/location fields, with compatibility for older card
images.

`make verify-radio-incoming-sms` drives those files from the radio boundary:
after paging and segmented SAPI-3 SMS-DELIVER traffic, firmware issues
`A0 DC 01 04 B0` and stores an unread `hello` message in record 1. The verifier
matches the exact persisted status, SMSC, originator, timestamp and GSM-7 user
data; no APDU or card contents are injected by the test.

The Smart Messaging ringtone fixture deliberately has the opposite card
result: after part 1 of its concatenated port-`1581` RTPL tone crosses SAPI 3,
firmware does not issue `UPDATE RECORD` and record 1 remains free. Part 2 is
queued behind the organic CP/RP close. This application-routing contract is
owned by the GSM fixture, not by a special SIM file or card-side decoder.

## Remaining card work

The card filesystem covers boot and one complete mutable linear-record path.
Extend it when an organic firmware request or focused protocol conformance test
establishes a concrete requirement. Later work includes:

- completing UPDATE BINARY, SEEK, cyclic/INCREASE, invalidation and CHV
  semantics;
- supplying coherent FPLMN and optional EFs beyond the current matched
  IMSI/SST/PLMN-selector/SPN profile;
- testing card removal, reset, timeout, parity/error, and proactive-SIM status;
- deriving model-specific filesystem profiles without phone-ROM special cases
  in the transport.

The implementation tracks current DF separately from selected EF and declares
file parent, size, structure and record length as profile metadata. Remaining
architectural work is to stabilize controller/card timing and errors and move
the remaining subscriber constants into reusable profiles.

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

Primary references:

- `sim_subsystem.md` - concise hardware and transaction contract;
- `sim_registration.md` - firmware ownership of initialization and adjacent session paths;
- `dsp_interface.md` - known DSP transports and their exclusions;
- `driver_structure.md` - component boundaries and extraction status.
