# Evidence regime

This project records conclusions in four artifact classes. A finding belongs in
the narrowest class that describes what was actually observed; not every result
is a message-topology edge.

## Artifact classes

| Class | Authoritative source | Use |
| --- | --- | --- |
| Message topology | `tools/profiles/*.json` plus `message_census.py` output | Producers, consumers, callbacks, ownership and transport direction. |
| Hardware contract | `evidence/hardware_contracts.json` | Register/device boundaries and their fidelity level. |
| State predicate | `evidence/state_predicates.json` | Firmware readiness conditions and whether the current model satisfies them. |
| Falsification | `evidence/falsifications.json` | Disproven or unsupported hypotheses worth preventing from recurring. |

`python3 tools/validate_evidence.py` checks ledger schemas, unique IDs, enum
values, evidence links and named runtime manifests. `make census` runs this
validation before static extraction.

## Runtime manifests

Runtime observations are scoped by manifests under `tools/run_manifests/`:

- `default`: canonical CONTACT SERVICE oracle;
- `deep-gsm`: coherent generic-service/GSM frontier trace;
- `contact-service`: class-`0x40` constructor/send/receive traces; and
- `3330-smoke`: bounded second-ROM portability run.

Each runtime regex declares a subsystem. A manifest can only contribute to the
subsystems it names, so a contact-only trace cannot be interpreted as evidence
that GSM callbacks were absent. Missing raw logs are reported as missing.
Reviewed runtime conclusions remain explicit `runtime_claims` in the firmware
profile with their source document; they are not silently replaced by an empty
run.

Raw logs and run directories remain generated, ignored artifacts. The manifest
defines how to classify a compatible run; it does not make a transient log part
of the source tree.

## Stable identities

Topology nodes use symbolic IDs such as `task14_object_decoder` and
`external_service_peer`. Firmware addresses live under each node's
`rom_addresses` map. Cross-ROM work should add another address mapping or mark
the node absent; it must not create a second conceptual node merely because an
address changed.

## Documentation authority

Concise subsystem documents and normalized artifacts contain current
conclusions. Detailed firmware maps preserve addresses and reproducible
control-flow conclusions, but chronological probe narratives belong in Git
history and do not form a separate documentation authority.

When a new dig closes:

1. add or update one normalized artifact;
2. attach static or runtime provenance and confidence;
3. update the concise subsystem conclusion;
4. retain a negative result only when it prevents a plausible repeated mistake;
5. do not preserve an implementation shim merely as history.

## Research-force policy

Supported behavior may model only a hardware-visible device, register, signal,
or transport response. It must not advance boot by rewriting firmware RAM,
substituting a firmware return value or instruction, replaying callback keys, or
posting an internal message without a modeled external producer.

Temporary probes are permitted when they are opt-in, answer one pre-registered
question, and are deleted after their conclusion is normalized. A forced branch
is evidence about that branch only; it is never evidence that the forcing
mechanism belongs in the emulator. Disproven hypotheses worth retaining belong
in `evidence/falsifications.json`. Exact deleted knob names and implementations
belong in Git history, not current documentation.
