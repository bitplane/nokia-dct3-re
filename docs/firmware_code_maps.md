# Firmware code → meaning maps

Reference tables curated from trace-helper `*_desc()` functions before those were
stripped from the driver. These map runtime *values* (not addresses, so they're
not in the Ghidra symbol DB) to meanings. Re-add a trace using these if needed.

## Contact-service response codes (`response` byte)

Map from the task-7 contact response dispatcher
(`contact_service_response_dispatch_237400`). The current contract is documented
in `contact_service_topology.md`.

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

From `task1_service_event_code_decode_21c646` / the state-`0x28` etc. dispatcher.

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

## The 23-task system table (`0x2d7090`, stride `0xc` = {init_ptr, w1, flags})

Every RTOS task is created from this table (both scheduler-init walkers loop all 23;
see `service_bootstrap.md`). Roles below are confidence-tagged (✅ RE'd, 🟡 inferred
from region/behaviour, ❓ unknown). Tasks 10–21 are the subsystem-init "reporters"
that fill the `0x112280` readiness checklist (the `0x15` barrier).

| # | init | role | conf |
|---|------|------|------|
| 0 | `0x2a92d2` | startup supervisor / task-activation (`0x2a9xxx`) | 🟡 |
| 1 | `0x270170` | startup state machine (mode chain `000d→…`) | ✅ |
| 2 | `0x237bb4` | contact service (`contact_service_mode7_loop`) | ✅ |
| 3 | `0x2b18a0` | LCD / display driver (`0x2b1xxx`, near `lcd_write 0x2b1c96`) | 🟡 |
| 4 | `0x2b3fb8` | display/render helper (`0x2b3xxx`; state at `0x112500`) | ❓ |
| 5 | `0x2af630` | task-5 MMI render / state-machine VM (`task5_mmi_main_loop`) | ✅ |
| 6 | `0x297fc4` | MMI / display manager (`display_manager_297fc4`) | ✅ |
| 7 | `0x2a5890` | service / SIM-adjacent task (`0x2a5xxx`; reads `[0x117194]`) | 🟡 |
| 8 | `0x283ce8` | service transport / node-`0x18` (`service_lower_event_bridge`) | ✅ |
| 9 | `0x28e164` | unknown (`0x28exxx`; state at `0x1123ac/b0`) | ❓ |
| 10 | `0x21bf60` | subsystem-init reporter → checklist code `0x11` | 🟡 |
| 11 | `0x2159c4` | subsystem-init reporter → `0x0b` | 🟡 |
| 12 | `0x273ea0` | subsystem-init reporter → `0x0c` (startup fallback) | 🟡 |
| 13 | `0x23ebd0` | subsystem-init reporter → `0x0d` | 🟡 |
| 14 | `0x248318` | subsystem-init reporter → `0x0e` | 🟡 |
| 15 | `0x20a8a8` | subsystem-init reporter → `0x0f` | 🟡 |
| 16 | `0x24f5a0` | subsystem-init reporter → `0x10` | 🟡 |
| 17 | `0x22391c` | subsystem-init reporter → `0x11` (task-1 service dispatch) | 🟡 |
| 18 | `0x285c14` | subsystem-init reporter → `0x12` (service-lower bridge) | 🟡 |
| 19 | `0x21de4c` | subsystem-init reporter → `0x13` | 🟡 |
| 20 | `0x208134` | subsystem-init reporter → `0x14` | 🟡 |
| 21 | `0x27eae0` | subsystem-init reporter → `0x15` (battery state) | 🟡 |
| 22 | `0x2b6548` | DSP-interface task (`dsp_if_task_2b6548`; mailbox recv/dispatch) | ✅ |

Note: the **GSM-L1 / network protocol stack is NOT a task here** — it is a
message-driven subsystem behind task 22 (the DSP-IF task), in the DSP-driver layer
`0x2b7xxx–0x2c9xxx`, never entered on our boot (see `network_scouting.md`).
