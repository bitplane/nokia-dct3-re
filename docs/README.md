# Documentation map

Documentation is organized by authority rather than investigation date.

When documents overlap, use this precedence:

1. reviewed `evidence/*.json` and named runtime manifests state accepted or
   falsified conclusions;
2. concise subsystem contracts state current ownership, behavior, and remaining
   uncertainty;
3. generated/static censuses state mechanically recovered coverage and facts;
4. deep reverse-engineering maps retain address-level detail but do not override
   a newer subsystem contract or evidence entry; and
5. Git history retains chronology and discarded implementations.

The source implements the current model, but a calibrated implementation is not
automatically a hardware fact. The relevant subsystem contract must label that
distinction.

## Start here

| Document | Purpose |
| --- | --- |
| `hardware_atlas.md` | High-level firmware-to-hardware boundary. |
| `mad2_fidelity.md` | Authoritative MAD2 implementation and uncertainty ledger. |
| `mad2_static_access.md` | Generated paired-ROM census of direct MAD2 MMIO accesses. |
| `board_io_static_census.md` | Generated five-ROM PUP, KBGPIO, UIF and SELECT direct-access census. |
| `driver_structure.md` | Code ownership and quarantine rules. |
| `driver_vision.md` | Current modularization path and configuration taxonomy. |
| `product_configuration_audit.md` | Post-refactor product-contract dependency audit and next bounded refactor. |
| `ccont_subsystem.md` | Current CCONT contract, conclusions and fidelity backlog. |
| `gensio_controller.md` | Extracted GENSIO endpoint, serial and SELECT-latch contract. |
| `mbus_controller.md` | Extracted MBUS controller, firmware data path, and attachment boundary. |
| `cross_rom_confidence.md` | Cross-product and cross-ROM boot, idle, and input evidence. |
| `model_coverage.md` | Evidence-gated per-product compatibility and fidelity matrix. |
| `6110_bringup.md` | Authoritative NSE-3 hardware/firmware map and blocked resumption boundary. |
| `6110_bootstrap_capture.md` | Physical NSE-3 DSP-bootstrap capture format and acceptance contract. |
| `structural_regression.md` | Acceptance profiles, semantic predicates, and frame oracles. |
| `evidence_regime.md` | Normalized topology, hardware, predicate and falsification evidence rules. |
| `research_cleanup.md` | Repeatable cleanup protocol for hypotheses, naming residue, diagnostics, and evidence retention. |
| `rtos_tasks.md` | Authoritative ROM-specific task identities, neutral aliases, and naming cautions. |
| `normalization_status.md` | Coverage boundary and remaining model, topology and instrumentation debt. |
| `tooling.md` | Analysis, census and acceptance tooling reference. |

## Validated startup and UI boundaries

| Document | Purpose |
| --- | --- |
| `sim_subsystem.md` | Concise SIM ownership and interface summary. |
| `mmi_layer.md` | Keypad, security editor, interactive idle/menu, and power/shutdown lifecycle. |
| `resource_providers.md` | Resource-provider ownership and excluded conditional startup paths. |
| `external_service_topology.md` | Class-`0x40` service-command producers, acknowledgements, and external-service boundary. |
| `service_bootstrap.md` | Service-session startup prerequisites and acceptance contract. |
| `service_firmware_map.md` | Concise lower-service and service-session firmware address map. |
| `scheduler_delivery.md` | Reusable scheduler message/event encoding contract. |
| `eeprom_analysis.md` | EEPROM usage map, checksum contracts and the generated provisioning fixture. |

## Active protocol and HLE boundaries

| Document | Purpose |
| --- | --- |
| `network_scouting.md` | Validated camp, Location Updating, channel-release and operator-presentation contract. |
| `grey_salamander_integration.md` | Knowledge-integration catalogue and dependency order for LAPDm, paging, calls, SMS and SIM extensions. |
| `dsp_interface.md` | Detailed MCU/DSP transport and later lower-radio maps. |
| `dsp_shared_memory_inventory.md` | Generated two-ROM inventory of reachable firmware reads from DSP shared RAM. |
| `dsp_shared_memory_transitions.md` | Generated two-ROM transaction census for DSP-owned shared-RAM scalar state. |
| `dsp_packet_semantics.md` | Generated two-ROM inventory of DSP packet vocabulary and current HLE disposition. |
| `dsp_service_transport_contract.md` | DSP/generic-service ownership and acceptance contract. |

## Address-level reverse-engineering references

The following are detailed firmware maps that remain useful when working in the
corresponding subsystem:

- `sim_registration.md`
- `sim_emulator_scope.md`
- `firmware_code_maps.md`
- `message_topology_census.md`
- `battery_classifier_analysis.md` (mapped battery ADC/classifier contract)

Absolute addresses apply to the 3210 v6.00 firmware unless explicitly stated
otherwise. These maps are retained because their coverage and exact addresses
make future work cheaper; their older interpretations are not authoritative.

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
