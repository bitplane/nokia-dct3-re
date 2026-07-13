# 3210 v6.00 message-topology census

This report separates extracted ROM facts, reviewed static semantics, and coherent runtime observations.

## Coverage

- Known API callsites: 2739 (3296 / 4217 arguments resolved, 78.2%)
- Callback-table entries: 126 (`0x2db720` through `0x2dbb0f`; index `0x28` is `0x2db860`)
- Known consumer entries: 5 (5 entry addresses decode)
- Descriptor registrations: 149 (112 ROM descriptors decoded, 37 RAM-built or unresolved)
- Runtime observations: 46

## Runtime manifests

- `contact-service` (available): Default and bounded deep contact-service command-direction traces [subsystems: contact_service]
- `deep-gsm` (available): Coherent deep 3210 GSM/service trace with task and lower-boundary taps [subsystems: generic_service]

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
- **PASS** `task14_opcode_2a_to_lower_result_0fbf`: `0x09d8` -> `0x0fbf` (dormant, reviewed_static)
- **PASS** `lower_result_0fbf_to_task10_1392`: `0x0fbf` -> `0x1392` (dormant, reviewed_static)
- **PASS** `task17_completion_to_task5_13e2`: `0x0434` -> `0x13e2` (dormant, reviewed_static)
- **PASS** `task5_13e2_to_task14_1776`: `0x13e2` -> `0x1776` (dormant, reviewed_static)

## 0x05e8 boundary

The census finds `0x05e8` as the registered input of callback-table entry `0x28` (`0x2618e9`), not as a direct immediate producer callsite. The callback's object-bearing completion would return `0x05ea`, and the provider then constructs task-15 `0x07dd`.

The strongest evidenced missing predecessor is therefore **generic-service session/queue population before callback dispatch**: firmware must register or populate an object-bearing transaction that selects callback `0x28` and supplies `0x05e8`. Directly posting `0x05e8`, `0x05ea`, `0x07dd`, or `0x09d8` would skip this ownership boundary.

The census does find argumentless in-ROM generators of the global `0x05e8` event (`0xbd << 3`). They are triggers, not object producers: the packed-event ABI encodes zero argument words, so none supplies the object the callback path later expects. The quantified absence is narrower and stronger: no literal load and no recovered `0x05e8` generator carries an argument word, while unresolved RAM-built descriptors remain outside static coverage.

Observed service-5 callback inputs in supplied coherent logs: `0x05e2`, `0x05f3`.

Reviewed runtime claims:
- `deep_gsm_service5_inputs` (deep-gsm, reviewed_runtime): An unforced coherent deep run delivered service-5 callback inputs 0x05f3 and 0x05e2, but no object-bearing 0x05e8.
- `deep_gsm_target_chain_absent` (deep-gsm, reviewed_runtime): No 0x05e8, 0x05ea, 0x07dd, 0x09d8, or 0x0434 target-chain message was observed in the retained coherent-run analysis.
- `deep_gsm_task14_dormant` (deep-gsm, reviewed_runtime): Task 14 initialized its controller slots but received no subsequent message; decoder 0x267258 did not execute.

Target-chain statuses observed as task messages: `0x05e8`=0, `0x05ea`=0, `0x07dd`=0, `0x09d8`=0, `0x0434`=0.

## Contact-service command family

The ROM scan recovered 98 calls to `contact_message_alloc_234634`. The five target commands each have exactly one constructor; constructor existence is not treated as proof of an initiating producer.

### Command `0x64`: completion_or_timeout_status

- Incoming consumer: `0x236dc4`
- MCU constructor(s): `0x236dd8`/len `9` (status/timeout frame built by the same routine that consumes the incoming completion code)
- Initiating-producer classification: **organic MCU outbound status; external service peer is the counterparty** (high)
- Runtime construct/send/receive occurrences: 257 / 257 / 80
- Evidence: The constructor is reachable from the D9 watchdog timeout and from received-command dispatch. With the service address learned, its frame is destination 0x02/source 0x00 (phone), matching an outbound service response; the model supplies the healthy inbound completion.

### Command `0x65`: startup_status_bits

- Incoming consumer: `0x236bac`
- MCU constructor(s): `0x29bd4a`/len `1` (MCU status notification containing the byte at 0x11fed3)
- Initiating-producer classification: **organic MCU outbound notification to the external service peer** (high static, dormant runtime)
- Runtime construct/send/receive occurrences: 0 / 0 / 0
- Evidence: The constructor has callers at 0x236c52, 0x2379f4, and 0x292462; the last is outside the contact response dispatcher. Neither supplied runtime profile reaches it.

### Command `0x70`: channel_map_enable

- Incoming consumer: `0x23670c`
- MCU constructor(s): `0x236742`/len `1` (acknowledgement built only after an incoming 0x70 applies its 0x40-byte channel map)
- Initiating-producer classification: **external service/test peer request; MCU acknowledgement** (high)
- Runtime construct/send/receive occurrences: 0 / 0 / 0
- Evidence: The sole constructor is dominated by the incoming 0x70 branch in handler 0x23670c. Contact frames pass through task 7's external service transport, and MCU output addresses the learned service node. No natural transaction occurs in the supplied profiles.

### Command `0x71`: channel_map_disable

- Incoming consumer: `0x23670c`
- MCU constructor(s): `0x236736`/len `0` (acknowledgement built only after an incoming 0x71 disables the channel map)
- Initiating-producer classification: **external service/test peer request; MCU acknowledgement** (high)
- Runtime construct/send/receive occurrences: 0 / 0 / 0
- Evidence: The sole constructor is dominated by the incoming non-0x70 branch in handler 0x23670c. Contact frames pass through task 7's external service transport, and MCU output addresses the learned service node. No natural transaction occurs in the supplied profiles.

### Command `0x74`: indexed_nv_event_write

- Incoming consumer: `0x236560`
- MCU constructor(s): `0x23662c`/len `0` (completion acknowledgement after processing an incoming indexed NV operation)
- Initiating-producer classification: **external service/test peer request; MCU acknowledgement** (high)
- Runtime construct/send/receive occurrences: 0 / 0 / 0
- Evidence: The sole command constructor is inside the incoming indexed-operation handler. Contact frames pass through task 7's external service transport. It is unrelated to the direct scheduler-event 0x74 producers at 0x213fcc and 0x214836.

## Contact-service transport boundary

The strongest evidenced boundary is the external service/test peer behind task 7's lower service transport. Task 2 builds class-0x40 frames at 0x234634; 0x234684 offers them to channel 0x6400 via 0x2b203e and queues them through 0x2b0482; 0x2b0482 routes ordinary frames to task 7. The frame header is [0]=destination, [1]=source: after service discovery, MCU output is destination 0x02/source 0x00 (phone). The ROM census finds organic MCU output for 0x64 and 0x65, while 0x70, 0x71, and command 0x74 are external requests whose sole MCU constructors are acknowledgements. Task 7 is the on-device transport adapter, not the semantic producer of those requests.

The numeric command and scheduler event `0x74` are separate namespaces. The ROM contains direct MCU producers of scheduler event `0x74` at `0x213fcc` and `0x214836`; they do not construct contact-service command `0x74`.


## Phase-two decision

This bounded contact-service phase classifies the available MCU constructors and the observed transport behavior. A future full-ROM contract census can reuse the same distinction between an initiating request, a response/acknowledgement with the same id, and a scheduler event in another namespace.
