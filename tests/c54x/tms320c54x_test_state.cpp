// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::xf_w(int state)
{
	m_xf_pin = bool(state);
	++m_xf_publications;
}

int tms320c54x_test_state::bio_r()
{
	if (m_phase == 252670)
	{
		expect(!m_pipeline_register_writes && m_cpu->pipeline_cycles() == 6, "BIO branch samples its line exactly once at retained Read");
		++m_pipeline_register_writes;
	}
	return m_bio_level;
}

void tms320c54x_test_state::machine_start()
{
	save_item(NAME(m_bus_ready));
	save_item(NAME(m_stack_mmr));
	save_item(NAME(m_bus_start_cycles));
	m_check_timer = timer_alloc(FUNC(tms320c54x_test_state::check_results), this);
	save_item(NAME(m_idle_dma_start));
	save_item(NAME(m_idle_dma_rate));
	save_item(NAME(m_idle_dma_first_ticks));
	save_item(NAME(m_shifted_bus_word));
	save_item(NAME(m_shifted_bsp.control));
	save_item(NAME(m_shifted_bsp.receive));
	save_item(NAME(m_shifted_bsp.receive_ready));
	save_item(NAME(m_shifted_bsp.receive_overrun));
}

void tms320c54x_test_state::machine_reset()
{
	using self = tms320c54x_test_state;
	m_result_owner = nullptr;
	m_pipeline_counter_count = 0;
	m_context_coverage = {};
	if (start_profile(*this, machine().options().bios(), {
		{ {"rom4-context"}, [](self &test) { test.m_result_owner = &self::check_rom4_context_results; test.start_pipeline_mu4_prologue(); } },
		{ {"reg-banks"}, [](self &test) { test.start_pipeline_accumulator_arithmetic(); } },
		{ {"power-idle"}, [](self &test) { test.m_result_owner = &self::check_pipeline_idle_results; test.start_pipeline_idle1(0, true); } },
		{ {"pop-irq"}, [](self &test) { test.start_pipeline_mmr_pop(); } },
		{ {"mvpd-extended"}, [](self &test) { test.start_extended_mvpd(); } },
		{ {"mvpd-controls"}, [](self &test) { test.start_pipeline_mvpd(); } },
		{ {"rpt-writa", "rpt-writa16"}, [](self &test) { test.start_repeated_writa(); } },
		{ {"std-swwsr"}, [](self &test) { test.start_standard_swwsr_test(); } },
	}))
		return;
	if (standard_bus_io()) { start_bus_io_test(); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"std-stm"}, [](self &test) { test.start_staged_bus_stm(); } },
		{ {"clock-stm"}, [](self &test) { test.start_staged_bus_stm(); } },
		{ {"serial-stm"}, [](self &test) { test.start_staged_bus_stm(); } },
	}))
		return;
	if (status_bitfield()) { start_pipeline_mapped_bitfield(); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"rptb-exit", "rptb-exit-delay"}, [](self &test) { test.start_block_exit(); } },
	}))
		return;
	if (profile_is("reada-brc") || standard_reada_brc()) { start_reada_brc(); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"st1-rules"}, [](self &test) { test.check_st1_memory_rules(); } },
		{ {"stlm-st1"}, [](self &test) { test.start_stlm_status(); } },
		{ {"stm-st1"}, [](self &test) { test.start_stlm_status(); } },
		{ {"stlm-st1-irq"}, [](self &test) { test.start_mvmd_status_irq(); } },
	}))
		return;
	if (profile_is("shift-periph") || shifted_bsp_fixture() || shifted_unowned_fixture()) { start_shifted_peripheral(); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"stlm-imr"}, [](self &test) { test.start_pipeline_imr_store(); } },
		{ {"pmst-map"}, [](self &test) { test.start_pipeline_pmst(); } },
		{ {"pmst-pop"}, [](self &test) { test.start_pipeline_pmst(); } },
		{ {"pmst-popd"}, [](self &test) { test.start_pipeline_pmst(); } },
		{ {"pmst-prefetch"}, [](self &test) { test.start_pipeline_pmst(); } },
		{ {"pmst-iptr"}, [](self &test) { test.start_pipeline_pmst(); } },
		{ {"pop-repeat"}, [](self &test) { test.start_pipeline_pmst(); } },
		{ {"stack-irq"}, [](self &test) { test.start_stack_irq_order(); } },
		{ {"stack-irq-return"}, [](self &test) { test.start_stack_irq_order(); } },
		{ {"mcbsp-control"}, [](self &test) { test.start_mcbsp_control_sequence(); } },
		{ {"rmw-long-periph"}, [](self &test) { test.start_pipeline_long_rmw(); } },
		{ {"rmw-long"}, [](self &test) { test.start_pipeline_long_rmw(); } },
		{ {"xorm-absolute"}, [](self &test) { test.start_pipeline_long_rmw(); } },
		{ {"st-long-periph"}, [](self &test) { test.start_pipeline_long_immediate_store(); } },
		{ {"st-mvmm"}, [](self &test) { test.start_pipeline_store_mvmm(); } },
		{ {"st-acc-mvmm"}, [](self &test) { test.start_pipeline_store_mvmm(); } },
		{ {"push-mvmm-sp"}, [](self &test) { test.start_push_mvmm(); } },
		{ {"ldm-ar-age"}, [](self &test) { test.start_pipeline_mapped_load_age(); } },
		{ {"mapped-load-age"}, [](self &test) { test.start_pipeline_mapped_load_age(); } },
		{ {"shift16-load-age"}, [](self &test) { test.start_pipeline_mapped_load_age(); } },
		{ {"addsub-ar-age"}, [](self &test) { test.start_pipeline_mapped_load_age(); } },
		{ {"preinc-st"}, [](self &test) { test.start_preincrement_store(); } },
		{ {"move-brc"}, [](self &test) { test.start_move_brc(); } },
		{ {"move-brc-delay"}, [](self &test) { test.start_move_brc(); } },
		{ {"move-rpt-owner"}, [](self &test) { test.start_move_repeat_owner(); } },
		{ {"block-xc-irq", "block-xc-end-irq"}, [](self &test) { test.start_block_xc_irq(); } },
		{ {"block-xc"}, [](self &test) { test.start_block_xc(); } },
		{ {"stm-brc"}, [](self &test) { test.start_mvmd_status_block(); } },
		{ {"pop-brc"}, [](self &test) { test.start_mvmd_status_block(); } },
		{ {"preinc-store"}, [](self &test) { test.start_preincrement_store(); } },
		{ {"ldu-periph"}, [](self &test) { test.start_ldu_peripheral(); } },
		{ {"ldu-imr"}, [](self &test) { test.start_ldu_imr(); } },
		{ {"ldu-st1"}, [](self &test) { test.start_ldu_status(); } },
		{ {"mvmd-status-irq"}, [](self &test) { test.start_mvmd_status_irq(); } },
		{ {"mvmd-block"}, [](self &test) { test.start_mvmd_status_block(); } },
		{ {"mvmd-status-alu"}, [](self &test) { test.start_mvmd_status_alu(); } },
		{ {"mpy-delay", "mac-delay", "maca-delay"}, [](self &test) { test.start_delay_multiply(); } },
		{ {"mpyr-delay"}, [](self &test) { test.start_delay_multiply(); } },
		{ {"bd-maca-long"}, [](self &test) { test.start_delay_multiply(); } },
		{ {"square-delay"}, [](self &test) { test.start_delay_multiply(); } },
		{ {"squr-delay"}, [](self &test) { test.start_delay_multiply(); } },
		{ {"delay-copy"}, [](self &test) { test.start_delay_copy(); } },
		{ {"delay-copy-ctrl"}, [](self &test) { test.start_delay_copy(); } },
		{ {"move-t"}, [](self &test) { test.start_t_register_move(); } },
		{ {"store-delay"}, [](self &test) { test.start_delay_store(); } },
		{ {"mvkd-sp"}, [](self &test) { test.start_mvkd_sp(); } },
		{ {"add-bk", "add-absolute", "add-sp"}, [](self &test) { test.start_pipeline_absolute_add(); } },
		{ {"instr-observer"}, [](self &test) { test.start_instruction_observer(); } },
		{ {"software-intr"}, [](self &test) { test.start_software_intr(); } },
		{ {"rptb-br-edges"}, [](self &test) { test.start_block_branch_edge(); } },
		{ {"bio-branch"}, [](self &test) { test.start_pipeline_bio_branch(); } },
		{ {"rptb-branch"}, [](self &test) { test.start_block_branch(); } },
		{ {"mpy-mem-imm"}, [](self &test) { test.start_memory_immediate_multiply(); } },
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
		{ {"store-observe"}, [](self &test) { test.start_pipeline_store_case(); } },
		{ {"push-observe"}, [](self &test) { test.start_pipeline_xpc_push(); } },
		{ {"mul-t-sources"}, [](self &test) { test.start_pipeline_memory_multiply(); } },
	}))
		return;
	if (profile_is("squr-smem")) { check_control_disassembler(); start_pipeline_memory_multiply(); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"shifted-alu", "shifted-bk"}, [](self &test) { test.start_pipeline_shifted_arithmetic(); } },
		{ {"indirect-load"}, [](self &test) { test.start_pipeline_indirect_case(); } },
	}))
		return;
	if (profile_is("ldu-shift16")) { start_pipeline_offset_load(12672); return; }
	if (profile_is("indirect-dld")) { start_pipeline_extended_dst(); return; }
	if (profile_is("load-variable")) { start_pipeline_offset_load(127872); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"rpt-subc-st"}, [](self &test) { test.start_pipeline_memory_repeat(); } },
		{ {"mpy-t-imm", "mac-t-imm"}, [](self &test) { test.start_pipeline_immediate_multiply(); } },
		{ {"mar-index"}, [](self &test) { test.start_indexed_mar(); } },
		{ {"mar-reverse"}, [](self &test) { test.start_indexed_mar(); } },
		{ {"poll-interrupt"}, [](self &test) { test.start_poll_interrupt(); } },
		{ {"compare-delay"}, [](self &test) { test.start_delay_compare(); } },
	}))
		return;
	if (profile_is("t-load-control")) { start_pipeline_memory_multiply(165888); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"st-t", "st-t-consumer"}, [](self &test) { test.start_t_store(); } },
		{ {"rptb-single"}, [](self &test) { test.start_block_single(); } },
		{ {"rptb-memory"}, [](self &test) { test.start_block_single(); } },
		{ {"rptb-page"}, [](self &test) { test.start_block_page(); } },
		{ {"rptb-fcall", "rptb-fcalld"}, [](self &test) { test.start_block_far_call(); } },
		{ {"rptb-back"}, [](self &test) { test.start_block_back(); } },
		{ {"load-delay", "add-delay", "dld-delay"}, [](self &test) { test.start_delay_load(); } },
		{ {"st-dagen"}, [](self &test) { test.start_constant_dagen(); } },
		{ {"shift-delay"}, [](self &test) { test.start_delay_load(); } },
		{ {"shift-alu-delay"}, [](self &test) { test.start_delay_load(); } },
		{ {"rptbd-shift-st"}, [](self &test) { test.start_rptbd_shift_store(); } },
		{ {"rpt-smem"}, [](self &test) { test.start_basic_repeat(); } },
		{ {"rpt-ld"}, [](self &test) { test.start_repeat_load(); } },
		{ {"rpt-sfta"}, [](self &test) { test.start_repeat_load(); } },
	}))
		return;
	if (profile_is("rpt-ld-block")) { start_repeat_load(198656); return; }
	if (profile_is("stlm-pmst")) { start_stlm_pmst(); return; }
	if (profile_is("cond-store") || profile_is("cond-store-edge")) { start_staged_conditional_store(); return; }
	if (profile_is("cond-store-ar")) { start_staged_conditional_store(); return; }
	if (profile_is("dual-sub-native")) { start_native_dual_sub(0); return; }
	if (profile_is("parallel-sub")) { start_parallel_sub_fraction(0); return; }
	if (profile_is("pshd-long")) { start_extended_push(); return; }
	if (profile_is("pshd-long-map")) { start_extended_push(96000); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"mvdk-mapped", "mvdk-acc", "clock-mvdk"}, [](self &test) { test.start_mvdk_mapped(); } },
		{ {"mvdk-producer", "mvdk-acc-prod"}, [](self &test) { test.start_mvdk_producer(); } },
		{ {"mvdd-periph"}, [](self &test) { test.start_mvdd_peripheral(); } },
		{ {"mvdd-source", "mvdd-source-rpt"}, [](self &test) { test.start_mvdd_source(); } },
		{ {"mvmd-t", "mvmd-bk"}, [](self &test) { test.start_mvmd_word_register(); } },
		{ {"mvmd-acc", "mvmd-st1"}, [](self &test) { test.start_mvmd_accumulator(); } },
		{ {"alu-map", "alu-map-edge", "alu-map-irq", "alu-low-bus"}, [](self &test) { test.start_mapped_dual(); } },
		{ {"xc-ret"}, [](self &test) { test.start_xc_return(); } },
		{ {"drsub", "long-pair"}, [](self &test) { test.start_pipeline_long_pair(); } },
		{ {"xmem-store"}, [](self &test) { test.start_xmem_store(); } },
		{ {"xmem-load", "xmem-alu"}, [](self &test) { test.start_xmem_load(); } },
		{ {"mvdd"}, [](self &test) { test.start_mvdd(); } },
		{ {"mvdd-repeat"}, [](self &test) { test.start_mvdd(); } },
		{ {"logic-absolute"}, [](self &test) { test.start_absolute_logic(); } },
		{ {"popd-mapped", "popd-repeat"}, [](self &test) { test.start_popd_mapped(); } },
		{ {"popd-edges"}, [](self &test) { test.start_popd_edges(); } },
		{ {"popd-ram"}, [](self &test) { test.start_pipeline_data_pop(); } },
		{ {"bacc", "fbacc", "bacc-producer", "fbacc-producer"}, [](self &test) { test.start_accumulator_branch(); } },
		{ {"logic-offset"}, [](self &test) { test.start_offset_logic(); } },
		{ {"logic-peripheral"}, [](self &test) { test.start_pipeline_peripheral_logic(); } },
		{ {"load-offset"}, [](self &test) { test.start_pipeline_offset_load(); } },
		{ {"write-a"}, [](self &test) { test.start_write_a(); } },
		{ {"mvdp"}, [](self &test) { test.start_mvdp(); } },
		{ {"mvdp-pair"}, [](self &test) { test.start_mvdp_pair(); } },
		{ {"rpt-mvdp"}, [](self &test) { test.start_repeat_mvdp(); } },
		{ {"rpt-mvkd"}, [](self &test) { test.start_repeat_mvkd(); } },
		{ {"rpt-mvdp-irq"}, [](self &test) { test.start_repeat_mvdp_irq(); } },
		{ {"ret-load"}, [](self &test) { test.start_return_load(); } },
		{ {"conditional-cd"}, [](self &test) { test.start_delayed_conditional_call(); } },
		{ {"tc-delay-call"}, [](self &test) { test.start_delayed_conditional_call(); } },
		{ {"rptb-ccd"}, [](self &test) { test.start_block_conditional_call(); } },
		{ {"repeat-isr"}, [](self &test) { test.start_saved_repeat(); } },
		{ {"repeat-stlm"}, [](self &test) { test.start_saved_repeat(); } },
		{ {"repeat-body-isr"}, [](self &test) { test.start_body_repeat(); } },
		{ {"repeat-context"}, [](self &test) { test.start_repeat_context(); } },
		{ {"repeat-pmst"}, [](self &test) { test.start_repeat_context(); } },
		{ {"repeat-popd"}, [](self &test) { test.start_repeat_context(); } },
		{ {"rptb-bcd", "rptb-bd", "rptb-banz", "rptb-tc-delay"}, [](self &test) { test.start_block_bcd(); } },
		{ {"push-pop-same"}, [](self &test) { test.start_push_pop_same(); } },
		{ {"fcala-producer"}, [](self &test) { test.start_accumulator_call(); } },
		{ {"fcala-delay"}, [](self &test) { test.start_fcala_delay(); } },
		{ {"fcala"}, [](self &test) { test.start_accumulator_call(); } },
		{ {"cala-producer"}, [](self &test) { test.start_accumulator_call(); } },
		{ {"cala"}, [](self &test) { test.start_accumulator_call(); } },
	}))
		return;
	if (profile_is("far-atomic")) { m_cpu->set_pipeline_frontend(false); start_far_case(0); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"t-peripheral"}, [](self &test) { test.start_t_peripheral(); } },
		{ {"st-t-branch"}, [](self &test) {
			test.m_result_owner = &self::check_t_store_branch_results;
			test.start_t_store_branch();
		} },
		{ {"portr-circular"}, [](self &test) { test.start_circular_portr(); } },
		{ {"portr-block"}, [](self &test) { test.start_circular_portr_block(); } },
	}))
		return;
	if (start_profile(*this, machine().options().bios(), {
		{ {"sqdst"}, [](self &test) { test.start_sqdst(); } },
		{ {"stld", "st-par-alu"}, [](self &test) { test.start_parallel_load(); } },
		{ {"stld-map"}, [](self &test) { test.start_parallel_load(); } },
		{ {"stld-bsp"}, [](self &test) { test.start_parallel_load(); } },
		{ {"stld-periph"}, [](self &test) { test.start_parallel_load(); } },
		{ {"st-par-mpy"}, [](self &test) { test.start_parallel_load(); } },
		{ {"portr-bypass"}, [](self &test) { test.start_portr_bypass(); } },
		{ {"mvdk-circular", "mvdk-index"}, [](self &test) { test.start_circular_move(); } },
		{ {"mvmd-repeat", "mvmd-rptz"}, [](self &test) { test.start_repeated_mvmd(); } },
		{ {"rptbd-t-load"}, [](self &test) { test.start_rptbd_t_load(); } },
		{ {"banzd-t-load"}, [](self &test) { test.start_rptbd_t_load(); } },
		{ {"ctrl-t-load"}, [](self &test) { test.start_rptbd_t_load(); } },
		{ {"sqacc-repeat"}, [](self &test) { test.start_poly(); } },
		{ {"tmac-repeat"}, [](self &test) { test.start_poly(); } },
		{ {"mac-rpt"}, [](self &test) { test.start_poly(); } },
		{ {"lms"}, [](self &test) { test.start_sqdst(); } },
		{ {"bd-rpt"}, [](self &test) { test.start_branch_repeat(); } },
		{ {"retd-rpt"}, [](self &test) { test.start_branch_repeat(); } },
		{ {"calld-rpt"}, [](self &test) { test.start_branch_repeat(); } },
		{ {"bcd-rpt"}, [](self &test) { test.start_branch_repeat(); } },
		{ {"bcd-rpt-edge"}, [](self &test) { test.start_conditional_delay_repeat_edge(); } },
		{ {"rptbd-rpt"}, [](self &test) { test.start_block_delay_repeat(); } },
		{ {"rptbd-rpt-edge"}, [](self &test) { test.start_block_delay_repeat_edge(); } },
		{ {"bd-rpt-edge"}, [](self &test) { test.start_branch_repeat_edge(); } },
		{ {"stm-ind", "stm-ind-edge"}, [](self &test) { test.start_indirect_stm(); } },
		{ {"stm-low-ram"}, [](self &test) { test.start_indirect_stm(); } },
		{ {"stld-repeat"}, [](self &test) { test.start_repeated_stld(); } },
		{ {"st-alu-rpt"}, [](self &test) { test.start_repeated_stld(); } },
		{ {"mvdk-repeat", "mvdk-index-rpt"}, [](self &test) { test.start_repeated_mvdk(); } },
		{ {"rptb-call"}, [](self &test) { test.start_block_near_call(); } },
		{ {"rpt-mem-copy", "rpt-mem-wrap", "rpt-mem-chain"}, [](self &test) { test.start_memory_copy_repeat(); } },
		{ {"addc"}, [](self &test) { test.start_addc(); } },
		{ {"unsigned-map"}, [](self &test) { test.start_addc(); } },
		{ {"shift16-map"}, [](self &test) { test.start_addc(); } },
		{ {"logic-map"}, [](self &test) { test.start_addc(); } },
		{ {"addc-chain"}, [](self &test) { test.start_addc_chain(); } },
		{ {"mvkd-peripheral"}, [](self &test) { test.start_mvkd_peripheral(); } },
		{ {"cmpm-acc"}, [](self &test) { test.start_accumulator_cmpm(); } },
		{ {"cmpm-map", "cmpm-map-edge"}, [](self &test) { test.start_accumulator_cmpm(); } },
		{ {"st-extended-ar", "st-ar-consumer", "st-shift-ar", "st-shift-ar-use"}, [](self &test) { test.start_extended_ar_store(); } },
		{ {"ifr-pair"}, [](self &test) { test.start_ifr_pair(); } },
		{ {"mvmd-ar"}, [](self &test) { test.start_mvmd_ar(); } },
		{ {"mac-smem", "mac-bk"}, [](self &test) { test.start_mac_smem(); } },
		{ {"far-call-delay"}, [](self &test) { test.start_delayed_far_call(); } },
	}))
		return;
	if (profile_is("mvdm-delay") || mvkd_delay()) { start_mvdm_delay(); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"mvdm-ar"}, [](self &test) { test.start_mvdm_ar(); } },
		{ {"mvdm-t"}, [](self &test) { test.start_mvdm_t(); } },
		{ {"mvdm-acc"}, [](self &test) { test.start_mvdm_acc(); } },
		{ {"stack-page0", "stack-page0-edge"}, [](self &test) { test.start_page_zero_stack(); } },
	}))
		return;
	if (absolute_memory_dp()) { start_pipeline_memory_dp(); return; }
	if (start_profile(*this, machine().options().bios(), {
		{ {"read-a-mapped"}, [](self &test) { test.start_mapped_read_a(); } },
		{ {"bus-stm"}, [](self &test) { test.start_staged_bus_stm(); } },
		{ {"timer-stm"}, [](self &test) { test.start_staged_bus_stm(); } },
		{ {"pshd-mapped"}, [](self &test) { test.start_mapped_pshd(); } },
		{ {"pshd-repeat"}, [](self &test) { test.start_repeat_push(); } },
		{ {"bus-io-compose"}, [](self &test) { test.start_atomic_io_composition(); } },
	}))
		return;
	if (!strcmp(machine().system().name, "tms54test") && (profile_is("bus-io") || atomic_bus_io())) { start_bus_io_test(); return; }
	if (!strcmp(machine().system().name, "tms54test") && profile_is("bus-registers"))
	{
		start_bus_register_test(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && profile_is("imr-store"))
	{
		start_pipeline_imr_store(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && profile_is("long-mvkd"))
	{
		start_pipeline_long_immediate_store(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && profile_is("mpy-mapped"))
	{
		start_pipeline_multiply_mapped(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && profile_is("mapped-bitf"))
	{
		start_pipeline_mapped_bitfield(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && profile_is("mapped-word"))
	{
		start_pipeline_mapped_word_arithmetic(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") &&
			profile_is("timer"))
	{
		start_stopped_timer_reload_case(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4"))
	{
		auto &data = m_cpu->space(AS_DATA);
		u16 const *const initial = &memregion("dspdata")->as_u16();
		for (unsigned address = 0; address != 0x10000; ++address)
			// A memory snapshot seeds RAM, not live CPU/peripheral registers.
			if (data.get_write_ptr(address)) data.write_word(address, initial[address]);
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(rom4_codec_is("efr-dtx-decoder") || rom4_codec_is("efr-erasure") || rom4_codec_is("efr-loss-observe") ||
			 rom4_codec_is("efr-homing") || rom4_codec_is("efr-home-errors") || rom4_codec_is("efr-invalid-sid") || rom4_codec_is("efr-lost-sid") || rom4_codec_is("efr-lsid-observe")))
	{
		enable_rom4_codec_pipeline(rom4_codec_is("efr-lost-sid") || rom4_codec_is("efr-lsid-observe") ? "EFR lost SID decoder" : rom4_codec_is("efr-dtx-decoder") ? "EFR DTX decoder" : rom4_codec_is("efr-invalid-sid") ? "EFR invalid SID decoder" : rom4_codec_is("efr-erasure") || rom4_codec_is("efr-loss-observe") ? "EFR erasure decoder" : "EFR homing decoder");
		start_rom4_efr_decoder(0, true);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(rom4_codec_is("efr-encoder") || rom4_codec_is("efr-dtx-encoder") || rom4_codec_is("efr-enc-homing")))
	{
		enable_rom4_codec_pipeline(rom4_codec_is("efr-dtx-encoder") ? "EFR DTX encoder" : rom4_codec_is("efr-enc-homing") ? "EFR homing encoder" : "EFR encoder");
		start_rom4_efr_encoder(0, true);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(profile_is("efr-decoder") || profile_is("efr-decoder-pipe")))
	{
		start_rom4_delayed_repeat(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			profile_is("efr-packing"))
	{
		start_rom4_efr_packing(0, 0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			profile_is("efr-crc"))
	{
		start_rom4_efr_crc(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(profile_is("fr-encoder") || profile_is("fr-encoder-pipe")))
	{
		if (profile_is("fr-encoder-pipe"))
		{
			m_cpu->set_pipeline_frontend(true);
			osd_printf_info("ROM4 FR encoder frontend: staged handset_audio_claim=0\n");
		}
		start_rom4_fr_encoder(0, 0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(profile_is("fr-decoder") || profile_is("fr-decoder-pipe")))
	{
		if (profile_is("fr-decoder-pipe"))
		{
			m_cpu->set_pipeline_frontend(true);
			osd_printf_info("ROM4 FR decoder frontend: staged handset_audio_claim=0\n");
		}
		start_rom4_fr_decoder(0, true);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			profile_is("fr-packing"))
	{
		start_rom4_fr_packing(0, 0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			profile_is("crc-packed"))
	{
		start_rom4_crc_packed(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			profile_is("crc-helper"))
	{
		start_rom4_crc_helper(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") && rom4_codec_is("rf-pairs"))
	{
		enable_rom4_codec_pipeline("RF pairs");
		start_rom4_pairs(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") && rom4_codec_is("rf-bitmap"))
	{
		enable_rom4_codec_pipeline("RF bitmap");
		start_rom4_bitmap(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(profile_is("cold") || profile_is("cold-pipeline")))
	{
		bool const staged = profile_is("cold-pipeline");
		m_phase = staged ? rom4_staged_cold_phase : 4;
		if (staged) m_cpu->set_pipeline_frontend(true);
		m_rom4_checks = 0;
		m_check_timer->adjust(attotime::from_usec(100));
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4"))
	{
		m_rom4_checks = 0;
		auto &data = m_cpu->space(AS_DATA);
		u16 const *const drom = &memregion("dspdrom")->as_u16();
		for (unsigned address = 0xb000; address != 0xf000; ++address)
			data.write_word(address, drom[address]);
		// The sparse entry snapshot predates the firmware-provided challenge.
		// Supply the factory-profile record encoded for COBBA 00160010 while
		// retaining the deterministic peripheral-free entry state.
		static constexpr u16 challenge[] = {
			0xd6fb, 0x4394, 0xe437, 0xda16, 0x9668, 0x964f, 0x5cd4,
			0x32fe, 0x5be2, 0xdba6, 0x9643, 0x82d7, 0x0000, 0x0000
		};
		for (unsigned i = 0; i != std::size(challenge); ++i)
			data.write_word(0x0825 + i, challenge[i]);
		m_phase = 2;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x4b73}, {cpu_regs::STATE_SP, 0x1ec3}, {cpu_regs::STATE_ST0, 0x281f},
			{cpu_regs::STATE_ST1, 0x4101}, {cpu_regs::STATE_PMST, 0xffac}, {cpu_regs::STATE_BK, 0x0052},
			{cpu_regs::STATE_T, 0x000e}, {cpu_regs::STATE_A, 0x0000004b73}, {cpu_regs::STATE_B, 0xfffffffffe},
		});
		static constexpr u16 ar[] = {
			0x0001, 0xb0bc, 0x0825, 0x06e3,
			0x001a, 0x12ca, 0x06e3, 0x0000
		};
		for (unsigned i = 0; i != std::size(ar); ++i)
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, ar[i]);
		m_check_timer->adjust(attotime::from_usec(100));
		return;
	}

	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	// CALL/RET, then a three-word RPT copy.
	check_mmr_bus();
	write_words(program, {
		{0x0100, 0xf074}, {0x0101, 0x0200}, {0x0102, 0xec02}, {0x0103, 0xe598},
	});

	// Reset source/destination pointers and execute a two-word CALL at the
	// end of an RPTB block three times.
	write_words(program, {
		{0x0104, 0x7712}, {0x0105, 0x0500}, {0x0106, 0x7714}, {0x0107, 0x0600}, {0x0108, 0x771a}, {0x0109, 0x0002},
		{0x010a, 0xf072}, {0x010b, 0x010d}, {0x010c, 0xf074}, {0x010d, 0x0210},
	});

	// Three circular moves starting at the final element must wrap through
	// the first two elements and return to the final element.
	write_words(program, {
		{0x010e, 0x7710}, {0x010f, 0x0001}, {0x0110, 0x7712}, {0x0111, 0x0802}, {0x0112, 0x7713}, {0x0113, 0x0900},
		{0x0114, 0x7719}, {0x0115, 0x0003}, {0x0116, 0xec02}, {0x0117, 0xe5c9}, {0x0118, 0xe726}, {0x0119, 0xf074},
		{0x011a, 0x0220},
	});

	write_words(program, {
		{0x011b, 0x70f8}, {0x011c, 0x0904}, {0x011d, 0x0801}, {0x011e, 0x7714}, {0x011f, 0x0a00}, {0x0120, 0x70f8},
		{0x0121, 0x0905}, {0x0122, 0x0014}, {0x0123, 0x7712}, {0x0124, 0x0800}, {0x0125, 0x7192}, {0x0126, 0x0014},
		{0x0127, 0x7713}, {0x0128, 0x0800}, {0x0129, 0x1293}, {0x012a, 0xf0c8}, {0x012b, 0xf0e8}, {0x012c, 0xf0f8},
		{0x012d, 0x7713}, {0x012e, 0x0800}, {0x012f, 0x1393}, {0x0130, 0xf330}, {0x0131, 0x00ff}, {0x0132, 0xf3e8},
		{0x0133, 0x7df8}, {0x0134, 0x0800}, {0x0135, 0x0907}, {0x0136, 0x7cf8}, {0x0137, 0x0908}, {0x0138, 0x0907},
	});
	// ROM4 receive enqueue: copy 26 words from *AR3+ into the circular
	// MDIRCV ring at *AR2+0%.  E59C's Y operand is AR2, not AR6.
	write_words(program, {
		{0x0139, 0x7710}, {0x013a, 0x0001}, {0x013b, 0x7712}, {0x013c, 0x0882}, {0x013d, 0x7713}, {0x013e, 0x1300},
		{0x013f, 0x7719}, {0x0140, 0x0052}, {0x0141, 0xec19}, {0x0142, 0xe59c}, {0x0143, 0xf5e1},
	});

	write_words(program, {
		{0x0200, 0x7680}, {0x0201, 0xbeef}, {0x0202, 0xfc00}, {0x0210, 0x1092}, {0x0211, 0x8094}, {0x0212, 0xfc00},
		{0x0220, 0x61f8}, {0x0221, 0x0800}, {0x0222, 0x8000}, {0x0223, 0xfc30}, {0x0224, 0x7680}, {0x0225, 0xdead},
		{0x0226, 0xfc00},
	});

	// Final ROM4 challenge-transform loop. These are observed operands and
	// generic instruction encodings, not firmware code or a canned result.
	write_words(program, {
		{0x0300, 0x7712}, {0x0301, 0x13d9}, {0x0302, 0x7713}, {0x0303, 0x13d7}, {0x0304, 0x7714}, {0x0305, 0x1208},
		{0x0306, 0x771a}, {0x0307, 0x0005}, {0x0308, 0xf072}, {0x0309, 0x030b}, {0x030a, 0xf074}, {0x030b, 0x0320},
		{0x0320, 0x108a}, {0x0321, 0xf493}, {0x0322, 0x1a8b}, {0x0323, 0x6d8c}, {0x0324, 0x1c84}, {0x0325, 0x8084},
		{0x0326, 0xfc00},
	});
	// Long-immediate repeat executes the following two-word instruction
	// exactly lk + 1 times.
	write_words(program, {
		{0x0350, 0xf062}, {0x0351, 0x1234}, {0x0352, 0x4ef8}, {0x0353, 0x090c}, {0x0354, 0xe800}, {0x0355, 0xf070},
		{0x0356, 0x0002}, {0x0357, 0x6d10}, {0x0358, 0xf5e1}, {0x0360, 0x76f8}, {0x0361, 0x0910}, {0x0362, 0x5678},
		{0x0363, 0x7214}, {0x0364, 0x0912}, {0x0365, 0x57f8}, {0x0366, 0x0914}, {0x0367, 0xf793}, {0x0368, 0xff0c},
		{0x0369, 0xf495}, {0x036a, 0xf793}, {0x036b, 0xf065}, {0x036c, 0x00ff}, {0x036d, 0xf054}, {0x036e, 0x00f0},
		{0x036f, 0xf5e1}, {0x0370, 0xf171}, {0x0371, 0x0001}, {0x0372, 0x6d10}, {0x0373, 0xf5e1}, {0x0374, 0x47f8},
		{0x0375, 0x0918}, {0x0376, 0x6bf8}, {0x0377, 0x091a}, {0x0378, 0x0001}, {0x0379, 0xf5e1}, {0x037a, 0xf070},
		{0x037b, 0x0001}, {0x037c, 0x7d92}, {0x037d, 0x0924}, {0x037e, 0xf5e1}, {0x0380, 0xf273}, {0x0381, 0x0390},
		{0x0382, 0xf495}, {0x0383, 0xf495}, {0x0384, 0xf5e1}, {0x0390, 0xf5e1}, {0x0392, 0xf4eb}, {0x0398, 0xf5e1},
		{0x039a, 0xfa45}, {0x039b, 0x03a0}, {0x039c, 0xf495}, {0x039d, 0xf495}, {0x039e, 0xf5e1}, {0x03a0, 0xf5e1},
		{0x03a2, 0x6ff8}, {0x03a3, 0x0920}, {0x03a4, 0x0c48}, {0x03a5, 0xf5e1}, {0x03a6, 0xf5e2}, {0x03b0, 0xf5e1},
		{0x03b2, 0x09f8}, {0x03b3, 0x0918}, {0x03b4, 0xf5e1}, {0x03b6, 0xfc4b}, {0x03b7, 0xf5e1}, {0x03c0, 0xf5e1},
		{0x03c2, 0xf947}, {0x03c3, 0x03d0}, {0x03c4, 0xf5e1}, {0x03d0, 0xf5e1}, {0x03d2, 0xf520}, {0x03d3, 0xf5e1},
		{0x03d4, 0xf070}, {0x03d5, 0x0001}, {0x03d6, 0x7f92}, {0x03d7, 0xf5e1}, {0x03d8, 0x3292}, {0x03d9, 0xf5e1},
		{0x03da, 0xeeff}, {0x03db, 0xee02}, {0x03dc, 0xf5e1}, {0x03e0, 0xf0ff}, {0x03e1, 0xf5e1},
	});
	write_words(data, {
		{0x0918, 1}, {0x091a, 0}, {0x0920, 0xaaaa}, {0x0921, 0xbbbb}, {0x0912, 0xabcd}, {0x0914, 0x1234},
		{0x0915, 0x5678},
	});

	write_words(data, {
		{0x0500, 0x1111}, {0x0501, 0x2222}, {0x0502, 0x3333}, {0x0800, 0xaaaa}, {0x0801, 0xbbbb}, {0x0802, 0xcccc},
		{0x13d2, 0x6d4d}, {0x13d3, 0xc431}, {0x13d4, 0xbfe4}, {0x13d5, 0x5d91}, {0x13d6, 0x71b1}, {0x13d7, 0x9ac9},
		{0x13d8, 0x6d4d}, {0x13d9, 0xc431}, {0x1202, 0x71b1}, {0x1203, 0x9ac9}, {0x1204, 0x6d4d}, {0x1205, 0xc431},
		{0x1206, 0xbfe4}, {0x1207, 0x5d91},
	});
	for (unsigned i = 0; i != 26; ++i)
		data.write_word(0x1300 + i, 0x6000 + i);
	m_phase = 0;
	m_bleq_case = 0;
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PC, 0x0100}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR0, 0x0700},
		{cpu_regs::STATE_AR2, 0x0400}, {cpu_regs::STATE_AR3, 0x0500},
	});

	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::program_map(address_map &map)
{
	map(0x0000, 0xffff).ram();
	map(0x010000, 0x03ffff).ram();
	map(0x7f0000, 0x7fffff).ram();
}

void tms320c54x_test_state::mcbsp_control_map(address_map &map)
{
	map(0, 0xffff).ram();
	map(0x38, 0x39).rw(m_idle_mcbsp, FUNC(tms320c54x_mcbsp_device::control_r), FUNC(tms320c54x_mcbsp_device::control_w));
}

void tms320c54x_test_state::data_map(address_map &map)
{
	map(0x0000, 0xffff).ram();
	map(0x0060, 0x0061).rw(FUNC(tms320c54x_test_state::repeat_irq_r), FUNC(tms320c54x_test_state::stack_mmr_w));
}

void tms320c54x_test_state::plain_data_map(address_map &map)
{
	map(0, 0xffff).ram();
}

void tms320c54x_test_state::io_map(address_map &map)
{
	map(0x0000, 0xffff).ram();
	map(0x0123, 0x0123).r(FUNC(tms320c54x_test_state::test_port_r));
	map(0x0124, 0x0124).w(FUNC(tms320c54x_test_state::test_port_w));
}

void tms320c54x_test_state::rom4_program_map(address_map &map)
{
	map(0x0000, 0xffff).rom().region("dspprg", 0);
	map(0x0f00, 0x0f00).r(FUNC(tms320c54x_test_state::rom4_loader_entry_r));
}

void tms320c54x_test_state::rom4_data_map(address_map &map)
{
	map(0x0000, 0xffff).ram();
}

void tms320c54x_test_state::expect(bool condition, const char *message)
{
	if (!condition)
	{
		unsigned case_index = m_pipeline_peripheral_case, tick = m_idle_fixture_tick;
		bool replay = m_pipeline_replay;
		auto progress = [&](auto const &fixture) {
			case_index = fixture.variant;
			tick = fixture.tick;
			replay = fixture.replay;
		};
		switch (m_phase)
		{
		case 255800: case 255810: progress(m_saved_irq_repeat); break;
		case 255840: case 255850: progress(m_body_irq_repeat); break;
		case 255480: case 255490: progress(m_repeat_context); break;
		case 255820: case 255830: progress(m_fcala_delay); break;
		case 292000: case 292010: progress(m_instruction_observer); break;
		case 255320: case 255330: case 255331:
			progress(m_rptbd); break;
		}
		osd_printf_info("c54x_failure: phase=%u case=%u tick=%u replay=%u pc=%06x rc=%04x ar1=%04x a=%010llx b=%010llx st0=%04x illegal=%u reads=%u writes=%u\n",
			m_phase, case_index, tick, unsigned(replay), unsigned(m_cpu->state_int(STATE_GENPC)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_RC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),
			(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)),
			m_pipeline_operand_reads, m_pipeline_store_writes);
		throw emu_fatalerror("TMS320C54x core conformance: %s", message);
	}
}

void tms320c54x_test_state::expect_opcode(u16 opcode, bool condition, const char *message)
{
	expect(condition, message);
	// Deduplicate only the diagnostic record, never the assertion itself.
	if (!m_asserted_opcodes.test(opcode))
	{
		m_asserted_opcodes.set(opcode);
		logerror("[opassert] op=%04x\n", opcode);
	}
}

void tms320c54x_test_state::test(machine_config &config)
{
	TMS320C54X(config, m_cpu, 13'000'000);
	if (!strcmp(config.options().bios(), "instr-observer"))
		m_cpu->instruction_observer_cb().set([this](offs_t address, u16 opcode) { m_instruction_observer.observed.emplace_back(address, opcode); });
	else if (!strcmp(config.options().bios(), "rpt-smem") || !strcmp(config.options().bios(), "rpt-ld") || !strcmp(config.options().bios(), "rpt-sfta") || !strcmp(config.options().bios(), "rpt-ld-block") || !strcmp(config.options().bios(), "rptb-call") || !strcmp(config.options().bios(), "rpt-mem-chain") || !strcmp(config.options().bios(), "bd-rpt") || !strcmp(config.options().bios(), "retd-rpt") || !strcmp(config.options().bios(), "calld-rpt") || !strcmp(config.options().bios(), "bcd-rpt"))
		m_cpu->instruction_observer_cb().set([this](offs_t address, u16 opcode) { m_observed_instructions.emplace_back(address, opcode); });
	if (!strcmp(config.options().bios(), "mpy-mem-imm"))
		m_cpu->instruction_observer_cb().set([this](offs_t address, u16 opcode) {
			u16 source = 0xdead, operand = 0xbeef, literal = 0xcafe;
			bool const retained = m_cpu->observe_execute_memory_multiply(address, opcode, source, operand, literal);
			auto const f = memory_immediate_case(m_pipeline_peripheral_case);
			if (address == f.base + 0x600)
				expect(retained && source == f.address && operand == f.word && literal == f.literal,
					"Execute observer exposes the exact retained multiply operand without bus accesses");
			else
				expect(!retained && source == 0xdead && operand == 0xbeef && literal == 0xcafe,
					"Non-multiply observation leaves diagnostic outputs untouched");
			expect(!m_cpu->observe_execute_memory_multiply(address ^ 1, opcode, source, operand, literal),
				"Retained multiply observation rejects a mismatched instruction address");
		});
	m_cpu->set_extended_program(true);
	if (!strcmp(config.options().bios(), "std-swwsr") || !strcmp(config.options().bios(), "std-stm") || !strcmp(config.options().bios(), "std-io") || !strcmp(config.options().bios(), "std-io-atomic")) m_cpu->set_standard_swwsr(true);
	if (!strcmp(config.options().bios(), "bus-registers") || !strcmp(config.options().bios(), "bus-io") || !strcmp(config.options().bios(), "bus-io-atomic") || !strcmp(config.options().bios(), "bus-io-compose") || !strcmp(config.options().bios(), "bus-stm")) m_cpu->set_c5416_bus_profile(true);
	m_cpu->ready_in_cb().set([this]() { return int(m_bus_ready); });
	m_cpu->clkout_divisor_cb().set([this](u8 value) { m_bus_divisor = value; });
	m_cpu->iack_enable_cb().set([this](int value) { m_bus_iack_enable = bool(value); });
	m_cpu->set_irq_acknowledge_callback([this](device_t &, int line) {
		if (m_phase == 256100)
		{
			expect(line == ((m_pipeline_peripheral_case / 32) % 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0x602, "Polling loop acknowledges its pending source once with the branch target as continuation");
			++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (m_phase == 259960)
		{
			unsigned const variant = m_pipeline_peripheral_case;
			unsigned const count = std::array<unsigned, 4>{0, 1, 7, 255}[variant % 4];
			bool const nmi = (variant / 16) % 2, long_count = (variant / 8) % 2;
			expect(line == (nmi ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == count + 1 && m_cpu->state_int(tms320c54x_device::STATE_RTN) == (long_count ? 0x604 : 0x603), "Repeated MVDP acknowledges the pending source only after all writes with the unexecuted continuation");
			++m_vector_acks;
		}
		if (m_phase == 310560)
		{
			auto const f = extended_push_case(m_pipeline_peripheral_case);
			expect(f.irq && line == (f.irq == 2 || f.irq == 4 ? INPUT_LINE_NMI : 2) && !m_rptbd_counts.acks++, "Extended push acknowledges its original pending interrupt once"); m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (m_phase == 310520)
		{
			auto const f = stlm_pmst_case(m_pipeline_peripheral_case);
			expect(f.irq && line == (f.irq == 17 ? INPUT_LINE_NMI : int(f.irq) - 1) && !m_rptbd_counts.acks++, "STLM PMST interrupt acknowledges its original pending source once"); m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (m_phase == 310480)
		{
			auto const f = repeat_load_case(m_pipeline_peripheral_case); unsigned iterations = 0;
			for (auto const &[address, word] : m_observed_instructions) if (address == f.body) ++iterations;
			expect(f.irq && line == (f.irq == 2 ? INPUT_LINE_NMI : 2) && !m_rptbd_counts.acks++ && iterations == f.count + 1, "Repeated accumulator load acknowledges its pending source only after every arithmetic iteration");
			m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (m_phase == 310440)
		{
			auto const f = basic_repeat_case(m_pipeline_peripheral_case); unsigned iterations = 0;
			for (auto const &[address, word] : m_observed_instructions) if (address == f.body) ++iterations;
			if (!f.irq || line != (f.irq == 2 ? INPUT_LINE_NMI : 2) || m_rptbd_counts.acks || iterations != f.count + 1)
				osd_printf_info("Memory repeat interrupt mismatch case=%u replay=%u line=%d acks=%u iterations=%u count=%u cycle=%llu\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), line, m_rptbd_counts.acks, iterations, f.count, (unsigned long long)m_cpu->pipeline_cycles());
			expect(f.irq && line == (f.irq == 2 ? INPUT_LINE_NMI : 2) && !m_rptbd_counts.acks++ && iterations == f.count + 1, "Memory repeat defers IRQ/NMI until its complete body count"); m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (m_phase == 310400)
		{
			auto const f = rptbd_shift_store_case(m_pipeline_peripheral_case);
			expect(f.irq && line == (f.irq == 2 || f.irq == 4 ? INPUT_LINE_NMI : 2) && !m_rptbd_counts.acks++ && m_rptbd_counts.delay_writes == 1 && m_cpu->pipeline_cycles() >= 9, "RPTBD shifted-store interrupt is acknowledged only after its complete delay publication"); m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (m_phase == 255240) { auto const f = block_back_case(m_pipeline_peripheral_case); expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 2) && !m_vector_acks && m_pipeline_store_writes, "Backward block acknowledges its pending source once after the first copy"); ++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE); }
		if (m_phase == 255200) { auto const f = block_far_call_case(m_pipeline_peripheral_case, profile_is("rptb-fcalld")); expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 2) && !m_vector_acks && m_pipeline_operand_reads, "In-block far call acknowledges the actual pending source once after callee entry"); ++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE); }
		if (m_phase == 62150) { auto const f = block_near_call_case(m_pipeline_peripheral_case); expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 2) && !m_vector_acks && m_pipeline_operand_reads, "In-block near call acknowledges its actual pending interrupt once"); ++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE); }
		if (m_phase == 62154) { auto const f = memory_copy_repeat_case(m_pipeline_peripheral_case); expect(f.irq && line == (f.irq == 2 ? INPUT_LINE_NMI : 2) && !m_vector_acks && m_pipeline_store_writes == f.count + 1 && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0, "Memory-count repeat defers its actual pending interrupt until all copies complete"); ++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE); }
		if (m_phase == 254360) { unsigned const interrupt = m_pipeline_peripheral_case / 4096; expect(interrupt && line == (interrupt == 2 ? INPUT_LINE_NMI : 2) && !m_vector_acks, "In-block branch interrupt acknowledges its actual pending source once"); ++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE); }
		if (m_phase == 253980) { expect(line == ((m_pipeline_peripheral_case / 4) % 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_cpu->pipeline_cycles() == 6, "Stack ordering preserves the selected IRQ/NMI acknowledgement"); ++m_vector_acks; }
		if (m_phase == 254140) { auto const &f = m_mvdd_fixture; expect(f.irq && line == (f.irq == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == f.count + 1 && m_cpu->pipeline_cycles() > f.first_write() + f.count, "MVDD acknowledges IRQ/NMI only after the final retained destination write"); ++m_vector_acks; }
		if (m_phase == 254200) { auto const &f = m_popd_fixture; expect(f.irq && line == (f.irq == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == f.count + 1 && m_cpu->pipeline_cycles() > f.read_clock() + f.count + 1, "POPD acknowledges IRQ/NMI only after the last repeat iteration retires"); ++m_vector_acks; }
		if (m_phase == 254240) { auto const &f = m_acc_branch_fixture; expect(f.irq && line == (f.irq == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && !m_pipeline_store_writes && !m_pipeline_register_writes, "Accumulator branch completes target selection before IRQ without allocating a call frame"); ++m_vector_acks; }
		if (m_phase == 254070) { auto const f = repeat_push_case(m_pipeline_peripheral_case); expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == f.count + 1 && m_cpu->pipeline_cycles() > f.first_write() + f.count, "Repeated PSHD acknowledges IRQ/NMI only after its final Execute"); ++m_vector_acks; }
		if (m_phase == 253890) { expect(line == (m_pipeline_peripheral_case & 8 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == 1, "Full status publication completes before interrupt entry without an RSBX-specific guard"); m_timer_probe_return_pc = m_cpu->state_int(tms320c54x_device::STATE_RTN); expect(m_timer_probe_return_pc >= (profile_is("stlm-st1-irq") ? 0x601 : 0x602) && m_timer_probe_return_pc <= 0x621, "IRQ/NMI retains an unexecuted foreground continuation"); ++m_vector_acks; }
		if (m_phase == 253790) { unsigned const interrupt = m_pipeline_peripheral_case / 72; expect(interrupt && line == (interrupt == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == (near_accumulator_fixture() ? 1 : 2), "IRQ/NMI cannot split the retained accumulator-call frame"); ++m_vector_acks; }
		if (m_phase == 253730) { unsigned const variant = m_pipeline_peripheral_case; expect(variant >= 48 && line == (variant < 72 ? 12 : INPUT_LINE_NMI) && !m_vector_acks && m_pipeline_store_writes == 1, "IFR clearance cannot cancel a selected IRQ or independent NMI"); ++m_vector_acks; }
		if (m_phase == 253550) { auto const f = bus_io_case(m_pipeline_peripheral_case); expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && bus_io_clock() > f.complete, "Interrupt acknowledgement cannot split the pending external transaction"); ++m_vector_acks; }
		if (m_phase == 253570) { unsigned const transfers = m_pipeline_peripheral_case / 8 ? 1 : 1 + (m_pipeline_peripheral_case / 2) % 4; expect(line == 0 && !m_vector_acks && m_pipeline_operand_reads + m_pipeline_store_writes == transfers, "IRQ acknowledgement follows the complete guarded I/O sequence"); ++m_vector_acks; }
		if (m_phase == 254980) { auto const f = delay_compare_case(m_pipeline_peripheral_case); expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_operand_reads == (f.prefix ? 2U : 1U) && bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) == f.final_tc && m_cpu->pipeline_cycles() > f.read, "IRQ NMI cannot enter before the complete delayed comparison publishes TC"); ++m_vector_acks; }
		if (m_phase == 253650) { unsigned const source = (m_pipeline_peripheral_case / 64) % 3; expect(source && line == (source == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == 1, "IRQ/NMI acknowledgement cannot split the two-word MVDM delay slot"); ++m_vector_acks; }
		if (m_phase == 253670) { unsigned const source = (m_pipeline_peripheral_case / 27) % 3; expect(source && line == (source == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == 2, "Pending IRQ/NMI cannot split the delayed far-call frame"); ++m_vector_acks; }
		if (m_phase == 253530) { expect(line == 0 && !m_vector_acks, "Logical IRQ callback remains enabled with IACKOFF set"); ++m_vector_acks; }
		if (m_phase == 253510)
		{
			auto const f = profile_is("stlm-imr") ? stlm_imr_case(m_pipeline_peripheral_case) : imr_store_case(m_pipeline_peripheral_case);
			expect(f.ack_cycle && line == int(f.kind == 2 ? INPUT_LINE_NMI : f.source) && !m_vector_acks && m_cpu->pipeline_cycles() == f.ack_cycle,
				"STM IMR acknowledges the actual pending source after eligibility and preserves prior acceptance across a clear"); ++m_vector_acks;
		}
		if (m_phase == 253230)
		{
			auto const f = memory_repeat_case(m_pipeline_peripheral_case);
			bool const store = m_pipeline_peripheral_case >= 38400;
			unsigned const last = store ? (f.long_form ? 9 : 8) + 2 * f.count : (f.long_form ? 8 : 7) + f.count;
			expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 0) && m_cpu->pipeline_cycles() > last && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_RTN) == u16(f.body + (store ? 2 : 1)) && !m_vector_acks, "Memory repeat acknowledges its pending interrupt after the final iteration at the fresh continuation"); ++m_vector_acks;
		}
		if (m_phase == 252230)
		{
			unsigned const kind = pipeline_idle_kind(m_pipeline_peripheral_case);
			expect(line == (kind == 5 ? INPUT_LINE_NMI : kind == 3 ? 3 : 0) && m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0x601,
				"IDLE1 wake acknowledges the actual source with its unexecuted continuation");
			++m_vector_acks;
		}
		observe_context_irq(line);
		if (m_phase == 251090)
		{
			expect(line == 0 && m_cpu->pipeline_cycles() == 6, "Indirect load drains while vector IACK occurs");
			++m_vector_acks;
		}
		if (m_phase == 251070)
		{
			expect(line == 0 && m_cpu->pipeline_cycles() == 6,
				"Direct store retires during vector Fetch/IACK");
			++m_vector_acks;
		}
		return 0;
	}, "vector_acknowledge");
	m_cpu->bio_in_cb().set(FUNC(tms320c54x_test_state::bio_r));
	m_cpu->xf_out_cb().set(FUNC(tms320c54x_test_state::xf_w));
	TMS320C54X_MCBSP(config, m_idle_mcbsp, 13'000'000);
	m_cpu->peripheral_clock_stop_cb().set(m_idle_mcbsp, FUNC(tms320c54x_mcbsp_device::internal_clock_stop_w));
	m_idle_mcbsp->tx_word_cb().set([this](u16 value) {
		expect(value == 0xa55a, "deep-idle McBSP preserves the queued word");
		++m_idle_mcbsp_words;
	});
	m_idle_mcbsp->tx_irq_cb().set([this](int state) {
		if (m_idle_mcbsp_external) m_cpu->set_input_line(5, state);
	});
	TMS320C54X_DMA(config, m_idle_dma, 13'000'000);
	m_idle_dma->set_cpu(m_cpu);
	m_idle_dma->set_input_clock(3'250'000);
	m_cpu->pll_clock_stop_cb().set(m_idle_dma, FUNC(tms320c54x_dma_device::pll_clock_stop_w));
	m_cpu->system_clock_cb().set([this](u32 hz) {
		m_idle_dma->set_unscaled_clock(hz); m_idle_mcbsp->set_unscaled_clock(hz);
	});
	m_idle_dma->completion_cb().set([this](u8 channel) {
		expect(channel == 0, "idle DMA fixture completes only its configured channel");
		if (m_phase < 79600 || m_phase > 79604)
			expect(machine().time() - m_idle_dma_start == attotime::from_ticks(4, m_idle_dma_rate) * 3 +
			attotime::from_ticks(m_idle_dma_first_ticks, m_idle_dma_rate),
			"internal DMA retains its first-word clock phase through clock loss and replay");
		++m_idle_dma_completions;
		// Fixture routing selects the documented DMAC0/BRINT2 vector slot.
		m_cpu->set_input_line(6, ASSERT_LINE);
		m_cpu->set_input_line(6, CLEAR_LINE);
	});
	m_cpu->set_addrmap(AS_PROGRAM, &tms320c54x_test_state::program_map);
	if (!strcmp(config.options().bios(), "mcbsp-control")) m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::mcbsp_control_map);
	// The low-page copy must see RAM at 0060, not the legacy IRQ probe.
	else if (!strncmp(config.options().bios(), "mvdd-source", 11) || !strcmp(config.options().bios(), "stm-ind") || !strcmp(config.options().bios(), "stm-ind-edge") || !strcmp(config.options().bios(), "stm-low-ram") || !strcmp(config.options().bios(), "bitt-ctrl")) m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::plain_data_map);
	else m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::data_map);
	m_cpu->set_addrmap(AS_IO, &tms320c54x_test_state::io_map);
}

void tms320c54x_test_state::rom4(machine_config &config)
{
	TMS320C54X(config, m_cpu, 13'000'000);
	bool const trace_encoder = !strcmp(config.options().bios(),"fr-encoder") || !strcmp(config.options().bios(),"fr-encoder-pipe");
	if (config.options().verbose() && (trace_encoder || !strcmp(config.options().bios(), "fr-decoder") || !strcmp(config.options().bios(), "fr-decoder-pipe")))
		m_cpu->instruction_observer_cb().set([this,trace_encoder](offs_t address, u16 opcode) {
			if (m_phase != (trace_encoder ? 74002U : 72017U) || m_rom4_codec_replay) return;
			if (++m_rom4_fr_trace_count <= 200000) trace_rom4_fr_state("execute", address, opcode);
		});
	if (config.options().verbose() && (!strcmp(config.options().bios(), "efr-encoder") || !strcmp(config.options().bios(), "efr-encoder-pipe")))
		m_cpu->instruction_observer_cb().set([this](offs_t address, u16 opcode) {
			if (m_phase == 304000 && ++m_rom4_fr_trace_count <= 8192)
				trace_rom4_fr_state("efr_reset_execute", address, opcode);
			else if (m_phase == 304001 && !m_rom4_codec_replay && ++m_rom4_fr_trace_count <= 200000)
				trace_rom4_fr_state("efr_frame_execute", address, opcode);
		});
	if (!strcmp(config.options().bios(), "efr-loss-observe") || !strcmp(config.options().bios(), "efr-loss-pipe"))
		m_cpu->instruction_observer_cb().set([this](offs_t address,u16 opcode) {
			observe_rom4_efr_state(address,opcode);
			if (machine().options().verbose() && m_phase == 302009 && !m_rom4_codec_replay && ++m_rom4_fr_trace_count <= 200000)
				trace_rom4_fr_state("efr_loss_execute",address,opcode);
		});
	if (!strcmp(config.options().bios(), "efr-home-errors") || !strcmp(config.options().bios(), "efr-herr-pipe"))
		m_cpu->instruction_observer_cb().set([this](offs_t address, u16 opcode) {
			// Original recovery may edit an input parameter before the full DHF
			// comparison. Observe it; never repair the parameter or reset flag.
			if (!m_rom4_codec_replay && (m_phase == 302002 || m_phase == 302025 || m_phase == 302048))
			{
				auto &data = m_cpu->space(AS_DATA);
				if (address == 0xc8dc || address == 0xc930)
					osd_printf_info("ROM4 homing recovery parameter: frame=%u pc=%04x actual=%04x table=%04x\n",
						m_phase - 302001, unsigned(address), data.read_word(0x1371), data.read_word(0xeb59));
				if (address == 0x28dc && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1371 && data.read_word(0x1371) == 0x000b)
				{
					expect(opcode == 0x9e0f, "Original recovery store is SACCD A, *AR2, BLEQ");
					osd_printf_info("ROM4 homing recovery store: frame=%u a=%010llx b=%010llx parameter=%04x\n",
						m_phase - 302001, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
						(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), data.read_word(0x1371));
				}
			}
			if (address == 0xc941 && !m_rom4_codec_replay && m_phase >= 302001 && m_phase <= 302049)
			{
				expect(opcode == 0x4a07, "Homing reset observation is the original routine entry");
				++m_rom4_homing_resets;
			}
		});
	m_cpu->set_addrmap(AS_PROGRAM, &tms320c54x_test_state::rom4_program_map);
	m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::rom4_data_map);
}

} // namespace c54x_test
