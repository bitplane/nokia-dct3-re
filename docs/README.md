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
| `gensio_controller.md` | Extracted GENSIO endpoint, serial and SELECT-latch contract. |
| `mbus_controller.md` | Extracted MBUS controller, firmware data path, and attachment boundary. |
| `cross_rom_confidence.md` | 3210/3330 portability evidence and ROM-input status. |
| `structural_regression.md` | Default mid-boot oracle and current deep-profile result. |
| `evidence_regime.md` | Normalized topology, hardware, predicate and falsification evidence rules. |
| `research_cleanup.md` | Repeatable cleanup protocol for hypotheses, naming residue, diagnostics, and evidence retention. |
| `rtos_tasks.md` | Authoritative ROM-specific task identities, neutral aliases, and naming cautions. |
| `normalization_status.md` | Coverage boundary and remaining model, topology and instrumentation debt. |
| `tooling.md` | Analysis, census and acceptance tooling reference. |

## Validated startup and UI boundaries

| Document | Purpose |
| --- | --- |
| `sim_subsystem.md` | Concise SIM ownership and interface summary. |
| `mmi_settlement.md` | Validated idle/menu settlement plus security-editor and conditional power lifecycles. |
| `mmi_layer.md` | Keypad IRQ, mailbox, matrix-scan and decoded-key acceptance path. |
| `resource_providers.md` | Resource-provider ownership and excluded conditional startup paths. |
| `external_service_topology.md` | Class-`0x40` service-command producers, acknowledgements, and external-service boundary. |
| `service_bootstrap.md` | Service-session startup prerequisites and acceptance contract. |
| `service_firmware_map.md` | Concise lower-service and service-session firmware address map. |
| `scheduler_delivery.md` | Reusable scheduler message/event encoding contract. |
| `eeprom_analysis.md` | EEPROM usage map, checksum contracts and the generated provisioning fixture. |

## Mapped downstream work

| Document | Purpose |
| --- | --- |
| `network_scouting.md` | Later GSM registration and operator-content lifecycle. |
| `dsp_interface.md` | Detailed MCU/DSP transport and later lower-radio maps. |
| `dsp_shared_memory_inventory.md` | Generated two-ROM inventory of reachable firmware reads from DSP shared RAM. |
| `dsp_shared_memory_transitions.md` | Generated two-ROM transaction census for DSP-owned shared-RAM scalar state. |
| `dsp_packet_semantics.md` | Generated two-ROM inventory of DSP packet vocabulary and current HLE disposition. |
| `dsp_service_transport_contract.md` | DSP/generic-service ownership and acceptance contract. |

## Deep reverse-engineering references

The following are detailed firmware maps that remain useful when working in the
corresponding subsystem:

- `sim_registration.md`
- `sim_emulator_scope.md`
- `firmware_code_maps.md`
- `message_topology_census.md`
- `battery_classifier_analysis.md` (active investigation)

Treat concise subsystem documents, normalized evidence, and current source code
as authoritative. Absolute addresses apply to the 3210 v6.00 firmware unless
explicitly stated otherwise.

Normalized reviewed evidence lives in `evidence/*.json`; named runtime inputs
live in `tools/run_manifests/*.json`. Run `make evidence-check` before banking a
new conclusion.

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

Failed forcing implementations live in Git history. Only reusable negative
conclusions belong in `evidence/falsifications.json`; the current force policy
lives in `evidence_regime.md`.
