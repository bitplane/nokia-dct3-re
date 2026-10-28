# Documentation map

Documentation is organized by authority rather than investigation date.

## Ownership

| Information | Single home |
| --- | --- |
| Demonstrated handset capability and composition | `model_coverage.md`; detailed gates and qualifications in the linked product contract. |
| Remaining hardware work | `driver_vision.md`; register-level meaning and physical unknowns in the owning subsystem contract. |
| Device graph and native/HLE substitution | `upstream_architecture.md` and `driver_structure.md`. |
| Acceptance inputs, predicates and oracles | `structural_regression.md`, named manifests and reviewed evidence. |
| Evidence coverage, runtime-control classes and instrumentation limits | `normalization_status.md`. |
| Address-level supporting observations | The focused maps below; their bounded captures do not override current acceptance. |

Overview documents link to these owners instead of maintaining additional
feature lists. Generated censuses own their counts and must be regenerated,
not hand-edited to match a narrative.

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
| `mad2_residual_census.md` | Generated five-ROM closure of unresolved CTSI surfaces. |
| `board_io_static_census.md` | Generated five-ROM PUP, KBGPIO, UIF and SELECT direct-access census. |
| `driver_structure.md` | Code ownership and quarantine rules. |
| `driver_vision.md` | Current modularization path and configuration taxonomy. |
| `upstream_architecture.md` | Target MAME device graph, DSP substitution seam, ROM policy, and upstream staging. |
| `product_configuration_audit.md` | Product-contract invariants, independent evidence identities and configuration dependency limits. |
| `external_call_bridge.md` | Versioned host telephony API for network state, calls, GSM-FR media, SMS and USSD. |
| `subscriber_profile.md` | Shared immutable SIM/network identity and authentication contract. |
| `gsm_fr_codec.md` | Generic GSM-FR utility, bundled dependency, predictor snapshots and voice regression gates. |
| `ccont_subsystem.md` | Current CCONT contract, conclusions and fidelity backlog. |
| `flash_storage.md` | Native flash ownership, ST protection/CFI/OTP/supply contracts, acceptance and electrical limits. |
| `gensio_controller.md` | Extracted GENSIO endpoint, serial and SELECT-latch contract. |
| `mbus_controller.md` | Extracted MBUS controller, firmware data path, and attachment boundary. |
| `cross_rom_confidence.md` | Cross-product and cross-ROM boot, idle, and input evidence. |
| `model_coverage.md` | Evidence-gated per-product compatibility and fidelity matrix. |
| `c54x_execution_contracts.md` | Processor-owned instruction/composition contracts and the current complete C54x regression checkpoint; separate from handset and loader acceptance. |
| `5110i_bringup.md` | Own NSE-2 inputs, NV integrity, native ROM4 compatibility, graphical/input/security, cold SIM contacts, offline SMS storage and receive-supervision boundary. |
| `6110_bringup.md` | Authoritative NSE-3 hardware/firmware map and blocked resumption boundary. |
| `6130_bringup.md` | Own NSK-3 inputs, physical Net monitor exit and cold idle, bounded SIM storage/security, native radio/liveness boundaries. |
| `5130_bringup.md` | Own NSK-1 startup/storage/input contracts; native compatibility boot, physical security, persistent contacts and offline SMS save/cold read. |
| `6150_bringup.md` | NSM-1 compatibility boot/provisioning, physical UI/SIM/storage contracts and fitted DSP/radio prerequisites. |
| `8810_bringup.md` | Own NSE-6 v6.02 graphical native compatibility fixture, upload geometry and unresolved native serial-control completion. |
| `5210_bringup.md` | Validated NSM-5 v5.40 product contracts and remaining scope. |
| `5510_bringup.md` | NPM-5 handset boundary and separate MU4 staged recording/storage/playback contracts; atomic negative controls are explicitly separate. |
| `aac_validation.md` | Independent native music-decoder procedure, original reference fixtures and unresolved measurement contract. |
| `8210_bringup.md` | NSM-3 normal/native boundary, acquired base-record research-HLE phone services, own PCM/audio and restoration acceptance. |
| `8290_bringup.md` | Own NSB-7 uploads and explicit research-HLE UI, SIM security/storage/Toolkit, laboratory registration, host call/SMS signaling and restoration; native DSP/speech remain unproved. |
| `8xxx_bringup.md` | Acquired 8250/8850/8890 GENSIO contracts and bounded software-only frontiers. |
| `8855_bringup.md` | Own NSM-4 MCU/storage, register/key/upload contracts and unacknowledged DSP ownership boundary. |
| `6210_bringup.md` | NPE-3 normal/native boundary, own-PMM research-HLE phone services, accessory audit and isolated restoration acceptance. |
| `6250_bringup.md` | Product-local staged DSP boundary and research boot, input, SIM, phonebook, registration, call-signaling and SMS acceptance. |
| `7110_bringup.md` | NSE-5 upload, SED1565/keypad/roller contracts, fail-closed product boundary and explicit ROM4 compatibility instrument. |
| `2100_bringup.md` | Own v5.21 C3 security/storage checkpoint, unresolved idle/interrupt boundary and separate v5.84 portability fixture. |
| `3610_bringup.md` | NAM-1 v5.11 executable profile, GENSIO/CCONT evidence and current bring-up boundary. |
| `3350_bringup.md` | NHM-9 v5.30 own MCU upload/first-wait boundary and partial hardware map; no phone-boot acceptance. |
| `5190_bringup.md` | NSB-1 v6.71 own MCU/storage, native compatibility menus, physical diagnostic exit/cold idle, SIM security, contacts, offline SMS and Toolkit; fitted mask/provisioning remain unproved. |
| `6190_bringup.md` | NSB-3 own provisioning/ROM4 compatibility, physical menus, cold SIM phonebook/SMS/security and permanent PUK-loss controls; native radio, late reset and display-settlement boundaries remain open. |
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
| `sim_subsystem.md` | Authoritative SIMI/controller and card interface, timing, persistence and acceptance contract. |
| `mmi_layer.md` | Keypad, security editor, interactive idle/menu, and power/shutdown lifecycle. |
| `resource_providers.md` | Resource-provider ownership and excluded conditional startup paths. |
| `external_service_topology.md` | Class-`0x40` service-command producers, acknowledgements, and external-service boundary. |
| `service_bootstrap.md` | Service-session startup prerequisites and acceptance contract. |
| `service_firmware_map.md` | Concise lower-service and service-session firmware address map. |
| `scheduler_delivery.md` | Reusable scheduler message/event encoding contract. |
| `eeprom_analysis.md` | EEPROM usage map, checksum contracts and the generated provisioning fixture. |
| `storage_static_census.md` | Five-ROM direct EEPROMSelX and PUP access absence/ownership census. |
| `dct3_type_1f.md` | Cross-model status-2 type-`0x1f` envelope shared by 3210, 3310 and 6110 images. |
| `gate_migration.md` | Acceptance-gate authoring, regeneration and independent GNU make database comparison. |
| `gate_parity_audit.md` | Generated audit of where sibling product gates differ. |

## Active protocol and HLE boundaries

| Document | Purpose |
| --- | --- |
| `network_scouting.md` | Validated camp, Location Updating, channel-release and operator-presentation contract. |
| `cell_broadcast_and_ems.md` | NSE-8 EMS application boundary and Cell Broadcast page/CBCH contract. |
| `supplementary_call_control.md` | Validated NSE-8 DTMF and hold/retrieve call-control lifecycle. |
| `gsm_supplementary_services.md` | Call-independent GSM 04.80, call-divert and host USSD contracts. |
| `grey_salamander_integration.md` | Current LAPDm/GSM capability catalogue and retained external-reference boundaries. |
| `dsp_interface.md` | Detailed MCU/DSP transport and later lower-radio maps. |
| `dsp_shared_memory_inventory.md` | Generated two-ROM inventory of reachable firmware reads from DSP shared RAM. |
| `dsp_shared_memory_transitions.md` | Generated two-ROM transaction census for DSP-owned shared-RAM scalar state. |
| `dsp_packet_semantics.md` | Generated two-ROM inventory of DSP packet vocabulary and current HLE disposition. |
| `dsp_service_transport_contract.md` | DSP/generic-service ownership and acceptance contract. |
| `cobba_control_boundary.md` | Opaque DSP-to-COBBA control ownership, capture seam, and promotion requirements. |
| `rom4_dsp_loader.md` | Recovered ROM4 DSP block descriptor, reset and demand-load contract. |
| `djr_dsp_integration.md` | DSP architectural knowledge reviewed from the parallel `djr-747` emulator without importing its code. |
| `gsm_a5_ciphering.md` | Standards-owned A5 keystream boundary, `COUNT` derivation and ciphering lifecycle. |
| `gsm_tch_f_l1_completion.md` | Audit of the generic GSM full-rate traffic-channel Layer 1 milestone against evidence. |
| `ordinary_sms.md` | Authoritative boundary for ordinary `SMS-DELIVER` and `SMS-SUBMIT`. |

## Native DSP and music-DSP reference maps

These maps retain source identities, addresses, negative controls and bounded
observations. They support the contracts above; an old capture is not a new
acceptance baseline and an isolated codec is not a working handset.

| Document | Owned detail |
| --- | --- |
| `rom4_dsp_execution.md` | Loader execution, staged/atomic comparison, instruction reachability and timing observations. |
| `rom4_native_observations.md` | Native bootstrap, compatibility-assist negatives and unassisted capture controls. |
| `rom4_dsp_capture.md` | RF/XF acquisition and vendor-tool reference methods and limits. |
| `c54x_arithmetic_observations.md` | Independent arithmetic and original-code operand/result controls. |
| `c54x_mu4_observations.md` | MU4 CPU composition, bus and atomic negative-control maps. |
| `rom4_rf_control.md` | Synthesizer, tuning and receive-window maps. |
| `rom4_audio_transport.md` | Data/I/O distinction, BSP, DTMF and physical clock observations. |
| `native_speech_activation.md` | Handset activation and native PCM-buffer observations, separate from isolated codecs. |
| `native_efr_codec.md` | Isolated EFR encoder/decoder, DTX, erasure, recovery and homing contracts. |
| `mu4_hardware.md` | Music-DSP storage, clocks, DMA, serial and MCU consumer contracts. |
| `mu4_recording.md` | Original recording, metadata and cold-catalogue maps. |
| `mu4_playback.md` | Original playback, serial provenance, refill and pending-tail maps. |
| `mad2_acquisition.md` | Retained MADos and physical acquisition references, not admitted peripheral semantics. |

## Address-level reverse-engineering references

The following are detailed firmware maps that remain useful when working in the
corresponding subsystem:

- `sim_registration.md`
- `sim_emulator_scope.md`
- `firmware_code_maps.md`
- `games_applications.md` (built-in games, their framework and boundary services)
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
