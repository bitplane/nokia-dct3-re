# Service-startup contract

This document describes the current Nokia 3210 v6.00 prerequisites for leaving
CONTACT SERVICE. Command direction and session ordering are authoritative in
`contact_service_topology.md`; branch addresses are collected in
`service_firmware_map.md`.

## Current status

The coherent profile completes contact startup without calling firmware
handlers, posting synthetic RTOS messages, or writing firmware state. The
request-driven peer:

1. answers organic DSP D0 discovery;
2. completes the DSP type-`0x70`/`0x74` contact transaction;
3. initiates the external class-`0x40` contact session through task 7;
4. supplies the channel map and healthy result; and
5. acknowledges the final `0x622a` service-empty transaction.

Firmware retains contact-present bit 6, clears busy bit 2, resumes the extended
application tasks, and starts ordinary SIM traffic.

## CONTACT SERVICE outcome

Task 2's D9 watchdog at `0x237b28` increments counter `0x11fed6`. On timeout it
calls `0x2b4dda`, producing CONTACT SERVICE. The watchdog arms when service-present
bit 6 of `0x11fed0` is absent. Its acknowledgement byte `0x11fedb` is not a
viable fix target; reachable startup code initializes it to zero but does not
use a nonzero value to establish the healthy session.

Contact initialization initially sets bit 6, then removes it when a prerequisite
or lower-service transaction fails. The watchdog is therefore the visible end
of an incomplete startup contract, not the root hardware boundary.

## Hardware and data prerequisites

| Prerequisite | Firmware observation | Current owner |
| --- | --- | --- |
| DSP lower-service completion | pending counter `0x100e4` drains and MAD2 IRQ 4 invokes `0x2af3ca`; `0x291068` sets `0x110c2c = 1` | `MODEL_DSP_SERVICE` prototype |
| CCONT presence | live serial read of CCONT register `0x0e`, bit 0 | `nokia_ccont_device` with the CCONT-present scenario |
| EEPROM configuration | checksum over `0x120..0x243` matches stored value at `0x244` | generated EEPROM profile through native `I2C_24C128` |
| EEPROM tune/security block | checksum over the security/tune region matches the stored word at `0x11c` | generated EEPROM profile |
| Contact DSP completion | TX type `0x70`, payload `0d00`, receives RX type `0x74`, payload `0d00` | request-driven DSP/contact peer |
| External contact session | class-`0x40` address learning, channel map, healthy result, and correlated acknowledgements through task 7 | request-driven DSP/contact peer |

The DSP service ready byte is reset during startup phases, so the completion is
not a one-shot boot constant. Both counter drain and IRQ 4 are required; crossed
experiments showed that either signal alone leaves `service_ready` clear.

## EEPROM gates

Configuration check `0x234810` computes the checksum for EEPROM block
`0x120..0x243` and compares it with `0x244..0x245`. The service-status array
also includes a tune/security validity result derived from the block ending at
`0x11c`. `make_eeprom_profile.py` computes these fields; the native I2C device
delivers them through ordinary GENSIO signaling.

These checks are real provisioning requirements. Their exact block layout and
security identity fields are documented in `eeprom_analysis.md`.

## CCONT gate

The service status loop reads CCONT register `0x0e` through the serial command
path and requires bit 0. This is a presence/status bit, distinct from the upper
interrupt-source bits consumed by the CCONT IRQ dispatcher. The extracted CCONT
device owns the register and interrupt behavior.

## Lower-service and contact ordering

The contact session has three distinct stages:

```text
MCU lower-service request
  -> DSP pending-counter drain + IRQ 4
  -> DSP contact request 70/0d00 and response 74/0d00
  -> task-7 external class-40 session
  -> firmware contact result and channel-map processing
  -> correlated transport acknowledgements
  -> service-empty report 622a completion
```

The external peer must be request-driven and preserve address, route, sequence,
and ownership metadata. Delivering only `{class=0x40, command=0x64, result=5}`
can reach the healthy branch in isolation but cannot establish a coherent
session; that historical bridge and its direct busy-state completion have been
deleted.

## Extended-task resume

Supervisor `startup_power_service_init_gate 0x2a8ff2` requests service-channel
empty through `0x2b13d4`, then waits at `0x29bb06` for busy bit 2 in `0x11fed1`
to clear. Once the organic transaction completes, its second resume group wakes
the application tasks. Their initialization fills the checklist at `0x112280`,
posts event `0x15`, and lets mode `0x000d` advance.

This task lifecycle is part of contact completion. A healthy result delivered
without the corresponding transport completion is insufficient.

The ordering run records the `0x622a` transport acknowledgement at `1.285269 s`,
the first group-two checklist write at `1.286232 s`, and the final task-18 write
at `1.297865 s`. The first battery classification has already changed state
`0 -> 1` at `0.364121 s`. Thus the ordering difference is owned by the whole
service-gated resume group, not by a slow task-18 initializer.

## Key addresses and state

| Address | Role |
| --- | --- |
| `0x110c2c` | DSP service-ready byte |
| `0x11fed0` | contact status, including service-present bit 6 |
| `0x11fed1` | contact/service busy flags |
| `0x11fed6` | D9 watchdog counter |
| `0x234750` | contact startup status initialization |
| `0x234810` | EEPROM configuration checksum gate |
| `0x237b28` | D9 watchdog poll |
| `0x291068` | DSP service-ready setter |
| `0x29bafc` | service-empty request/gate |
| `0x2b13d4` | shared service report entry |
| `0x2b4dda` | terminal startup outcome |

## Acceptance

`make verify` protects the authentic default CONTACT SERVICE frame. The coherent
profile is accepted only when `make verify-frontier` and the contact runtime
manifest show:

- no firmware result or RAM-state force;
- contact status `0x49`;
- startup modes `0x0001, 0x000d, 0x0004`;
- firmware-owned busy-bit completion;
- extended-task/checklist progress; and
- ordinary SIM initialization beginning afterward.

Machine-readable predicates live in `evidence/state_predicates.json`; failed
producer and forcing hypotheses live in `evidence/falsifications.json`.
