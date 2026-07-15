# 3210 v6.00 message-topology census

This report separates extracted ROM facts, reviewed static semantics, and coherent runtime observations.

## Coverage

- Known API callsites: 2739 (3296 / 4217 arguments resolved, 78.2%)
- Callback-table entries: 126 (`0x2db720` through `0x2dbb0f`; index `0x28` is `0x2db860`)
- Known consumer entries: 6 (6 entry addresses decode)
- Descriptor registrations: 149 (112 ROM descriptors decoded, 37 RAM-built or unresolved)
- Runtime observations: 59

Descriptor storage: `dynamic_ram`=12, `fixed_ram`=6, `rom`=112, `stack`=18, `unresolved_pointer`=1.
Service-5 candidacy among unresolved descriptors: `dynamic_service_unresolved`=7, `excluded_other_service`=30.

## Runtime manifests

- `contact-service` (available): Default and bounded deep contact-service command-direction traces [subsystems: contact_service]
- `deep-gsm` (available): Current coherent 3210 contact-peer, SIM and lower-service frontier trace [subsystems: generic_service]

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

## Object-bearing 0x05dc lifecycle constructors

The ROM scan recovered 38 direct packed `0x05e0` constructors with at least two argument words. Argumentless and selector-only lifecycle events are excluded.

- `0x227c22`: selector `0x0023`, object `?`, argc `2`; **candidate_owner_0x23** - callback 0x23 lifecycle object; object register unresolved.
- `0x228a66`: selector `0x0022`, object `0x0001`, argc `2`; **downstream_failure_lifecycle** - callback 0x22 scalar lifecycle constructed inside callback 0x21.
- `0x228aea`: selector `0x0021`, object `?`, argc `3`; **later_service_lifecycle** - callback 0x21 allocated object built from callback 0x24 in framework mode 9 after status 0x013d.
- `0x228b98`: selector `0x0021`, object `?`, argc `3`; **later_service_lifecycle** - callback 0x21 allocated object built from callback 0x54 after status 0x1a34.
- `0x229b36`: selector `0x0026`, object `?`, argc `3`; **later_service_lifecycle** - callback 0x26 normalized object built by callback 0x1f.
- `0x229b4e`: selector `0x0026`, object `0x0000`, argc `3`; **scalar_control** - callback 0x26 null-object lifecycle control.
- `0x22aa6a`: selector `0x001f`, object `0x0002`, argc `3`; **scalar_control** - callback 0x1f lifecycle with constant object word 2.
- `0x22baa4`: selector `0x0026`, object `?`, argc `3`; **later_service_lifecycle** - callback 0x26 normalized object built by callback 0x1e.
- `0x248f7a`: selector `0x0020`, object `0x0000`, argc `3`; **scalar_control** - callback 0x20 null-object lifecycle control.
- `0x24a8f4`: selector `0x0054`, object `0x0000`, argc `2`; **downstream_failure_lifecycle** - callback 0x54 null-object lifecycle constructed inside callback 0x55.
- `0x24aa2a`: selector `0x0006`, object `0x0001`, argc `2`; **scalar_control** - callback 0x06 lifecycle with constant object word 1.
- `0x24d712`: selector `0x0040`, object `?`, argc `2`; **candidate_owner_0x40** - callback 0x40 firmware-context object.
- `0x24e754`: selector `0x0007`, object `?`, argc `2`; **dormant_configuration_cycle** - callback 0x07 context object; requires callback-state 0x5d activation and is not a bootstrap predecessor.
- `0x255424`: selector `0x002c`, object `?`, argc `2`; **scalar_or_state** - callback 0x2c lifecycle object sourced from byte state.
- `0x25c4aa`: selector `0x0051`, object `?`, argc `2`; **candidate_owner_0x51** - callback 0x51 lifecycle object.
- `0x25ca50`: selector `0x0026`, object `?`, argc `3`; **later_service_lifecycle** - callback 0x26 normalized object built by callback 0x51.
- `0x25d25c`: selector `0x003c`, object `0x0003`, argc `2`; **downstream_failure_lifecycle** - callback 0x3c scalar lifecycle constructed inside callback 0x51.
- `0x25d6d0`: selector `0x003c`, object `0x0001`, argc `2`; **downstream_failure_lifecycle** - callback 0x3c scalar lifecycle constructed inside callback 0x25.
- `0x25d8d0`: selector `0x003d`, object `0x0000`, argc `2`; **scalar_control** - callback 0x3d null-object lifecycle control.
- `0x2613d4`: selector `0x0026`, object `?`, argc `3`; **later_service_lifecycle** - callback 0x26 normalized object built by callback 0x29.
- `0x261f26`: selector `0x0029`, object `?`, argc `3`; **candidate_owner_0x29** - callback 0x29 three-word lifecycle object.
- `0x26800c`: selector `0x0039`, object `?`, argc `2`; **sat_controller_classifier** - dynamic callbacks 0x35..0x3a; exhaustive concrete dispatch maps modes 0x10/0x11/0x13/0x20/0x21/0x22/0x23/0x24 exclusively from proactive-SIM events 0x177c/0x1782/0x1779/0x177a/0x177d/0x1780/0x1781/0x1777; status 0x1978 only reuses existing state.
- `0x26d39c`: selector `0x0072`, object `?`, argc `2`; **candidate_owner_0x72** - callback 0x72 lifecycle object.
- `0x26d3fc`: selector `0x0073`, object `?`, argc `2`; **candidate_owner_0x73** - callback 0x73 lifecycle object.
- `0x26d49a`: selector `0x0073`, object `?`, argc `2`; **candidate_owner_0x73** - callback 0x73 lifecycle object.
- `0x26dcc4`: selector `0x0026`, object `?`, argc `3`; **later_service_lifecycle** - callback 0x26 normalized object built by callback 0x79.
- `0x26de7c`: selector `0x0079`, object `?`, argc `3`; **candidate_owner_0x79** - callback 0x79 three-word lifecycle object.
- `0x26e386`: selector `0x0073`, object `?`, argc `2`; **candidate_owner_0x73** - callback 0x73 lifecycle object.
- `0x26e398`: selector `0x0077`, object `0x0000`, argc `3`; **scalar_control** - callback 0x77 null-object lifecycle control.
- `0x26e578`: selector `0x0076`, object `?`, argc `2`; **candidate_owner_0x76** - callback 0x76 lifecycle object.
- `0x28886e`: selector `0x0006`, object `0x0003`, argc `3`; **scalar_control** - callback 0x06 three-word lifecycle control with constant object word 3.
- `0x2888c6`: selector `0x0006`, object `0x0003`, argc `3`; **scalar_control** - callback 0x06 three-word lifecycle control with constant object word 3.
- `0x28ee92`: selector `0x0061`, object `?`, argc `2`; **candidate_owner_0x61** - callback 0x61 lifecycle object.
- `0x299064`: selector `0x006d`, object `?`, argc `2`; **candidate_owner_0x6d** - callback 0x6d lifecycle object.
- `0x2991ca`: selector `0x006e`, object `?`, argc `2`; **candidate_owner_0x6e** - callback 0x6e lifecycle object.
- `0x299470`: selector `0x006c`, object `?`, argc `2`; **candidate_owner_0x6c** - callback 0x6c lifecycle object.
- `0x2a8868`: selector `0x0054`, object `0x0001`, argc `3`; **downstream_failure_lifecycle** - callback 0x54 scalar lifecycle constructed inside callback 0x42.
- `0x2ade5c`: selector `0x002c`, object `?`, argc `2`; **scalar_or_state** - callback 0x2c lifecycle object sourced from byte state.

Assessment coverage: **complete**; missing 0, stale 0.

### Runtime-built packed events

The extractor leaves 22 packed-event values runtime-built; 22 have reviewed bounds. Assessment coverage is **complete**.
Reviewed two-word candidates: 5; can publish lifecycle event `0x05e0`: **no**.

- `0x26b5fa`: events `0x0594`, `0x0c2d`; **subscription_completion** - 0x26b58c caller-supplied completion with selector and object; both ROM callers reviewed.
- `0x26b642`: events `0x0594`, `0x0c2d`; **subscription_completion** - same parameterized subscription callback.
- `0x26b65c`: events `0x0594`, `0x0c2d`; **subscription_completion** - same parameterized subscription callback.
- `0x26b678`: events `0x0594`, `0x0c2d`; **subscription_completion** - same parameterized subscription callback.
- `0x26b6c0`: events `0x0594`, `0x0c2d`; **subscription_completion** - same parameterized subscription callback.

### Registration-relevant intersection

Intersecting these constructors with callbacks that contain direct `0x05e8` publishers leaves owners `0x21`, `0x22`, `0x26`, `0x3c`, and `0x54`, plus the dynamic `0x35..0x3a` classifier. Exhaustive concrete dispatch maps every classifier mode exclusively from proactive-SIM events `0x1777..0x1782`, excluding the whole classifier from ordinary registration. Backward tracing excludes every other member from the coherent bootstrap: callback `0x24` builds `0x21` only in framework mode 9 and emits the `0x0388` consumed by `0x26` only in mode 11, while the coherent run remains in mode 0; `0x22` is constructed by `0x21`, `0x3c` by `0x51`/`0x25`, and `0x54` by `0x55`/`0x42`. These are later fallback/cleanup convergence paths, not demonstrated ordinary-registration predecessors.

## Dispatcher scratch contract

- Range: `0x110f1c..0x110f2b` (transient_callback_abi)
- Argument 1 / object slot when supplied by the callback ABI: `0x110f20`
- Coherent runtime writer PCs: `0x200168`, `0x20016a`, `0x2af594`, `0x2af696`, `0x2af75e`, `0x2af770`, `0x2af78e`
- Coverage: Coherent RAM write watch; startup clearing, task-5 receive copies, and internal queue bookkeeping. No DSP/device writer observed.

## Acceptance chains

- **PASS** `service5_status_completion`: `0x05e8` -> `0x05ea` (dormant, reviewed_static)
- **PASS** `provider_05ea_to_task15_07dd`: `0x05ea` -> `0x07dd` (dormant, reviewed_static)
- **PASS** `task15_generated_09d8`: `0x07dd` -> `0x09d8` (dormant, reviewed_static)
- **PASS** `task10_1391_completion_0434`: `0x1391` -> `0x0434` (dormant, reviewed_static)
- **PASS** `corrected_lower_result_0fbf`: `0x102f` -> `0x0fbf` (disproven_alternative, reviewed_static)
- **PASS** `task14_opcode_2a_to_lower_result_0fbf`: `0x09d8` -> `0x0fbf` (dormant, reviewed_static)
- **PASS** `lower_result_0fbf_to_context_handler`: `0x0fbf` -> `dispatch 0x253610` (dormant, reviewed_static)
- **PASS** `lower_result_0fc1_to_task10_1391_completion`: `0x0fc1` -> `0x1391` (dormant, reviewed_static)
- **PASS** `task14_opcode_36_to_lower_result_0fc1`: `0x09d8 object opcode 0x36` -> `0x1033 -> 0x0fc1` (dormant, reviewed_static)
- **PASS** `lower_result_0fc2_to_task10_1392_update`: `0x0fc2` -> `0x1392` (dormant, reviewed_static)
- **PASS** `corrected_task10_1392_dispatch`: `0x1392` -> `dispatch 0x21b790` (disproven_alternative, reviewed_static)
- **PASS** `task17_completion_to_task5_13e2`: `0x0434` -> `0x13e2` (dormant, reviewed_static)
- **PASS** `task5_13e2_to_task14_1776`: `0x13e2` -> `0x1776` (dormant, reviewed_static)
- **PASS** `task14_opcode_3a_to_context_initialization`: `0x09d8 object opcode 0x3a` -> `0x0fc8 -> type-2 object -> 0x0ac8 command 0x16` (dormant_downstream_cycle, reviewed_static)
- **PASS** `context_09cd_to_callback7_object`: `context slot matches 0x10e89a and callback-state slot 0x5d at 0x11fcdd is 1 or 2` -> `0x09cd -> 0x85e0(selector 7, object)` (dormant, reviewed_static_and_runtime)
- **PASS** `context_09d0_to_callback5d_code7`: `callback-state slot 0x45 is neither 0 nor 5 (with state-6 active-slot exception)` -> `0x09d0 -> eligible callback 0x5d action 0x00dc -> code-7 completion` (mapped_nonordinary_unproved, reviewed_static_and_runtime)
- **PASS** `callback5d_delayed_code7_completion`: `0x05e1/0x05e7/0x05dc -> task-local class 0x52; or direct 0x05eb/0x06c5` -> `0x06c5 -> report code 0x07` (dormant, reviewed_static_and_runtime)
- **PASS** `sim_task_notification_120c`: `notification_byte_0x10dcb7` -> `0x120c` (dormant_sat, reviewed_static_and_runtime)
- **PASS** `task20_120c_to_sim_a012`: `0x120c` -> `A0/12` (dormant_sat, reviewed_static)
- **PASS** `sim_a012_response_to_d0_object`: `0x006a` -> `D0_object` (dormant_sat, reviewed_static)
- **PASS** `sim_d0_object_to_lower_classifier`: `0x1770..0x1788` -> `0x85e0` (conditional_sat, reviewed_static)

## 0x05e8 boundary

Callback-table entry `0x28` (`0x2618e9`) accepts numeric status `0x05e8` and returns `0x05ea`. Reviewed control flow shows that branch does not consume an object argument from dispatcher scratch `0x110f1c`; the earlier object-bearing interpretation was incorrect.

The census recovers argumentless in-ROM generators of global event `0x05e8` (`0xbd << 3`). That is the expected ABI: the packed event becomes the callback input even with zero argument words. These are genuine candidate publishers, not incomplete object constructors.

The mapped task-21 `0x120c` -> `A0/12` -> D0 -> `0x177x` path is GSM 11.14 SIM Toolkit, not the ordinary registration predecessor. The current EF_PHASE=2 card correctly leaves it dormant; posting downstream events remains an invalid substitute.

Observed service-5 callback inputs in supplied coherent logs: `0x05e2`, `0x05f3`.

Reviewed runtime claims:
- `deep_gsm_transient_registration_scope` (deep-gsm, reviewed_runtime): The eight-second coherent run executes transient registrations only for service 0x0a at callers 0x26341e, 0x296ec8 and 0x296f16; no transient service-5 registration executes.
- `deep_gsm_resident_registration_scope` (deep-gsm, reviewed_runtime): No resident registration through 0x263d30 executes in the eight-second coherent run, including the indirect callsite at 0x28c672.
- `deep_gsm_service5_inputs` (deep-gsm, reviewed_runtime): An unforced coherent deep run delivered service-5 callback inputs 0x05f3 and 0x05e2, but no 0x05e8.
- `deep_gsm_target_chain_absent` (deep-gsm, reviewed_runtime): No 0x05e8, 0x05ea, 0x07dd, 0x09d8, or 0x0434 target-chain message was observed in the retained coherent-run analysis.
- `deep_gsm_task14_dormant` (deep-gsm, reviewed_runtime): Task 14 initialized its controller slots but received no subsequent message; decoder 0x267258 did not execute.
- `deep_gsm_05e8_publishers_dormant` (deep-gsm, reviewed_runtime): All 13 callback owners containing the 16 direct 0x05e8 publishers received the global 0x05e2 sweep once; none generated 0x05e8.
- `deep_gsm_callback24_lifecycle_dormant` (deep-gsm, reviewed_runtime): Callback 0x24 receives only the global 0x05e2 sweep with framework mode 0. Its mode-9 callback-0x21 constructor and mode-11 0x0388 publishers do not execute; callback 0x21 and 0x26 therefore cannot reach their 0x05e8 fallback branches in the coherent boot.
- `deep_gsm_sim_registration_notification_dormant` (deep-gsm, reviewed_runtime): Ordinary task-21 SIM traffic executes, while the EF_PHASE=2 card correctly produces no TERMINAL PROFILE, 0x120c, FETCH A0/12, or proactive D0 command. A phase-3 isolation card cannot reach the downstream profile-download function before the registration wall.

Target-chain statuses observed as task messages: `0x05e8`=0, `0x05ea`=0, `0x07dd`=0, `0x09d8`=0, `0x0434`=0.

## Contact-service command family

The ROM scan recovered 98 calls to `contact_message_alloc_234634`. The five target commands each have exactly one constructor; constructor existence is not treated as proof of an initiating producer.

### Command `0x64`: completion_or_timeout_status

- Incoming consumer: `0x236dc4`
- MCU constructor(s): `0x236dd8`/len `9` (status/timeout frame built by the same routine that consumes the incoming completion code)
- Initiating-producer classification: **organic MCU outbound status; external service peer is the counterparty** (high)
- Runtime construct/send/receive occurrences: 3 / 2 / 2
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
- Runtime construct/send/receive occurrences: 1 / 1 / 1
- Evidence: The sole constructor is dominated by the incoming 0x70 branch in handler 0x23670c. Contact frames pass through task 7's external service transport, and MCU output addresses the learned service node. The canonical frontier profile observes one peer request, one firmware acknowledgement construction, and one send.

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

### Command `0x8e`: battery_service_control

- Incoming consumer: `0x235848`
- MCU constructor(s): `0x2358a0`/len `1` (acknowledgement echoing the selector byte after the incoming command dispatches one of five service actions)
- Initiating-producer classification: **external service/test peer request; MCU acknowledgement** (high static, dormant runtime)
- Runtime construct/send/receive occurrences: 0 / 0 / 0
- Evidence: The task-2 dispatcher selects 0x235848 only for incoming command 0x8e. Payload selector 3 calls 0x2a6880 to post task-19 event 0x43 and sets the associated state through 0x2a67d0; selector 4 analogously posts event 0x44. The sole 0x8e constructor at 0x2358a0 is dominated by this incoming handler and echoes the selector in its one-byte acknowledgement. No supplied coherent contact manifest observes the command.

## Contact-service transport boundary

The strongest evidenced boundary is the external service/test peer behind task 7's lower service transport. Task 2 builds class-0x40 frames at 0x234634; 0x234684 offers them to channel 0x6400 via 0x2b203e and queues them through 0x2b0482; 0x2b0482 routes ordinary frames to task 7. The frame header is [0]=destination, [1]=source: after service discovery, MCU output is destination 0x02/source 0x00 (phone). The ROM census finds organic MCU output for 0x64 and 0x65, while 0x70, 0x71, 0x74, and 0x8e are external requests whose sole MCU constructors are acknowledgements. Task 7 is the on-device transport adapter, not the semantic producer of those requests.

The numeric command and scheduler event `0x74` are separate namespaces. The ROM contains direct MCU producers of scheduler event `0x74` at `0x213fcc` and `0x214836`; they do not construct contact-service command `0x74`.


## Phase-two decision

This bounded contact-service phase classifies the available MCU constructors and the observed transport behavior. A future full-ROM contract census can reuse the same distinction between an initiating request, a response/acknowledgement with the same id, and a scheduler event in another namespace.
