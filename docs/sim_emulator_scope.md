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

The device may provide behavior owned by a physical card:

- activation and ATR;
- T=0 procedure bytes and status words;
- GSM 11.11 SELECT, STATUS, GET RESPONSE, READ BINARY/RECORD and CHV behavior;
- persistent current DF/EF selection and coherent file metadata/content;
- serial timing, ready/busy state, and interrupt generation.

It must not provide behavior owned by phone firmware or the GSM/baseband peer:

- injected RTOS messages or task events;
- writes to firmware SIM, registration, scheduler, or configuration RAM;
- forced callback selection, commit keys, or reply values;
- radio-session, PLMN-selection, network-registration, or DSP objects;
- direct UI state changes.

## Implemented hardware path

`nokia_sim_card_device` is connected to the MAD2 SIM register block. The
firmware writes command bytes to the emulated UART, receives bytes at serial
cadence, and is interrupted through the normal FIQ6 path. The device currently
supports:

- configurable ATR with the default `3b 10 05`;
- PPS echo;
- T=0 command header/body sequencing;
- SELECT and GET RESPONSE for MF, DF_GSM, and known EFs;
- STATUS for the current directory;
- READ BINARY/RECORD with a minimal coherent GSM filesystem;
- save-state coverage for transport and selection state.

The organic 3210 conversation currently reaches:

```text
ATR -> PPS
  -> SELECT DF_GSM -> STATUS
  -> SELECT MF -> SELECT/READ ICCID
  -> SELECT DF_GSM -> SELECT/GET RESPONSE/READ ECC
  -> SELECT/READ PHASE
  -> periodic DF_GSM STATUS
```

All of this travels through device registers, FIQ6, task 21, and the firmware's
normal T=0 implementation. No message responder or SIM-state write is needed.

## STATUS polling conclusion

The repeated `A0 F2 00 00 16` after `EF_PHASE (6FAE)` is not evidence of a bad
FCP. Each command is scheduled by firmware. The response follows the normal
code-`0x0b` data path at `0x27ee94`; the epilogue at `0x27ef0a` explicitly
handles INS `0xf2`, runs `0x27ea20`, and waits for the next message at
`0x27efb0`. The run does not reset the card or report no-SIM.

Do not change card data merely to suppress this stable presence poll.

## Remaining card work

The card filesystem is intentionally minimal. Extend it only when an organic
firmware request establishes a concrete requirement. Likely later work includes:

- separating current DF and selected EF explicitly if a cross-directory
  sequence proves it necessary;
- completing access-condition, invalidation, record-layout, and CHV semantics;
- supplying coherent IMSI, SST, LOCI, Kc, FPLMN, AD, SPN and optional EFs;
- testing card removal, reset, timeout, parity/error, and proactive-SIM status;
- deriving model-specific filesystem profiles without phone-ROM special cases
  in the transport.

These are not the present boot blocker. Firmware stops before requesting the
extended IMSI pass.

## Current boundary outside the SIM

The next registration transaction depends on an object-bearing GSM/radio
session. Firmware must construct callback 7 with lifecycle `0x05dc`; its own
code then attaches a radio context and emits:

```text
callback 7 -> 0x5518 -> task 17 0x1583
  -> registration/session commit -> task 20 0x1196
  -> parser reply code 2 -> SIM read ENABLE
```

Callback 7 receives only the global `0x05e2` sweep in the current run. The SIM
card does not own its lifecycle or object, so neither belongs in
`nokia_sim_card_device`. The next peer work must begin from an observed
firmware request or hardware-visible transition at the GSM/resource/DSP
boundary.

## Acceptance rules

A SIM increment is accepted only when:

1. the stimulus originates at the emulated card/register boundary;
2. firmware receives it through the normal FIQ/message path;
3. no firmware-owned message, callback, return value, or RAM state is forced;
4. the resulting APDU/state transition is reproducible in one coherent boot;
5. both 3210 regression oracles and the 3330-E smoke baseline remain healthy.

Primary references:

- `sim_subsystem.md` - concise hardware and transaction contract;
- `sim_registration.md` - firmware ownership and registration chain;
- `dsp_interface.md` - known DSP transports and their exclusions;
- `driver_structure.md` - component boundaries and extraction status.
