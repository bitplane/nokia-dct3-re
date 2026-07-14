# Historical boot to "Insert SIM card" milestone

> **Historical conclusion.** This page records what the first Insert-SIM boot
> established. The firmware bridges used to isolate the path have been removed.
> Use `contact_service_topology.md`, `sim_subsystem.md`, and
> `structural_regression.md` for the current implementation and acceptance state.

All addresses refer to the pinned 3210 v6.00 image. Firmware bytes use the
big-endian ARM7 byte lanes described in `hardware_atlas.md`.

## Durable result

The first post-CONTACT-SERVICE milestone established the complete firmware-side
startup barrier:

```
contact-service completion
  -> startup mode 0x000d
  -> startup_power_service_init_gate 0x2a8ff2
  -> block-2 task resume
  -> eleven-byte checklist at 0x112280 becomes complete
  -> event 0x15
  -> mode 0x000d advances
  -> MMI initialization
  -> SIM absence renders "Insert SIM card"
```

The old experiment completed the service-busy transition directly. That was
useful branch-isolation evidence, but it was not a faithful transport model and
has been deleted. The current `MODEL_DSP_CONTACT_PEER` instead observes organic
requests and completes D0 discovery, DSP type-`0x70`/`0x74`, class-`0x40`, and
the final `0x622a` transaction in one coherent boot. Firmware clears its own
busy state and resumes the extended tasks.

## Mode-0x000d readiness barrier

Handler `0x270e22` accumulates startup flags in `0x112399`. Event `0x15` supplies
bit `0x04`. Its producer `0x2af208`, called from `0x2521cc`, fires only when the
eleven-byte checklist at `0x112280` is complete. The checklist entries are
written by initialization functions belonging to the application tasks in the
registration table at `0x2d7090`.

All task control blocks are created by the scheduler walkers at `0x26a74c` and
`0x26a7b0`, initially dormant. `startup_power_service_init_gate` resumes them in
groups. Its second group contains the tasks that complete the checklist and is
gated at `0x2a9182` by the service-channel completion path.

The gate calls `service_channel_request_empty 0x2b13d4`, which publishes report
`0x622a`, then waits at `0x29bb06` for `0x11fed1` bit 2 to clear. Firmware seeds
that bit at `0x2347d0` during contact startup. A real peer must acknowledge the
transaction soon enough for the bounded readiness check; merely reporting a
healthy contact result is insufficient.

## Why the old experiment mattered

The deleted bridge proved three things that remain valid:

1. The missing block-2 resumes, not scheduler construction, caused the original
   `0x000d` stall.
2. Clearing the service transaction through the proper lifecycle causes every
   downstream link to fire: task resumes, checklist completion, event `0x15`,
   mode advance, and rendered MMI content.
3. Transaction ordering matters. A completion delivered before its organic
   request can change scheduling while failing to model a coherent boot.

These conclusions motivated the request-driven contact peer and the structural
oracle counters that protect the current implementation.

## Current interpretation

"Insert SIM card" is a valid terminal UI state for a SIM-less phone, not proof
of full interactive startup. The project subsequently implemented a stateful
SIM device and advanced through SIM initialization to the current provisioned
Menu presentation. Keypad handoff remains unresolved; see
`interactive_handoff.md`.
