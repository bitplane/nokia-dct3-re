# Removed forcing and diagnostic controls

This is a compact policy and lineage record. Exact implementations and the
chronology of individual experiments remain available in Git history.

## Policy

The supported driver may model hardware-visible behavior at a device or
transport boundary. It must not complete startup by changing firmware RAM,
returning a desired firmware result from a PC hook, replaying internal callback
keys, or posting an internal message that no modeled device produced.

Temporary probes are acceptable only when opt-in, explicitly diagnostic,
measured against a pre-registered prediction, and deleted after the conclusion
is recorded. A probe result is evidence about a branch; it is not evidence that
the probe mechanism belongs in the emulator.

## Deleted forcing families

The following families have no implementation or supported configuration in
the current tree:

- Direct startup-result forces: `EXPERIMENT_FORCE_CODE7`, VBAT overrides,
  checklist/event forces, and `MODEL_STARTUP_REPORTS`.
- SIM registration forces: `MODEL_SIM_INIT_KICK`, `SIM_REG_BOOTSTRAP`,
  `REG_COMMIT`, `REG_ROUTE`, `1196_HANDSHAKE`, rearm/defer variants, and direct
  SIM enable/no-SIM RAM writes.
- Contact-service bridges: `MODEL_SVC_RESPONDER`,
  `MODEL_SVC_CHANNEL_DRAIN`, command-`0x65` injection, channel-ready writes, and
  the standalone `nokia_service_transport_device`.
- Display and DSP shortcuts: forced LCD FIQ transfer, independent D0 injection,
  independent DSP-ring drain, heartbeat experiments, and guessed inbound DSP
  replies.
- Broad one-off probe families: `PROBE_*`, resource/display peer probes,
  scheduler recodes, and callback-key replay.

The durable conclusions from these experiments live in the subsystem documents
and in `evidence/falsifications.json` and `evidence/state_predicates.json`.

## Removed trace weaves

Large research tracers were also deleted after their findings were normalized:

- `TRACE_SIMKICK`, `TRACE_SIM_SERVER`, and `TRACE_SIM_CONFIG`
- the 60-block `TRACE_GSM_LOWER` weave
- `TRACE_MMIVM`
- `TRACE_TASK5_REG`

Current traces are narrow boundary or frontier instruments. Their inventory and
retirement criteria are maintained in `normalization_status.md`.

## Surviving models

The supported coherent profile uses device-boundary models only:

- `MODEL_DSP_SERVICE`
- `MODEL_CCONT_PRESENT`
- `MODEL_DSP_CONTACT_PEER`
- `MODEL_SIM_DEVICE`

These models respond to firmware-visible hardware or transport activity. Their
remaining fidelity debt is documented in `driver_structure.md` and
`normalization_status.md`; their presence is not a license to add new firmware
bridges.
