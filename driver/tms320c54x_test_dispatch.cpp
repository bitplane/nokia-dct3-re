// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

TIMER_CALLBACK_MEMBER(tms320c54x_test_state::check_results)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	if (check_repeated_writa_results()) return;
	if (check_branch_arithmetic_results()) return;
	if (check_circular_mar_results()) return;
	if (check_tc_return_results()) return;
	if (check_bitt_results()) return;
	if (check_bd_read_a_results()) return;
	if (check_mvdd_follow_results()) return;
	if (check_delay_bitt_results()) return;
	if (check_repeat_store_results()) return;
	if (check_rptz_results()) return;
	if (check_sat_results()) return;
	if (check_literal_guard_results()) return;
	if (check_mmr_long_edge_results()) return;
	if (check_shifted_store_dld_results()) return;
	if (check_idle_nmi_result_results()) return;
	if (check_shifted_high_store_results()) return;
	if (check_idle_wake_results()) return;
	if (check_rounded_multiply_results()) return;
	if (check_stopped_timer_reload_results()) return;
	if (check_atomic_program_results()) return;
	if (check_far_save_results()) return;
	if (check_bitf_offset_results()) return;
	if (check_bio_results()) return;
	if (check_bit_results()) return;
	if (check_banz_offset_results()) return;
	if (check_cmpr_results()) return;
	if (check_carry_branch_results()) return;
	if (check_cmpm_offset_results()) return;
	if (check_compact_load_results()) return;
	if (check_return_alt_results()) return;
	if (check_software_interrupt_results()) return;
	if (check_trn_store_results()) return;
	if (check_parallel_load_multiply_results()) return;
	if (check_compact_store_results()) return;
	if (check_acc_delayed_results()) return;
	if (check_delayed_call_results()) return;
	if (check_asm_store_results()) return;
	if (check_store_shift_results()) return;
	if (check_reverse_carry_reference_results()) return;
	if (check_intm_protection_results()) return;
	if (check_zero_bk_results()) return;
	if (check_saccd_results()) return;
	if (check_bsp_interrupt_results()) return;
	if (check_vector_prefetch_results()) return;
	if (check_braf_control_results()) return;
	if (check_native_address_gap_results()) return;
	if (check_native_math_gap_results()) return;
	if (check_native_memory_gap_results()) return;
	if (check_native_final_gap_results()) return;
	if (check_indirect_popm_results()) return;
	if (check_shifted_arithmetic_selector_results()) return;
	if (check_interrupt_entry_spacing_results()) return;
	if (check_rounding_boundary_results()) return;
	if (check_pipeline_frontend_results()) return;
	if (check_multiword_pipeline_results()) return;
	if (check_pipeline_store_results()) return;
	if (check_pipeline_rmw_results()) return;
	if (check_constant_dagen_results()) return;
	if (check_peripheral_mmr_timing_audit_results()) return;
	if (check_pipeline_immediate_store_results()) return;
	if (check_pipeline_far_return_results()) return;
	if (check_pipeline_far_irq_results()) return;
	if (check_pipeline_intm_results()) return;
	if (check_pipeline_push_sp_latency_results()) return;
	if (check_interrupt_pop_edge_results()) return;
	if (check_pipeline_mmr_pop_results()) return;
	if (check_pipeline_mu4_prologue_results()) return;
	if (check_pipeline_dst_results()) return;
	if (check_pipeline_logical_immediate_results()) return;
	if (check_absolute_memory_dp_results()) return;
	if (check_pipeline_conditional_branch_results()) return;
	if (check_pipeline_xc_irq_results()) return;
	if (check_pipeline_xc_tc_results()) return;
	if (check_pipeline_xc_accumulator_results()) return;
	if (check_pipeline_delayed_return_results()) return;
	if (check_pipeline_original_worker_roundtrip_results()) return;
	if (check_pipeline_original_context_call_results()) return;
	if (check_addm_expected_results()) return;
	if (check_pipeline_data_push_results()) return;
	if (check_pipeline_retd_data_order_results()) return;
	if (check_pipeline_data_pop_results()) return;
	if (check_pipeline_cmpr_results()) return;
	if (check_pipeline_memory_logic_results()) return;
	if (check_pipeline_mu4_full_worker_results()) return;
	if (check_pipeline_mu4_timer_context_results()) return;
	if (check_pipeline_absolute_load_results()) return;
	if (check_pipeline_far_call_results()) return;
	if (check_pipeline_idle_sp_tail_results()) return;
	if (check_repeat_pop_exclusion_results()) return;
	if (check_pipeline_sp_rmw_results()) return;
	if (check_pipeline_memory_repeat_read_a_results()) return;
	if (check_instruction_observer_results()) return;
	if (check_mvkd_sp_results()) return;
	if (check_pipeline_add_sp_context_results()) return;
	if (check_pipeline_peripheral_logic_results()) return;
	if (check_pipeline_direct_immediate_store_results()) return;
	if (check_ts_load_results()) return;
	if (check_extended_ar_store_results()) return;
	if (check_accumulator_logic_results()) return;
	if (check_data_copy_results()) return;
	if (check_accumulator_shift_results()) return;
	if (check_peripheral_long_store_results()) return;
	if (check_pipeline_mvmm_sp_results()) return;
	if (check_branch_repeat_results()) return;
	if (check_rptbd_t_load_results()) return;
	if (check_pipeline_repeat_nop_results()) return;
	if (check_pipeline_xmem_bit_results()) return;
	if (check_pipeline_port_read_results()) return;
	if (check_long_accumulator_store_results()) return;
	if (check_pipeline_long_move_results()) return;
	if (check_absolute_xorm_results()) return;
	if (check_signed_branch_taken_results()) return;
	if (check_bitfield_address_results()) return;
	if (check_staged_load_results()) return;
	if (check_pipeline_extended_banz_results()) return;
	if (check_immediate_arithmetic_results()) return;
	if (check_memory_repeat_results()) return;
	if (check_banzd_results()) return;
	if (check_stlm_t_results()) return;
	if (check_pipeline_swap_body_results()) return;
	if (check_push_pop_same_results()) return;
	if (check_shift_delay_edge_results()) return;
	if (check_asm_store_results_part2()) return;
	if (check_repeat_rearm_results()) return;
	if (check_saved_repeat_results()) return;
	if (check_repeat_context_results()) return;
	if (check_mvdk_mapped_edge_results()) return;
	if (check_clkmd_move_edge_results()) return;
	if (check_mvdk_mapped_results()) return;
	if (check_basic_repeat_edge_results()) return;
	if (check_rptbd_shift_store_results()) return;
	if (check_rptbd_layout_results()) return;
	if (check_negate_results()) return;
	if (check_long_mapped_original_results()) return;
	if (check_mcbsp_control_sequence_results()) return;
	if (check_preincrement_immediate_results()) return;
	if (check_stack_irq_order_results()) return;
	if (check_repeat_pop_exclusion_results_part2()) return;
	if (check_accumulator_cmpm_results()) return;
	if (check_staged_conditional_store_results()) return;
	if (check_accumulator_stm_results()) return;
	if (check_ldu_peripheral_results()) return;
	if (check_ldu_status_value_results()) return;
	if (check_ldu_imr_results()) return;
	if (check_mvmd_status_block_results()) return;
	if (check_shifted_peripheral_edge_results()) return;
	if (check_stm_status_edge_results()) return;
	if (check_rptbd_square_results()) return;
	if (check_delay_copy_results()) return;
	if (check_staged_overflow_results()) return;
	if (check_mapped_dual_results()) return;
	if (check_mvmd_accumulator_results()) return;
	if (check_write_a_edge_results()) return;
	if (check_repeat_mvdp_irq_results()) return;
	if (check_repeat_mvkd_results()) return;
	if (check_conditional_call_results()) return;
	if (check_near_accumulator_results()) return;
	if (check_extended_t_store_results()) return;
	if (check_circular_portr_results()) return;
	if (check_ifr_pair_results()) return;
	if (check_mvmd_ar_results()) return;
	if (check_poly_results()) return;
	if (check_delayed_far_call_results()) return;
	if (check_mvkd_delay_results()) return;
	if (check_absolute_logic_results()) return;
	if (check_popd_mapped_results()) return;
	if (check_accumulator_branch_results()) return;
	if (check_mvmd_word_register_results()) return;
	if (check_mvdd_peripheral_results()) return;
	if (check_source_alias_results()) return;
	if (check_source_domains()) return;
	if (check_mapped_immediate_mac_results()) return;
	if (check_block_branch_count_results()) return;
	if (check_bd_long_accumulator_results()) return;
	if (check_t_move_results()) return;
	if (check_block_xc_edge_results()) return;
	if (check_block_xc_irq_results()) return;
	if (check_move_repeat_owner_results()) return;
	if (check_move_brc_edge_results()) return;
	if (check_nibble_input_results()) return;
	if (check_software_intr_results()) return;
	if (check_block_branch_edge_results()) return;
	if (check_circular_load_results()) return;
	if (check_delay_dld_results()) return;
	if (check_asm_mapped_results()) return;
	if (check_t_store_edge_results()) return;
	if (check_ts_arithmetic_edge_results()) return;
	if (check_asm_edge_results()) return;
	if (check_delay_compare_edge_results()) return;
	if (check_delayed_condition_return_results()) return;
	if (check_poll_interrupt_results()) return;
	if (check_delay_compare_results()) return;
	if (check_asm_circular_store_results()) return;
	if (check_minmax_results()) return;
	if (check_indexed_mar_results()) return;
	if (check_addc_chain_results()) return;
	if (check_mapped_parallel_load_results()) return;
	if (check_mvdm_acc_results()) return;
	if (check_memory_copy_repeat_results()) return;
	if (check_block_near_call_edge_results()) return;
	if (check_repeated_mvdk_results()) return;
	if (check_portr_bypass_results()) return;
	if (check_sqdst_results()) return;
	if (check_signed_xc_results()) return;
	if (check_normalization_results()) return;
	if (check_xmem_load_results()) return;
	if (check_exponent_results()) return;
	if (check_repeat_push_results()) return;
	if (check_block_exit_results()) return;
	if (check_stlm_bsp_edge_results()) return;
	if (check_stlm_t_results_part2()) return;
	if (check_staged_bus_stm_results()) return;
	if (check_rom4_fr_decoder_results()) return;
	if (check_native_dual_sub_results()) return;
	if (check_xc_barrier_results()) return;
	if (check_atomic_xorm_results()) return;
	if (check_conditional_register_results()) return;
	if (check_rom4_codec_is_results()) return;
	if (check_roltc_results()) return;
	if (check_rom4_crc_input_results()) return;

	osd_printf_info("TMS320C54x test state: pc=%04x sp=%04x brc=%04x "
			"rpt=%04x,%04x,%04x block=%04x,%04x,%04x\n",
			u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_SP)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_BRC)),
			data.read_word(0x0400), data.read_word(0x0401), data.read_word(0x0402),
			data.read_word(0x0600), data.read_word(0x0601), data.read_word(0x0602));
	expect(data.read_word(0x0700) == 0xbeef, "CALL/RET continuation");
	expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
			"balanced system stack");
	expect(data.read_word(0x0400) == 0x1111 &&
			data.read_word(0x0401) == 0x2222 &&
			data.read_word(0x0402) == 0x3333, "RPT memory transfer");
	expect(data.read_word(0x0600) == 0x1111 &&
			data.read_word(0x0601) == 0x2222 &&
			data.read_word(0x0602) == 0x3333, "RPTB multiword CALL");
	expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0,
			"RPTB terminal count");
	expect(!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000),
			"RPTB terminal state clears ST1.BRAF");
	expect(data.read_word(0x0900) == 0xcccc &&
			data.read_word(0x0901) == 0xaaaa &&
			data.read_word(0x0902) == 0xbbbb,
			"circular addressing wrap order");
	expect(m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0802,
			"circular addressing final pointer");
	expect(data.read_word(0x0903) == 0x0000,
			"BITF and conditional return");
	expect(data.read_word(0x0904) == 0xbbbb,
			"absolute data-memory copy");
	expect(data.read_word(0x0905) == 0x0a00,
			"memory-mapped auxiliary-register read");
	expect(m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xaaaa,
			"memory-mapped auxiliary-register write");
	expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00aa00aa,
			"unsigned data operand load and accumulator XOR shift");
	expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 0xaa00,
			"immediate accumulator mask and rotate");
	expect(m_cpu->space(AS_PROGRAM).read_word(0x0907) == 0xaaaa,
			"data-to-program memory transfer");
	expect(data.read_word(0x0908) == 0xaaaa,
			"program-to-data memory transfer");
	for (unsigned i = 0; i != 26; ++i)
		expect(data.read_word(0x0882 + i) == 0x6000 + i,
				"MVDD dual-operand circular transfer");
	expect_opcode(0xe59c, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x089c &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x131a,
			"ROM4 MVDD receive-ring copy updates both operands after 26 checked words");

	// IDLE3 must retain its continuation PC, then an enabled source must
	// vector through PMST.IPTR and preserve the return address on stack.
	program.write_word(0x0048, 0x7680);
	program.write_word(0x0049, 0xcafe);
	program.write_word(0x004a, 0xf49b); // RETF
	program.write_word(0x0400, 0xf5e1);
	program.write_word(0x0401, 0xf5e1);
	m_fast_stack_reads = 0;
	data.install_read_tap(0x02ff, 0x02ff, "fast_return_stack",
		[this](offs_t, u16 &, u16) {
			if (m_phase == 3 && !machine().side_effects_disabled())
				++m_fast_stack_reads;
		});
	m_phase = 3;
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0400);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0000);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x0000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0x0004);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0a10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_check_timer->adjust(attotime::from_usec(100));
}

} // namespace c54x_test
