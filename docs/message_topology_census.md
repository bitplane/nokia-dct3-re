# 3210 v6.00 message-topology census

This report separates extracted ROM facts, reviewed static semantics, and coherent runtime observations.

## Coverage

- Known API callsites: 2494 (3114 / 3776 arguments resolved, 82.5%)
- Callback-table entries: 126 (`0x2db720` through `0x2dbb0f`; index `0x28` is `0x2db860`)
- Known consumer entries: 5 (5 entry addresses decode)
- Descriptor registrations: 149 (112 ROM descriptors decoded, 37 RAM-built or unresolved)
- Runtime observations: 32

Runtime profile: coherent deep boot with NOKI3210_TRACE_TASKS=1 and NOKI3210_TRACE_GSM_LOWER=1.
Runtime source(s): `mame/error.log`.

## 0x05e8 inventory

- Effective literal loads: 0
- Recovered argumentless global-event generators: 16
- Decoded registration fields: 1

## Acceptance chains

- **PASS** `service5_object_completion`: `0x05e8` -> `0x05ea` (dormant, reviewed_static)
- **PASS** `provider_05ea_to_task15_07dd`: `0x05ea` -> `0x07dd` (dormant, reviewed_static)
- **PASS** `task15_generated_09d8`: `0x07dd` -> `0x09d8` (dormant, reviewed_static)
- **PASS** `task10_completion_0434`: `0x1392` -> `0x0434` (dormant, reviewed_static)
- **PASS** `corrected_lower_result_0fbf`: `0x102f` -> `0x0fbf` (disproven_alternative, reviewed_static)

## 0x05e8 boundary

The census finds `0x05e8` as the registered input of callback-table entry `0x28` (`0x2618e9`), not as a direct immediate producer callsite. The callback's object-bearing completion would return `0x05ea`, and the provider then constructs task-15 `0x07dd`.

The strongest evidenced missing predecessor is therefore **generic-service session/queue population before callback dispatch**: firmware must register or populate an object-bearing transaction that selects callback `0x28` and supplies `0x05e8`. Directly posting `0x05e8`, `0x05ea`, `0x07dd`, or `0x09d8` would skip this ownership boundary.

The census does find argumentless in-ROM generators of the global `0x05e8` event (`0xbd << 3`). They are triggers, not object producers: the packed-event ABI encodes zero argument words, so none supplies the object the callback path later expects. The quantified absence is narrower and stronger: no literal load and no recovered `0x05e8` generator carries an argument word, while unresolved RAM-built descriptors remain outside static coverage.

Observed service-5 callback inputs in supplied coherent logs: `0x05e2`, `0x05f3`.

Target-chain statuses observed as task messages: `0x05e8`=0, `0x05ea`=0, `0x07dd`=0, `0x09d8`=0, `0x0434`=0.

## Phase-two decision

A broader census is justified, but should be a separate phase. Its acceptance question should be: **does any in-ROM path self-issue contact-service commands `0x64`, `0x65`, `0x70`, `0x71`, or the `0x74` producer family, or are those contracts external?** That phase needs consumer-cascade recovery and RAM-built descriptor data-flow; merely adding more direct callsites would not answer it.
