// Export selected Nokia 3210 firmware functions as Ghidra decompiler text.
// @category Nokia3210

import java.io.File;
import java.io.PrintWriter;
import java.math.BigInteger;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.lang.Register;
import ghidra.program.model.lang.RegisterValue;
import ghidra.program.model.symbol.SourceType;

public class ExportNokiaFunctions extends GhidraScript {
	private static class Target {
		final String name;
		final long addr;

		Target(String name, long addr) {
			this.name = name;
			this.addr = addr;
		}
	}

	private static class Label {
		final String name;
		final long addr;

		Label(String name, long addr) {
			this.name = name;
			this.addr = addr;
		}
	}

	private static final Label[] LABELS = new Label[] {
		new Label("battery_classifier_high_threshold_defaults_2e1ff4", 0x002e1ff4L),
		new Label("battery_source_channel_map_2e2d74", 0x002e2d74L),
		new Label("contact_service_case_e2_event_source_update_2379e8", 0x002379e8L),
		new Label("contact_service_case_e1_clear_display_state_2379f2", 0x002379f2L),
		new Label("contact_service_case_de_powerdown_reset_2379fa", 0x002379faL),
		new Label("contact_service_case_dd_start_service_sweep_237a18", 0x00237a18L),
		new Label("contact_service_case_dc_schedule_event1c_237a70", 0x00237a70L),
		new Label("contact_service_case_db_common_reset_state_237a7a", 0x00237a7aL),
		new Label("contact_service_case_da_status_update_237ade", 0x00237adeL),
		new Label("contact_service_d9_5f00_request_call_2379a4", 0x002379a4L),
		new Label("contact_service_d9_dispatch_after_5f00_request_2379aa", 0x002379aaL),
		new Label("contact_service_case_d9_watchdog_poll_237b28", 0x00237b28L),
		new Label("contact_service_case_df_e0_noop_237ba8", 0x00237ba8L),
		new Label("contact_startup_fault_seed_bit08_234750", 0x00234750L),
		new Label("contact_startup_fault_seed_bit40_234758", 0x00234758L),
		new Label("contact_startup_fault_seed_bit80_234760", 0x00234760L),
		new Label("contact_startup_fault_clear_bit40_if_not_service1_2347b2", 0x002347b2L),
		new Label("contact_startup_service_ready_sets_bit04_2347d0", 0x002347d0L),
		new Label("contact_startup_fault_clear_bit40_after_nv_compare_234832", 0x00234832L),
		new Label("contact_startup_resource_fault_scan_clear_bit40_234896", 0x00234896L),
		new Label("contact_startup_bit80_selects_status19_delay_234916", 0x00234916L),
		new Label("contact_startup_fault_schedule_status19_234922", 0x00234922L),
		new Label("contact_response_dispatch_gate_check_23742a", 0x0023742aL),
		new Label("contact_response_code65_startup_status_call_237840", 0x00237840L),
		new Label("contact_response_code65_status_detail_load_237842", 0x00237842L),
		new Label("contact_response_gated_or_unknown_drop_237896", 0x00237896L),
		new Label("contact_response65_status_bit1_event15_check_236bc8", 0x00236bc8L),
		new Label("contact_response65_status_bit2_event17_check_236bde", 0x00236bdeL),
		new Label("contact_response65_status_bit3_event18_check_236bf4", 0x00236bf4L),
		new Label("contact_response65_status_bit4_event19_check_236c08", 0x00236c08L),
		new Label("contact_response65_status_bit5_event1a_check_236c20", 0x00236c20L),
		new Label("contact_response65_no_startup_bits_clear_236c4a", 0x00236c4aL),
		new Label("contact_service_timeout_entry_arg2_236dc4", 0x00236dc4L),
		new Label("contact_service_timeout_build_fatal64_236dd4", 0x00236dd4L),
		new Label("contact_service_timeout_set_detail45_236df2", 0x00236df2L),
		new Label("contact_service_timeout_mark_status_bit0_236e24", 0x00236e24L),
		new Label("contact_service_timeout_queue_fatal_236f44", 0x00236f44L),
		new Label("contact_service_mode7_event_loop_237bb6", 0x00237bb6L),
		new Label("contact_service_mode7_recv_next_message_237bc6", 0x00237bc6L),
		new Label("contact_service_mode7_dispatch_message_237bca", 0x00237bcaL),
		new Label("contact_service_mode7_frame_type_dispatch_237bde", 0x00237bdeL),
		new Label("contact_service_mode7_non40_status_or_reply_237c46", 0x00237c46L),
		new Label("contact_service_mode7_channel_reply_237c54", 0x00237c54L),
		new Label("contact_service_mode7_status40_dispatch_237c70", 0x00237c70L),
		new Label("contact_service_mode7_unknown_response_drop_237c88", 0x00237c88L),
		new Label("contact_service_d9_ack_byte_check_237b42", 0x00237b42L),
		new Label("contact_service_d9_ack_resets_watchdog_counter_237b4e", 0x00237b4eL),
		new Label("contact_service_d9_counter_reset_or_increment_237b50", 0x00237b50L),
		new Label("contact_service_d9_posts_event19_237b2e", 0x00237b2eL),
		new Label("contact_service_d9_self_rearm_event19_237b32", 0x00237b32L),
		new Label("contact_service_da_clears_startup_bits40_04_237b04", 0x00237b04L),
		new Label("contact_service_timeout_threshold_237b7c", 0x00237b7cL),
		new Label("contact_service_d9_timeout_arg2_call_237b80", 0x00237b80L),
		new Label("battery_vbat_check_start_21e01e", 0x0021e01eL),
		new Label("battery_vbat_precheck_and_state_dispatch_21e098", 0x0021e098L),
		new Label("battery_vbat_check_request_7201_21e0b0", 0x0021e0b0L),
		new Label("battery_vbat_state_dispatch_21e0de", 0x0021e0deL),
		new Label("battery_vbat_state2_countdown_21e154", 0x0021e154L),
		new Label("battery_vbat_state3_post_e4_21e1ae", 0x0021e1aeL),
		new Label("battery_vbat_state4_reset_21e224", 0x0021e224L),
		new Label("battery_vbat_state28_common_next_state_21e236", 0x0021e236L),
		new Label("battery_vbat_state28_common_phase_store_21e24a", 0x0021e24aL),
		new Label("battery_vbat_state3_log_string_21e338", 0x0021e338L),
		new Label("battery_classifier_state1_keep_startup_27cc3e", 0x0027cc3eL),
		new Label("battery_classifier_state2_mid_range_27cc4a", 0x0027cc4aL),
		new Label("battery_classifier_state3_below_low_threshold_27cc4e", 0x0027cc4eL),
		new Label("battery_classifier_return_27cc70", 0x0027cc70L),
		new Label("battery_classifier_store_state_27dcfa", 0x0027dcfaL),
		new Label("task1_service_state31_event_source_rearm_21dff6", 0x0021dff6L),
		new Label("task1_service_state27_adc_monitor_service_220050", 0x00220050L),
		new Label("task1_decode_raw_1a2_to_state31_21c6ba", 0x0021c6baL),
		new Label("task1_decode_raw_1a4_indirect_state_21c6ce", 0x0021c6ceL),
		new Label("task1_service_state28_battery_vbat_worker_21e01e", 0x0021e01eL),
		new Label("adc_monitor_walk_next_source_2a6f82", 0x002a6f82L),
		new Label("adc_monitor_classifier_call_2a6ffc", 0x002a6ffcL),
		new Label("adc_monitor_scaled_score_store_2a7006", 0x002a7006L),
		new Label("adc_monitor_repost_e2_if_not_done_2a7048", 0x002a7048L),
		new Label("adc_monitor_ring_full_post_state30_2a7222", 0x002a7222L),
		new Label("adc_monitor_source_present_wrapper_2a6578", 0x002a6578L),
		new Label("adc_monitor_source_absent_wrapper_2a6596", 0x002a6596L),
		new Label("adc_monitor_current_source_wrapper_2a65ba", 0x002a65baL),
		new Label("adc_monitor_selected_source_wrapper_2a65d2", 0x002a65d2L),
		new Label("startup_event14_source7_absent_producer_2abdc0", 0x002abdc0L),
		new Label("startup_event14_source7_present_producer_2abde4", 0x002abde4L),
		new Label("adc_monitor_source7_present_call_2abe0e", 0x002abe0eL),
		new Label("adc_monitor_source_write_index_store_2a71b4", 0x002a71b4L),
		new Label("adc_monitor_source_wrap_alloc_2a71f4", 0x002a71f4L),
		new Label("adc_monitor_init_post_e2_2a72c4", 0x002a72c4L),
		new Label("contact_startup_mode7_reset_status_record_234704", 0x00234704L),
		new Label("contact_startup_mode7_check_service_status_234722", 0x00234722L),
		new Label("contact_startup_mode7_mark_status_missing_234730", 0x00234730L),
		new Label("contact_startup_mode7_request_6200_23478e", 0x0023478eL),
		new Label("contact_startup_mode7_optional_flag_234796", 0x00234796L),
		new Label("contact_startup_mode7_check_service_buffer_2347a4", 0x002347a4L),
		new Label("contact_startup_mode7_status65_ready_2347c0", 0x002347c0L),
		new Label("contact_startup_mode7_status65_marks_ready_2347d0", 0x002347d0L),
		new Label("contact_startup_mode7_followup_request_620f_234852", 0x00234852L),
		new Label("startup_service_status_byte_read_2a9368", 0x002a9368L),
		new Label("startup_service_status_byte_write_2a936e", 0x002a936eL),
		new Label("startup_service_table_copy_test8_call_28e06a", 0x0028e06aL),
		new Label("startup_service_table_copy_test9_call_28e090", 0x0028e090L),
		new Label("startup_service_table_copy_loop_28e0aa", 0x0028e0aaL),
		new Label("startup_service_buffer_update_dispatch_290d00", 0x00290d00L),
		new Label("startup_service_buffer_field30_update_290e42", 0x00290e42L),
		new Label("startup_service_buffer_field32_update_290e22", 0x00290e22L),
		new Label("startup_service_buffer_dirty_flag_291032", 0x00291032L),
		new Label("startup_mode5_event1_enters_mode8_271456", 0x00271456L),
		new Label("startup_mode5_event6_terminal_reset_2714d6", 0x002714d6L),
		new Label("startup_mode8_event2_or_74_poweroff_2714a4", 0x002714a4L),
		new Label("startup_mode8_poweroff_call_2714c8", 0x002714c8L),
		new Label("startup_terminal_reason0c_call_2714fc", 0x002714fcL),
		new Label("startup_predicate_loop_body_2a92fc", 0x002a92fcL),
		new Label("startup_task14_ready_check_result_2a931e", 0x002a931eL),
		new Label("startup_predicates_complete_2a934c", 0x002a934cL),
		new Label("startup_ready_gate_call_2a9358", 0x002a9358L),
		new Label("startup_ready_gate_result_check_2a935c", 0x002a935cL),
		new Label("startup_ready_gate_true_timer_update_2a9360", 0x002a9360L),
		new Label("startup_ready_gate_loop_continue_2a9364", 0x002a9364L),
		new Label("startup_ready_gate_failed_repoll_2a92f6", 0x002a92f6L),
		new Label("startup_ready_gate_bit_check_2aaaa0", 0x002aaaa0L),
		new Label("startup_ready_gate_timer_elapsed_check_2aaacc", 0x002aaaccL),
		new Label("startup_ready_gate_delay_remaining_check_2aaae6", 0x002aaae6L),
		new Label("startup_ready_gate_false_return_2aaaf2", 0x002aaaf2L),
		new Label("startup_ready_gate_true_return_2aaaf6", 0x002aaaf6L),
		new Label("startup_ready_gate_post_update_2aab5c", 0x002aab5cL),
		new Label("sched_post_task_message_queue_write_26a2a2", 0x0026a2a2L),
		new Label("sched_post_task_message_state_check_26a2fc", 0x0026a2fcL),
		new Label("sched_post_task_message_wake_waiting_task_26a30e", 0x0026a30eL),
		new Label("sched_post_task_message_state_wake_26a31a", 0x0026a31aL),
		new Label("startup_post74_event03_terminal_branch_2713fc", 0x002713fcL),
		new Label("startup_post74_reason2_terminal_handoff_27141e", 0x0027141eL),
		new Label("startup_terminal_common_dispatch_2714fe", 0x002714feL),
		new Label("contact_service_terminal_reason_store_2b4e08", 0x002b4e08L),
		new Label("contact_service_terminal_mad2_reset_or_2b4e12", 0x002b4e12L),
		new Label("contact_service_display_request_7105_25edea", 0x0025edeaL),
		new Label("contact_service_display_done_posts_event32_2abd88", 0x002abd88L),
		new Label("task5_status_descriptor_lookup_2aed5c", 0x002aed5cL),
		new Label("task5_status_descriptor_match_2aeda0", 0x002aeda0L),
		new Label("task5_status_descriptor_children_2aef44", 0x002aef44L),
		new Label("task5_status_side_effect_commit_2aef7e", 0x002aef7eL),
		new Label("task5_status_descriptor_eval_2aeec6", 0x002aeec6L),
		new Label("task5_status_recv_message_2af57c", 0x002af57cL),
		new Label("task5_generated_status_next_2af5d2", 0x002af5d2L),
		new Label("task5_generated_payload_next_2af5fc", 0x002af5fcL),
		new Label("task5_status_dispatch_loop_2af630", 0x002af630L),
		new Label("task5_sequence_append_payload_pair_2af744", 0x002af744L),
		new Label("task5_sequence_append_status_2af77c", 0x002af77cL),
		new Label("task5_sequence_expand_status_with_payload_2af798", 0x002af798L),
		new Label("task5_status_handler_a_2aefba", 0x002aefbaL),
		new Label("task5_status_handler_b_2ac3f2", 0x002ac3f2L),
		new Label("task5_status_handler_c_2638e4", 0x002638e4L),
		new Label("task5_status_handler_d_24bd30", 0x0024bd30L),
		new Label("task5_status_handler_e_28676c", 0x0028676cL),
		new Label("task5_status_handler_f_24cb0c", 0x0024cb0cL),
		new Label("task5_status_handler_g_253e20", 0x00253e20L),
		new Label("task5_status_display_handler_28bddc", 0x0028bddcL),
		new Label("display_state_store_and_apply_2ac3e0", 0x002ac3e0L),
		new Label("display_status_0130_init_case_28c958", 0x0028c958L),
		new Label("display_profile_selector_29ad74", 0x0029ad74L),
		new Label("display_profile_mirror_apply_29a72c", 0x0029a72cL),
		new Label("display_capability_record_init_29a768", 0x0029a768L),
		new Label("display_capability_record_sanitize_2b5eaa", 0x002b5eaaL),
		new Label("display_type2_state_seed_2b1e80", 0x002b1e80L),
		new Label("display_status_05e2_delay_counter_case_28c464", 0x0028c464L),
		new Label("display_status_0199_case_28c8d0", 0x0028c8d0L),
		new Label("display_status_0199_ready_emit_4197_28c8ea", 0x0028c8eaL),
		new Label("display_status_0199_fallback_emit_4197_28c8f4", 0x0028c8f4L),
		new Label("display_status_emit_05e2_case_28c9a6", 0x0028c9a6L),
		new Label("display_status_emit_common_28ca06", 0x0028ca06L),
		new Label("eeprom_write_profile_byte_loop_2af946", 0x002af946L),
		// --- Service-ready / bit6 / DSP-handshake key points (this work) ---
		new Label("startup_service_ready_store_1_29109e", 0x0029109eL),
		new Label("startup_service_ready_gate_dsp_counter_291096", 0x00291096L),
		new Label("mad2_irq_bit4_calls_service_ready_setter_2af416", 0x002af416L),
		new Label("startup_resume_gate_service_ready_2a9132", 0x002a9132L),
		new Label("startup_batch2_phase_gate_2a9192", 0x002a9192L),
		new Label("readiness_loop_head_2a92fc", 0x002a92fcL),
		new Label("readiness_loop_success_2a934c", 0x002a934cL),
		// EEPROM config-block checksum (sum16) compare gating bit6 (routine = nv_pair_read_234588)
		new Label("eeprom_config_checksum_compare_234810", 0x00234810L),
		// D9 watchdog: ack 0x11fedb resets counter 0x11fed6; 0x0f -> CONTACT SERVICE
		new Label("contact_watchdog_poll_ack_counter_237b2e", 0x00237b2eL),
	};

	private static final Target[] TARGETS = new Target[] {
		new Target("sched_post_event_delay_2697aa", 0x002697aaL),
		new Target("sched_post_event2_2698e4", 0x002698e4L),
		new Target("sched_delay_queue_insert_2699be", 0x002699beL),
		new Target("sched_delay_queue_service_269acc", 0x00269accL),
		new Target("sched_assert_unlinked_or_reset6_2aca44", 0x002aca44L),
		new Target("sched_timer_remaining_update_2aaa6a", 0x002aaa6aL),
		new Target("sched_task_start_if_registered_269c6e", 0x00269c6eL),
		new Target("sched_recv_current_task_message_26a458", 0x0026a458L),
		new Target("sched_context_entry_25fa70", 0x0025fa70L),
		new Target("sched_context_entry_25fada", 0x0025fadaL),
		new Target("sched_context_entry_25fb52", 0x0025fb52L),
		new Target("sched_context_entry_25fbc6", 0x0025fbc6L),
		new Target("sched_context_entry_260018", 0x00260018L),
		new Target("sched_context_entry_260031", 0x00260031L),
		new Target("sched_context_entry_260139", 0x00260139L),
		new Target("sched_post_task_message_26a204", 0x0026a204L),
		new Target("sched_context_post_message_26a354", 0x0026a354L),
			new Target("sched_context_register_26a776", 0x0026a776L),
			new Target("sched_context_setup_26a931", 0x0026a931L),
			new Target("sched_startup_recv_event_wrapper_26ff14", 0x0026ff14L),
			new Target("startup_event_wrapper_read_26ff4c", 0x0026ff4cL),
			new Target("startup_branch_2700c8", 0x002700c8L),
		new Target("startup_mode_dispatch_setter_270180", 0x00270180L),
		new Target("startup_branch_2702f8", 0x002702f8L),
		new Target("startup_branch_2706fc", 0x002706fcL),
		new Target("startup_branch_270804", 0x00270804L),
		new Target("startup_branch_270a88", 0x00270a88L),
		new Target("startup_mode1_wait_startup_events_270d00", 0x00270d00L),
		new Target("startup_mode13_wait_startup_events_270e1c", 0x00270e1cL),
		new Target("startup_mode7_wait_battery_ready_event_270f4c", 0x00270f4cL),
		new Target("startup_charger_present_event_wait_2711f6", 0x002711f6L),
		new Target("startup_charger_event14_followup_wait_27120e", 0x0027120eL),
		new Target("startup_charger_absent_event_wait_27124e", 0x0027124eL),
		new Target("startup_post_charger_sequence_continue_271266", 0x00271266L),
		new Target("startup_post_charger_completion_accumulate_2712cc", 0x002712ccL),
		new Target("startup_post_charger_completion_flags_check_271326", 0x00271326L),
		new Target("startup_mode_fallback_27150a", 0x0027150aL),
		new Target("startup_loop_270c78", 0x00270c78L),
		new Target("startup_mode5_event_wait_271446", 0x00271446L),
		new Target("startup_mode8_wait_or_poweroff_branch_27148c", 0x0027148cL),
		new Target("startup_terminal_reason0c_2714fc", 0x002714fcL),
		new Target("ccont_startup_step_2af058", 0x002af058L),
		new Target("ccont_startup_step_2af12e", 0x002af12eL),
		new Target("ccont_state_2af0ae", 0x002af0aeL),
		new Target("init_2a107a", 0x002a107aL),
		new Target("init_2b1c32", 0x002b1c32L),
		new Target("init_2b4712", 0x002b4712L),
		new Target("startup_branch_or_selftest_2a0ee6", 0x002a0ee6L),
		new Target("startup_event14_latch_and_schedule_2a0fae", 0x002a0faeL),
		new Target("startup_event14_cancel_2a0fa4", 0x002a0fa4L),
		new Target("startup_event14_service_2a0fc4", 0x002a0fc4L),
		new Target("init_2af130", 0x002af130L),
		new Target("service_channel_request_then_post_event37_2abea0", 0x002abea0L),
		new Target("service_channel_startup_7204_7205_7201_init_2abeb2", 0x002abeb2L),
		new Target("init_2a85ae", 0x002a85aeL),
		new Target("init_29b700", 0x0029b700L),
		new Target("startup_nv_calibration_read_29bbd0", 0x0029bbd0L),
		new Target("ccont_battery_init_post_event15_2b09f2", 0x002b09f2L),
		new Target("ccont_irq_status_dispatch_2b08c6", 0x002b08c6L),
		new Target("ccont_irq_charger_event16_payload6_2b0958", 0x002b0958L),
		new Target("init_2a26e6", 0x002a26e6L),
		new Target("ccont_init_post_startup_event17_2af086", 0x002af086L),
		new Target("battery_post_startup_ready_check_2a6942", 0x002a6942L),
		new Target("battery_state_get_27d654", 0x0027d654L),
		new Target("battery_classifier_27cbec", 0x0027cbecL),
		new Target("battery_classifier_input_update_27cc74", 0x0027cc74L),
		new Target("battery_classifier_tables_init_27d56c", 0x0027d56cL),
		new Target("battery_source7_poweroff_guard_27d5fc", 0x0027d5fcL),
		new Target("battery_state_init_27dd30", 0x0027dd30L),
		new Target("battery_state_force_27def4", 0x0027def4L),
		new Target("battery_adc_sample_counter_update_27d51c", 0x0027d51cL),
		new Target("battery_state_from_adc_update_27daf6", 0x0027daf6L),
		new Target("battery_adc_history_update_27dbc2", 0x0027dbc2L),
		new Target("battery_status_flags_update_27dcf8", 0x0027dcf8L),
		new Target("battery_adc_initial_profile_27dde8", 0x0027dde8L),
		new Target("battery_vbat_moving_average_27d500", 0x0027d500L),
		new Target("battery_vbat_sample_collect_27d2ec", 0x0027d2ecL),
		new Target("adc_monitor_source_read_2b1bb2", 0x002b1bb2L),
		new Target("battery_vbat_periodic_worker_21e00f", 0x0021e00fL),
		new Target("battery_vbat_periodic_status_writeback_21e104", 0x0021e104L),
		new Target("battery_state_probe_2a68f2", 0x002a68f2L),
		new Target("adc_monitor_source_present_wrapper_2a6578", 0x002a6578L),
		new Target("adc_monitor_source_absent_wrapper_2a6596", 0x002a6596L),
		new Target("adc_monitor_current_source_wrapper_2a65ba", 0x002a65baL),
		new Target("adc_monitor_selected_source_wrapper_2a65d2", 0x002a65d2L),
		new Target("adc_monitor_walk_sources_2a6f1c", 0x002a6f1cL),
		new Target("adc_monitor_source_install_2a7180", 0x002a7180L),
		new Target("adc_monitor_init_2a7230", 0x002a7230L),
		new Target("adc_monitor_init_alt_2a72d8", 0x002a72d8L),
		new Target("startup_event14_source7_absent_producer_2abdc0", 0x002abdc0L),
		new Target("startup_event14_source7_present_producer_2abde4", 0x002abde4L),
		new Target("ccont_adc_start_conversion_2b532a", 0x002b532aL),
		new Target("ccont_adc_read_lsb_2b533c", 0x002b533cL),
		new Target("ccont_adc_read_msb_2b5358", 0x002b5358L),
		new Target("ccont_adc_read_sequence_2b3beb", 0x002b3bebL),
		new Target("keypad_matrix_scan_2b2f90", 0x002b2f90L),
		new Target("startup_post74_boot_decision_271364", 0x00271364L),
		new Target("startup_post74_wait_event_loop_27138e", 0x0027138eL),
		new Target("startup_post74_event_store_271392", 0x00271392L),
		new Target("startup_post74_accept_event74_compare_271396", 0x00271396L),
		new Target("startup_post74_battery_ready_compare_27139e", 0x0027139eL),
		new Target("startup_post74_event74_producer_call_27136c", 0x0027136cL),
		new Target("startup_mode8_event74_producer_call_271488", 0x00271488L),
		new Target("startup_event74_post_guarded_2a8340", 0x002a8340L),
		new Target("startup_event74_post_unconditional_2a832e", 0x002a832eL),
		new Target("charger_present_check_2b084c", 0x002b084cL),
		new Target("ccont_irq_status_dispatch_2b08c6", 0x002b08c6L),
		new Target("event_source_29697e", 0x0029697eL),
		new Target("event_source_21c4b6", 0x0021c4b6L),
		new Target("event_source_283db6", 0x00283db6L),
		new Target("service_record_alloc_template_282476", 0x00282476L),
		new Target("service_record_submit_2824a2", 0x002824a2L),
		new Target("service_record_state_machine_2824e4", 0x002824e4L),
		new Target("service_record_encode_hex_282a98", 0x00282a98L),
		new Target("service_record_build_contact_pair_282afc", 0x00282afcL),
		new Target("service_record_alloc_header8_282d10", 0x00282d10L),
		new Target("service_event_b1_handler_283b4e", 0x00283b4eL),
		new Target("service_event_counter_reset_283a38", 0x00283a38L),
		new Target("service_event_state_reset_283b20", 0x00283b20L),
		new Target("service_event_8e_handler_2837bc", 0x002837bcL),
		new Target("service_event_125_handler_283c7e", 0x00283c7eL),
		new Target("service_event_126_handler_283c22", 0x00283c22L),
		new Target("service_event_127_handler_283c08", 0x00283c08L),
		new Target("service_event_128_handler_283bca", 0x00283bcaL),
		new Target("service_event_129_handler_283268", 0x00283268L),
		new Target("service_event_generic_handler_283316", 0x00283316L),
		new Target("service_event_ack_or_state_2834ea", 0x002834eaL),
		new Target("service_event_record_process_283530", 0x00283530L),
		new Target("service_event_record_complete_2835cc", 0x002835ccL),
		new Target("service_event_record_lookup_283654", 0x00283654L),
		new Target("service_event_record_special_2836f8", 0x002836f8L),
		new Target("service_event_record_d7_case_283758", 0x00283758L),
		new Target("service_event_schedule_283a88", 0x00283a88L),
		new Target("service_event_schedule_or_fault_283ae6", 0x00283ae6L),
		new Target("contact_service_d9_e2_command_dispatch_237994", 0x00237994L),
		new Target("contact_service_unknown_response_237960", 0x00237960L),
		new Target("contact_service_response_dispatch_237400", 0x00237400L),
		new Target("contact_service_mode7_loop_237bb4", 0x00237bb4L),
		new Target("contact_service_mode7_non40_handler_237c46", 0x00237c46L),
		new Target("contact_response_small_status_forwarder_236c70", 0x00236c70L),
		new Target("contact_response_startup_status_bits_236bac", 0x00236bacL),
		new Target("contact_response_code_66_handler_236918", 0x00236918L),
		new Target("contact_response_code_68_handler_2368b8", 0x002368b8L),
		new Target("contact_response_code_6a_6b_handler_23682c", 0x0023682cL),
		new Target("contact_response_code_6e_handler_2367b8", 0x002367b8L),
		new Target("contact_response_code_6f_handler_23675a", 0x0023675aL),
		new Target("contact_response_code_70_71_handler_23670c", 0x0023670cL),
		new Target("contact_response_70_channel_map_apply_2366c8", 0x002366c8L),
		new Target("contact_response_indexed_nv_event_73_75_236560", 0x00236560L),
		new Target("contact_response_code_88_handler_235a2c", 0x00235a2cL),
		new Target("contact_response_code_89_handler_235970", 0x00235970L),
		new Target("contact_response_code_8b_8c_handler_2358ec", 0x002358ecL),
		new Target("contact_response_text_config_c9_234f38", 0x00234f38L),
		new Target("contact_response_code_ac_handler_234fc8", 0x00234fc8L),
		new Target("contact_response_code_ca_cb_handler_234e74", 0x00234e74L),
		new Target("contact_response_code_cf_handler_234e3a", 0x00234e3aL),
		new Target("contact_response_code_d0_handler_234e06", 0x00234e06L),
		new Target("contact_response_code_d1_d2_handler_234d94", 0x00234d94L),
		new Target("contact_response_code_d3_handler_234d40", 0x00234d40L),
		new Target("contact_response_code_d4_d5_handler_234c18", 0x00234c18L),
		new Target("contact_response_ui_string_action_7c_237354", 0x00237354L),
		new Target("contact_response_code_ad_handler_2370f8", 0x002370f8L),
		new Target("contact_response_timed_action_b0_237034", 0x00237034L),
		new Target("contact_response_code_72_handler_29fcf4", 0x0029fcf4L),
		new Target("contact_response_code_ae_handler_29f8c4", 0x0029f8c4L),
		new Target("contact_response_code_af_handler_29ff2c", 0x0029ff2cL),
		new Target("contact_service_task_loop_237bb4", 0x00237bb4L),
		new Target("contact_service_event_237c96", 0x00237c96L),
		new Target("contact_service_state_alloc_234634", 0x00234634L),
		new Target("contact_service_state_release_234684", 0x00234684L),
		new Target("contact_service_state_init_2346b2", 0x002346b2L),
		new Target("contact_service_nv_pair_read_234588", 0x00234588L),
		new Target("startup_service_buffer_first_byte_get_2a8fec", 0x002a8fecL),
		new Target("startup_power_service_init_gate_2a8ff2", 0x002a8ff2L),
		new Target("startup_status_seed_service_buffer_2a929e", 0x002a929eL),
		new Target("startup_service_buffer_seed_2909e4", 0x002909e4L),
		new Target("startup_service_table_init_290b54", 0x00290b54L),
		new Target("contact_service_optional_flag_295ea2", 0x00295ea2L),
		new Target("service_channel_request_if_enabled_2b13a2", 0x002b13a2L),
		new Target("service_channel_request_empty_2b13d4", 0x002b13d4L),
		new Target("service_channel_lookup_2b12b4", 0x002b12b4L),
		new Target("service_channel_build_request_2b12dc", 0x002b12dcL),
		new Target("service_channel_queue_request_2b1388", 0x002b1388L),
		new Target("service_channel_reset_2b13c0", 0x002b13c0L),
		new Target("service_channel_lookup_mask_2b13e2", 0x002b13e2L),
		new Target("service_channel_config_2b140a", 0x002b140aL),
		new Target("task1_service_event_loop_21bfa6", 0x0021bfa6L),
		new Target("task1_service_timer_seed_21c4a0", 0x0021c4a0L),
		new Target("task1_service_timer_recompute_21c4f2", 0x0021c4f2L),
		new Target("task1_service_event_code_decode_21c646", 0x0021c646L),
		new Target("task1_service_event_dispatch_loop_2209fa", 0x002209faL),
		new Target("startup_status4_publish_2a28fc", 0x002a28fcL),
		new Target("startup_status_message_post_2af6ea", 0x002af6eaL),
		new Target("task5_status_descriptor_lookup_2aed5c", 0x002aed5cL),
		new Target("task5_status_descriptor_match_2aeda0", 0x002aeda0L),
		new Target("task5_status_descriptor_children_2aef44", 0x002aef44L),
		new Target("task5_status_side_effect_commit_2aef7e", 0x002aef7eL),
		new Target("task5_status_descriptor_eval_2aeec6", 0x002aeec6L),
		new Target("task5_status_recv_message_2af57c", 0x002af57cL),
		new Target("task5_generated_status_next_2af5d2", 0x002af5d2L),
		new Target("task5_generated_payload_next_2af5fc", 0x002af5fcL),
		new Target("task5_status_dispatch_loop_2af630", 0x002af630L),
		new Target("task5_sequence_append_payload_pair_2af744", 0x002af744L),
		new Target("task5_sequence_append_status_2af77c", 0x002af77cL),
		new Target("task5_sequence_expand_status_with_payload_2af798", 0x002af798L),
		new Target("task5_status_handler_a_2aefba", 0x002aefbaL),
		new Target("task5_status_handler_b_2ac3f2", 0x002ac3f2L),
		new Target("task5_status_handler_c_2638e4", 0x002638e4L),
		new Target("task5_status_handler_d_24bd30", 0x0024bd30L),
		new Target("task5_status_handler_e_28676c", 0x0028676cL),
		new Target("task5_status_handler_f_24cb0c", 0x0024cb0cL),
		new Target("task5_status_handler_g_253e20", 0x00253e20L),
		new Target("task5_status_display_handler_28bddc", 0x0028bddcL),
		new Target("display_state_store_and_apply_2ac3e0", 0x002ac3e0L),
		new Target("display_status_0130_init_case_28c958", 0x0028c958L),
		new Target("display_profile_selector_29ad74", 0x0029ad74L),
		new Target("display_profile_mirror_apply_29a72c", 0x0029a72cL),
		new Target("display_capability_record_init_29a768", 0x0029a768L),
		new Target("display_capability_record_sanitize_2b5eaa", 0x002b5eaaL),
		new Target("display_type2_state_seed_2b1e80", 0x002b1e80L),
		new Target("display_status_05e2_delay_counter_case_28c464", 0x0028c464L),
		new Target("display_status_0199_case_28c8d0", 0x0028c8d0L),
		new Target("display_status_0199_ready_emit_4197_28c8ea", 0x0028c8eaL),
		new Target("display_status_0199_fallback_emit_4197_28c8f4", 0x0028c8f4L),
		new Target("display_status_emit_05e2_case_28c9a6", 0x0028c9a6L),
		new Target("display_status_emit_common_28ca06", 0x0028ca06L),
		new Target("service_frame_send_2a9ea6", 0x002a9ea6L),
		new Target("service_transport_queue_2b0482", 0x002b0482L),
		new Target("service_transport_complete_2b052e", 0x002b052eL),
		new Target("service_transport_clear_2b05a0", 0x002b05a0L),
		new Target("service_transport_abort_2b05b2", 0x002b05b2L),
		new Target("service_transport_schedule_2b05c4", 0x002b05c4L),
		new Target("service_context_event_bit_clear_2b03f2", 0x002b03f2L),
		new Target("service_context_event_bit_set_2b0444", 0x002b0444L),
		new Target("contact_service_ui_gate_2b4696", 0x002b4696L),
		new Target("contact_service_display_enable_2b4dc0", 0x002b4dc0L),
		new Target("contact_service_reset_or_display_status_2b4dda", 0x002b4ddaL),
		new Target("ccont_poweroff_write_2b4e4a", 0x002b4e4aL),
		new Target("contact_service_draw_begin_260144", 0x00260144L),
		new Target("contact_service_display_request_7105_25edc0", 0x0025edc0L),
		new Target("contact_service_draw_text_25edf2", 0x0025edf2L),
		new Target("contact_service_draw_end_2abd7c", 0x002abd7cL),
		new Target("startup_mode1_post_event30_or_32_2abd90", 0x002abd90L),
		new Target("contact_service_timeout_signal_236dc4", 0x00236dc4L),
		new Target("contact_response65_store_status_detail_2b21b6", 0x002b21b6L),
		new Target("contact_response65_error_flags_handler_236970", 0x00236970L),
		new Target("contact_service_startup_fault_status_builder_234720", 0x00234720L),
		new Target("contact_startup_fault_status_builder_234720", 0x00234720L),
		new Target("contact_startup_fault_seed_bits_234750", 0x00234750L),
		new Target("contact_startup_resource_fault_scan_234896", 0x00234896L),
		new Target("startup_status_bit1_handler_2a1172", 0x002a1172L),
		new Target("startup_status_dependency_check_2a674c", 0x002a674cL),
		new Target("startup_status_schedule_event_2a28dc", 0x002a28dcL),
		new Target("startup_e2_event_post_source_2a72b8", 0x002a72b8L),
		new Target("startup_e2_event_post_call_2a72c8", 0x002a72c8L),
		new Target("startup_status_no_bits_handler_29bd14", 0x0029bd14L),
			new Target("contact_status_to_startup_mode_gate_29bc70", 0x0029bc70L),
		new Target("startup_flash_table_check_2961ec", 0x002961ecL),
		new Target("startup_resource_table_check_2961ac", 0x002961acL),
		new Target("resource_table_first_entry_2b0a74", 0x002b0a74L),
		new Target("resource_table_validate_2b0ba6", 0x002b0ba6L),
		new Target("contact_service_checksum_2a41d0", 0x002a41d0L),
		new Target("startup_service_byte0_get_2a9398", 0x002a9398L),
		new Target("startup_service_byte0_is_0a_02_03_2a93a4", 0x002a93a4L),
		new Target("startup_service_status_byte_read_2a9368", 0x002a9368L),
		new Target("startup_timer_schedule_2aa8c6", 0x002aa8c6L),
		new Target("startup_ready_gate_init_2aa9d6", 0x002aa9d6L),
		new Target("startup_ready_gate_2aaa9a", 0x002aaa9aL),
		new Target("startup_post_ready_timer_update_2aab5c", 0x002aab5cL),
		new Target("startup_service_test33_flag_update_2a14a0", 0x002a14a0L),
		new Target("startup_service_test12_flag_update_2a14b6", 0x002a14b6L),
		new Target("startup_mode_set_2a936e", 0x002a936eL),
		new Target("startup_mode_check_2a937c", 0x002a937cL),
		new Target("startup_mode_event1b_2a93c0", 0x002a93c0L),
		new Target("ui_event1b_short_delay_2974f8", 0x002974f8L),
		new Target("startup_status_get_26f930", 0x0026f930L),
		new Target("ccont_startup_status_get_2af066", 0x002af066L),
		new Target("startup_service_mode8_result_update_291028", 0x00291028L),
		new Target("startup_service_mode8_result_commit_291034", 0x00291034L),
		new Target("startup_service_buffer_update_290cf4", 0x00290cf4L),
		new Target("startup_service_buffer_field30_update_290e42", 0x00290e42L),
		new Target("startup_service_buffer_field32_update_290e22", 0x00290e22L),
		new Target("event_source_2a58a4", 0x002a58a4L),
		new Target("service_lower_frame_process_2a5818", 0x002a5818L),
		new Target("service_lower_frame_filter_2ad4a8", 0x002ad4a8L),
		new Target("service_lower_loopback_2a5468", 0x002a5468L),
		new Target("service_lower_event_bridge_283e1c", 0x00283e1cL),
		new Target("service_lower_frame_data_2a5508", 0x002a5508L),
		new Target("service_lower_frame_d0_2a57be", 0x002a57beL),
		new Target("service_lower_frame_d1_2a55bc", 0x002a55bcL),
		new Target("service_lower_tx_busy_set_2aaca8", 0x002aaca8L),
		new Target("service_lower_mbus_rx_state_machine_2aae76", 0x002aae76L),
		new Target("service_lower_mbus_tx_step_2aae2a", 0x002aae2aL),
		new Target("service_lower_queue_drain_2ad254", 0x002ad254L),
		new Target("service_lower_queue_event_2ad2c2", 0x002ad2c2L),
		new Target("service_lower_queue_total_store_2ad3d6", 0x002ad3d6L),
		new Target("service_lower_enqueue_external_frame_2ad3e4", 0x002ad3e4L),
		new Target("service_lower_build_status_frame_2ad422", 0x002ad422L),
		new Target("mad2_mbus_irq_handler_2b56cc", 0x002b56ccL),
		new Target("service_transport_d5_notify_wrapper_2b0474", 0x002b0474L),
		new Target("service_transport_build_d5_notify_2a594c", 0x002a594cL),
		new Target("service_context_ready_2b03d8", 0x002b03d8L),
		new Target("task14_startup_ready_28ff14", 0x0028ff14L),
		new Target("task14_helper_ready_26ec10", 0x0026ec10L),
		new Target("sim_or_radio_ready_2a6566", 0x002a6566L),
		new Target("selftest_not_pending_2a0ec4", 0x002a0ec4L),
		new Target("startup_always_ready_279282", 0x00279282L),
		new Target("service_lower_idle_check_2ad1c8", 0x002ad1c8L),
		new Target("service_event_queue_empty_283dce", 0x00283dceL),
		new Target("nv_record_descriptor_resolve_common_2abb3e", 0x002abb3eL),
		new Target("nv_record_descriptor_resolve_2abb2a", 0x002abb2aL),
		new Target("nv_record_read_to_buffer_alt_2abbae", 0x002abbaeL),
		new Target("nv_record_read_to_buffer_2abbf4", 0x002abbf4L),
		new Target("event_consumer_call_2af8e2", 0x002af8e2L),
		new Target("eeprom_nv_read_bytes_2af97a", 0x002af97aL),
		new Target("eeprom_write_profile_byte_loop_2af946", 0x002af946L),
		new Target("eeprom_i2c_read_open_2af858", 0x002af858L),
		new Target("eeprom_i2c_bus_lock_2aa8fa", 0x002aa8faL),
		new Target("eeprom_i2c_bus_unlock_2aa914", 0x002aa914L),
		new Target("eeprom_i2c_write_byte_2b0188", 0x002b0188L),
		new Target("eeprom_i2c_read_byte_2b024a", 0x002b024aL),
		new Target("eeprom_i2c_start_2b0318", 0x002b0318L),
		new Target("eeprom_i2c_stop_2b038a", 0x002b038aL),
		new Target("ccont_irq_callback_2af5aa", 0x002af5aaL),
		new Target("ccont_reg_read_2afb44", 0x002afb44L),
		new Target("ccont_reg_or_2afa74", 0x002afa74L),
		new Target("ccont_reg_write_2b5ae4", 0x002b5ae4L),
		new Target("ccont_reg_write_2b5b24", 0x002b5b24L),
		new Target("display_manager_297fc4", 0x00297fc4L),
		new Target("display_recv_298008", 0x00298008L),
		new Target("display_select_297c20", 0x00297c20L),
		new Target("display_type2_filter_297ed8", 0x00297ed8L),
		new Target("display_apply_297f60", 0x00297f60L),
		new Target("display_clear_297a8c", 0x00297a8cL),
		new Target("display_probe_297ba2", 0x00297ba2L),
		new Target("display_draw_2a1ea6", 0x002a1ea6L),
		new Target("display_hw_clear_2a1c88", 0x002a1c88L),
		new Target("display_idle_2a255c", 0x002a255cL),
		new Target("display_state_source_28c940", 0x0028c940L),
		new Target("display_state_emit_28c9c0", 0x0028c9c0L),
		new Target("display_state_main_28bddc", 0x0028bddcL),
		new Target("display_type2_post_2b1f24", 0x002b1f24L),
		new Target("lcd_init_2b1d14", 0x002b1d14L),
		new Target("lcd_write_2b1c96", 0x002b1c96L),
		new Target("divmod_2b5388", 0x002b5388L),
		new Target("ccont_adc_read_2b52cc", 0x002b52ccL),
		// --- Service-startup / DSP-handshake chain (CONTACT SERVICE root cause) ---
		// Sets startup service-ready byte 0x110c2c = 1 iff the DSP-shared pending
		// counter at 0x100e4 == 0. Reached only via MAD2 IRQ line 4 (DSP completion).
		new Target("startup_service_ready_setter_291068", 0x00291068L),
		// MAD2 IRQ handler: dispatches one routine per active IRQ_STATUS bit; bit 4
		// (the DSP service-completion interrupt) calls the service_ready setter.
		new Target("mad2_irq_dispatch_2af3ca", 0x002af3caL),
		// Startup state machine: resumes the task batches and runs the mode-0x000d
		// readiness loop (0x2a92fc) gating the transition out of mode 0x000d.
		new Target("startup_resume_and_readiness_2a922e", 0x002a922eL),
		// Extended-task resume sequence: batch gates on service_ready (0x110c2c),
		// bit6 (0x11fed1) and the startup phase byte (0x112449). Task 0x14 in batch 2.
		new Target("startup_resume_task_batches_2a9120", 0x002a9120L),
		// Service-startup command dispatcher: 52-entry jump table. cmd 0x12 = D0
		// response received; cmd 0x2f = completion. Status word at 0x110c2e.
		new Target("service_startup_dispatch_290cf8", 0x00290cf8L),
		// Task 8 (lower-service) inbound MBUS frame id dispatcher (ids 0xc0..0x1bf).
		new Target("service_lower_task8_frame_dispatch_283d6e", 0x00283d6eL),
		// --- PM / service-node read layer + contact-service response path ---
		// Guarded PM/service read: validates (0x2b12b4) then posts a remote read-request
		// for a logical address (e.g. 0x5f00) to a service node (e.g. 0x18). r0=count,
		// r1=address, r2=dest buffer.
		new Target("pm_guarded_read_2b13a2", 0x002b13a2L),
		// Builds + posts the async PM read-request message (address at [msg+8/9],
		// dest node at [msg+1]); the response returns as a message later.
		new Target("pm_read_post_request_2b12dc", 0x002b12dcL),
		// PM read validity: returns 0 (drop) unless service-channel enable flag 0x11fee4
		// != 0 AND the address is registered (ROM table 0x2e2f5c & RAM mask 0x11ff08).
		new Target("pm_read_validity_check_2b12b4", 0x002b12b4L),
		// Zeroes the service-channel enable flags (0x11fee4); the channels are thus never
		// opened on a blank unit, so all PM reads drop. Called from startup 0x2a9284.
		new Target("service_channel_reset_2b13c0", 0x002b13c0L),
		// Routes/posts a service request frame by destination node ([msg+1]).
		new Target("mbus_post_routed_by_node_2b0482", 0x002b0482L),
		// Contact-service result handler: sets substate from a result code; only r4==5
		// yields the healthy substate (5), else fault (2). 9 result-reporting callers.
		new Target("contact_service_result_handler_2355b6", 0x002355b6L),
		// Contact-service async response dispatcher (no static refs; computed-dispatch
		// on a received response message). r6 = response command; 0x05 => healthy
		// completion (jump table at 0x236e34, cmd 0x05 -> result 5 -> substate 5).
		new Target("contact_service_response_dispatch_236dc6", 0x00236dc6L),
	};

	private Function prepareThumbFunction(Target target, Register tmode, RegisterValue thumbMode) throws Exception {
		Address entry = toAddr(target.addr);
		clearListing(entry, entry.add(0x3ff));
		try {
			currentProgram.getProgramContext().setValue(tmode, entry, entry.add(0x3ff), BigInteger.ONE);
		} catch (Exception e) {
			printf("thumb context already constrained at %s: %s\n", entry, e.getMessage());
		}
		AddressSet set = new AddressSet(entry, entry);
		DisassembleCommand command = new DisassembleCommand(set, null, true);
		command.setInitialContext(thumbMode);
		command.applyTo(currentProgram, monitor);

		Function function = getFunctionAt(entry);
		if (function == null) {
			createFunction(entry, target.name);
			function = getFunctionAt(entry);
		}
		if (function == null) {
			printf("missing function at %s\n", entry);
			return null;
		}
		if (!function.getName().equals(target.name)) {
			try {
				function.setName(target.name, SourceType.USER_DEFINED);
			} catch (Exception e) {
				printf("could not rename %s to %s: %s\n", entry, target.name, e.getMessage());
			}
		}
		return function;
	}

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		File outDir = new File(args.length > 0 ? args[0] : "/tmp");
		outDir.mkdirs();

		DecompInterface decompiler = new DecompInterface();
		decompiler.openProgram(currentProgram);
		Register tmode = currentProgram.getProgramContext().getRegister("TMode");
		RegisterValue thumbMode = new RegisterValue(tmode, BigInteger.ONE);

		for (Target target : TARGETS)
			prepareThumbFunction(target, tmode, thumbMode);

		for (Label label : LABELS)
			createLabel(toAddr(label.addr), label.name, true);

		for (Target target : TARGETS) {
			Address entry = toAddr(target.addr);
			Function function = getFunctionAt(entry);
			if (function == null)
				continue;
			DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
			File outFile = new File(outDir, target.name + ".c");
			try (PrintWriter writer = new PrintWriter(outFile)) {
				writer.printf("// %s at %s%n%n", function.getName(), entry);
				if (results.decompileCompleted()) {
					writer.println(results.getDecompiledFunction().getC());
				} else {
					writer.printf("// decompile failed: %s%n", results.getErrorMessage());
				}
			}
			printf("exported %s\n", outFile.getAbsolutePath());
		}
		decompiler.dispose();
	}
}
