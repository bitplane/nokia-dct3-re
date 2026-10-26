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
	if (!strcmp(machine().options().bios(), "idle-modes")) { start_pipeline_idle1(); return; }
	if (!strcmp(machine().options().bios(), "pop-irq")) { start_pipeline_mmr_pop(); return; }
	if (!strcmp(machine().options().bios(), "pop-irq-edge")) { start_interrupt_pop_edge(); return; }
	if (!strcmp(machine().options().bios(), "clock-mvdk-edge")) { start_clkmd_move_edge(); return; }
	if (!strcmp(machine().options().bios(), "mvpd-extended")) { start_extended_mvpd(); return; }
	if (!strcmp(machine().options().bios(), "mvpd-ext-edge")) { start_extended_mvpd_edge(); return; }
	if (!strcmp(machine().options().bios(), "mvpd-controls")) { start_pipeline_mvpd(); return; }
	if (!strcmp(machine().options().bios(), "literal-guard")) { start_literal_guard(); return; }
	if (!strcmp(machine().options().bios(), "rpt-writa") || !strcmp(machine().options().bios(), "rpt-writa16")) { start_repeated_writa(); return; }
	if (!strcmp(machine().options().bios(), "bcd-arith-edge")) { start_branch_arithmetic_edge(); return; }
	if (!strcmp(machine().options().bios(), "bcd-arithmetic")) { start_branch_arithmetic(); return; }
	if (!strcmp(machine().options().bios(), "mar-circular")) { start_circular_mar(); return; }
	if (!strcmp(machine().options().bios(), "tc-return")) { start_tc_return(); return; }
	if (!strcmp(machine().options().bios(), "rcd")) { start_delayed_condition_return(); return; }
	if (!strcmp(machine().options().bios(), "bitt-retd")) { start_bitt_retd(); return; }
	if (!strcmp(machine().options().bios(), "bitt-ctrl")) { start_delay_bitt(); return; }
	if (!strcmp(machine().options().bios(), "mvdd-follow")) { start_mvdd_follow(); return; }
	if (!strcmp(machine().options().bios(), "bd-read-a")) { start_bd_read_a(); return; }
	if (!strcmp(machine().options().bios(), "bitt")) { start_bitt(); return; }
	if (!strcmp(machine().options().bios(), "rpt-store-edge")) { start_repeat_store_edge(); return; }
	if (!strcmp(machine().options().bios(), "rpt-store")) { start_repeat_store(); return; }
	if (!strcmp(machine().options().bios(), "rptz")) { start_rptz(); return; }
	if (!strcmp(machine().options().bios(), "sat")) { start_sat(); return; }
	if (!strcmp(machine().options().bios(), "rotate")) { start_rotate(); return; }
	if (!strcmp(machine().options().bios(), "addm-ar")) { start_ar_addm(); return; }
	if (!strcmp(machine().options().bios(), "addm-ar-short")) { start_ar_addm(); return; }
	if (!strcmp(machine().options().bios(), "addm-brc")) { start_ar_addm(); return; }
	if (!strcmp(machine().options().bios(), "addm-t")) { start_ar_addm(); return; }
	if (!strcmp(machine().options().bios(), "addm-brc-loop")) { start_addm_brc_loop(); return; }
	if (!strcmp(machine().options().bios(), "addm-ar-load")) { start_ar_addm_load(); return; }
	if (!strcmp(machine().options().bios(), "std-swwsr")) { start_standard_swwsr_test(); return; }
	if (standard_bus_io()) { start_bus_io_test(); return; }
	if (!strcmp(machine().options().bios(), "std-stm")) { start_staged_bus_stm(); return; }
	if (!strcmp(machine().options().bios(), "clock-stm")) { start_staged_bus_stm(); return; }
	if (!strcmp(machine().options().bios(), "serial-stm")) { start_staged_bus_stm(); return; }
	if (status_bitfield()) { start_pipeline_mapped_bitfield(); return; }
	if (!strcmp(machine().options().bios(), "stlm-bsp")) { start_stlm_bsp(); return; }
	if (!strcmp(machine().options().bios(), "stlm-bsp-edge")) { start_stlm_bsp_edge(); return; }
	if (!strcmp(machine().options().bios(), "rptb-exit") || !strcmp(machine().options().bios(), "rptb-exit-delay")) { start_block_exit(); return; }
	if (!strcmp(machine().options().bios(), "reada-brc") || standard_reada_brc()) { start_reada_brc(); return; }
	if (!strcmp(machine().options().bios(), "reada-brc-edge")) { start_reada_brc_edge(); return; }
	if (!strcmp(machine().options().bios(), "st1-rules")) { check_st1_memory_rules(); return; }
	if (!strcmp(machine().options().bios(), "st1-bits")) { start_pipeline_status_bit(); return; }
	if (!strcmp(machine().options().bios(), "stlm-st1")) { start_stlm_status(); return; }
	if (!strcmp(machine().options().bios(), "stm-st1")) { start_stlm_status(); return; }
	if (!strcmp(machine().options().bios(), "stm-st1-edge")) { start_stm_status_edge(); return; }
	if (!strcmp(machine().options().bios(), "stlm-st1-edge")) { start_stlm_status_edge(); return; }
	if (!strcmp(machine().options().bios(), "stlm-st1-irq")) { start_mvmd_status_irq(); return; }
	if (!strcmp(machine().options().bios(), "shift-periph") || shifted_bsp_fixture() || shifted_unowned_fixture()) { start_shifted_peripheral(); return; }
	if (!strcmp(machine().options().bios(), "shift-per-edge") || !strcmp(machine().options().bios(), "shift-bk-edge")) { start_shifted_peripheral_edge(); return; }
	if (!strcmp(machine().options().bios(), "stlm-cpl") || !strcmp(machine().options().bios(), "stlm-cmpt")) { start_pipeline_cpl(); return; }
	if (!strcmp(machine().options().bios(), "stm-cpl") || !strcmp(machine().options().bios(), "stm-cmpt")) { start_pipeline_cpl(); return; }
	if (!strcmp(machine().options().bios(), "stlm-imr")) { start_pipeline_imr_store(); return; }
	if (!strcmp(machine().options().bios(), "pmst-map")) { start_pipeline_pmst(); return; }
	if (!strcmp(machine().options().bios(), "pmst-pop")) { start_pipeline_pmst(); return; }
	if (!strcmp(machine().options().bios(), "pmst-popd")) { start_pipeline_pmst(); return; }
	if (!strcmp(machine().options().bios(), "pmst-prefetch")) { start_pipeline_pmst(); return; }
	if (!strcmp(machine().options().bios(), "pmst-iptr")) { start_pipeline_pmst(); return; }
	if (!strcmp(machine().options().bios(), "pop-repeat")) { start_pipeline_pmst(); return; }
	if (!strcmp(machine().options().bios(), "stack-irq")) { start_stack_irq_order(); return; }
	if (!strcmp(machine().options().bios(), "stack-irq-return")) { start_stack_irq_order(); return; }
	if (!strcmp(machine().options().bios(), "mcbsp-control")) { start_mcbsp_control_sequence(); return; }
	if (!strcmp(machine().options().bios(), "rmw-long-periph")) { start_pipeline_long_rmw(); return; }
	if (!strcmp(machine().options().bios(), "xorm-absolute")) { start_pipeline_long_rmw(); return; }
	if (!strcmp(machine().options().bios(), "st-long-periph")) { start_pipeline_long_immediate_store(); return; }
	if (!strcmp(machine().options().bios(), "st-mvmm")) { start_pipeline_store_mvmm(); return; }
	if (!strcmp(machine().options().bios(), "st-acc-mvmm")) { start_pipeline_store_mvmm(); return; }
	if (!strcmp(machine().options().bios(), "push-mvmm-sp")) { start_push_mvmm(); return; }
	if (!strcmp(machine().options().bios(), "ldm-ar-age")) { start_pipeline_mapped_load_age(); return; }
	if (!strcmp(machine().options().bios(), "mapped-load-age")) { start_pipeline_mapped_load_age(); return; }
	if (!strcmp(machine().options().bios(), "shift16-load-age")) { start_pipeline_mapped_load_age(); return; }
	if (!strcmp(machine().options().bios(), "addsub-ar-age")) { start_pipeline_mapped_load_age(); return; }
	if (!strcmp(machine().options().bios(), "preinc-st")) { start_preincrement_store(); return; }
	if (!strcmp(machine().options().bios(), "move-brc")) { start_move_brc(); return; }
	if (!strcmp(machine().options().bios(), "move-brc-delay")) { start_move_brc(); return; }
	if (!strcmp(machine().options().bios(), "move-brc-edge")) { start_move_brc_edge(); return; }
	if (!strcmp(machine().options().bios(), "move-rpt-owner")) { start_move_repeat_owner(); return; }
	if (!strcmp(machine().options().bios(), "block-xc-irq") || !strcmp(machine().options().bios(), "block-xc-end-irq")) { start_block_xc_irq(); return; }
	if (!strcmp(machine().options().bios(), "block-xc")) { start_block_xc(); return; }
	if (!strcmp(machine().options().bios(), "block-xc-edge")) { start_block_xc_edge(); return; }
	if (!strcmp(machine().options().bios(), "stm-brc")) { start_mvmd_status_block(); return; }
	if (!strcmp(machine().options().bios(), "pop-brc")) { start_mvmd_status_block(); return; }
	if (!strcmp(machine().options().bios(), "preinc-store")) { start_preincrement_store(); return; }
	if (!strcmp(machine().options().bios(), "ldu-periph")) { start_ldu_peripheral(); return; }
	if (!strcmp(machine().options().bios(), "ldu-imr")) { start_ldu_imr(); return; }
	if (!strcmp(machine().options().bios(), "ldu-st1")) { start_ldu_status(); return; }
	if (!strcmp(machine().options().bios(), "memory-shift16")) { start_pipeline_memory_arithmetic(); return; }
	if (!strcmp(machine().options().bios(), "mvmd-status-irq")) { start_mvmd_status_irq(); return; }
	if (!strcmp(machine().options().bios(), "mvmd-block")) { start_mvmd_status_block(); return; }
	if (!strcmp(machine().options().bios(), "mvmd-status-alu")) { start_mvmd_status_alu(); return; }
	if (!strcmp(machine().options().bios(), "mvmd-cpl") || !strcmp(machine().options().bios(), "mvmd-cmpt")) { start_pipeline_cpl(); return; }
	if (!strcmp(machine().options().bios(), "mpy-delay") || !strcmp(machine().options().bios(), "mac-delay") || !strcmp(machine().options().bios(), "maca-delay")) { start_delay_multiply(); return; }
	if (!strcmp(machine().options().bios(), "mpyr-delay")) { start_delay_multiply(); return; }
	if (!strcmp(machine().options().bios(), "bd-maca-long")) { start_delay_multiply(); return; }
	if (!strcmp(machine().options().bios(), "bd-maca-edge")) { start_delay_store_edge(); return; }
	if (!strcmp(machine().options().bios(), "square-delay")) { start_delay_multiply(); return; }
	if (!strcmp(machine().options().bios(), "squr-delay")) { start_delay_multiply(); return; }
	if (!strcmp(machine().options().bios(), "delay-copy")) { start_delay_copy(); return; }
	if (!strcmp(machine().options().bios(), "delay-copy-ctrl")) { start_delay_copy(); return; }
	if (!strcmp(machine().options().bios(), "mpy-delay-edge") || !strcmp(machine().options().bios(), "mac-delay-edge")) { start_delay_store_edge(); return; }
	if (!strcmp(machine().options().bios(), "mpyr-delay-edge")) { start_delay_store_edge(); return; }
	if (!strcmp(machine().options().bios(), "move-t")) { start_t_register_move(); return; }
	if (!strcmp(machine().options().bios(), "st-delay-edge")) { start_delay_store_edge(); return; }
	if (!strcmp(machine().options().bios(), "store-delay")) { start_delay_store(); return; }
	if (!strcmp(machine().options().bios(), "mvkd-sp")) { start_mvkd_sp(); return; }
	if (!strcmp(machine().options().bios(), "add-bk") || !strcmp(machine().options().bios(), "add-absolute") || !strcmp(machine().options().bios(), "add-sp")) { start_pipeline_absolute_add(); return; }
	if (!strcmp(machine().options().bios(), "instr-observer")) { start_instruction_observer(); return; }
	if (!strcmp(machine().options().bios(), "arith-bk") || !strcmp(machine().options().bios(), "arith-ar")) { start_pipeline_register_arithmetic(); return; }
	if (!strcmp(machine().options().bios(), "stlm-bk")) { start_stlm_bk(); return; }
	if (!strcmp(machine().options().bios(), "store-bypass") || !strcmp(machine().options().bios(), "xmem-store-bp") || !strcmp(machine().options().bios(), "asm-store-bp")) { start_store_bypass(); return; }
	if (!strcmp(machine().options().bios(), "software-intr")) { start_software_intr(); return; }
	if (!strcmp(machine().options().bios(), "rptb-br-edges")) { start_block_branch_edge(); return; }
	if (!strcmp(machine().options().bios(), "signed-branch")) { start_pipeline_signed_branch(); return; }
	if (!strcmp(machine().options().bios(), "tc-branch") || !strcmp(machine().options().bios(), "carry-branch")) { start_pipeline_tc_branch(); return; }
	if (!strcmp(machine().options().bios(), "bio-branch")) { start_pipeline_bio_branch(); return; }
	if (!strcmp(machine().options().bios(), "plain-branch")) { start_pipeline_plain_branch(); return; }
	if (!strcmp(machine().options().bios(), "rptb-branch")) { start_block_branch(); return; }
	if (!strcmp(machine().options().bios(), "mpy-mem-imm")) { start_memory_immediate_multiply(); return; }
	if (!strcmp(machine().options().bios(), "mac-map-imm")) { start_mapped_immediate_mac(); return; }
	if (!strcmp(machine().options().bios(), "mpy-abs-edge")) { start_source_alias_case(); return; }
	if (!strcmp(machine().options().bios(), "mac-abs-edge")) { start_source_alias_case(); return; }
	if (!strcmp(machine().options().bios(), "delay-abs-edge")) { start_source_alias_case(); return; }
	if (!strcmp(machine().options().bios(), "mem-t-abs-edge")) { start_source_alias_case(); return; }
	if (!strcmp(machine().options().bios(), "load-abs-edge")) { start_source_alias_case(); return; }
	if (!strcmp(machine().options().bios(), "pmac-abs-edge")) { start_source_alias_case(); return; }
	if (!strcmp(machine().options().bios(), "rmw-abs-edge")) { start_source_alias_case(); return; }
	if (!strcmp(machine().options().bios(), "logic-abs-edge")) { start_source_alias_case(); return; }
	if (!strcmp(machine().options().bios(), "arith-abs-edge")) { start_source_alias_case(); return; }
	if (!strcmp(machine().options().bios(), "long-abs-edge")) { start_source_alias_case(); return; }
	if (!strcmp(machine().options().bios(), "source-domain")) { start_source_domains(); return; }
	if (!strcmp(machine().options().bios(), "mpy-mod3-edge")) { start_short_read_mod3_edge(); return; }
	if (!strcmp(machine().options().bios(), "mmr-long-edge")) { start_mmr_long_edge(); return; }
	if (!strcmp(machine().options().bios(), "rpt-xfer-edge")) { start_mmr_long_edge(); return; }
	if (!strcmp(machine().options().bios(), "subc-mod3-edge")) { start_short_read_mod3_edge(256); return; }
	if (!strcmp(machine().options().bios(), "load-mod3-edge")) { start_short_read_mod3_edge(768); return; }
	if (!strcmp(machine().options().bios(), "bitf-mod3-edge")) { start_short_read_mod3_edge(10496); return; }
	if (!strcmp(machine().options().bios(), "arith-mod3-edge")) { start_short_read_mod3_edge(11008); return; }
	if (!strcmp(machine().options().bios(), "mul-mod3-edge")) { start_short_read_mod3_edge(46848); return; }
	if (!strcmp(machine().options().bios(), "pmac-mod3-edge")) { start_short_read_mod3_edge(58112); return; }
	if (!strcmp(machine().options().bios(), "delay-mod3-edge")) { start_short_read_mod3_edge(58624); return; }
	if (!strcmp(machine().options().bios(), "store-observe")) { start_pipeline_store_case(); return; }
	if (!strcmp(machine().options().bios(), "push-observe")) { start_pipeline_xpc_push(); return; }
	if (!strcmp(machine().options().bios(), "mul-t-sources")) { start_pipeline_memory_multiply(); return; }
	if (!strcmp(machine().options().bios(), "squr-smem")) { check_control_disassembler(); start_pipeline_memory_multiply(); return; }
	if (!strcmp(machine().options().bios(), "memory-alu")) { start_pipeline_memory_arithmetic(); return; }
	if (!strcmp(machine().options().bios(), "shifted-alu") || !strcmp(machine().options().bios(), "shifted-bk")) { start_pipeline_shifted_arithmetic(); return; }
	if (!strcmp(machine().options().bios(), "indirect-load")) { start_pipeline_indirect_case(); return; }
	if (!strcmp(machine().options().bios(), "ldu-shift16")) { start_pipeline_offset_load(12672); return; }
	if (!strcmp(machine().options().bios(), "indirect-dld")) { start_pipeline_extended_dst(); return; }
	if (!strcmp(machine().options().bios(), "load-variable")) { start_pipeline_offset_load(127872); return; }
	if (!strcmp(machine().options().bios(), "rpt-subc-st")) { start_pipeline_memory_repeat(); return; }
	if (!strcmp(machine().options().bios(), "mpy-t-imm") || !strcmp(machine().options().bios(), "mac-t-imm")) { start_pipeline_immediate_multiply(); return; }
	if (!strcmp(machine().options().bios(), "mar-index")) { start_indexed_mar(); return; }
	if (!strcmp(machine().options().bios(), "mar-reverse")) { start_indexed_mar(); return; }
	if (!strcmp(machine().options().bios(), "minmax") || !strcmp(machine().options().bios(), "squr")) { start_minmax(); return; }
	if (!strcmp(machine().options().bios(), "store-index") || !strcmp(machine().options().bios(), "imm-index") || !strcmp(machine().options().bios(), "store-circular") || !strcmp(machine().options().bios(), "asm-circular")) { start_indexed_store(); return; }
	if (!strcmp(machine().options().bios(), "poll-interrupt")) { start_poll_interrupt(); return; }
	if (!strcmp(machine().options().bios(), "compare-delay")) { start_delay_compare(); return; }
	if (!strcmp(machine().options().bios(), "cmp-delay-edge")) { start_delay_compare_edge(); return; }
	if (!strcmp(machine().options().bios(), "asm-load") || !strcmp(machine().options().bios(), "asm-consumer")) { start_asm_load(); return; }
	if (!strcmp(machine().options().bios(), "asm-immediate")) { start_asm_load(); return; }
	if (!strcmp(machine().options().bios(), "asm-edge")) { start_asm_edge(); return; }
	if (!strcmp(machine().options().bios(), "t-load-control")) { start_pipeline_memory_multiply(165888); return; }
	if (!strcmp(machine().options().bios(), "ts-alu") || !strcmp(machine().options().bios(), "ts-alu-producer")) { start_ts_arithmetic(); return; }
	if (!strcmp(machine().options().bios(), "ts-alu-edge")) { start_ts_arithmetic_edge(); return; }
	if (!strcmp(machine().options().bios(), "st-t") || !strcmp(machine().options().bios(), "st-t-consumer")) { start_t_store(); return; }
	if (!strcmp(machine().options().bios(), "st-t-edge")) { start_t_store_edge(); return; }
	if (!strcmp(machine().options().bios(), "asm-mapped")) { start_asm_mapped(); return; }
	if (!strcmp(machine().options().bios(), "rptb-single")) { start_block_single(); return; }
	if (!strcmp(machine().options().bios(), "rptb-memory")) { start_block_single(); return; }
	if (!strcmp(machine().options().bios(), "rptb-rpt-edge")) { start_block_single_edge(); return; }
	if (!strcmp(machine().options().bios(), "rptb-page")) { start_block_page(); return; }
	if (!strcmp(machine().options().bios(), "rptb-fcall") || !strcmp(machine().options().bios(), "rptb-fcalld")) { start_block_far_call(); return; }
	if (!strcmp(machine().options().bios(), "rptb-fcall-edge")) { start_block_far_call_edge(); return; }
	if (!strcmp(machine().options().bios(), "rptb-back")) { start_block_back(); return; }
	if (!strcmp(machine().options().bios(), "load-delay") || !strcmp(machine().options().bios(), "add-delay") || !strcmp(machine().options().bios(), "dld-delay")) { start_delay_load(); return; }
	if (!strcmp(machine().options().bios(), "dld-delay-edge")) { start_delay_store_edge(); return; }
	if (!strcmp(machine().options().bios(), "st-dagen")) { start_constant_dagen(); return; }
	if (!strcmp(machine().options().bios(), "asm-delay")) { start_delay_asm(); return; }
	if (!strcmp(machine().options().bios(), "asm-delay-edge")) { start_delay_store_edge(); return; }
	if (!strcmp(machine().options().bios(), "shift-delay")) { start_delay_load(); return; }
	if (!strcmp(machine().options().bios(), "shift-alu-delay")) { start_delay_load(); return; }
	if (!strcmp(machine().options().bios(), "shift-delay-edge")) { start_shift_delay_edge(); return; }
	if (!strcmp(machine().options().bios(), "ld-delay-edge")) { start_delay_load_edge(); return; }
	if (!strcmp(machine().options().bios(), "ld-ar0-index") || !strcmp(machine().options().bios(), "ld-circular")) { start_indexed_load(); return; }
	if (!strcmp(machine().options().bios(), "rptbd")) { start_rptbd(); return; }
	if (!strcmp(machine().options().bios(), "rptbd-square")) { start_rptbd_square(); return; }
	if (!strcmp(machine().options().bios(), "rptbd-squr")) { start_rptbd_square(); return; }
	if (!strcmp(machine().options().bios(), "rptbd-shift-st")) { start_rptbd_shift_store(); return; }
	if (!strcmp(machine().options().bios(), "rptbd-shift-edge")) { start_rptbd_shift_store_edge(); return; }
	if (!strcmp(machine().options().bios(), "rpt-smem")) { start_basic_repeat(); return; }
	if (!strcmp(machine().options().bios(), "rpt-smem-edge")) { start_basic_repeat_edge(); return; }
	if (!strcmp(machine().options().bios(), "rpt-ld")) { start_repeat_load(); return; }
	if (!strcmp(machine().options().bios(), "rpt-sfta")) { start_repeat_load(); return; }
	if (!strcmp(machine().options().bios(), "rpt-ld-block")) { start_repeat_load(198656); return; }
	if (!strcmp(machine().options().bios(), "stlm-pmst")) { start_stlm_pmst(); return; }
	if (!strcmp(machine().options().bios(), "stlm-t")) { start_pipeline_stlm_t(); return; }
	if (!strcmp(machine().options().bios(), "ov-branch")) { start_atomic_overflow_branch(0); return; }
	if (!strcmp(machine().options().bios(), "ov-branch-pipe")) { start_staged_overflow(); return; }
	if (!strcmp(machine().options().bios(), "cond-reg-store")) { check_control_disassembler(); start_conditional_register_store(0); return; }
	if (!strcmp(machine().options().bios(), "cond-store") || !strcmp(machine().options().bios(), "cond-store-edge")) { start_staged_conditional_store(); return; }
	if (!strcmp(machine().options().bios(), "cond-store-ar")) { start_staged_conditional_store(); return; }
	if (!strcmp(machine().options().bios(), "atomic-ret")) { start_atomic_conditional_return(0); return; }
	if (!strcmp(machine().options().bios(), "xorm")) { check_control_disassembler(); start_atomic_xorm(0); return; }
	if (!strcmp(machine().options().bios(), "xc-status")) { start_xc_status(0); return; }
	if (!strcmp(machine().options().bios(), "xc-latency")) { start_xc_latency(0); return; }
	if (!strcmp(machine().options().bios(), "xc-barrier")) { start_xc_barrier(0); return; }
	if (!strcmp(machine().options().bios(), "dual-sub-native")) { start_native_dual_sub(0); return; }
	if (!strcmp(machine().options().bios(), "parallel-sub")) { start_parallel_sub_fraction(0); return; }
	if (!strcmp(machine().options().bios(), "saccd")) { start_saccd_case(0); return; }
	if (!strcmp(machine().options().bios(), "stlm-pmst-edge")) { start_stlm_pmst_edge(); return; }
	if (!strcmp(machine().options().bios(), "pshd-long")) { start_extended_push(); return; }
	if (!strcmp(machine().options().bios(), "pshd-long-map")) { start_extended_push(96000); return; }
	if (!strcmp(machine().options().bios(), "pshd-long-edge")) { start_extended_push_edge(); return; }
	if (!strcmp(machine().options().bios(), "rpt-ld-edge")) { start_repeat_load_edge(); return; }
	if (!strcmp(machine().options().bios(), "rptbd-irq")) { start_rptbd(512); return; }
	if (!strcmp(machine().options().bios(), "mvdk-mapped") || !strcmp(machine().options().bios(), "mvdk-acc") || !strcmp(machine().options().bios(), "clock-mvdk")) { start_mvdk_mapped(); return; }
	if (!strcmp(machine().options().bios(), "mvdk-producer") || !strcmp(machine().options().bios(), "mvdk-acc-prod")) { start_mvdk_producer(); return; }
	if (!strcmp(machine().options().bios(), "mvdk-map-edge")) { start_mvdk_mapped_edge(); return; }
	if (!strcmp(machine().options().bios(), "rptbd-edge")) { start_rptbd_edge(); return; }
	if (!strcmp(machine().options().bios(), "mvdd-periph")) { start_mvdd_peripheral(); return; }
	if (!strcmp(machine().options().bios(), "mvdd-source") || !strcmp(machine().options().bios(), "mvdd-source-rpt") || !strcmp(machine().options().bios(), "mvdd-source-edge")) { start_mvdd_source(); return; }
	if (!strcmp(machine().options().bios(), "mvmd-t") || !strcmp(machine().options().bios(), "mvmd-bk")) { start_mvmd_word_register(); return; }
	if (!strcmp(machine().options().bios(), "mvmd-acc") || !strcmp(machine().options().bios(), "mvmd-st1")) { start_mvmd_accumulator(); return; }
	if (!strcmp(machine().options().bios(), "subs")) { start_pipeline_memory_arithmetic(); return; }
	if (!strcmp(machine().options().bios(), "adds")) { start_pipeline_memory_arithmetic(); return; }
	if (!strcmp(machine().options().bios(), "mpy-dual") || !strcmp(machine().options().bios(), "mpy-dual-edge") || !strcmp(machine().options().bios(), "alu-dual") || !strcmp(machine().options().bios(), "alu-dual-edge")) { start_dual_multiply(); return; }
	if (!strcmp(machine().options().bios(), "alu-map") || !strcmp(machine().options().bios(), "alu-map-edge") || !strcmp(machine().options().bios(), "alu-map-irq") || !strcmp(machine().options().bios(), "alu-low-bus")) { start_mapped_dual(); return; }
	if (!strcmp(machine().options().bios(), "macsu-dual") || !strcmp(machine().options().bios(), "macsu-dual-edge")) { start_dual_multiply(); return; }
	if (!strcmp(machine().options().bios(), "mac-dual") || !strcmp(machine().options().bios(), "mac-dual-edge")) { start_dual_multiply(); return; }
	if (!strcmp(machine().options().bios(), "xc-signed")) { start_signed_xc(); return; }
	if (!strcmp(machine().options().bios(), "xc-controls")) { start_pipeline_xc_irq(); return; }
	if (!strcmp(machine().options().bios(), "xc-ret")) { start_xc_return(); return; }
	if (!strcmp(machine().options().bios(), "norm") || !strcmp(machine().options().bios(), "norm-edge") || !strcmp(machine().options().bios(), "exp-norm")) { start_normalization(); return; }
	if (!strcmp(machine().options().bios(), "drsub") || !strcmp(machine().options().bios(), "long-pair")) { start_pipeline_long_pair(); return; }
	if (!strcmp(machine().options().bios(), "xmem-store")) { start_xmem_store(); return; }
	if (!strcmp(machine().options().bios(), "xmem-load") || !strcmp(machine().options().bios(), "xmem-alu")) { start_xmem_load(); return; }
	if (!strcmp(machine().options().bios(), "mvdd")) { start_mvdd(); return; }
	if (!strcmp(machine().options().bios(), "mvdd-repeat")) { start_mvdd(); return; }
	if (!strcmp(machine().options().bios(), "logic-absolute")) { start_absolute_logic(); return; }
	if (!strcmp(machine().options().bios(), "popd-mapped") || !strcmp(machine().options().bios(), "popd-repeat")) { start_popd_mapped(); return; }
	if (!strcmp(machine().options().bios(), "popd-edges")) { start_popd_edges(); return; }
	if (!strcmp(machine().options().bios(), "popd-ram")) { start_pipeline_data_pop(); return; }
	if (!strcmp(machine().options().bios(), "bacc") || !strcmp(machine().options().bios(), "fbacc") || !strcmp(machine().options().bios(), "bacc-producer") || !strcmp(machine().options().bios(), "fbacc-producer")) { start_accumulator_branch(); return; }
	if (!strcmp(machine().options().bios(), "logic-offset")) { start_offset_logic(); return; }
	if (!strcmp(machine().options().bios(), "logic-edges")) { start_offset_logic_edges(); return; }
	if (!strcmp(machine().options().bios(), "logic-short")) { start_pipeline_memory_logic(); return; }
	if (!strcmp(machine().options().bios(), "logic-peripheral")) { start_pipeline_peripheral_logic(); return; }
	if (!strcmp(machine().options().bios(), "load-offset")) { start_pipeline_offset_load(); return; }
	if (!strcmp(machine().options().bios(), "write-a-edge")) { start_write_a_edge(); return; }
	if (!strcmp(machine().options().bios(), "write-a")) { start_write_a(); return; }
	if (!strcmp(machine().options().bios(), "mvdp")) { start_mvdp(); return; }
	if (!strcmp(machine().options().bios(), "mvdp-pair")) { start_mvdp_pair(); return; }
	if (!strcmp(machine().options().bios(), "rpt-mvdp")) { start_repeat_mvdp(); return; }
	if (!strcmp(machine().options().bios(), "rpt-mvkd")) { start_repeat_mvkd(); return; }
	if (!strcmp(machine().options().bios(), "rpt-mvkd-edge")) { start_repeat_mvkd_edge(); return; }
	if (!strcmp(machine().options().bios(), "rpt-mvdp-irq")) { start_repeat_mvdp_irq(); return; }
	if (!strcmp(machine().options().bios(), "banzd-long")) { start_pipeline_extended_banz(); return; }
	if (!strcmp(machine().options().bios(), "ret-load")) { start_return_load(); return; }
	if (!strcmp(machine().options().bios(), "conditional-ret")) { start_conditional_call(); return; }
	if (!strcmp(machine().options().bios(), "signed-call")) { start_conditional_call(); return; }
	if (!strcmp(machine().options().bios(), "tc-call")) { start_conditional_call(); return; }
	if (!strcmp(machine().options().bios(), "conditional-call")) { start_conditional_call(); return; }
	if (!strcmp(machine().options().bios(), "conditional-cd")) { start_delayed_conditional_call(); return; }
	if (!strcmp(machine().options().bios(), "tc-delay-call")) { start_delayed_conditional_call(); return; }
	if (!strcmp(machine().options().bios(), "cond-cd-edge")) { start_delayed_conditional_call_edge(); return; }
	if (!strcmp(machine().options().bios(), "rptb-ccd")) { start_block_conditional_call(); return; }
	if (!strcmp(machine().options().bios(), "repeat-isr")) { start_saved_repeat(); return; }
	if (!strcmp(machine().options().bios(), "repeat-stlm")) { start_saved_repeat(); return; }
	if (!strcmp(machine().options().bios(), "repeat-body-isr")) { start_body_repeat(); return; }
	if (!strcmp(machine().options().bios(), "repeat-context")) { start_repeat_context(); return; }
	if (!strcmp(machine().options().bios(), "repeat-pmst")) { start_repeat_context(); return; }
	if (!strcmp(machine().options().bios(), "repeat-popd")) { start_repeat_context(); return; }
	if (!strcmp(machine().options().bios(), "repeat-rearm")) { start_repeat_rearm(); return; }
	if (!strcmp(machine().options().bios(), "rearm-edge")) { start_repeat_rearm(); return; }
	if (!strcmp(machine().options().bios(), "rptb-bcd") || !strcmp(machine().options().bios(), "rptb-bd") || !strcmp(machine().options().bios(), "rptb-banz") || !strcmp(machine().options().bios(), "rptb-tc-delay")) { start_block_bcd(); return; }
	if (!strcmp(machine().options().bios(), "asm-store") || !strcmp(machine().options().bios(), "asm-store-ld")) { start_asm_store(); return; }
	if (!strcmp(machine().options().bios(), "push-pop-same")) { start_push_pop_same(); return; }
	if (!strcmp(machine().options().bios(), "fcala-producer")) { start_accumulator_call(); return; }
	if (!strcmp(machine().options().bios(), "fcala-delay")) { start_fcala_delay(); return; }
	if (!strcmp(machine().options().bios(), "fcala")) { start_accumulator_call(); return; }
	if (!strcmp(machine().options().bios(), "cala-producer")) { start_accumulator_call(); return; }
	if (!strcmp(machine().options().bios(), "cala")) { start_accumulator_call(); return; }
	if (!strcmp(machine().options().bios(), "far-atomic")) { m_cpu->set_pipeline_frontend(false); start_far_case(0); return; }
	if (!strcmp(machine().options().bios(), "bitf-peripheral")) { start_pipeline_bitfield_ar7(); return; }
	if (!strcmp(machine().options().bios(), "t-peripheral")) { start_t_peripheral(); return; }
	if (!strcmp(machine().options().bios(), "st-t-long") || !strcmp(machine().options().bios(), "st-t-long-edge")) { start_extended_t_store(); return; }
	if (!strcmp(machine().options().bios(), "st-t-branch")) { start_t_store_branch(); return; }
	if (!strcmp(machine().options().bios(), "portr-circular")) { start_circular_portr(); return; }
	if (!strcmp(machine().options().bios(), "portr-block")) { start_circular_portr_block(); return; }
	if (!strncmp(machine().options().bios(), "mac-family", 10) || !strncmp(machine().options().bios(), "stmac", 5)) { start_dual_multiply(); return; }
	if (!strcmp(machine().options().bios(), "sqdst") || !strcmp(machine().options().bios(), "sqdst-edge")) { start_sqdst(); return; }
	if (!strcmp(machine().options().bios(), "stld") || !strcmp(machine().options().bios(), "stld-edge") || !strcmp(machine().options().bios(), "stld-rpt-edge") || !strcmp(machine().options().bios(), "st-par-alu") || !strcmp(machine().options().bios(), "st-par-edge")) { start_parallel_load(); return; }
	if (!strcmp(machine().options().bios(), "stld-map")) { start_parallel_load(); return; }
	if (!strcmp(machine().options().bios(), "stld-bsp")) { start_parallel_load(); return; }
	if (!strcmp(machine().options().bios(), "stld-periph")) { start_parallel_load(); return; }
	if (!strcmp(machine().options().bios(), "st-par-mpy") || !strcmp(machine().options().bios(), "st-mpy-edge")) { start_parallel_load(); return; }
	if (!strcmp(machine().options().bios(), "portr-bypass")) { start_portr_bypass(); return; }
	if (!strcmp(machine().options().bios(), "mvdk-circular") || !strcmp(machine().options().bios(), "mvdk-circ-edge") || !strcmp(machine().options().bios(), "mvdk-index") || !strcmp(machine().options().bios(), "mvdk-index-edge")) { start_circular_move(); return; }
	if (!strcmp(machine().options().bios(), "mvmd-repeat") || !strcmp(machine().options().bios(), "mvmd-rptz")) { start_repeated_mvmd(); return; }
	if (!strcmp(machine().options().bios(), "rptbd-t-load")) { start_rptbd_t_load(); return; }
	if (!strcmp(machine().options().bios(), "banzd-t-load")) { start_rptbd_t_load(); return; }
	if (!strcmp(machine().options().bios(), "ctrl-t-load")) { start_rptbd_t_load(); return; }
	if (!strcmp(machine().options().bios(), "sqacc-repeat")) { start_poly(); return; }
	if (!strcmp(machine().options().bios(), "tmac-repeat")) { start_poly(); return; }
	if (!strcmp(machine().options().bios(), "sqacc-rpt-edge")) { start_repeat_load_edge(); return; }
	if (!strcmp(machine().options().bios(), "tmac-rpt-edge")) { start_repeat_load_edge(); return; }
	if (!strcmp(machine().options().bios(), "mac-rpt")) { start_poly(); return; }
	if (!strcmp(machine().options().bios(), "lms") || !strcmp(machine().options().bios(), "lms-edge")) { start_sqdst(); return; }
	if (!strcmp(machine().options().bios(), "mac-rpt-edge")) { start_repeat_load_edge(); return; }
	if (!strcmp(machine().options().bios(), "bd-rpt")) { start_branch_repeat(); return; }
	if (!strcmp(machine().options().bios(), "retd-rpt")) { start_branch_repeat(); return; }
	if (!strcmp(machine().options().bios(), "calld-rpt")) { start_branch_repeat(); return; }
	if (!strcmp(machine().options().bios(), "bcd-rpt")) { start_branch_repeat(); return; }
	if (!strcmp(machine().options().bios(), "bcd-rpt-edge")) { start_conditional_delay_repeat_edge(); return; }
	if (!strcmp(machine().options().bios(), "rptbd-rpt")) { start_block_delay_repeat(); return; }
	if (!strcmp(machine().options().bios(), "rptbd-rpt-edge")) { start_block_delay_repeat_edge(); return; }
	if (!strcmp(machine().options().bios(), "bd-rpt-edge")) { start_branch_repeat_edge(); return; }
	if (!strcmp(machine().options().bios(), "stm-ind") || !strcmp(machine().options().bios(), "stm-ind-edge")) { start_indirect_stm(); return; }
	if (!strcmp(machine().options().bios(), "stm-low-ram")) { start_indirect_stm(); return; }
	if (!strcmp(machine().options().bios(), "stld-repeat")) { start_repeated_stld(); return; }
	if (!strcmp(machine().options().bios(), "st-alu-rpt")) { start_repeated_stld(); return; }
	if (!strcmp(machine().options().bios(), "st-alu-rpt-edge")) { start_parallel_load(); return; }
	if (!strcmp(machine().options().bios(), "mvdk-repeat") || !strcmp(machine().options().bios(), "mvdk-rpt-edge") || !strcmp(machine().options().bios(), "mvdk-index-rpt") || !strcmp(machine().options().bios(), "mvdk-irpt-edge")) { start_repeated_mvdk(); return; }
	if (!strcmp(machine().options().bios(), "rptb-call")) { start_block_near_call(); return; }
	if (!strcmp(machine().options().bios(), "rptb-call-edge")) { start_block_near_call_edge(); return; }
	if (!strcmp(machine().options().bios(), "rpt-mem-copy") || !strcmp(machine().options().bios(), "rpt-mem-wrap") || !strcmp(machine().options().bios(), "rpt-mem-chain")) { start_memory_copy_repeat(); return; }
	if (!strcmp(machine().options().bios(), "addc") || !strcmp(machine().options().bios(), "addc-edge")) { start_addc(); return; }
	if (!strcmp(machine().options().bios(), "unsigned-map") || !strcmp(machine().options().bios(), "unsigned-edge")) { start_addc(); return; }
	if (!strcmp(machine().options().bios(), "shift16-map") || !strcmp(machine().options().bios(), "shift16-map-edge")) { start_addc(); return; }
	if (!strcmp(machine().options().bios(), "logic-map") || !strcmp(machine().options().bios(), "logic-map-edge")) { start_addc(); return; }
	if (!strcmp(machine().options().bios(), "addc-chain")) { start_addc_chain(); return; }
	if (!strcmp(machine().options().bios(), "mvkd-peripheral")) { start_mvkd_peripheral(); return; }
	if (!strcmp(machine().options().bios(), "cmpm-absolute")) { start_pipeline_bitfield_ar7(); return; }
	if (!strcmp(machine().options().bios(), "cmpm-direct")) { start_direct_cmpm(); return; }
	if (!strcmp(machine().options().bios(), "cmpm-acc")) { start_accumulator_cmpm(); return; }
	if (!strcmp(machine().options().bios(), "cmpm-map") || !strcmp(machine().options().bios(), "cmpm-map-edge")) { start_accumulator_cmpm(); return; }
	if (!strcmp(machine().options().bios(), "stm-acc")) { start_accumulator_stm(); return; }
	if (!strcmp(machine().options().bios(), "cmpm-smem") || !strcmp(machine().options().bios(), "bitf-smem")) { start_pipeline_bitfield(); return; }
	if (!strcmp(machine().options().bios(), "cmpm-branch")) { start_pipeline_bitfield_ar7(); return; }
	if (!strcmp(machine().options().bios(), "st-extended-ar") || !strcmp(machine().options().bios(), "st-ar-consumer") || !strcmp(machine().options().bios(), "st-shift-ar") || !strcmp(machine().options().bios(), "st-shift-ar-use")) { start_extended_ar_store(); return; }
	if (!strcmp(machine().options().bios(), "st-shift-ar-bank")) { start_shifted_ar_conflict(); return; }
	if (!strcmp(machine().options().bios(), "st-shift-ar-edge")) { start_shifted_ar_edge(); return; }
	if (!strcmp(machine().options().bios(), "ld-ts") || !strcmp(machine().options().bios(), "ld-ts-producer")) { start_ts_load(); return; }
	if (!strcmp(machine().options().bios(), "ifr-pair")) { start_ifr_pair(); return; }
	if (!strcmp(machine().options().bios(), "mvmd-ar")) { start_mvmd_ar(); return; }
	if (!strcmp(machine().options().bios(), "mac-smem") || !strcmp(machine().options().bios(), "mac-bk")) { start_mac_smem(); return; }
	if (!strcmp(machine().options().bios(), "far-call-delay")) { start_delayed_far_call(); return; }
	if (!strcmp(machine().options().bios(), "mvdm-delay") || mvkd_delay()) { start_mvdm_delay(); return; }
	if (!strcmp(machine().options().bios(), "mvdm-ar")) { start_mvdm_ar(); return; }
	if (!strcmp(machine().options().bios(), "mvdm-t")) { start_mvdm_t(); return; }
	if (!strcmp(machine().options().bios(), "mvdm-acc") || !strcmp(machine().options().bios(), "mvdm-acc-edge")) { start_mvdm_acc(); return; }
	if (!strcmp(machine().options().bios(), "stack-page0") || !strcmp(machine().options().bios(), "stack-page0-edge")) { start_page_zero_stack(); return; }
	if (absolute_memory_dp()) { start_pipeline_memory_dp(); return; }
	if (!strcmp(machine().options().bios(), "read-a-mapped")) { start_mapped_read_a(); return; }
	if (!strcmp(machine().options().bios(), "bus-stm")) { start_staged_bus_stm(); return; }
	if (!strcmp(machine().options().bios(), "timer-stm")) { start_staged_bus_stm(); return; }
	if (!strcmp(machine().options().bios(), "pshd-mapped")) { start_mapped_pshd(); return; }
	if (!strcmp(machine().options().bios(), "pshd-repeat")) { start_repeat_push(); return; }
	if (!strcmp(machine().options().bios(), "exp-t")) { start_exponent(); return; }
	if (!strcmp(machine().options().bios(), "bus-io-compose")) { start_atomic_io_composition(); return; }
	if (!strcmp(machine().system().name, "tms54test") && (!strcmp(machine().options().bios(), "bus-io") || atomic_bus_io())) { start_bus_io_test(); return; }
	if (!strcmp(machine().system().name, "tms54test") && !strcmp(machine().options().bios(), "bus-registers"))
	{
		start_bus_register_test(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && !strcmp(machine().options().bios(), "imr-store"))
	{
		start_pipeline_imr_store(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && !strcmp(machine().options().bios(), "long-mvkd"))
	{
		start_pipeline_long_immediate_store(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && !strcmp(machine().options().bios(), "mpy-mapped"))
	{
		start_pipeline_multiply_mapped(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && !strcmp(machine().options().bios(), "mapped-bitf"))
	{
		start_pipeline_mapped_bitfield(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && !strcmp(machine().options().bios(), "mapped-word"))
	{
		start_pipeline_mapped_word_arithmetic(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && (!strcmp(machine().options().bios(), "negate") || !strcmp(machine().options().bios(), "absolute")))
	{
		start_pipeline_negate(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && (!strcmp(machine().options().bios(), "rmw-forward") || !strcmp(machine().options().bios(), "imm-forward") || !strcmp(machine().options().bios(), "mvkd-forward")))
	{
		start_pipeline_rmw_forward(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") && !strcmp(machine().options().bios(), "rptb"))
	{
		start_pipeline_rptb(); return;
	}
	if (!strcmp(machine().system().name, "tms54test") &&
			!strcmp(machine().options().bios(), "swap-join"))
	{
		start_pipeline_swap_join();
		return;
	}
	if (!strcmp(machine().system().name, "tms54test") &&
			!strcmp(machine().options().bios(), "swap-body"))
	{
		start_pipeline_swap_body();
		return;
	}
	if (!strcmp(machine().system().name, "tms54test") &&
			!strcmp(machine().options().bios(), "timer"))
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
			(!strcmp(machine().options().bios(), "efr-decoder") || !strcmp(machine().options().bios(), "efr-decoder-pipe")))
	{
		start_rom4_delayed_repeat(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			!strcmp(machine().options().bios(), "efr-packing"))
	{
		start_rom4_efr_packing(0, 0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			!strcmp(machine().options().bios(), "efr-crc"))
	{
		start_rom4_efr_crc(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(!strcmp(machine().options().bios(), "fr-encoder") || !strcmp(machine().options().bios(), "fr-encoder-pipe")))
	{
		if (!strcmp(machine().options().bios(), "fr-encoder-pipe"))
		{
			m_cpu->set_pipeline_frontend(true);
			osd_printf_info("ROM4 FR encoder frontend: staged handset_audio_claim=0\n");
		}
		start_rom4_fr_encoder(0, 0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			(!strcmp(machine().options().bios(), "fr-decoder") || !strcmp(machine().options().bios(), "fr-decoder-pipe")))
	{
		if (!strcmp(machine().options().bios(), "fr-decoder-pipe"))
		{
			m_cpu->set_pipeline_frontend(true);
			osd_printf_info("ROM4 FR decoder frontend: staged handset_audio_claim=0\n");
		}
		start_rom4_fr_decoder(0, true);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			!strcmp(machine().options().bios(), "fr-packing"))
	{
		start_rom4_fr_packing(0, 0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			!strcmp(machine().options().bios(), "crc-packed"))
	{
		start_rom4_crc_packed(0);
		return;
	}
	if (!strcmp(machine().system().name, "tms54rom4") &&
			!strcmp(machine().options().bios(), "crc-helper"))
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
			(!strcmp(machine().options().bios(), "cold") || !strcmp(machine().options().bios(), "cold-pipeline")))
	{
		bool const staged = !strcmp(machine().options().bios(), "cold-pipeline");
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
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x4b73);
		m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x281f);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
		m_cpu->set_state_int(tms320c54x_device::STATE_BK, 0x0052);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x000e);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0000004b73);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfffffffffe);
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
	program.write_word(0x0100, 0xf074);
	program.write_word(0x0101, 0x0200);
	program.write_word(0x0102, 0xec02);
	program.write_word(0x0103, 0xe598);

	// Reset source/destination pointers and execute a two-word CALL at the
	// end of an RPTB block three times.
	program.write_word(0x0104, 0x7712);
	program.write_word(0x0105, 0x0500);
	program.write_word(0x0106, 0x7714);
	program.write_word(0x0107, 0x0600);
	program.write_word(0x0108, 0x771a);
	program.write_word(0x0109, 0x0002);
	program.write_word(0x010a, 0xf072);
	program.write_word(0x010b, 0x010d);
	program.write_word(0x010c, 0xf074);
	program.write_word(0x010d, 0x0210);

	// Three circular moves starting at the final element must wrap through
	// the first two elements and return to the final element.
	program.write_word(0x010e, 0x7710);
	program.write_word(0x010f, 0x0001);
	program.write_word(0x0110, 0x7712);
	program.write_word(0x0111, 0x0802);
	program.write_word(0x0112, 0x7713);
	program.write_word(0x0113, 0x0900);
	program.write_word(0x0114, 0x7719);
	program.write_word(0x0115, 0x0003);
	program.write_word(0x0116, 0xec02);
	program.write_word(0x0117, 0xe5c9);
	program.write_word(0x0118, 0xe726);
	program.write_word(0x0119, 0xf074);
	program.write_word(0x011a, 0x0220);

	program.write_word(0x011b, 0x70f8);
	program.write_word(0x011c, 0x0904);
	program.write_word(0x011d, 0x0801);
	program.write_word(0x011e, 0x7714);
	program.write_word(0x011f, 0x0a00);
	program.write_word(0x0120, 0x70f8);
	program.write_word(0x0121, 0x0905);
	program.write_word(0x0122, 0x0014);
	program.write_word(0x0123, 0x7712);
	program.write_word(0x0124, 0x0800);
	program.write_word(0x0125, 0x7192);
	program.write_word(0x0126, 0x0014);
	program.write_word(0x0127, 0x7713);
	program.write_word(0x0128, 0x0800);
	program.write_word(0x0129, 0x1293);
	program.write_word(0x012a, 0xf0c8);
	program.write_word(0x012b, 0xf0e8);
	program.write_word(0x012c, 0xf0f8);
	program.write_word(0x012d, 0x7713);
	program.write_word(0x012e, 0x0800);
	program.write_word(0x012f, 0x1393);
	program.write_word(0x0130, 0xf330);
	program.write_word(0x0131, 0x00ff);
	program.write_word(0x0132, 0xf3e8);
	program.write_word(0x0133, 0x7df8);
	program.write_word(0x0134, 0x0800);
	program.write_word(0x0135, 0x0907);
	program.write_word(0x0136, 0x7cf8);
	program.write_word(0x0137, 0x0908);
	program.write_word(0x0138, 0x0907);
	// ROM4 receive enqueue: copy 26 words from *AR3+ into the circular
	// MDIRCV ring at *AR2+0%.  E59C's Y operand is AR2, not AR6.
	program.write_word(0x0139, 0x7710);
	program.write_word(0x013a, 0x0001);
	program.write_word(0x013b, 0x7712);
	program.write_word(0x013c, 0x0882);
	program.write_word(0x013d, 0x7713);
	program.write_word(0x013e, 0x1300);
	program.write_word(0x013f, 0x7719);
	program.write_word(0x0140, 0x0052);
	program.write_word(0x0141, 0xec19);
	program.write_word(0x0142, 0xe59c);
	program.write_word(0x0143, 0xf5e1);

	program.write_word(0x0200, 0x7680);
	program.write_word(0x0201, 0xbeef);
	program.write_word(0x0202, 0xfc00);
	program.write_word(0x0210, 0x1092);
	program.write_word(0x0211, 0x8094);
	program.write_word(0x0212, 0xfc00);
	program.write_word(0x0220, 0x61f8);
	program.write_word(0x0221, 0x0800);
	program.write_word(0x0222, 0x8000);
	program.write_word(0x0223, 0xfc30);
	program.write_word(0x0224, 0x7680);
	program.write_word(0x0225, 0xdead);
	program.write_word(0x0226, 0xfc00);

	// Final ROM4 challenge-transform loop. These are observed operands and
	// generic instruction encodings, not firmware code or a canned result.
	program.write_word(0x0300, 0x7712);
	program.write_word(0x0301, 0x13d9);
	program.write_word(0x0302, 0x7713);
	program.write_word(0x0303, 0x13d7);
	program.write_word(0x0304, 0x7714);
	program.write_word(0x0305, 0x1208);
	program.write_word(0x0306, 0x771a);
	program.write_word(0x0307, 0x0005);
	program.write_word(0x0308, 0xf072);
	program.write_word(0x0309, 0x030b);
	program.write_word(0x030a, 0xf074);
	program.write_word(0x030b, 0x0320);
	program.write_word(0x0320, 0x108a);
	program.write_word(0x0321, 0xf493);
	program.write_word(0x0322, 0x1a8b);
	program.write_word(0x0323, 0x6d8c);
	program.write_word(0x0324, 0x1c84);
	program.write_word(0x0325, 0x8084);
	program.write_word(0x0326, 0xfc00);
	// Long-immediate repeat executes the following two-word instruction
	// exactly lk + 1 times.
	program.write_word(0x0350, 0xf062);
	program.write_word(0x0351, 0x1234);
	program.write_word(0x0352, 0x4ef8);
	program.write_word(0x0353, 0x090c);
	program.write_word(0x0354, 0xe800);
	program.write_word(0x0355, 0xf070);
	program.write_word(0x0356, 0x0002);
	program.write_word(0x0357, 0x6d10);
	program.write_word(0x0358, 0xf5e1);
	program.write_word(0x0360, 0x76f8);
	program.write_word(0x0361, 0x0910);
	program.write_word(0x0362, 0x5678);
	program.write_word(0x0363, 0x7214);
	program.write_word(0x0364, 0x0912);
	program.write_word(0x0365, 0x57f8);
	program.write_word(0x0366, 0x0914);
	program.write_word(0x0367, 0xf793);
	program.write_word(0x0368, 0xff0c);
	program.write_word(0x0369, 0xf495);
	program.write_word(0x036a, 0xf793);
	program.write_word(0x036b, 0xf065);
	program.write_word(0x036c, 0x00ff);
	program.write_word(0x036d, 0xf054);
	program.write_word(0x036e, 0x00f0);
	program.write_word(0x036f, 0xf5e1);
	program.write_word(0x0370, 0xf171);
	program.write_word(0x0371, 0x0001);
	program.write_word(0x0372, 0x6d10);
	program.write_word(0x0373, 0xf5e1);
	program.write_word(0x0374, 0x47f8);
	program.write_word(0x0375, 0x0918);
	program.write_word(0x0376, 0x6bf8);
	program.write_word(0x0377, 0x091a);
	program.write_word(0x0378, 0x0001);
	program.write_word(0x0379, 0xf5e1);
	program.write_word(0x037a, 0xf070);
	program.write_word(0x037b, 0x0001);
	program.write_word(0x037c, 0x7d92);
	program.write_word(0x037d, 0x0924);
	program.write_word(0x037e, 0xf5e1);
	program.write_word(0x0380, 0xf273);
	program.write_word(0x0381, 0x0390);
	program.write_word(0x0382, 0xf495);
	program.write_word(0x0383, 0xf495);
	program.write_word(0x0384, 0xf5e1);
	program.write_word(0x0390, 0xf5e1);
	program.write_word(0x0392, 0xf4eb);
	program.write_word(0x0398, 0xf5e1);
	program.write_word(0x039a, 0xfa45);
	program.write_word(0x039b, 0x03a0);
	program.write_word(0x039c, 0xf495);
	program.write_word(0x039d, 0xf495);
	program.write_word(0x039e, 0xf5e1);
	program.write_word(0x03a0, 0xf5e1);
	program.write_word(0x03a2, 0x6ff8);
	program.write_word(0x03a3, 0x0920);
	program.write_word(0x03a4, 0x0c48);
	program.write_word(0x03a5, 0xf5e1);
	program.write_word(0x03a6, 0xf5e2);
	program.write_word(0x03b0, 0xf5e1);
	program.write_word(0x03b2, 0x09f8);
	program.write_word(0x03b3, 0x0918);
	program.write_word(0x03b4, 0xf5e1);
	program.write_word(0x03b6, 0xfc4b);
	program.write_word(0x03b7, 0xf5e1);
	program.write_word(0x03c0, 0xf5e1);
	program.write_word(0x03c2, 0xf947);
	program.write_word(0x03c3, 0x03d0);
	program.write_word(0x03c4, 0xf5e1);
	program.write_word(0x03d0, 0xf5e1);
	program.write_word(0x03d2, 0xf520);
	program.write_word(0x03d3, 0xf5e1);
	program.write_word(0x03d4, 0xf070);
	program.write_word(0x03d5, 0x0001);
	program.write_word(0x03d6, 0x7f92);
	program.write_word(0x03d7, 0xf5e1);
	program.write_word(0x03d8, 0x3292);
	program.write_word(0x03d9, 0xf5e1);
	program.write_word(0x03da, 0xeeff);
	program.write_word(0x03db, 0xee02);
	program.write_word(0x03dc, 0xf5e1);
	program.write_word(0x03e0, 0xf0ff);
	program.write_word(0x03e1, 0xf5e1);
	data.write_word(0x0918, 1);
	data.write_word(0x091a, 0);
	data.write_word(0x0920, 0xaaaa);
	data.write_word(0x0921, 0xbbbb);
	data.write_word(0x0912, 0xabcd);
	data.write_word(0x0914, 0x1234);
	data.write_word(0x0915, 0x5678);

	data.write_word(0x0500, 0x1111);
	data.write_word(0x0501, 0x2222);
	data.write_word(0x0502, 0x3333);
	data.write_word(0x0800, 0xaaaa);
	data.write_word(0x0801, 0xbbbb);
	data.write_word(0x0802, 0xcccc);
	data.write_word(0x13d2, 0x6d4d);
	data.write_word(0x13d3, 0xc431);
	data.write_word(0x13d4, 0xbfe4);
	data.write_word(0x13d5, 0x5d91);
	data.write_word(0x13d6, 0x71b1);
	data.write_word(0x13d7, 0x9ac9);
	data.write_word(0x13d8, 0x6d4d);
	data.write_word(0x13d9, 0xc431);
	data.write_word(0x1202, 0x71b1);
	data.write_word(0x1203, 0x9ac9);
	data.write_word(0x1204, 0x6d4d);
	data.write_word(0x1205, 0xc431);
	data.write_word(0x1206, 0xbfe4);
	data.write_word(0x1207, 0x5d91);
	for (unsigned i = 0; i != 26; ++i)
		data.write_word(0x1300 + i, 0x6000 + i);
	m_phase = 0;
	m_bleq_case = 0;
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0100);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0700);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0400);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0500);

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
		osd_printf_info("c54x_failure: phase=%u case=%u tick=%u replay=%u pc=%06x rc=%04x ar1=%04x a=%010llx b=%010llx st0=%04x illegal=%u reads=%u writes=%u\n",
			m_phase, m_pipeline_peripheral_case, m_idle_fixture_tick, unsigned(m_pipeline_replay), unsigned(m_cpu->state_int(STATE_GENPC)),
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
	if (!strcmp(config.options().bios(), "instr-observer") || !strcmp(config.options().bios(), "rpt-smem") || !strcmp(config.options().bios(), "rpt-ld") || !strcmp(config.options().bios(), "rpt-sfta") || !strcmp(config.options().bios(), "rpt-ld-block") || !strcmp(config.options().bios(), "rptb-call") || !strcmp(config.options().bios(), "rpt-mem-chain") || !strcmp(config.options().bios(), "bd-rpt") || !strcmp(config.options().bios(), "retd-rpt") || !strcmp(config.options().bios(), "calld-rpt") || !strcmp(config.options().bios(), "bcd-rpt"))
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
		if (m_phase == 255320)
		{
			unsigned const variant = m_pipeline_peripheral_case, layout = rptbd_layout(variant);
			expect(variant >= 512 && line == rptbd_irq_line(variant) && !m_rptbd_counts.acks && m_cpu->pipeline_cycles() >= 9 &&
				m_rptbd_counts.delay_reads == unsigned(layout == 2) && m_rptbd_counts.delay_writes == unsigned(layout == 1), "RPTBD acknowledges its pending source only after complete delay work");
			++m_rptbd_counts.acks; m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (m_phase == 310400)
		{
			auto const f = rptbd_shift_store_case(m_pipeline_peripheral_case);
			expect(f.irq && line == (f.irq == 2 || f.irq == 4 ? INPUT_LINE_NMI : 2) && !m_rptbd_counts.acks++ && m_rptbd_counts.delay_writes == 1 && m_cpu->pipeline_cycles() >= 9, "RPTBD shifted-store interrupt is acknowledged only after its complete delay publication"); m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (m_phase == 255240) { auto const f = block_back_case(m_pipeline_peripheral_case); expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 2) && !m_vector_acks && m_pipeline_store_writes, "Backward block acknowledges its pending source once after the first copy"); ++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE); }
		if (m_phase == 255200) { auto const f = block_far_call_case(m_pipeline_peripheral_case, !strcmp(machine().options().bios(), "rptb-fcalld")); expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 2) && !m_vector_acks && m_pipeline_operand_reads, "In-block far call acknowledges the actual pending source once after callee entry"); ++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE); }
		if (m_phase == 62150) { auto const f = block_near_call_case(m_pipeline_peripheral_case); expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 2) && !m_vector_acks && m_pipeline_operand_reads, "In-block near call acknowledges its actual pending interrupt once"); ++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE); }
		if (m_phase == 62154) { auto const f = memory_copy_repeat_case(m_pipeline_peripheral_case); expect(f.irq && line == (f.irq == 2 ? INPUT_LINE_NMI : 2) && !m_vector_acks && m_pipeline_store_writes == f.count + 1 && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0, "Memory-count repeat defers its actual pending interrupt until all copies complete"); ++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE); }
		if (m_phase == 254360) { unsigned const interrupt = m_pipeline_peripheral_case / 4096; expect(interrupt && line == (interrupt == 2 ? INPUT_LINE_NMI : 2) && !m_vector_acks, "In-block branch interrupt acknowledges its actual pending source once"); ++m_vector_acks; m_cpu->set_input_line(line, CLEAR_LINE); }
		if (m_phase == 253980) { expect(line == ((m_pipeline_peripheral_case / 4) % 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_cpu->pipeline_cycles() == 6, "Stack ordering preserves the selected IRQ/NMI acknowledgement"); ++m_vector_acks; }
		if (m_phase == 254140) { auto const &f = m_mvdd_fixture; expect(f.irq && line == (f.irq == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == f.count + 1 && m_cpu->pipeline_cycles() > f.first_write() + f.count, "MVDD acknowledges IRQ/NMI only after the final retained destination write"); ++m_vector_acks; }
		if (m_phase == 254200) { auto const &f = m_popd_fixture; expect(f.irq && line == (f.irq == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == f.count + 1 && m_cpu->pipeline_cycles() > f.read_clock() + f.count + 1, "POPD acknowledges IRQ/NMI only after the last repeat iteration retires"); ++m_vector_acks; }
		if (m_phase == 254240) { auto const &f = m_acc_branch_fixture; expect(f.irq && line == (f.irq == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && !m_pipeline_store_writes && !m_pipeline_register_writes, "Accumulator branch completes target selection before IRQ without allocating a call frame"); ++m_vector_acks; }
		if (m_phase == 254070) { auto const f = repeat_push_case(m_pipeline_peripheral_case); expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == f.count + 1 && m_cpu->pipeline_cycles() > f.first_write() + f.count, "Repeated PSHD acknowledges IRQ/NMI only after its final Execute"); ++m_vector_acks; }
		if (m_phase == 253890) { expect(line == (m_pipeline_peripheral_case & 8 ? INPUT_LINE_NMI : 0) && !m_vector_acks && m_pipeline_store_writes == 1, "Full status publication completes before interrupt entry without an RSBX-specific guard"); m_timer_probe_return_pc = m_cpu->state_int(tms320c54x_device::STATE_RTN); expect(m_timer_probe_return_pc >= (!strcmp(machine().options().bios(), "stlm-st1-irq") ? 0x601 : 0x602) && m_timer_probe_return_pc <= 0x621, "IRQ/NMI retains an unexecuted foreground continuation"); ++m_vector_acks; }
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
			auto const f = !strcmp(machine().options().bios(), "stlm-imr") ? stlm_imr_case(m_pipeline_peripheral_case) : imr_store_case(m_pipeline_peripheral_case);
			expect(f.ack_cycle && line == int(f.kind == 2 ? INPUT_LINE_NMI : f.source) && !m_vector_acks && m_cpu->pipeline_cycles() == f.ack_cycle,
				"STM IMR acknowledges the actual pending source after eligibility and preserves prior acceptance across a clear"); ++m_vector_acks;
		}
		if (m_phase == 252950)
		{
			expect(line == 0 && !m_vector_acks && m_cpu->pipeline_cycles() > 8,
				"Protected arithmetic acknowledges one pending IRQ only after retirement"); ++m_vector_acks;
		}
		if (m_phase == 253230)
		{
			auto const f = memory_repeat_case(m_pipeline_peripheral_case);
			bool const store = m_pipeline_peripheral_case >= 38400;
			unsigned const last = store ? (f.long_form ? 9 : 8) + 2 * f.count : (f.long_form ? 8 : 7) + f.count;
			expect(f.interrupt && line == (f.interrupt == 2 ? INPUT_LINE_NMI : 0) && m_cpu->pipeline_cycles() > last && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_RTN) == u16(f.body + (store ? 2 : 1)) && !m_vector_acks, "Memory repeat acknowledges its pending interrupt after the final iteration at the fresh continuation"); ++m_vector_acks;
		}
		if (m_phase == 252750)
		{
			unsigned const variant = m_pipeline_peripheral_case, interrupt = (variant / 10) % 3;
			unsigned const count = std::array<unsigned, 5>{0, 1, 3, 32, 255}[(variant / 2) % 5], first = variant % 2 ? 8 : 7;
			expect(interrupt && line == (interrupt == 2 ? INPUT_LINE_NMI : 0) && m_cpu->pipeline_cycles() > first + count &&
				m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RTN) == (variant % 2 ? 0x603 : 0x602) && !m_vector_acks,
				"NOP repeat acknowledges its original pending source only after the final body and stacks the fresh continuation"); ++m_vector_acks;
		}
		if (m_phase == 252410 && m_pipeline_peripheral_case >= 384)
		{
			unsigned const count = std::array<unsigned, 4>{0, 1, 3, 15}[m_pipeline_peripheral_case % 4];
			expect(line == (BIT(m_pipeline_peripheral_case, 2) ? INPUT_LINE_NMI : 0) && m_pipeline_store_writes == count + 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0x9003 && !m_vector_acks,
				"Memory RPT accepts its pending source only after the full copy and stacks the one-word READA continuation"); ++m_vector_acks;
		}
		if (m_phase == 252310 && m_pipeline_peripheral_case >= 128)
		{
			unsigned const count = std::array<unsigned, 4>{0, 1, 3, 15}[m_pipeline_peripheral_case % 4];
			expect(line == (BIT(m_pipeline_peripheral_case, 3) ? INPUT_LINE_NMI : 0) && m_pipeline_store_writes == count + 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0x9004 && !m_vector_acks,
				"RPT accepts the pending source only after its full copy and stacks the unexecuted continuation"); ++m_vector_acks;
		}
		if (m_phase == 252250 && m_pipeline_peripheral_case >= 8)
		{
			m_timer_probe_return_pc = m_cpu->state_int(tms320c54x_device::STATE_RTN);
			expect(line == 3 && (m_pipeline_peripheral_case >= 16 || m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0x98ea),
				"Original idle timer wake acknowledges TINT at the unexecuted post-IDLE continuation");
			++m_vector_acks;
		}
		if (m_phase == 252230)
		{
			unsigned const kind = pipeline_idle_kind(m_pipeline_peripheral_case);
			expect(line == (kind == 5 ? INPUT_LINE_NMI : kind == 3 ? 3 : 0) && m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0x601,
				"IDLE1 wake acknowledges the actual source with its unexecuted continuation");
			++m_vector_acks;
		}
		if (m_phase == 252110 || m_phase == 252130)
		{
			expect(line == 3, "Original MU4 timer fixture acknowledges only the hardware timer source");
			++m_vector_acks; m_timer_probe_return_pc = m_cpu->state_int(tms320c54x_device::STATE_RTN);
		}
		if (m_phase == 251170)
		{
			expect(m_pipeline_branch_case >= 8 && line == 0 && m_cpu->pipeline_cycles() == (m_pipeline_branch_case & 1 ? 10 : 12), "Branch IRQ acknowledges during its vector Fetch");
			++m_vector_acks;
		}
		if (m_phase == 251150)
		{
			u64 const cycle = m_vector_acks ? (m_pipeline_return_case & 1 ? 13 : 15) : 6;
			expect(line == 0 && m_cpu->pipeline_cycles() == cycle, "Fast-return fixture acknowledges each IRQ in its vector Fetch");
			++m_vector_acks;
		}
		if (m_phase == 251130)
		{
			expect(line == 0 && m_cpu->pipeline_cycles() == 8, "Accepted IRQ acknowledges once alongside mask RMW retirement");
			++m_vector_acks;
		}
		if (m_phase == 251110)
		{
			expect(line == 0 && m_cpu->pipeline_cycles() == ((m_pipeline_rmw_case / 2) % 3 == 1 ? 8 : 7),
				"RMW retires alongside the accepted vector Fetch/IACK");
			++m_vector_acks;
		}
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
		if (m_phase == 251040)
		{
			expect(line == 0 && m_cpu->pipeline_cycles() == 7,
				"Two-word instruction completes Decode before interrupt IACK");
			++m_vector_acks;
		}
		if (m_phase == 251030)
		{
			expect(line == (m_pipeline_empty_nmi ? INPUT_LINE_NMI : 0) && m_cpu->pipeline_cycles() == 4,
				"Empty frontend acknowledges the selected IRQ/NMI at vector Fetch");
			++m_vector_acks;
		}
		if (m_phase == 251000)
		{
			expect(line == 0 && m_cpu->pipeline_cycles() == 6 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800),
				"Staged vector IACK at cycle 6 clears its source and sets INTM");
			++m_vector_acks;
		}
		if (m_phase >= 450000 && m_phase <= 450002)
		{
			expect(line == (m_stage_restore_case >= 5 ? INPUT_LINE_NMI : 0),
				"Restored entry acknowledges only the originally accepted IRQ/NMI source");
			++m_vector_acks;
		}
		if (m_phase >= 190000 && m_phase <= 190003)
		{
			expect(m_entry_ack_source == -1 && m_cpu->state_int(tms320c54x_device::STATE_IFR) == 2,
				"Vector acknowledgement clears IFR exactly once at Fetch");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800,
				"Maskable vector acknowledgement sets INTM with IFR clearance");
			m_entry_ack_source = line;
		}
		if (m_phase >= 80000 && m_phase <= 80003)
		{
			expect(line == (m_vector_case ? INPUT_LINE_NMI : 0), "Vector acknowledgement retains accepted source identity");
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
				m_cpu->space(AS_DATA).read_word(0x0fff) == 0xbeef,
				"SPRU131G vector acknowledgement follows SP Access but precedes the continuation memory write");
			++m_vector_acks;
		}
		return 0;
	}, "vector_acknowledge");
	m_cpu->bio_in_cb().set(FUNC(tms320c54x_test_state::bio_r));
	m_cpu->xf_out_cb().set(FUNC(tms320c54x_test_state::xf_w));
	NOKIA_DSPIF(config, m_transport, 0);
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
