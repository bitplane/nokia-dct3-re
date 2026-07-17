# Firmware code → meaning maps

Reference tables mapping runtime *values* to meanings (not addresses, so they
are not in the Ghidra symbol DB). Curated from the driver's one-time
trace-helper `*_desc()` functions; a new trace can reuse these tables.

## Class-`0x40` service-command codes (`response` byte)

Map from the historically named service response dispatcher
(`external_service_response_dispatch_237400`). The current contract is documented
in `external_service_topology.md`.

| code | meaning |
|------|---------|
| 0x64 | fatal_timeout_error |
| 0x65 | startup_status_bits |
| 0x70 | channel_map_enable |
| 0x71 | channel_map_disable |
| 0x73 | indexed_nv_event_read |
| 0x74 | indexed_nv_event_write |
| 0x7c | ui_string_action |
| 0xb0 | timed_action |
| 0xc8 | text_config_alt |
| 0xc9 | text_config |

## Task-1 service codes / states (`code` word)

From `controller_service_event_code_decode_21c646` / the state-`0x28` etc.
dispatcher. These states feed task-1 reports but are not owned by RTOS task 1.

| code | meaning |
|------|---------|
| 0x019e | raw_19e_to_state49 |
| 0x019f | raw_19f_to_state4a_or_4b |
| 0x01a0 | raw_1a0_to_state4a |
| 0x01a2 | raw_1a2_to_state31_adc_tick |
| 0x01a3 | raw_1a3_to_state26_or_28 |
| 0x01a4 | raw_1a4_to_state27_adc_monitor_service |
| 0x0026 | state26 |
| 0x0027 | state27_adc_monitor_service |
| 0x0028 | state28_vbat_check |
| 0x0031 | state31_adc_source_tick |
| 0x0049 | state49 |
| 0x004a | state4a |
| 0x004b | state4b |

## The 23-task system table (`0x2d7090`, stride `0xc`)

The authoritative, evidence-qualified task registry is `rtos_tasks.md`. This
file remains a value/code map and must not be treated as a second task-name
authority.
