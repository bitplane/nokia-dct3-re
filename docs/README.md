# Documentation map

Documentation is organized by authority rather than investigation date.

## Start here

| Document | Purpose |
| --- | --- |
| `hardware_atlas.md` | High-level firmware-to-hardware boundary. |
| `mad2_fidelity.md` | Authoritative MAD2 implementation and uncertainty ledger. |
| `driver_structure.md` | Code ownership and quarantine rules. |
| `driver_vision.md` | Current modularization path and configuration taxonomy. |
| `ccont_subsystem.md` | Current CCONT contract, conclusions and fidelity backlog. |
| `cross_rom_confidence.md` | 3210/3330 portability evidence and ROM-input status. |
| `structural_regression.md` | Default mid-boot oracle and current deep-profile result. |

## Current boot frontier

| Document | Purpose |
| --- | --- |
| `boot_to_insert_sim.md` | Reproducible route and required models to the Insert SIM state. |
| `sim_subsystem.md` | Concise SIM ownership and interface summary. |
| `resource_providers.md` | Current organic GSM/resource-provider dependency chain. |
| `network_scouting.md` | Boundary between pre-idle startup and GSM/DSP work. |

## Deep reverse-engineering references

The following are address-heavy maps. They may contain dated checkpoints, but
remain useful when working in the corresponding firmware subsystem:

- `sim_registration.md`
- `sim_emulator_scope.md`
- `service_bootstrap.md`
- `scheduler_delivery.md`
- `static_branch_map.md`
- `interactive_handoff.md`
- `firmware_code_maps.md`

Treat concise subsystem documents and current source code as authoritative
when a deep reference conflicts with them. Absolute addresses apply to the
3210 v6.00 firmware unless explicitly stated otherwise.

## Evidence policy

Keep:

- register maps and interface contracts;
- exact addresses needed to reproduce a conclusion;
- current acceptance conditions and hashes;
- unresolved questions with a concrete observation needed; and
- negative conclusions that prevent a plausible failed approach being repeated.

Remove or condense:

- chronological session narratives;
- repeated progress summaries;
- superseded plans and speculative component sketches;
- raw trace dumps already represented by a table or conclusion; and
- descriptions of probes or knobs that no longer exist.

Failed forcing implementations are summarized in `removed_forcing_knobs.md`;
their source code should not remain merely to preserve history.
