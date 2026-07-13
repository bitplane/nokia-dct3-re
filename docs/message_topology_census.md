# 3210 v6.00 message-topology census

This report separates extracted ROM facts, reviewed static semantics, and coherent runtime observations.

## Coverage

- Known API callsites: 2739 (3296 / 4217 arguments resolved, 78.2%)
- Callback-table entries: 126 (`0x2db720` through `0x2dbb0f`; index `0x28` is `0x2db860`)
- Known consumer entries: 5 (5 entry addresses decode)
- Descriptor registrations: 149 (112 ROM descriptors decoded, 37 RAM-built or unresolved)
- Runtime observations: 49

Descriptor storage: `dynamic_ram`=12, `fixed_ram`=6, `rom`=112, `stack`=18, `unresolved_pointer`=1.
Service-5 candidacy among unresolved descriptors: `dynamic_service_unresolved`=7, `excluded_other_service`=30.

## Runtime manifests

- `contact-service` (available): Default and bounded deep contact-service command-direction traces [subsystems: contact_service]
- `deep-gsm` (available): Coherent deep 3210 GSM/service trace with task and lower-boundary taps [subsystems: generic_service]

## Dynamic descriptor assessment

- `0x26341a` (stack): event `0x0114`, callback `0x00dc`; **excluded_by_fields**.
- `0x2684dc` (dynamic_ram): event `0x0114`, callback `0x1776`; **excluded_by_fields**.
- `0x26b49e` (dynamic_ram): event `0x0114`, callback `0x06f5`; **excluded_by_fields**.
- `0x26b4d8` (dynamic_ram): event `0x0114`, callback `0x06f5`; **excluded_by_fields**.
- `0x26b53e` (dynamic_ram): event `0x0114`, callback `0x06ff`; **excluded_by_fields**.
- `0x2a6c0e` (dynamic_ram): event `0x0114`, callback `0x0e14_or_0x0e15`; **excluded_by_fields**.
- `0x28c672` (runtime_indirect_resident): descriptor_source `[0x110f1c]`; **dormant_in_deep_runtime**.

## 0x05e8 inventory

- Effective literal loads: 0
- Recovered argumentless global-event generators: 16
- Generator callsites: `0x227c70`, `0x228a88`, `0x22b87e`, `0x249be4`, `0x24a99e`, `0x25df08`, `0x267a52`, `0x267a7e`, `0x267b96`, `0x2686a2`, `0x2687d0`, `0x26910e`, `0x2694f6`, `0x2883c8`, `0x2888d2`, `0x29a4dc`
- Decoded registration fields: 1

### Publisher owners

- callback `0x22` / `0x227bc0`: `0x227c70`; triggers `0x0c2c`; status-specific fallback.
- callback `0x21` / `0x228210`: `0x228a88`; triggers `0x0580`; status-specific fallback.
- callback `0x26` / `0x22b6d4`: `0x22b87e`; triggers `0x0388`; status-specific fallback.
- callback `0x54` / `0x249aa4`: `0x249be4`; triggers `0x05e7`; state-gated helper failure.
- callback `0x55` / `0x24a84c`: `0x24a99e`; triggers `0x0578`; state-gated completion.
- callback `0x3c` / `0x25db88`: `0x25df08`; triggers `multiple`; multi-status cleanup/error convergence.
- callback `0x37` / `0x2679da`: `0x267a52`, `0x267a7e`, `0x267b96`; triggers `0x1778`, `0x0598`, `0x0599`, `0x05dc`; lower-radio object/control classifier.
- callback `0x34` / `0x268534`: `0x2686a2`; triggers `0x05a5`; object/state-gated failure.
- callback `0x39` / `0x26870c`: `0x2687d0`; triggers `0x0578`; lower-radio completion fallback.
- callback `0x38` / `0x26903c`: `0x26910e`, `0x2694f6`; triggers `0x0578`, `0x05dc`; lower-radio completion/object rejection.
- callback `0x57` / `0x2882dc`: `0x2883c8`; triggers `0x05e1`; state-gated lifecycle fallback.
- callback `0x52` / `0x28882c`: `0x2888d2`; triggers `0x035c`; status-specific fallback.
- callback `0x65` / `0x29a3a4`: `0x29a4dc`; triggers `multiple`; multi-status state-machine fallback.

## Dispatcher scratch contract

- Range: `0x110f1c..0x110f2b` (transient_callback_abi)
- Argument 1 / object slot when supplied by the callback ABI: `0x110f20`
- Coherent runtime writer PCs: `0x200168`, `0x20016a`, `0x2af594`, `0x2af696`, `0x2af75e`, `0x2af770`, `0x2af78e`
- Coverage: Coherent RAM write watch; startup clearing, task-5 receive copies, and internal queue bookkeeping. No DSP/device writer observed.

## Acceptance chains

- **PASS** `service5_status_completion`: `0x05e8` -> `0x05ea` (dormant, reviewed_static)
- **PASS** `provider_05ea_to_task15_07dd`: `0x05ea` -> `0x07dd` (dormant, reviewed_static)
- **PASS** `task15_generated_09d8`: `0x07dd` -> `0x09d8` (dormant, reviewed_static)
- **PASS** `task10_completion_0434`: `0x1392` -> `0x0434` (dormant, reviewed_static)
- **PASS** `corrected_lower_result_0fbf`: `0x102f` -> `0x0fbf` (disproven_alternative, reviewed_static)
- **PASS** `task14_opcode_2a_to_lower_result_0fbf`: `0x09d8` -> `0x0fbf` (dormant, reviewed_static)
- **PASS** `lower_result_0fbf_to_task10_1392`: `0x0fbf` -> `0x1392` (dormant, reviewed_static)
- **PASS** `task17_completion_to_task5_13e2`: `0x0434` -> `0x13e2` (dormant, reviewed_static)
- **PASS** `task5_13e2_to_task14_1776`: `0x13e2` -> `0x1776` (dormant, reviewed_static)
- **PASS** `sim_task_notification_120c`: `notification_byte_0x10dcb7` -> `0x120c` (dormant, reviewed_static_and_runtime)
- **PASS** `task20_120c_to_sim_a012`: `0x120c` -> `A0/12` (dormant, reviewed_static)
- **PASS** `sim_a012_response_to_d0_object`: `0x006a` -> `D0_object` (dormant, reviewed_static)
- **PASS** `sim_d0_object_to_lower_classifier`: `0x1770..0x1788` -> `0x85e0` (dormant, reviewed_static)

## 0x05e8 boundary

Callback-table entry `0x28` (`0x2618e9`) accepts numeric status `0x05e8` and returns `0x05ea`. Reviewed control flow shows that branch does not consume an object argument from dispatcher scratch `0x110f1c`; the earlier object-bearing interpretation was incorrect.

The census recovers argumentless in-ROM generators of global event `0x05e8` (`0xbd << 3`). That is the expected ABI: the packed event becomes the callback input even with zero argument words. These are genuine candidate publishers, not incomplete object constructors.

The strongest object-bearing predecessor is now mapped through task-21 `0x120c`, synchronous `A0/12`, a `0x006a`/D0 response, the `0x177x` router, and classifier `0x267e68`. That chain remains dormant before `0x120c`; posting downstream events remains an invalid substitute.

Observed service-5 callback inputs in supplied coherent logs: `0x05e2`, `0x05f3`.

Reviewed runtime claims:
- `deep_gsm_transient_registration_scope` (deep-gsm, reviewed_runtime): The eight-second coherent run executes transient registrations only for service 0x0a at callers 0x26341e, 0x296ec8 and 0x296f16; no transient service-5 registration executes.
- `deep_gsm_resident_registration_scope` (deep-gsm, reviewed_runtime): No resident registration through 0x263d30 executes in the eight-second coherent run, including the indirect callsite at 0x28c672.
- `deep_gsm_service5_inputs` (deep-gsm, reviewed_runtime): An unforced coherent deep run delivered service-5 callback inputs 0x05f3 and 0x05e2, but no 0x05e8.
- `deep_gsm_target_chain_absent` (deep-gsm, reviewed_runtime): No 0x05e8, 0x05ea, 0x07dd, 0x09d8, or 0x0434 target-chain message was observed in the retained coherent-run analysis.
- `deep_gsm_task14_dormant` (deep-gsm, reviewed_runtime): Task 14 initialized its controller slots but received no subsequent message; decoder 0x267258 did not execute.
- `deep_gsm_05e8_publishers_dormant` (deep-gsm, reviewed_runtime): All 13 callback owners containing the 16 direct 0x05e8 publishers received the global 0x05e2 sweep once; none generated 0x05e8.
- `deep_gsm_sim_registration_notification_dormant` (deep-gsm, reviewed_runtime): Ordinary task-21 SIM traffic executes, but task-21 status 0x120c and its downstream A0/12/D0/0x177x classifier chain do not occur; no setting write to notification byte 0x10dcb7 was observed.

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
