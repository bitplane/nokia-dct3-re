// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_STATE_H
#define MAME_NOKIA_TMS320C54X_TEST_STATE_H

#include "emuopts.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "nokia_dspif.h"
#include "nokia_cobba.h"
#include "nokia_dsp_c54x.h"
#include "gsm_tch_f_l1.h"
#include "util/gsmfr.h"
#include "tms320c54x_mcbsp.h"
#include "tms320c54x_dma.h"
#include "tms320c54x_bsp_state.h"
#include "tms320c54x_test_profiles.h"
#include <sstream>
#include <bitset>

namespace c54x_test {

class tms320c54x_test_state : public driver_device
{
public:
	tms320c54x_test_state(const machine_config &mconfig, device_type type,
			const char *tag) :
		driver_device(mconfig, type, tag),
		m_cpu(*this, "maincpu"),
		m_transport(*this, "dspif"),
		m_idle_mcbsp(*this, "idle_mcbsp"),
		m_idle_dma(*this, "idle_dma")
	{
	}

	void test(machine_config &config);
	void rom4(machine_config &config);

private:
	static constexpr u64 SMUL_RESULTS[4][3] = {
		{0x003fffffffULL, 0x007fffffffULL, 0x007ffffffeULL},
		{0x0040000000ULL, 0x0080000000ULL, 0x0080000000ULL},
		{0xffc0000001ULL, 0xff80000001ULL, 0xff80000002ULL},
		{0xffc0000000ULL, 0xff80000000ULL, 0xff80000000ULL},
	};
	void start_smul_control_case(unsigned index);
	struct store_shift_vector { u64 value; int shift; u32 results[4]; };
	static constexpr store_shift_vector STORE_SHIFTS[] = {
		{0x8000000001ULL, -16, {0x00800000, 0xff800000, 0x00800000, 0xff800000}},
		{0xffffffffffULL, -16, {0x00ffffff, 0xffffffff, 0x00ffffff, 0xffffffff}},
		{0x8000000001ULL, -1, {0x00000000, 0x00000000, 0xffffffff, 0x80000000}},
	};
	void start_store_shift_case(unsigned index);
	struct saccd_boundary_vector { u64 value; int shift; u16 logical; u16 arithmetic; };
	static constexpr saccd_boundary_vector SACCD_BOUNDARIES[] = {
		{0x0180012345ULL, 0, 0x8001, 0x8001},
		{0xfe7ffe1234ULL, 0, 0x7ffe, 0x7ffe},
		{0x8000000001ULL, -16, 0x0080, 0xff80},
		{0xffffffffffULL, -16, 0x00ff, 0xffff},
		{0xffffffffffULL, -1, 0xffff, 0xffff},
		{0x0080012345ULL, 15, 0x91a2, 0x91a2},
	};
	void start_saccd_boundary_case(unsigned index);
	void start_firs_sum_case(unsigned index);
	void start_poly_boundary_case(unsigned index);
	void start_abdst_overflow_case(unsigned index);
	void start_sqdst_difference_case(unsigned index);
	void start_sqdst_overflow_case(unsigned index);
	void start_rounding_boundary_case(unsigned index);
	void xf_w(int state);
	bool m_xf_pin = true;
	unsigned m_xf_publications = 0;
	void start_repeat_endpoint_case(unsigned index);
	void start_indirect_popm_case(unsigned index);
	void start_shifted_arithmetic_selector_case(unsigned index);
	void start_interrupt_entry_spacing_case(unsigned index);
	void start_interrupt_acceptance_checkpoint();
	void start_entry_stage_restore_case(unsigned index);
	void start_native_final_gap_case(unsigned index);
	void start_native_memory_gap_case(unsigned index);
	void start_native_math_gap_case(unsigned index);
	void start_native_address_gap_case(unsigned index);
	void start_native_gap_case(unsigned index);
	void start_cala_rtn_case(unsigned index);
	void start_operand_fetch_case(unsigned index);
	void start_loop_xc_case(unsigned index);
	void start_nested_repeat_case(unsigned index);
	void start_braf_control_case(unsigned index);
	void start_xc_irq_case(unsigned index);
	void check_control_disassembler();
	void start_idle_nmi_case(unsigned index);
	void check_idle_nmi_result(unsigned index);
	void start_idle_dma_case(unsigned index);
	void start_idle_external_serial_case(unsigned index);
	void start_idle_timer_case(unsigned index);
	int bio_r();
	struct rounded_multiply_case
	{
		u16 opcode, t, memory, status;
		u64 initial, expected;
		bool overflow;
		u16 pmst = 0;
	};
	static rounded_multiply_case const &rounded_multiply_vector(unsigned index);
	void start_rounded_multiply_case(unsigned index);
	bool check_rounded_multiply();
	void start_idle_wake_case(unsigned index);
	void start_stopped_timer_reload_case(unsigned index);
	void start_timer_prescaler_divider_case(unsigned divider);
	void start_timer_counter_write_case(unsigned divider);
	void check_timer_reserved_fields();
	void start_clkmd_write_case(unsigned index);
	void start_pipeline_frontend_case(bool memory = false, bool memory_b = false);
	void start_empty_pipeline_case(bool nmi);
	void start_multiword_pipeline_case(unsigned variant = 0);
	void start_multiword_stream_case();
	void start_pipeline_store_case(unsigned variant = 0);
	void start_pipeline_indirect_case(unsigned variant = 0);
	void start_pipeline_rmw_case(unsigned variant = 0);
	void start_pipeline_mask_rmw_case();
	void start_pipeline_return_case(unsigned variant = 0);
	void start_pipeline_branch_case(unsigned variant = 0);
	struct delay_asm_fixture
	{
		unsigned control, form, raw, pattern, flags, checkpoint;
		u32 base;
		u64 result;
		unsigned prefix() const { return control == 3; }
		bool long_form() const { return form >= 3; }
		unsigned read() const { return delay_multiply_fixture::operand_read(control == 3 ? 0 : control, long_form() ? 1 : 0) + prefix(); }
		unsigned literal_clock() const { return 5 + unsigned(control == 1 || control == 2) + prefix(); }
		unsigned target() const { return control == 2 ? 0x604 : 0x700; }
		unsigned store() const { return target() + unsigned(long_form()); }
		u16 source_word() const { return 0xa5a0 | raw; }
		u16 st1() const { return 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (form == 1 ? 0x4000 : 0); }
	};
	static delay_asm_fixture delay_asm_case(unsigned v);
	void start_delay_asm(unsigned variant = 0);
	struct constant_dagen_fixture
	{
		unsigned form, target, pattern, prefix, gap, checkpoint;
		u32 base;
		u16 value;
		unsigned words() const { return 2 + form; }
		unsigned prefix_words() const { return unsigned(prefix != 0); }
		unsigned write() const { return 5 + prefix_words() + form + unsigned(prefix == 1 && !form); }
		unsigned consumer() const { return 0x600 + prefix_words() + words() + gap + (target == 9 ? 1 + unsigned(prefix == 1 && !form) : 0); }
		unsigned read() const { return consumer() - 0x600 + 5; }
		u16 circular_source() const { return pattern == 1 ? 0x5002 : 0x5003; }
	};
	static constant_dagen_fixture constant_dagen_case(unsigned v);
	void start_constant_dagen(unsigned variant = 0);
	void start_constant_dagen_edge(unsigned variant = 0);
	struct indirect_stm_fixture
	{
		unsigned selector, ar, snapshot;
		u16 destination, literal, st1;
		u32 base;
		bool edge;
		bool direct = false;
		u16 st0 = 0xa633;
		unsigned gap = 0;
	};
	indirect_stm_fixture indirect_stm_case(unsigned v) const;
	void start_indirect_stm(unsigned variant = 0);
	struct accumulator_stm_fixture
	{
		unsigned lane, snapshot;
		u16 literal, st1;
		u32 base;
		u64 a, b, final_a, final_b;
	};
	static accumulator_stm_fixture accumulator_stm_case(unsigned v);
	void start_accumulator_stm(unsigned variant = 0);
	void start_pipeline_stm_case(unsigned variant = 0);
	void start_peripheral_mmr_timing_audit(unsigned variant = 0);
	void start_pipeline_peripheral_case(unsigned variant = 0);
	void start_pipeline_immediate_store(unsigned variant = 0);
	void start_pipeline_xpc_push(unsigned variant = 0);
	void start_pipeline_far_return(unsigned variant = 0);
	void start_pipeline_far_irq(unsigned variant = 0);
	void start_pipeline_intm(unsigned variant = 0);
	void start_pipeline_status_bit(unsigned variant = 0);
	void start_pipeline_cpl(unsigned variant = 0);
	void start_pipeline_dp_immediate(unsigned variant = 0);
	void start_pipeline_frame(unsigned variant = 0);
	void start_pipeline_push_sp_latency(unsigned variant = 0);
	struct irq_pop_fixture
	{
		u16 destination, value, pending, mask;
		u32 base;
		unsigned checkpoint;
	};
	static irq_pop_fixture irq_pop_case(unsigned variant);
	void start_pipeline_mmr_pop(unsigned variant = 0);
	void start_interrupt_pop_edge(unsigned variant = 0);
	void start_pipeline_pop_dagen_conflict(unsigned variant = 0);
	void start_pipeline_mu4_prologue(unsigned variant = 0);
	void start_pipeline_dld(unsigned variant = 0);
	void start_pipeline_immediate_add(unsigned variant = 0);
	void start_pipeline_dst(unsigned variant = 0);
	void start_pipeline_ldm(unsigned variant = 0);
	void start_pipeline_logical_immediate(unsigned variant = 0);
	bool absolute_memory_dp() const;
	void start_pipeline_memory_dp(unsigned variant = 0);
	void start_pipeline_conditional_branch(unsigned variant = 0);
	void start_pipeline_plain_return(unsigned variant = 0);
	void start_pipeline_call(unsigned variant = 0);
	void start_pipeline_direct_move(unsigned variant = 0);
	void start_pipeline_worker_xc(unsigned variant = 0);
	void start_pipeline_xc_irq(unsigned variant = 0);
	void start_xc_return(unsigned variant = 0);
	void start_pipeline_xc_tc(unsigned variant = 0);
	void start_pipeline_plain_branch(unsigned variant = 0);
	void start_pipeline_xc_accumulator(unsigned variant = 0);
	struct signed_xc_fixture
	{
		unsigned condition, layout;
		bool b, taken;
		u64 value, opposite;
		u32 base;
	};
	static signed_xc_fixture signed_xc_case(unsigned variant);
	void start_signed_xc(unsigned variant = 0);
	void start_pipeline_delayed_return(unsigned variant = 0);
	void start_pipeline_original_worker_tail(unsigned variant = 0);
	void start_pipeline_original_worker_roundtrip(unsigned variant = 0);
	void start_pipeline_original_context_call(unsigned variant = 0, bool full_body = false);
	static std::array<u16, 2> addm_inputs(unsigned variant);
	static std::array<u16, 2> addm_expected(unsigned variant);
	void start_pipeline_addm(unsigned variant = 0);
	void start_pipeline_data_push(unsigned variant = 0);
	void start_pipeline_retd_data_order(unsigned variant = 0);
	void start_pipeline_data_pop(unsigned variant = 0);
	void start_pipeline_register_store(unsigned variant = 0);
	void start_pipeline_cmpr(unsigned variant = 0);
	void start_pipeline_mar(unsigned variant = 0);
	void start_pipeline_memory_logic(unsigned variant = 0);
	void start_pipeline_mu4_logic_tail(unsigned variant = 0);
	void start_pipeline_mu4_full_worker(unsigned variant = 0);
	void start_pipeline_far_branch(unsigned variant = 0);
	void start_pipeline_mu4_timer_context(unsigned variant = 0);
	void start_pipeline_mu4_recurring_timer(unsigned variant = 0);
	void start_pipeline_absolute_load(unsigned variant = 0);
	void start_pipeline_far_call(unsigned variant = 0);
	void start_pipeline_explicit_sp(unsigned variant = 0);
	void start_pipeline_idle_sp_tail(unsigned variant = 0);
	unsigned pipeline_idle_kind(unsigned variant) const;
	void start_pipeline_idle1(unsigned variant = 0);
	void start_pipeline_original_idle(unsigned variant = 0);
	void start_pipeline_pmst(unsigned variant = 0);
	void start_stack_irq_order(unsigned variant = 0);
	void start_repeat_pop_exclusion(unsigned variant = 0);
	void start_pipeline_mvpd(unsigned variant = 0);
	void start_extended_mvpd(unsigned variant = 0);
	void start_extended_mvpd_edge(unsigned variant = 0);
	void start_literal_guard(unsigned variant = 0);
	struct ar_addm_fixture
	{
		unsigned reg, checkpoint;
		u32 base;
		u16 initial, immediate, result, st0, st1, final_st0;
		bool t = false, short_form = false;
		unsigned short_ar_form = 0;
		unsigned read = 7, write = 8, extensions = 2;
	};
	ar_addm_fixture ar_addm_case(unsigned variant) const;
	ar_addm_fixture mapped_addm_case(unsigned variant) const;
	void start_ar_addm(unsigned variant = 0);
	void start_addm_brc_loop(unsigned variant = 0);
	struct repeated_writa_fixture
	{
		unsigned form, setup, reg, count, words, read, write, checkpoint;
		bool mapped, standard_width;
		u16 source, count_address;
		u32 target, page;
		int direction;
	};
	static repeated_writa_fixture repeated_writa_case(unsigned variant, bool standard_width = false);
	static u16 repeated_writa_word(unsigned reg, unsigned index);
	static u32 repeated_writa_destination(repeated_writa_fixture const &f, unsigned index);
	void start_repeated_writa(unsigned variant = 0);
	struct branch_arithmetic_fixture
	{
		unsigned operation, destination, branch_b, form, flags, checkpoint;
		bool taken;
		u16 memory, source, st0, final_st0;
		u32 page;
		u64 a, b, result;
	};
	static branch_arithmetic_fixture branch_arithmetic_case(unsigned variant);
	void start_branch_arithmetic(unsigned variant = 0);
	void start_branch_arithmetic_edge(unsigned variant = 0);
	struct circular_mar_fixture
	{
		unsigned mode, reg, selected, checkpoint;
		bool compatibility;
		u16 size, result, st0;
		u32 page;
		std::array<u16, 8> initial;
	};
	circular_mar_fixture circular_mar_case(unsigned variant);
	void start_circular_mar(unsigned variant = 0);
	struct delayed_condition_return_fixture
	{
		u32 base;
		u16 opcode, sp, st0, final_st0;
		u64 a, b, final_a, final_b;
		unsigned condition, layout, checkpoint;
		bool taken;
	};
	static delayed_condition_return_fixture delayed_condition_return_case(unsigned v);
	void start_delayed_condition_return(unsigned variant = 0);
	void start_tc_return(unsigned variant = 0);
	void start_bitt(unsigned variant = 0);
	void start_bitt_retd(unsigned variant = 0);
	struct delay_bitt_fixture
	{
		unsigned control, form, slot, selector, snapshot;
		bool upper, invert, initial_tc;
		u32 base;
		unsigned timing_kind() const { return control >= 6 ? 1 : control; }
		unsigned read() const { return delay_multiply_fixture::operand_read(timing_kind(), slot); }
		unsigned producer_read() const { return delay_multiply_fixture::operand_read(timing_kind(), 0); }
		u16 t() const { return selector | (upper ? 0xa5f0 : 0); }
		u16 final_t() const { return upper ? t() ^ 15 : t(); }
		u16 value() const { u16 const mask = 1U << (15 - selector); return invert ? u16(~mask) : mask; }
		u16 opcode() const { return 0x3400 | (form == 0 || form == 3 ? 0x60 : form == 1 ? 0x84 : 0x94); }
		u16 final_sp() const { return 0x3000 + (control == 3 ? -1 : control == 4 ? -2 : control == 5 ? 1 : 0); }
		u16 source() const { return form == 0 ? 0x1060 : form == 3 ? final_sp() + 0x60 : 0x5000; }
		u16 status(unsigned tick) const { return 0xa820 | (tick > read() ? invert ? 0 : 0x1000 : initial_tc ? 0x1000 : 0); }
		u16 st1() const { return 0xb00 | (form == 3 ? 0x4000 : 0); }
		bool taken() const { return control == 2 ? false : control == 6 ? !initial_tc : control == 7 ? initial_tc : true; }
		u32 target_base() const { return control == 4 ? base ^ 0x20000 : base; }
		u32 selected() const { return taken() ? target_base() + 0x700 : base + 0x604; }
		u16 delay_start() const { return control == 5 ? 0x601 : 0x602; }
		u64 a() const { return control == 1 ? 0 : control == 2 ? 1 : 0xff12345678ULL; }
	};
	static delay_bitt_fixture delay_bitt_case(unsigned v);
	void start_delay_bitt(unsigned variant = 0);
	struct bd_read_a_fixture
	{
		unsigned slot, form, pattern, snapshot;
		u32 base, source;
		u16 value() const { return std::array<u16,4>{0,1,0x8000,0xffff}[pattern]; }
		unsigned read() const { return 7+slot; }
		unsigned checkpoint() const { return std::array<unsigned,4>{3,read(),read()+1,read()+4}[snapshot]; }
	};
	static bd_read_a_fixture bd_read_a_case(unsigned v);
	void start_bd_read_a(unsigned variant=0);
	struct mvdd_follow_fixture
	{
		unsigned pattern, modifier, consumer, gap, layout, snapshot;
		u32 base;
		u16 value() const { return std::array<u16,4>{0,1,0x8000,0xffff}[pattern]; }
		unsigned at() const { return (layout ? 0x700 : 0x601) + gap; }
		unsigned checkpoint() const { return std::array<unsigned,4>{3,5,6,8}[snapshot]; }
	};
	static mvdd_follow_fixture mvdd_follow_case(unsigned v);
	void start_mvdd_follow(unsigned variant=0);
	struct repeat_store_case
	{
		unsigned form, mode, reg, count, setup, pattern, flags, checkpoint;
		u32 base;
	};
	static repeat_store_case repeat_store_fixture(unsigned variant);
	static u64 repeat_store_value(unsigned pattern);
	void start_repeat_store(unsigned variant = 0);
	void start_repeat_store_edge(unsigned variant = 0);
	void start_rptz(unsigned variant = 0);
	static constexpr std::array<u64, 16> sat_inputs = {
		0, 1, 0x7ffffffeULL, 0x7fffffffULL, 0x80000000ULL, 0x80000001ULL,
		0xff7fffffffULL, 0xff80000000ULL, 0xff80000001ULL, 0xffffffffffULL,
		0x7fffffffffULL, 0x8000000000ULL, 0x0100000000ULL, 0xfeffffffffULL,
		0x007fffff00ULL, 0xff80000100ULL
	};
	void start_sat(unsigned variant = 0);
	void start_rotate(unsigned variant = 0);
	void start_ar_addm_load(unsigned variant = 0);
	void start_pipeline_repeat_mvpd(unsigned variant = 0);
	void start_pipeline_sp_rmw(unsigned variant = 0);
	void start_pipeline_arp(unsigned variant = 0);
	void start_pipeline_read_a(unsigned variant = 0);
	void start_pipeline_banz(unsigned variant = 0);
	void start_pipeline_memory_repeat_read_a(unsigned variant = 0);
	void start_pipeline_absolute_add(unsigned variant = 0);
	void start_instruction_observer(unsigned variant = 0);
	void start_pipeline_add_sp_context(unsigned variant = 0);
	void start_pipeline_add_sp_exclusion(unsigned variant = 0);
	void start_pipeline_direct_rmw(unsigned variant = 0);
	void start_pipeline_peripheral_logic(unsigned variant = 0);
	void start_pipeline_port_write(unsigned variant = 0);
	void start_pipeline_direct_immediate_store(unsigned variant = 0);
	static u16 stlm_fixture_value(unsigned variant);
	struct memory_arithmetic_fixture
	{
		unsigned form, reg, selected, read_cycle;
		u16 opcode, literal, address, next_ar, memory, st0, st1, result_st0;
		u64 source, result;
		u16 bk = 16;
	};
	static u64 ts_load_expected(u16 memory, u16 t, bool sxm);
	struct ts_load_fixture
	{
		memory_arithmetic_fixture address;
		u16 memory, t;
		unsigned destination, producer_kind, prefix;
		u32 base;
		u64 a, b, result;
	};
	ts_load_fixture ts_load_case(unsigned variant, bool producer);
	void start_ts_load(unsigned variant = 0);
	struct extended_ar_store_fixture
	{
		unsigned form, reg, selected, kind, flags, gap;
		int shift;
		bool shifted;
		u16 literal, initial_ar, next_ar, result;
		u64 value;
		u32 base;
	};
	void start_shifted_ar_conflict(unsigned variant = 0);
	void start_shifted_ar_edge(unsigned variant = 0);
	static extended_ar_store_fixture extended_ar_store_case(unsigned variant, bool consumer, bool shifted = false);
	void start_extended_ar_store(unsigned variant = 0);
	void start_pipeline_stlm(unsigned variant = 0);
	struct dst_extended_fixture
	{
		unsigned reg, selected, source, kind;
		u16 literal, address, next_ar;
		u64 value;
		u32 stored;
		u16 initial, bk;
	};
	static unsigned circular_fixture_mask(u16 bk);
	static bool circular_fixture_domain(u16 initial, int step, u16 bk);
	static u16 circular_fixture_next(u16 initial, int step, u16 bk);
	void verify_paired_circular_domain();
	static dst_extended_fixture extended_dst_fixture(unsigned variant);
	void start_pipeline_extended_dst(unsigned variant = 0);
	static std::array<u64, 2> accumulator_logic_fixture(unsigned variant, bool after);
	void start_pipeline_accumulator_logic(unsigned variant = 0);
	struct mvkd_sp_fixture
	{
		unsigned form, read, checkpoint;
		u16 opcode, literal, destination, next_ar, value;
		u32 base;
		bool compiler;
	};
	static mvkd_sp_fixture mvkd_sp_case(unsigned variant);
	void start_mvkd_sp(unsigned variant = 0);
	struct mvkd_fixture
	{
		unsigned form, read_cycle;
		u16 source, destination, next_ar, value;
	};
	static mvkd_fixture data_copy_fixture(unsigned variant);
	void start_pipeline_data_copy(unsigned variant = 0);
	static std::array<u64, 3> accumulator_shift_fixture(unsigned variant, bool after);
	void start_pipeline_accumulator_shift(unsigned variant = 0);
	struct long_pair_fixture_data
	{
		unsigned form, reg, read_cycle;
		u16 address, next_ar, literal, opcode, st0, st1;
		u64 source, result;
		u32 memory;
		u16 result_st0;
	};
	static long_pair_fixture_data long_pair_fixture(unsigned variant);
	static long_pair_fixture_data long_add_fixture(unsigned variant);
	static long_pair_fixture_data long_reverse_fixture(unsigned variant);
	void start_pipeline_long_pair(unsigned variant = 0);
	struct long_store_fixture
	{
		unsigned reg, selected, kind;
		u16 offset, value, address, next_ar, st0;
		bool compatibility;
		u16 initial = 0x1007, bk = 16;
	};
	static long_store_fixture immediate_long_store_fixture(unsigned variant);
	static long_store_fixture peripheral_long_store_fixture(unsigned variant);
	void start_pipeline_long_immediate_store(unsigned variant = 0);
	void start_pipeline_bio_branch(unsigned variant = 0);
	void start_pipeline_mmr_move(unsigned variant = 0);
	struct push_mvmm_fixture
	{
		unsigned pushes, mmr, gap, destination, checkpoint;
		u16 sp;
		u32 base;
		unsigned read() const { return 3 + pushes + gap; }
		u16 copied() const { return sp - pushes; }
		u16 source(unsigned i) const { return pushes == 2 && !i ? 0x12 : mmr; }
		u16 value(unsigned i) const { return source(i) == 0x18 ? u16(sp-i) : source(i) == 6 ? 0xa020 : source(i) == 7 ? 0x800 : 0x5002; }
		std::vector<std::array<unsigned,3>> reads() const
		{
			std::vector<std::array<unsigned,3>> result;
			for (unsigned i=0;i<pushes;++i) result.push_back({5+i,source(i),value(i)});
			result.push_back({read(),0x18,u16(copied()+unsigned(!gap))});
			std::stable_sort(result.begin(),result.end(),[](auto const &a,auto const &b) { return a[0]<b[0]; });
			return result;
		}
	};
	static push_mvmm_fixture push_mvmm_case(unsigned v);
	void start_push_mvmm(unsigned variant=0);
	void start_pipeline_mvmm_sp(unsigned variant = 0);
	void start_pipeline_mapped_load_age(unsigned variant = 0);
	void start_pipeline_store_mvmm(unsigned variant = 0);
	void start_pipeline_mvmm_producer(unsigned variant = 0);
	void start_pipeline_repeat_nop(unsigned variant = 0);
	void start_pipeline_xmem_bit(unsigned variant = 0);
	struct indexed_store_fixture;
	static indexed_store_fixture circular_portr_case(unsigned v);
	void start_circular_portr(unsigned variant = 0);
	struct portr_bypass_fixture
	{
		bool chain, other, destination, sxm;
		unsigned operation, gap, snapshot;
		u32 base;
		u16 sample, request;
		u64 a, b;
	};
	static portr_bypass_fixture portr_bypass_case(unsigned variant);
	void start_portr_bypass(unsigned variant = 0);
	struct circular_portr_block_fixture
	{
		unsigned count, size, reg, selected, snapshot, index;
		bool compatibility;
		u16 buffer, st0;
		u32 base;
	};
	static circular_portr_block_fixture circular_portr_block_case(unsigned v);
	void start_circular_portr_block(unsigned variant = 0);
	void start_pipeline_port_read(unsigned variant = 0);
	struct accumulator_store_fixture
	{
		unsigned reg, selected, kind, selector;
		u16 opcode, literal, address, next_ar, st0, st1, pmst, value;
		u64 source;
		u16 initial = 0x1007, bk = 16;
	};
	static accumulator_store_fixture long_accumulator_store_fixture(unsigned variant);
	struct long_mvkd_fixture
	{
		accumulator_store_fixture address;
		u16 source, value;
		u32 base;
		unsigned checkpoint;
	};
	static long_mvkd_fixture long_mvkd_case(unsigned variant);
	void start_pipeline_long_mvkd(unsigned variant = 0);
	struct imr_store_fixture
	{
		u16 old_mask, mask, st1, bit;
		unsigned source, kind, ack_cycle, checkpoint;
		u32 base;
	};
	static imr_store_fixture imr_store_case(unsigned variant);
	static imr_store_fixture stlm_imr_case(unsigned variant);
	void start_pipeline_imr_store(unsigned variant = 0);
	void start_shifted_store_dld(unsigned variant = 0);
	void start_pipeline_long_accumulator_store(unsigned variant = 0);
	void start_pipeline_long_move(unsigned variant = 0);
	void start_pipeline_original_long_move(unsigned variant = 0);
	void start_pipeline_tc_branch(unsigned variant = 0);
	static std::array<u16, 2> long_rmw_expected(unsigned arithmetic, unsigned operation, u16 flags);
	static accumulator_store_fixture peripheral_long_rmw_fixture(unsigned variant);
	static accumulator_store_fixture absolute_xorm_fixture(unsigned variant);
	void start_pipeline_long_rmw(unsigned variant = 0);
	static memory_arithmetic_fixture memory_arithmetic_case(unsigned variant, bool unsigned_memory = false, unsigned shift16_selector = 8);
	static memory_arithmetic_fixture memory_unsigned_case(unsigned variant, bool subtract = true);
	static memory_arithmetic_fixture memory_shift16_case(unsigned variant);
	void start_pipeline_memory_arithmetic(unsigned variant = 0);
	static memory_arithmetic_fixture register_arithmetic_case(unsigned variant, bool bk = false);
	void start_pipeline_register_arithmetic(unsigned variant = 0);
	void start_pipeline_xc_arithmetic(unsigned variant = 0);
	static u64 signed_branch_source(unsigned variant);
	static bool signed_branch_taken(unsigned variant);
	void start_pipeline_signed_branch(unsigned variant = 0);
	void start_pipeline_signed_branch_producer(unsigned variant = 0);
	static std::array<u16, 2> bitfield_inputs(unsigned variant, bool compare = false);
	static memory_arithmetic_fixture bitfield_address_case(unsigned variant);
	void start_pipeline_bitfield(unsigned variant = 0);
	struct accumulator_cmpm_fixture
	{
		unsigned lane, form, read, snapshot, pointer;
		u16 source, value, literal, st1;
		u32 base;
		bool tc, initial_tc, excluded = false;
		u64 a, b;
	};
	static accumulator_cmpm_fixture accumulator_cmpm_case(unsigned v, bool mapped = false, bool edge = false);
	void start_accumulator_cmpm(unsigned variant = 0);
	void start_direct_cmpm(unsigned variant = 0);
	void start_pipeline_bitfield_ar7(unsigned variant = 0);
	static memory_arithmetic_fixture offset_load_case(unsigned variant);
	static memory_arithmetic_fixture unsigned_load_case(unsigned variant);
	static memory_arithmetic_fixture shifted_memory_load_case(unsigned variant);
	static memory_arithmetic_fixture staged_load_case(unsigned variant);
	void start_pipeline_offset_load(unsigned variant = 0);
	static dst_extended_fixture indirect_dld_case(unsigned variant);
	void start_pipeline_indirect_dld(unsigned variant = 0);
	struct extended_banz_fixture
	{
		unsigned form, reg, selected;
		u16 initial, offset, resolved, next_ar;
		u32 base;
		u16 bk;
	};
	static extended_banz_fixture extended_banz_case(unsigned variant);
	void start_pipeline_extended_banz(unsigned variant = 0);
	struct immediate_arithmetic_fixture
	{
		unsigned selector, shift, flags, gap, execute_cycle;
		bool producer, add;
		u16 opcode, literal, st0, st1, result_st0;
		u64 source, result;
		u32 base;
	};
	static immediate_arithmetic_fixture immediate_arithmetic_case(unsigned variant);
	void start_pipeline_immediate_arithmetic(unsigned variant = 0);
	struct register_arithmetic_fixture
	{
		unsigned selector, gap, execute_cycle;
		bool producer;
		u16 opcode, st0, st1, result_st0;
		std::array<u64, 2> initial, before, after;
		u32 base;
	};
	static register_arithmetic_fixture register_arithmetic_case_full(unsigned variant);
	static register_arithmetic_fixture register_load_case(unsigned variant);
	void start_pipeline_accumulator_arithmetic(unsigned variant = 0);
	struct memory_multiply_fixture
	{
		memory_arithmetic_fixture operand;
		u64 a, b, result;
		u16 result_st0, pmst, t = 0x9abc;
	};
	static memory_multiply_fixture memory_multiply_case(unsigned variant, bool square = false);
	static memory_arithmetic_fixture delay_copy_case(unsigned variant);
	void start_delay_copy(unsigned variant = 0);
	void start_pipeline_memory_multiply(unsigned variant = 0);
	static std::array<u64, 5> t_multiply_pair_case(unsigned variant);
	void start_pipeline_t_multiply_pair(unsigned variant = 0);
	struct shifted_arithmetic_fixture
	{
		memory_arithmetic_fixture operand;
		u16 extension, result_st0;
		u64 a, b, result;
	};
	static shifted_arithmetic_fixture shifted_arithmetic_case(unsigned variant, bool bk = false);
	void start_pipeline_shifted_arithmetic(unsigned variant = 0);
	struct memory_repeat_fixture
	{
		bool long_form, destination;
		unsigned count, interrupt, flags, form, pattern;
		u32 base, body;
		u16 opcode, divisor;
		u64 dividend;
	};
	static memory_repeat_fixture memory_repeat_case(unsigned variant);
	static std::array<u64, 2> repeat_subc_result(memory_repeat_fixture const &f, unsigned iterations);
	void start_pipeline_memory_repeat(unsigned variant = 0);
	struct banzd_fixture
	{
		unsigned encoded, selected, modification;
		bool compatibility, two_word;
		u16 value, next_ar;
		u32 base;
	};
	static banzd_fixture banzd_case(unsigned variant);
	void start_pipeline_banzd(unsigned variant = 0);
	struct stlm_t_fixture
	{
		bool trn, source;
		unsigned gap, read_cycle;
		u16 stored, consumed, memory, st0, st1, pmst, result_st0, consumer;
		u64 value, result;
		u32 base;
		bool brc = false;
	};
	static stlm_t_fixture stlm_t_case(unsigned variant);
	void start_stlm_bsp(unsigned variant = 0);
	void start_stlm_bsp_edge(unsigned variant = 0);
	void start_pipeline_stlm_t(unsigned variant = 0);
	static u16 swap_body_source(unsigned variant);
	void start_pipeline_swap_body(unsigned variant = 0);
	void start_pipeline_swap_join(unsigned variant = 0);
	static unsigned rptb_count(unsigned variant);
	static u16 rptb_source(unsigned variant, unsigned index);
	void start_pipeline_rptb(unsigned variant = 0);
	static unsigned rptbd_seed(unsigned v);
	static unsigned rptbd_count(unsigned v);
	static unsigned rptbd_layout(unsigned v);
	static u32 rptbd_base(unsigned v);
	static u16 rptbd_source(unsigned v, unsigned i);
	static int rptbd_irq_line(unsigned v);
	static bool rptbd_late_irq(unsigned v);
	struct extended_push_fixture
	{
		unsigned form, selected, pattern, irq, checkpoint;
		bool mapped, compatibility, compiler;
		u32 base;
		u16 mode, literal, address, value, st0, final_st0, st1, bk, t, trn, pmst;
		u64 a, b;
		std::array<u16, 8> initial, final;
	};
	static extended_push_fixture extended_push_case(unsigned v);
	void start_extended_push(unsigned variant = 0);
	static extended_push_fixture extended_pop_case(unsigned variant);
	void start_extended_pop(unsigned variant = 0);
	void start_extended_push_edge(unsigned variant = 0);
	void start_extended_pop_edge(unsigned variant = 0);
	struct stlm_pmst_fixture
	{
		unsigned pattern, irq, checkpoint;
		bool saturation, sign, source_b, compiler, old_overlay, low_entry, late;
		u32 base;
		u16 entry, old_pmst, value, st1;
		u64 source;
	};
	static stlm_pmst_fixture stlm_pmst_case(unsigned v);
	static u32 pmst_fixture_physical(u32 base, u16 address, u16 pmst);
	void start_stlm_pmst(unsigned variant = 0);
	void start_stlm_pmst_edge(unsigned variant = 0);
	struct repeat_load_fixture
	{
		unsigned selector, flags, pattern, count, setup, irq, checkpoint, first, outer;
		bool asm_form, block, arithmetic;
		int shift;
		u32 base, body;
		u16 word, st0, st1, address;
	};
	repeat_load_fixture repeat_load_case(unsigned v);
	static std::array<u64, 3> repeat_load_initial(repeat_load_fixture const &f);
	static std::array<u64, 3> repeat_load_step(repeat_load_fixture const &f, std::array<u64, 3> state);
	void start_repeat_load(unsigned variant = 0);
	void start_repeat_load_edge(unsigned variant = 0);
	struct basic_repeat_fixture
	{
		unsigned form, count, irq, checkpoint, selected, modification;
		bool compiler, compatibility, absolute;
		u32 base, body;
		u16 mode, address, st0, st1;
	};
	static basic_repeat_fixture basic_repeat_case(unsigned v);
	void start_basic_repeat(unsigned variant = 0);
	void start_basic_repeat_edge(unsigned variant = 0);
	struct rptbd_shift_store_fixture
	{
		unsigned count, form, kind, flags, irq, checkpoint;
		int shift;
		bool compiler;
		u32 base;
		u16 mode, address, initial_ar3, final_ar3, value, st0, st1;
		u64 source;
	};
	static rptbd_shift_store_fixture rptbd_shift_store_case(unsigned v);
	void start_rptbd_shift_store(unsigned variant = 0);
	void start_rptbd_shift_store_edge(unsigned variant = 0);
	struct rptbd_square_fixture
	{
		unsigned operation, destination, pattern, flags, layout, form, count, snapshot;
		u32 base;
		bool memory_square = false;
		unsigned operations() const { return layout >= 2 ? 2 : 1; }
		unsigned slot(unsigned ordinal) const { return layout == 1 ? 1 : ordinal; }
		unsigned target(unsigned ordinal) const { return memory_square && layout == 4 && ordinal ? 1 : destination ^ unsigned(layout == 3 && ordinal); }
		u16 memory(unsigned ordinal) const { return std::array<u16, 8>{0, 1, 0xffff, 0x7fff, 0x8000, 0x8001, 0x1234, 0xf495}[(pattern + (form == 1 ? 0 : ordinal * 3)) % 8]; }
		u16 address(unsigned ordinal) const { return (form ? 0x5000 : 0x1060) + (form == 1 ? 0 : ordinal); }
		u16 opcode(unsigned slot) const { if ((layout == 0 && slot == 1) || (layout == 1 && slot == 0)) return 0xf495; unsigned const ordinal = layout >= 2 ? slot : 0; u16 const family = memory_square ? layout == 4 && ordinal ? 0x3100 : 0x2600 | (target(ordinal) << 8) : 0x3800 | (operation << 9) | (target(ordinal) << 8); return family | (form == 0 ? 0x60 + ordinal : form == 1 ? 0x85 : 0x95); }
		u16 st0() const { return 0xa820 | (pattern & 1 ? 0x600 : 0); }
		u16 st1() const { return 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 8 ? 0x100 : 0); }
		std::array<u64, 3> result(unsigned completed) const
		{
			u64 const initial = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x1234567833ULL, 0x80aa001234ULL}[pattern];
			std::array<u64, 3> out{initial, (initial ^ 0x5555555555ULL) & 0xffffffffffULL, st0()};
			for (unsigned i = 0; i < completed; ++i)
			{
				unsigned const dst = target(i);
				s64 const old = BIT(out[dst], 39) ? s64(out[dst]) - (s64(1) << 40) : s64(out[dst]);
				s64 const value = s16(memory(i));
				s64 product = value * value * (flags & 1 ? 2 : 1);
				if (memory_square && layout == 4 && i) { s32 const high = (out[0] >> 16) & 0x1ffff; product = s64(high & 0x10000 ? high - 0x20000 : high) * value * (flags & 1 ? 2 : 1); }
				s64 const sum = memory_square ? product : operation ? old - product : old + product;
				bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
				out[2] |= overflow ? dst ? 0x200 : 0x400 : 0;
				out[dst] = u64(overflow && (flags & 2) ? std::clamp(sum, s64(-2147483648LL), s64(2147483647LL)) : sum) & 0xffffffffffULL;
			}
			return out;
		}
	};
	rptbd_square_fixture rptbd_square_case(unsigned v) const;
	void start_rptbd_square(unsigned variant = 0);
	void start_rptbd(unsigned variant = 0);
	void start_rptbd_edge(unsigned variant = 0);
	void start_pipeline_rptb_edge(unsigned variant = 0);
	struct memory_immediate_fixture
	{
		unsigned form, read, access, checkpoint, words;
		u16 mode, address, word, literal, extension, st0, st1, result_st0;
		u32 base;
		u64 a, b, result;
		bool bdst;
		bool bsrc = false, accumulate = false;
		u16 pmst = 0;
	};
	static memory_immediate_fixture memory_immediate_case(unsigned variant);
	void start_memory_immediate_multiply(unsigned variant = 0);
	struct mapped_immediate_mac_fixture
	{
		memory_immediate_fixture f;
		unsigned pointer, pattern;
		u64 late_a() const { return std::array<u64,4>{0,1,0x7fffffff,0xff80000000ULL}[pattern]; }
		u64 late_b() const { return std::array<u64,4>{0xffffffffffULL,0x0080000000ULL,0xff80000001ULL,0x123456789aULL}[pattern]; }
	};
	static mapped_immediate_mac_fixture mapped_immediate_mac_case(unsigned v);
	void start_mapped_immediate_mac(unsigned variant=0);
	void start_memory_immediate_mac_exclusion(unsigned variant = 0);
	void start_source_alias_case(unsigned variant = 0);
	void start_source_domains(unsigned variant = 0);
	bool check_source_domains();
	void start_short_read_mod3_edge(unsigned variant = 0);
	void start_mmr_long_edge(unsigned variant = 0);
	static unsigned block_branch_count(unsigned variant);
	static u16 block_branch_source(unsigned variant, unsigned i);
	struct block_exit_fixture
	{
		unsigned count, exit, reads, body_writes, writes, checkpoint, form;
		u32 base;
		u16 end;
		bool b, taken, endpoint_store, delayed, reverse_delay;
		u16 tail() const { return delayed ? 0x607 : 0x605; }
		u16 value(bool condition) const
		{
			if (!delayed || !form) return condition ? 0 : 1;
			switch ((form - 1) % 6)
			{
			case 0: return condition ? 0 : 0xffff; // >= 0
			case 1: return condition ? 0xffff : 0; // < 0
			case 2: return condition ? 1 : 0; // != 0
			case 3: return condition ? 0 : 1; // == 0
			case 4: return condition ? 1 : 0; // > 0
			default: return condition ? 0 : 1; // <= 0
			}
		}
	};
	static block_exit_fixture block_exit_case(unsigned v, bool delayed = false);
	void start_block_exit(unsigned variant = 0);
	void start_block_branch(unsigned variant = 0);
	void start_block_branch_edge(unsigned variant = 0);
	struct delay_multiply_fixture
	{
		unsigned kind, form, destination, flags, checkpoint;
		u16 memory, t, source;
		u32 caller, target;
		u64 a, b, result;
		bool accumulate = false;
		unsigned operation = 0;
		bool accumulator_source = false;
		bool rounded = false;
		bool square = false;
		bool memory_square = false;
		bool extended_accumulator = false;
		unsigned address_ar = 2;
		u16 extension() const { return extended_accumulator ? form == 2 ? source : form ? 0xeb08 : 3 : form == 3 ? 3 : source; }
		unsigned delay() const { return kind == 5 ? 1 : 2; }
		static unsigned operand_read(unsigned kind, unsigned form) { return 7 + unsigned(form != 0) + unsigned(kind == 5 || ((kind == 1 || kind == 2) && form)); }
		unsigned read() const { return extended_accumulator ? 8 : operand_read(kind, accumulator_source ? 0 : form); }
		unsigned literal_clock() const { return kind == 5 ? 4 : kind == 1 || kind == 2 ? 6 : 5; }
		s64 arithmetic() const
		{
			if (memory_square) return s64(s16(memory)) * s16(memory) * (flags & 1 ? 2 : 1);
			if (square)
			{
				s64 const product = s64(s16(memory)) * s16(memory) * (flags & 1 ? 2 : 1);
				u64 const source = destination ? b : a;
				s64 const current = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
				return operation ? current - product : current + product;
			}
			if (accumulator_source)
			{
				s32 const bits = (a >> 16) & 0x1ffff;
				s64 product = s64(bits & 0x10000 ? bits - 0x20000 : bits) * s16(memory) * (flags & 1 ? 2 : 1);
				if (operation && (flags & 7) == 7) product = std::clamp(product, s64(-2147483648LL), s64(2147483647LL));
				s64 const old_b = BIT(b, 39) ? s64(b) - (s64(1) << 40) : s64(b);
				s64 const value = !operation ? product : operation == 1 ? old_b - product : old_b + product;
				return operation == 3 ? (value + 32768) & ~s64(65535) : value;
			}
			s64 product = s64(s16(memory)) * s16(t) * (flags & 1 ? 2 : 1);
			if (!rounded && (flags & 7) == 7) product = std::clamp(product, s64(-2147483648LL), s64(2147483647LL));
			if (rounded) product = (product + 32768) & ~s64(65535);
			if (!accumulate) return product;
			u64 const source = destination ? b : a;
			s64 sum = (BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source)) + (operation & 2 ? -product : product);
			return operation & 1 ? (sum + 32768) & ~s64(65535) : sum;
		}
		bool overflow() const { return arithmetic() < -2147483648LL || arithmetic() > 2147483647LL; }
	};
	static delay_multiply_fixture delay_multiply_case(unsigned variant, bool mac = false, bool rounded = false);
	static delay_multiply_fixture delay_accumulator_multiply_case(unsigned variant);
	static delay_multiply_fixture bd_long_accumulator_case(unsigned v);
	static delay_multiply_fixture delay_square_case(unsigned variant);
	static delay_multiply_fixture delay_memory_square_case(unsigned variant);
	void start_delay_multiply(unsigned variant = 0);
	struct t_move_fixture
	{
		unsigned form, destination, flags, gap, checkpoint;
		u16 value, source;
		u32 base;
		unsigned words() const { return form >= 3 ? 3 : 2; }
		unsigned read() const { return 5 + unsigned(form >= 3); }
		unsigned consume() const { return read() + (destination ? 2 : 3) + gap; }
	};
	static t_move_fixture t_move_case(unsigned variant);
	void start_t_register_move(unsigned variant = 0);
	struct delay_store_fixture
	{
		unsigned kind, operation, mode, checkpoint;
		u16 literal, address, next_ar, value;
		u32 base;
		u64 a, b;
		unsigned delay() const { return kind == 4 ? 1 : 2; }
		unsigned literal_clock() const { return kind == 4 ? 4 : kind == 1 || kind == 2 ? 6 : 5; }
		// SPRU131G example 7-8 holds RETD's prefetched words until its stack Read completes.
		unsigned write_clock() const { return kind == 0 || kind == 3 ? 9 : 10; }
	};
	static delay_store_fixture delay_store_case(unsigned variant);
	void start_delay_store(unsigned variant = 0);
	void start_delay_store_edge(unsigned variant = 0);
	void start_stlm_bk(unsigned variant = 0);
	static u16 store_bypass_value(unsigned variant);
	static u16 xmem_store_bypass_value(unsigned variant);
	void start_block_xc_irq(unsigned variant = 0);
	struct block_xc_fixture
	{
		unsigned predicate, layout, count, snapshot, rotation;
		bool endpoint;
		u32 base;
		bool b() const { return predicate >= 6 && predicate < 12; }
		u16 word(unsigned i) const { return std::array<u16, 8>{0, 1, 0x8000, 0xffff, 0x7fff, 2, 0x8001, 0xfffe}[(i + rotation * 4) % 8]; }
		bool taken(unsigned i) const
		{
			if (predicate >= 12) return predicate == 14 || ((predicate == 13 || predicate == 15) ? bool(rotation) : !rotation);
			s32 const value = s16(word(i));
			return std::array<bool, 6>{value >= 0, value < 0, value != 0, value == 0, value > 0, value <= 0}[predicate % 6];
		}
		unsigned selected(unsigned ordinal) const { for (unsigned i = 0; i <= count; ++i) if (taken(i) && !ordinal--) return i; return count + 1; }
		unsigned first() const { unsigned n = 0; for (unsigned i = 0; i <= count; ++i) n += taken(i); return n; }
		unsigned second() const { return layout == 1 ? first() : layout >= 2 && endpoint ? 1 : count + 1; }
	};
	static block_xc_fixture block_xc_case(unsigned v);
	void start_block_xc(unsigned variant = 0);
	void start_block_xc_edge(unsigned variant = 0);
	struct move_brc_fixture
	{
		unsigned form, count, gap, snapshot, read_tick;
		bool delayed;
		u32 base;
		u16 source, opcode, offset, st1;
		unsigned control_kind = 0;
	};
	static move_brc_fixture move_brc_case(unsigned variant, bool control = false);
	void start_move_brc(unsigned variant = 0);
	void start_move_repeat_owner(unsigned variant = 0);
	void start_move_brc_edge(unsigned variant = 0);
	static u16 asm_store_bypass_value(unsigned variant);
	static u16 nibble_input(unsigned variant);
	void start_original_nibble(unsigned variant = 0);
	void start_store_bypass(unsigned variant = 0);
	void start_software_intr(unsigned variant = 0);
	struct immediate_multiply_fixture
	{
		bool producer, source, destination;
		unsigned start, read_cycle, checkpoint;
		u16 t, literal, st0, st1, pmst, result_st0;
		u64 a, b, result;
		u32 base;
	};
	static immediate_multiply_fixture immediate_multiply_case(unsigned variant, bool mac = false);
	void start_pipeline_immediate_multiply(unsigned variant = 0);
	struct multiply_mapped_fixture
	{
		bool producer_b, consumer_b;
		unsigned gap, checkpoint;
		u16 t, literal, source, st1, sampled;
	};
	static multiply_mapped_fixture multiply_mapped_case(unsigned variant);
	void start_pipeline_multiply_mapped(unsigned variant = 0);
	struct immediate_mapped_fixture
	{
		bool original, selected_b;
		unsigned gap, checkpoint;
		u16 opcode, literal, source, sampled, bits, st1;
		u32 base;
		u64 initial, result;
	};
	static immediate_mapped_fixture immediate_mapped_case(unsigned variant);
	void start_immediate_mapped(unsigned variant = 0);
	void start_long_mapped_original(unsigned variant = 0);
	struct dst_bypass_fixture
	{
		bool producer_b, sxm, sst, same;
		unsigned kind, mode, gap, checkpoint;
		u32 base, stored;
		u64 source, loaded;
	};
	static dst_bypass_fixture dst_bypass_case(unsigned variant);
	void start_dst_bypass(unsigned variant=0);
	void start_pll_transition_case();
	void start_rate_dma(bool input_clock = false);
	void start_clkmd_rate_case();
	void start_idle3_pll_relock_case();
	void start_vector_prefetch_case(unsigned index);
	void check_mmr_bus();
	void start_timer_boundary_probe(unsigned index, bool staged = false);
	void start_intm_protection_case(unsigned index);
	static u16 reverse_carry_reference(u16 address, u16 offset, bool subtract);
	void start_reverse_carry_case(unsigned index);
	void start_reverse_carry_sequence(bool subtract);
	void start_shifted_high_store_case(unsigned index);
	void start_asm_store_case(unsigned index);
	void start_delayed_call_case(unsigned index);
	void start_acc_delayed_case(unsigned index);
	void start_compact_store_case(unsigned index);
	void start_software_interrupt_case(unsigned index, bool trap = false);
	void start_trn_store_case(unsigned index);
	void start_indirect_pshm_case(unsigned index);
	void start_parallel_load_multiply_case(unsigned index);
	void start_rounded_load_case(unsigned index);
	void start_return_alt_case(unsigned index);
	void start_stack_address_latency_case(unsigned index);
	void start_immediate_dp_case(unsigned index);
	void start_compact_load_case(unsigned index);
	void start_bitf_offset_case(unsigned index);
	void start_cmpm_offset_case(unsigned index);
	void start_carry_branch_case(unsigned index);
	static constexpr u16 cmpr_values[][2] = {{0, 0}, {0, 1}, {1, 0}, {0xffff, 0x8000}, {0x8000, 0xffff}, {0xffff, 0xffff}};
	void start_cmpr_case(unsigned index);
	void start_banz_offset_case(unsigned index);
	void start_bit_case(unsigned index);
	static constexpr u16 bio_opcodes[] = {
		0xf802, 0xf803, 0xfa02, 0xfa03, 0xf902, 0xf903,
		0xfc02, 0xfc03, 0xfe02, 0xfe03, 0xfd02, 0xfd03, 0xff02, 0xff03
	};
	void start_bio_case(unsigned index);
	static constexpr u16 far_opcodes[] = {
		0xf882, 0xfa82, 0xf4e6, 0xf5e6, 0xf6e6, 0xf7e6,
		0xf982, 0xfb82, 0xf4e7, 0xf5e7, 0xf6e7, 0xf7e7,
		0xf4e4, 0xf6e4, 0xf4e5, 0xf6e5
	};

	void start_far_case(unsigned index);

	void start_far_save_case(bool returning);

	void start_pipeline_rmw_forward(unsigned variant = 0);

	struct negate_fixture
	{
		u64 a, b, source, result;
		u16 opcode, st0, result_st0, st1;
		u32 base;
		unsigned prefix;
	};
	static negate_fixture negate_case(unsigned variant, bool absolute = false);
	void start_pipeline_negate(unsigned variant = 0);

	struct mapped_word_arithmetic_fixture
	{
		u64 a, b, after_a, after_b, result;
		u16 t, trn, address, memory, opcode, st0, st1, result_st0;
		u32 base;
		unsigned prefix, read_cycle, mutation_state;
		bool absolute;
	};
	static mapped_word_arithmetic_fixture mapped_word_arithmetic_case(unsigned variant);
	void start_pipeline_mapped_word_arithmetic(unsigned variant = 0);

	struct mapped_bitfield_fixture
	{
		mapped_word_arithmetic_fixture registers;
		u16 memory, mask, st0;
		unsigned mutation_state;
	};
	bool status_bitfield() const;
	mapped_bitfield_fixture mapped_bitfield_case(unsigned variant) const;
	void start_pipeline_mapped_bitfield(unsigned variant = 0);

	struct bus_io_fixture
	{
		unsigned waits, first, complete, checkpoint, interrupt;
		bool write, hold;
		u16 value;
	};
	bool standard_bus_io() const;
	bool atomic_bus_io() const;
	unsigned bus_io_form(unsigned variant) const;
	u16 bus_io_address(unsigned variant) const;
	u64 bus_io_clock() const;
	bus_io_fixture bus_io_case(unsigned variant) const;
	void start_bus_io_test(unsigned variant = 0);
	void start_standard_swwsr_test();
	void start_bus_register_test();
	void start_atomic_io_composition(unsigned variant = 0);
	struct mvdd_fixture
	{
		unsigned x, y, count, irq, prefix, snapshot;
		bool repeated, linear;
		u32 base;
		u16 input, st0, st1;
		std::array<u16, 4> initial, final;
		std::array<u16, 16> source, destination, word;
		unsigned first_write() const { return 6 + prefix; }
		static u16 seed(unsigned address) { return 0xa500U ^ ((address - 0x1000) * 73U); }
	};
	static mvdd_fixture mvdd_case(unsigned v, bool repeated);
	void start_mvdd(unsigned variant = 0);
	struct popd_fixture
	{
		unsigned mode, count, prefix, irq, snapshot;
		bool repeated, cmpt;
		u32 base;
		u16 pointer, st0, st1;
		int step;
		std::array<u16, 16> destination, word;
		std::array<u16, 21> initial;
		static constexpr u16 reg(unsigned i) { return i < 16 ? 8 + i : 0x19 + i - 16; }
		unsigned read_clock(unsigned i = 0) const { return 5 + prefix + i; }
		unsigned write_clock(unsigned i) const { return read_clock(i) + unsigned(destination[i] < 0x10 || destination[i] > 0x19); }
		u32 write_mask(unsigned tick) const { u32 mask = 0; for (unsigned i = 0; i <= count; ++i) if (tick >= write_clock(i)) mask |= 1U << i; return mask; }
		unsigned write_count(unsigned tick) const { unsigned n = 0; for (unsigned i = 0; i <= count; ++i) if (tick >= write_clock(i)) ++n; return n; }
		u16 opcode() const { return 0x8b00 | mode; }
	};
	static popd_fixture popd_case(unsigned v, bool repeated);
	void start_popd_mapped(unsigned variant = 0);
	void start_popd_edges(unsigned variant = 0);
	struct accumulator_branch_fixture
	{
		bool near, b;
		unsigned producer, irq, prefix, wait, snapshot;
		u32 base, destination, table;
		u16 hint, st0, st1;
		u64 initial, other;
		unsigned sample() const { return 5 + prefix + wait; }
		u16 opcode() const { return (near ? 0xf4e2 : 0xf4e6) | (b ? 0x100 : 0); }
		u16 marker() const { return b ? u16(other) : 0xbeef; }
	};
	accumulator_branch_fixture accumulator_branch_case(unsigned v);
	void start_accumulator_branch(unsigned variant = 0);
	struct offset_logic_fixture
	{
		unsigned operation, encoded, selected, kind, snapshot;
		bool b, cmpt, linear;
		u16 literal, address, next, st0, st1, value;
		u32 base;
		u64 source;
		static u16 initial_ar(unsigned r) { return 0x100c + r * 0x100; }
		u16 opcode() const { return (0x1800 + operation * 0x200) | (b ? 0x100 : 0) | (0xe0 + kind * 8 + encoded); }
		u16 final_st0() const { return cmpt && encoded ? (st0 & 0x1fff) | (encoded << 13) : st0; }
		u64 result() const { return operation == 0 ? source & value : operation == 1 ? source | value : source ^ value; }
	};
	static offset_logic_fixture offset_logic_case(unsigned v);
	void start_offset_logic(unsigned variant = 0);
	void start_offset_logic_edges(unsigned variant = 0);
	struct absolute_logic_fixture
	{
		unsigned kind, snapshot;
		u32 base;
		u16 address, value, status;
		u64 source;
		u16 opcode() const { return 0x18f8 + (kind / 2) * 0x200 + (kind & 1) * 0x100; }
		u64 result() const { return kind < 2 ? source & value : kind < 4 ? source | value : source ^ value; }
	};
	static absolute_logic_fixture absolute_logic_case(unsigned v);
	void start_absolute_logic(unsigned variant = 0);
	struct t_store_fixture
	{
		unsigned form, reg, selected, kind, flags, layout, consumer_read, checkpoint;
		bool consumer, compatibility, long_consumer, merge;
		u32 base;
		u16 mode, literal, word, memory, st0, st1, pmst, final_st0;
		u64 value, result;
		std::array<u16, 8> initial, final;
	};
	static t_store_fixture t_store_case(unsigned v, bool consumer);
	void start_t_store(unsigned variant = 0);
	void start_t_store_edge(unsigned variant = 0);
	struct ts_arithmetic_fixture
	{
		memory_arithmetic_fixture address;
		unsigned selector, prefix, producer_kind, gap, checkpoint;
		bool producer;
		u32 base;
		u16 memory, t, sampled_t, result_st0;
		u64 a, b, result;
	};
	static ts_arithmetic_fixture ts_arithmetic_case(unsigned variant, bool producer);
	void start_ts_arithmetic(unsigned variant = 0);
	void start_ts_arithmetic_edge(unsigned variant = 0);
	struct delay_load_fixture
	{
		unsigned kind, form, ar, selected, destination, flags, read, checkpoint;
		bool compatibility;
		u16 literal, address, memory, st0, final_st0, st1;
		u32 caller, target;
		u64 a, b, result;
		std::array<u16, 8> initial, final;
		u16 memory_low = 0;
		unsigned delay() const { return kind == 5 ? 1 : 2; }
		unsigned literal_clock() const { return kind == 5 ? 4 : kind == 1 || kind == 2 ? 6 : 5; }
	};
	static delay_load_fixture delay_load_case(unsigned v, bool shifted = false);
	static delay_load_fixture delay_shift_alu_case(unsigned variant);
	static delay_load_fixture delay_dld_case(unsigned v);
	static delay_load_fixture delay_add_case(unsigned v);
	void start_delay_load(unsigned variant = 0);
	void start_delay_load_edge(unsigned variant = 0);
	void start_shift_delay_edge(unsigned variant = 0);
	struct block_back_fixture
	{
		unsigned outer, inner, kind, interrupt, checkpoint;
		bool b;
		u32 base;
		u16 end, st1;
	};
	static block_back_fixture block_back_case(unsigned v);
	void start_block_back(unsigned variant = 0);
	struct block_page_fixture
	{
		unsigned count, checkpoint;
		u32 caller, callee;
		bool staged, hazard, delayed;
	};
	static block_page_fixture block_page_case(unsigned variant);
	void start_block_page(unsigned variant = 0);
	struct block_far_call_fixture
	{
		unsigned count, layout, depth, interrupt, checkpoint, delay_form;
		u32 caller, callee, nested;
		u16 call, st1;
		bool delayed;
	};
	static block_far_call_fixture block_far_call_case(unsigned v, bool delayed = false);
	void start_block_far_call(unsigned variant = 0);
	void start_block_far_call_edge(unsigned variant = 0);
	struct block_near_call_fixture
	{
		unsigned count, layout, depth, interrupt, checkpoint;
		u32 base;
		u16 call, end, st1;
		bool delayed, delayed_return;
	};
	static block_near_call_fixture block_near_call_case(unsigned v);
	void start_block_near_call(unsigned variant = 0);
	void start_block_near_call_edge(unsigned variant = 0);
	struct block_single_fixture
	{
		unsigned outer, inner, kind, checkpoint, total;
		bool tail;
		u32 base;
		u16 end;
		bool memory = false;
		unsigned count_form = 0, body = 0x604;
	};
	block_single_fixture block_single_case(unsigned v) const;
	void start_block_single(unsigned variant = 0);
	void start_block_single_edge(unsigned variant = 0);
	struct asm_mapped_fixture
	{
		unsigned source, form, raw, flags, ar, read, checkpoint;
		u32 base;
		u16 word, st1, t, trn;
		u64 a, b;
		std::array<u16, 8> initial, final;
	};
	static asm_mapped_fixture asm_mapped_case(unsigned v);
	void start_asm_mapped(unsigned variant = 0);
	struct asm_load_fixture
	{
		unsigned form, ar, selected, raw, flags, read, consumer_clock, checkpoint, operation, gap;
		bool consumer, compatibility;
		u32 base;
		u16 mode, word, st0, st1, final_st0;
		u64 a, b, result;
		std::array<u16, 8> initial, final;
	};
	static asm_load_fixture asm_load_case(unsigned v, bool consumer);
	void start_asm_load(unsigned variant = 0);
	void start_asm_edge(unsigned variant = 0);
	void start_poll_interrupt(unsigned variant = 0);
	struct delay_compare_fixture
	{
		unsigned kind, form, interrupt, prefix, read, literal_clock, checkpoint;
		bool bitfield, initial_tc, taken, final_tc;
		u16 memory, literal;
		u32 base;
	};
	static delay_compare_fixture delay_compare_case(unsigned v);
	void start_delay_compare(unsigned variant = 0);
	void start_delay_compare_edge(unsigned variant = 0);
	struct indexed_store_fixture
	{
		unsigned ar, selected, selector, snapshot;
		bool add, compatibility, excluded;
		u32 base;
		u64 value;
		u16 word, st0, st1, pmst, address, final_ar, bk;
		u8 mode;
		std::array<u16, 8> initial;
	};
	static indexed_store_fixture indexed_store_case(unsigned v, bool immediate = false);
	void start_indexed_store(unsigned variant = 0);
	static indexed_store_fixture indexed_load_case(unsigned v);
	static indexed_store_fixture circular_load_case(unsigned v);
	static indexed_store_fixture circular_store_case(unsigned v);
	static indexed_store_fixture asm_circular_store_case(unsigned v);
	void start_indexed_load(unsigned variant = 0);
	struct minmax_fixture
	{
		unsigned selector, snapshot;
		u32 base;
		u64 a, b, result;
		u16 st0, st1, result_st0;
	};
	static minmax_fixture minmax_case(unsigned v, bool square = false);
	void start_minmax(unsigned variant = 0);
	struct indexed_mar_fixture
	{
		unsigned ar, selected, snapshot;
		bool add, compatibility;
		u32 base;
		u16 index, result, st0;
		std::array<u16, 8> initial;
	};
	static indexed_mar_fixture indexed_mar_case(unsigned v, bool reverse = false);
	void start_indexed_mar(unsigned variant = 0);
	struct dual_multiply_fixture
	{
		unsigned x, y, destination, snapshot, flags, source, operation;
		bool linear;
		u32 base;
		std::array<u16, 4> initial, final;
		u16 xaddress, yaddress, xword, yword, st0, st1, pmst, result_st0, stored;
		u64 result;
	};
	static dual_multiply_fixture dual_multiply_case(unsigned variant, bool edge, bool alu = false, bool macsu = false, bool signed_mac = false, bool family = false, bool parallel = false);
	static u64 dual_macsu_seed(unsigned variant, bool edge);
	void start_dual_multiply(unsigned variant = 0);
	struct mapped_dual_fixture
	{
		u32 base;
		u16 opcode, xaddress, yaddress, xword, yword, st0, st1, result_st0, bk, imr = 0, ifr = 0;
		unsigned checkpoint, destination;
		u64 a, b, result;
		std::array<u16, 8> initial, final;
	};
	static mapped_dual_fixture mapped_dual_case(unsigned variant, bool edge, bool interrupt_registers = false, bool low_bus = false);
	void start_mapped_dual(unsigned variant = 0);
	struct repeated_stld_fixture
	{
		unsigned form, layout, iterations, checkpoint;
		u16 st1, pmst;
		u32 base;
		bool arithmetic = false;
	};
	repeated_stld_fixture repeated_stld_case(unsigned v) const;
	static u16 repeated_stld_address(repeated_stld_fixture const &f, unsigned i, bool y);
	static u16 repeated_stld_sample(unsigned i);
	static u16 repeated_stld_store(repeated_stld_fixture const &f, std::array<u64, 3> const &state);
	void start_repeated_stld(unsigned variant = 0);
	struct parallel_load_fixture
	{
		dual_multiply_fixture memory;
		unsigned form;
		u64 a, b, result_a, result_b;
		u16 imr = 0, ifr = 0;
		u16 t = 0xbeef;
	};
	static void finish_parallel_load_case(parallel_load_fixture &c);
	static parallel_load_fixture parallel_load_case(unsigned variant, bool edge, bool original_edge_sources = false);
	static parallel_load_fixture mapped_parallel_load_case(unsigned v);
	static parallel_load_fixture parallel_arithmetic_case(unsigned variant, bool edge);
	static parallel_load_fixture parallel_multiply_case(unsigned variant, bool edge);
	static parallel_load_fixture bsp_parallel_load_case(unsigned variant);
	static parallel_load_fixture peripheral_parallel_load_case(unsigned variant);
	void start_parallel_load(unsigned variant = 0);
	struct sqdst_fixture
	{
		dual_multiply_fixture memory;
		u64 a, b, result_a, result_b;
		u16 result_st0;
	};
	static sqdst_fixture sqdst_case(unsigned variant, bool edge, bool lms = false);
	void start_sqdst(unsigned variant = 0);
	struct circular_move_fixture
	{
		indexed_store_fixture memory;
		u16 destination, word, sp = 0x2200, bk, brc = 0, t = 0xbeef, trn = 0x1357;
		u64 a = 0x123456789aULL, b = 0xfeabcd9876ULL;
		std::array<u16, 8> final;
		bool excluded;
		unsigned write_clock;
	};
	static circular_move_fixture circular_move_case(unsigned variant, bool edge);
	static circular_move_fixture indexed_move_case(unsigned variant, bool edge);
	void start_circular_move(unsigned variant = 0);
	struct repeated_mvdk_fixture
	{
		u16 opcode, destination, bk, st0, st1;
		u32 base;
		unsigned count, snapshot;
		std::array<u16, 8> initial, final;
		std::array<u16, 16> sources, values;
		bool excluded;
		static u16 seed(u16 address) { return u16((unsigned(address) * 0x3917) ^ 0xa55a); }
	};
	static repeated_mvdk_fixture repeated_mvdk_case(unsigned variant, bool edge = false, bool indexed = false);
	struct rptbd_t_load_fixture
	{
		unsigned count, slot, form, snapshot, accumulator;
		u16 value, status, mode, source;
		u32 base;
		bool branch = false;
		unsigned branch_count = 0;
		bool ordinary = false;
		unsigned control = 0, late_t = 0;
		unsigned read_clock() const { return ordinary ? delay_multiply_fixture::operand_read(control, slot) : 7 + slot; }
		u16 target() const { return accumulator ? 0x580 : 0x610; }
		u32 target_base() const { return ordinary && control == 4 ? base ^ 0x20000 : base; }
		u16 loaded_t() const { return late_t == 2 ? 0x5aa5 : late_t ? value ^ 0xffff : value; }
		u16 product() const { return u16(s32(s16(loaded_t())) * 3); }
		u16 final_sp() const { return 0x1e00 + (ordinary ? control == 3 ? -1 : control == 4 ? -2 : control == 5 ? 1 : 0 : 0); }
	};
	rptbd_t_load_fixture rptbd_t_load_case(unsigned v) const;
	void start_rptbd_t_load(unsigned variant = 0);
	struct branch_repeat_fixture
	{
		unsigned kind, count, following, checkpoint, limit;
		bool staged;
		u32 base;
		u16 target, seed, status;
		bool conditional = false;
		unsigned condition = 0, predicate_pattern = 0;
		u16 body() const { return std::array<u16,3>{0x8094,0xe5ab,0xf495}[kind]; }
		u64 tested_value() const { return std::array<u64,4>{0,1,0xffffffffffULL,0x0080000000ULL}[predicate_pattern]; }
		bool test(u64 value) const { bool const negative = value & 0x8000000000ULL, zero = !value; return std::array<bool,6>{!negative,negative,!zero,zero,!negative && !zero,negative || zero}[condition % 6]; }
		bool taken() const { return !conditional || test(tested_value()); }
		u64 poisoned_value() const { for (u64 value : {0ULL,1ULL,0xffffffffffULL}) if (test(value) != taken()) return value; return 0; }
		u64 a(bool final = false) const { return conditional && condition < 6 ? final && staged ? poisoned_value() : tested_value() : 0x1234560000ULL | seed; }
		u64 b(bool final = false) const { return conditional && condition >= 6 ? final && staged ? poisoned_value() : tested_value() : 0xfeabcd0000ULL | u16(~seed); }
		u16 continuation() const { return taken() ? target : 0x604; }
	};
	branch_repeat_fixture branch_repeat_case(unsigned v) const;
	void start_branch_repeat(unsigned variant = 0);
	void start_block_delay_repeat(unsigned variant = 0);
	void start_branch_repeat_edge(unsigned variant = 0);
	void start_block_delay_repeat_edge(unsigned ordinal = 0);
	void start_conditional_delay_repeat_edge(unsigned ordinal = 0);
	struct repeated_mvmd_fixture
	{
		unsigned count, source, snapshot, read_clock, clear_accumulator;
		u16 value, seed, status, destination;
		u32 base, literal;
		bool long_header;
		u16 sample(unsigned index) const { return index > count / 2 ? u16(~value) : value; }
		u64 initial_accumulator() const { return 0x1234560000ULL | seed; }
		u64 selected_after_clear() const { return clear_accumulator == unsigned(source == 0x0b) ? 0 : initial_accumulator(); }
		u64 other_after_clear() const { return clear_accumulator == unsigned(source == 8) ? 0 : 0xfeabcd9876ULL; }
	};
	static repeated_mvmd_fixture repeated_mvmd_case(unsigned v, bool zero = false);
	void start_repeated_mvmd(unsigned variant = 0);
	void start_repeated_mvdk(unsigned variant = 0);
	struct memory_copy_repeat_fixture
	{
		unsigned form, count, body_kind, irq, snapshot, source_ar, destination_ar, sentinel_ar, pattern;
		u16 address, mode, opcode, st0, st1, origin;
		u32 base, body;
		bool absolute, chain;
		unsigned read_clock() const { return absolute ? 6 : 5; }
		unsigned body_words() const { return body_kind ? 2 : 1; }
		u16 payload(unsigned index) const { return pattern ? u16(~(0x9000 + index)) : u16(0x9000 + index); }
	};
	static memory_copy_repeat_fixture memory_copy_repeat_case(unsigned v, bool wrap = false, bool chain = false);
	void set_repeat_count_source(u16 address, u16 value);
	void start_memory_copy_repeat(unsigned variant = 0);
	struct addc_fixture
	{
		u64 a, b, after_a, after_b, final_a, final_b;
		u16 opcode, literal = 0, address, word, st0, st1, execute_st0, final_st0, bk = 0, t, trn;
		u32 base;
		std::array<u16, 8> initial;
		u16 next_ar3;
		unsigned read_clock, snapshot;
		bool extension, late, excluded = false;
	};
	static std::array<u64, 2> addc_expected(u64 source, u16 word, u16 st0, u16 st1, bool b, unsigned family = 0);
	static addc_fixture addc_case(unsigned v, bool edge, unsigned family = 0);
	static addc_fixture mapped_shift16_case(unsigned variant, bool edge);
	static addc_fixture mapped_logic_case(unsigned variant, bool edge);
	void start_addc(unsigned variant = 0);
	struct addc_chain_fixture
	{
		u64 a, b, final_a, final_b;
		u16 st0, st1, final_st0;
		u32 base;
		unsigned checkpoint;
		std::array<u16, 2> words, opcodes;
	};
	static addc_chain_fixture addc_chain_case(unsigned v);
	void start_addc_chain(unsigned variant = 0);
	struct xmem_load_fixture
	{
		unsigned x, shift, destination, flags;
		u16 value;
		u32 base;
		bool linear;
		u16 st0() const { return 0xa002 | (flags & 4 ? 0x1e00 : 0); }
		u16 st1() const { return 0x800 | ((flags & 3) << 8) | (value & 2 ? 0x4020 : 0); }
		u16 address() const { return 0x100f + 0x100 * (x & 3); }
		u16 next_ar() const { return address() + std::array<int, 4>{0, -1, 1, linear ? 3 : -13}[x >> 2]; }
		u64 result() const { return u64((flags & 1 ? s64(s16(value)) : s64(value)) * (s64(1) << shift)) & 0xffffffffffULL; }
	};
	static xmem_load_fixture xmem_load_case(unsigned v);
	struct xmem_arithmetic_result
	{
		u64 source, result;
		u16 st0;
	};
	static xmem_arithmetic_result xmem_arithmetic_expected(unsigned variant);
	void start_xmem_load(unsigned variant = 0);
	static u64 xmem_store_source(unsigned variant);
	static u16 xmem_store_expected(unsigned variant);
	void start_xmem_store(unsigned variant = 0);
	struct exponent_fixture
	{
		u64 value;
		bool b, producer;
		unsigned gap;
		u16 st0, st1;
		u32 base;
		u64 source() const { return producer ? u16(value) : value; }
		u16 exponent() const
		{
			if (!source()) return 0;
			u64 magnitude = source();
			if (magnitude & 0x8000000000ULL) magnitude = (~magnitude) & 0xffffffffffULL;
			unsigned significant = 0;
			while (magnitude) { ++significant; magnitude >>= 1; }
			return u16(s16(31 - int(significant)));
		}
	};
	static exponent_fixture exponent_case(unsigned v);
	struct normalization_fixture
	{
		unsigned selector, prefix, gap, read_cycle, execute_cycle;
		u16 t, st0, st1, result_st0;
		u32 base;
		std::array<u64, 2> before, after;
	};
	static normalization_fixture normalization_case(unsigned variant, bool producer, bool edge);
	void start_normalization(unsigned variant = 0);
	void start_exponent(unsigned variant = 0);
	struct repeat_push_fixture
	{
		unsigned count, kind, interrupt;
		bool long_form;
		u32 base;
		unsigned first_write() const { return long_form ? 8 : 7; }
		u16 first_source() const { return kind >= 2 ? 11 : kind == 0 ? 0x900 + count : 0x900; }
		u16 source(unsigned i) const { return kind == 1 || kind == 3 ? first_source() + i : first_source() - i; }
		u16 value(unsigned i) const
		{
			if (kind < 2) return 0x5100 + i;
			if (source(i) >= 0x10) return first_source();
			return std::array<u16, 16>{u16(interrupt == 1), u16(interrupt == 1), 0x9002, 0x9003, 0x9004, 0x9005, 0xa600, 0, 0x789a, 0x3456, 0x12, 0x9876, 0xabcd, 0xfe, 0x5678, 0x9abc}[source(i)];
		}
	};
	static repeat_push_fixture repeat_push_case(unsigned v);
	void start_repeat_push(unsigned variant = 0);
	void start_push_exclusion(bool self_ar = false);
	void start_mapped_pshd(unsigned variant = 0);
	void start_staged_bus_stm(unsigned variant = 0);
	struct reada_brc_fixture
	{
		unsigned form, reg, body_reg, gap, words, read, write, checkpoint;
		u32 logical, physical;
		u16 value, mode, st1;
		bool overlay, loop;
	};
	bool standard_reada_brc() const;
	reada_brc_fixture reada_brc_case(unsigned v) const;
	void start_reada_brc(unsigned variant = 0);
	void start_reada_brc_edge(unsigned variant = 0);
	void start_mapped_read_a(unsigned variant = 0);
	struct page_zero_stack_fixture
	{
		unsigned form, snapshot;
		u16 address, value, sp, st1;
		u32 base;
	};
	static page_zero_stack_fixture page_zero_stack_case(unsigned v, bool edge);
	void start_page_zero_stack(unsigned variant = 0);
	struct mvdm_acc_fixture
	{
		unsigned reg, snapshot;
		u16 source, value, st1;
		u32 base;
		u64 a, b;
	};
	static mvdm_acc_fixture mvdm_acc_case(unsigned v, bool edge);
	void start_mvdm_acc(unsigned variant = 0);
	struct mvdm_t_fixture
	{
		unsigned destination, gap, checkpoint;
		u16 source, value, st1;
		u32 base;
	};
	static mvdm_t_fixture mvdm_t_case(unsigned v);
	void start_mvdm_t(unsigned variant = 0);
	void start_mvdm_ar(unsigned variant = 0);
	bool mvkd_delay() const;
	void start_mvdm_delay(unsigned variant = 0);
	void start_delayed_far_call(unsigned variant = 0);
	struct mac_smem_fixture
	{
		u16 opcode, memory, t, st0, st1, pmst, address, result_st0;
		u64 a, b, result;
		unsigned read;
		u32 base = 0;
	};
	static mac_smem_fixture mac_smem_case(unsigned variant, bool bk = false, bool square = false);
	struct poly_fixture
	{
		unsigned form, iterations, checkpoint;
		u32 base;
		u16 st0, st1, t;
		u64 a, b;
		bool square = false;
		bool tmac = false;
		unsigned operation = 0;
		bool dual = false;
		unsigned selector = 0;
		u16 xseed = 0, yseed = 0, pmst = 0;
	};
	poly_fixture poly_case(unsigned variant) const;
	static u16 poly_memory(unsigned ordinal);
	static u16 square_repeat_memory(poly_fixture const &f, unsigned ordinal);
	static u16 mac_repeat_address(poly_fixture const &f, unsigned ordinal, bool y);
	static u16 mac_repeat_memory(poly_fixture const &f, unsigned ordinal, bool y);
	static std::array<u64,3> mac_repeat_step(poly_fixture const &f, std::array<u64,3> state, unsigned ordinal);
	static std::array<u64,3> square_repeat_step(poly_fixture const &f, std::array<u64,3> state, unsigned ordinal);
	static std::array<u64,3> poly_reference(poly_fixture const &f);
	void start_poly(unsigned variant = 0);
	struct register_product_fixture
	{
		u16 opcode, t, st0, st1, pmst;
		u64 a, b;
		u32 base;
		unsigned mode, checkpoint;
		bool smem = false, mapped_t = false;
		bool poly = false, immediate = false;
		unsigned length = 1;
	};
	static register_product_fixture register_product_case(unsigned variant);
	static u16 product_input_t(register_product_fixture const &f);
	static std::array<u64,3> register_product_reference(register_product_fixture const &f);
	void start_register_product(unsigned variant = 0);
	void start_mac_smem(unsigned variant = 0);
	void start_mvmd_ar(unsigned variant = 0);
	void start_ifr_pair(unsigned variant = 0);
	struct mvkd_peripheral_fixture
	{
		u16 destination, source, value;
		u32 base;
		unsigned form, read, write, checkpoint;
	};
	static mvkd_peripheral_fixture mvkd_peripheral_case(unsigned variant);
	void start_mvkd_peripheral(unsigned variant = 0);
	struct extended_t_store_fixture
	{
		unsigned form, reg, selected, snapshot, write;
		bool trn, compatibility, excluded;
		u16 mode, literal, destination, value, st0;
		u32 base;
		std::array<u16, 8> initial, final;
	};
	static extended_t_store_fixture extended_t_store_case(unsigned v, bool excluded);
	void start_extended_t_store(unsigned variant = 0);
	void start_t_store_branch(unsigned variant = 0);
	struct t_peripheral_fixture
	{
		u16 source, destination, value;
		u32 base;
		unsigned gap, write, checkpoint;
		bool postincrement;
	};
	static t_peripheral_fixture t_peripheral_case(unsigned variant);
	void start_t_peripheral(unsigned variant = 0);
	bool near_accumulator_fixture() const;
	struct fcala_delay_fixture
	{
		bool b, wide;
		unsigned source, checkpoint;
		u32 caller, target;
	};
	static fcala_delay_fixture fcala_delay_case(unsigned v);
	struct fcala_delay_observations
	{
		unsigned frames = 0, reads = 0, delays = 0, callee = 0, vectors = 0, irq_reads = 0, target_fetches = 0;
		u16 irq_pc = 0;
	};
	void start_fcala_delay(unsigned variant = 0);
	void start_accumulator_call(unsigned variant = 0);
	void start_mvdp(unsigned variant = 0);
	struct repeat_mvkd_fixture
	{
		unsigned form, reg, count, prefix, read, snapshot;
		u32 base;
		u16 destination, initial_ar, opcode, st0, st1;
	};
	static repeat_mvkd_fixture repeat_mvkd_case(unsigned v);
	static bool repeat_mvkd_bypass(repeat_mvkd_fixture const &f, unsigned n);
	static u16 repeat_mvkd_value(repeat_mvkd_fixture const &f, unsigned n);
	void start_repeat_mvkd(unsigned variant = 0);
	void start_repeat_mvkd_edge(unsigned variant = 0);
	void start_repeat_mvdp(unsigned variant = 0);
	void start_repeat_mvdp_irq(unsigned variant = 0);
	void start_mvdp_pair(unsigned variant = 0);
	void start_write_a(unsigned variant = 0);
	void start_write_a_edge(unsigned variant = 0);
	void start_return_load(unsigned variant = 0);
	struct delayed_conditional_call_fixture
	{
		unsigned condition, layout, checkpoint;
		bool b, taken;
		bool tc=false, initial_tc=false;
		u32 base;
		u64 initial, changed;
		u16 literal;
		u16 opcode() const { return tc ? 0xfb20+(condition ? 0x10 : 0) : 0xfb42+condition+(b ? 8 : 0); }
		u16 status(bool final=false) const { return 0xa608|(tc && (initial_tc!=final) ? 0x1000 : 0); }
		u16 delay(unsigned i) const
		{
			if(tc) return i==layout ? (initial_tc ? 0xf4bc : 0xf5bc) : 0x6d95;
			return !i ? layout ? (b ? 0xf120 : 0xf020) : (b ? 0xf793 : 0xf493) : layout ? literal : 0x6d95;
		}
	};
	static delayed_conditional_call_fixture delayed_conditional_call_case(unsigned v, bool tc=false);
	void start_delayed_conditional_call(unsigned variant = 0);
	void start_delayed_conditional_call_edge(unsigned variant = 0);
	struct block_conditional_call_fixture
	{
		unsigned count, checkpoint;
		bool b, equal, zero, endpoint, long_delay;
		u32 base;
		u16 end() const { return endpoint ? 0x609 : 0x60b; }
		bool taken() const { return equal == zero; }
	};
	static block_conditional_call_fixture block_conditional_call_case(unsigned v);
	void start_block_conditional_call(unsigned variant = 0);
	struct asm_store_fixture
	{
		unsigned selector, raw, form, read, checkpoint;
		u32 base;
		u16 st1, pmst, word;
		u64 value;
	};
	static asm_store_fixture asm_store_case(unsigned v, bool prefix);
	void start_asm_store(unsigned variant = 0);
	static u16 block_bcd_source(unsigned variant, unsigned iteration);
	static bool block_bcd_taken(unsigned variant, u16 word);
	static extended_banz_fixture block_banz_case(unsigned v);
	static u16 block_banz_resolved(unsigned v, unsigned iteration);
	void start_block_bcd(unsigned variant = 0);
	void start_repeat_rearm(unsigned variant = 0);
	struct saved_repeat_fixture
	{
		unsigned count, inner, snapshot;
		bool after_end, nmi, delayed;
		u32 base;
		u16 sp;
		u16 end() const { return after_end ? 0x609 : 0x60b; }
		u16 saved_count() const { return count - unsigned(after_end && count); }
		u16 saved_st1() const { return 0x900 | (count ? 0x8000 : 0); }
		u16 vector() const { return nmi ? 4 : 0x40; }
	};
	static saved_repeat_fixture saved_repeat_case(unsigned v);
	struct saved_repeat_observations
	{
		unsigned outer_reads = 0, outer_writes = 0, leaves = 0, inner_writes = 0;
		unsigned assertions = 0, vectors = 0, mapped_reads = 0, stack_reads = 0, stack_writes = 0, controls = 0, resumed = 0;
		u16 return_pc = 0;
		u64 vector_cycle = 0, inner_cycle = 0, restore_cycle = 0, st1_cycle = 0, resume_cycle = 0;
	};
	u16 saved_repeat_word(saved_repeat_fixture const &f, unsigned slot) const;
	void start_saved_repeat(unsigned variant = 0);
	static saved_repeat_fixture body_repeat_case(unsigned v);
	u16 body_repeat_word(saved_repeat_fixture const &f, unsigned slot) const;
	void start_body_repeat(unsigned variant = 0);
	struct repeat_context_fixture
	{
		bool replace, span;
		unsigned count, checkpoint;
		u32 base;
		u16 sp;
		u16 old_end() const { return 0x60b + unsigned(span); }
		u16 start() const { return replace ? span ? 0x1800 : 0x800 : 0x604; }
		u16 end() const { return replace ? start() + 2 : old_end(); }
		unsigned copies() const { return count + 1 + unsigned(replace); }
		unsigned calls() const { return replace ? 1 : count + 1; }
	};
	static repeat_context_fixture repeat_context_case(unsigned v);
	struct repeat_context_observations
	{
		unsigned body_reads = 0, body_writes = 0, mapped_reads = 0, saved_writes = 0;
		unsigned restore_reads = 0, control_writes = 0, frame_reads = 0, frame_writes = 0;
		unsigned pmst_reads = 0, pmst_saves = 0, pmst_pops = 0, pmst_writes = 0;
	};
	u16 repeat_context_word(repeat_context_fixture const &f, unsigned part) const;
	void start_repeat_context(unsigned variant = 0);
	struct push_pop_same_fixture
	{
		u16 source, destination, value, sp;
		u32 base;
		unsigned checkpoint;
	};
	static push_pop_same_fixture push_pop_same_case(unsigned v);
	void start_push_pop_same(unsigned variant = 0);
	void start_conditional_call(unsigned variant = 0);
	static bool mvdd_source_owned(u16 address);
	static u64 mvdd_source_a(unsigned pattern);
	static u64 mvdd_source_b(unsigned pattern);
	static u16 mvdd_source_word(u16 address, unsigned pattern, u32 base, u16 ar2, u16 ar3);
	struct mvdd_source_fixture
	{
		unsigned source, pattern, xmode, ymode, prefix, count, snapshot;
		bool repeated, fixed;
		u32 base;
		u16 destination = 0x1000;
		u16 address(unsigned i) const { return source + (fixed ? 0 : i); }
		unsigned read_clock(unsigned i) const
		{
			unsigned waits = 0;
			for (unsigned n = 0; n <= i; ++n) waits += address(n) >= 0x20 && address(n) < 0x60;
			return 5 + prefix + i + waits;
		}
		unsigned write_clock(unsigned i) const { return repeated && i + 1 < count ? read_clock(i + 1) : read_clock(i) + 1; }
		u16 word(unsigned i) const
		{
			if (fixed && i) return 0;
			unsigned const issued = std::min(i + 2, count);
			return mvdd_source_word(address(i), pattern, base, repeated ? 0x1000 + issued : 0x2222, repeated ? source + (fixed ? 0 : issued) : 0x3333);
		}
	};
	static mvdd_source_fixture mvdd_source_case(unsigned variant, bool repeated, bool edge = false);
	void start_mvdd_source(unsigned variant = 0);
	void start_mvdd_peripheral(unsigned variant = 0);
	void start_mvmd_word_register(unsigned variant = 0);
	struct mvdk_mapped_fixture
	{
		u16 source, destination, value;
		unsigned form, access, checkpoint;
		u32 base;
	};
	static mvdk_mapped_fixture mvdk_mapped_case(unsigned v, bool accumulator_destination = false, bool clock = false);
	void start_mvdk_mapped(unsigned variant = 0);
	void start_clkmd_move_edge(unsigned variant = 0);
	struct mvdk_producer_fixture
	{
		u16 source, destination, value, expected;
		unsigned form, producer, prefix, access, checkpoint;
		u32 base;
	};
	static mvdk_producer_fixture mvdk_producer_case(unsigned v, bool accumulator_destination = false);
	void start_mvdk_producer(unsigned variant = 0);
	void start_mvdk_mapped_edge(unsigned variant = 0);
	void start_mvmd_accumulator(unsigned variant = 0);
	void start_mcbsp_control_sequence(unsigned variant = 0);
	static accumulator_store_fixture preincrement_store_case(unsigned variant);
	static accumulator_store_fixture preincrement_immediate_case(unsigned variant);
	void start_preincrement_store(unsigned variant = 0);
	static u16 ldu_status_value(unsigned variant);
	struct stlm_status_fixture
	{
		unsigned family, gap, checkpoint;
		bool source;
		u32 base;
		u16 before, requested, status;
		u64 value;
	};
	static stlm_status_fixture stlm_status_case(unsigned variant, bool immediate = false);
	void start_stlm_status(unsigned variant = 0);
	void start_stlm_status_edge(unsigned variant = 0);
	void start_stm_status_edge(unsigned ordinal = 0);
	void check_st1_memory_rules();
	void start_ldu_status(unsigned variant = 0);
	struct shifted_peripheral_fixture
	{
		u16 address, value, st1, opcode, operation;
		unsigned form, read, checkpoint, literals;
		bool b, bsp, unowned;
		u32 base;
		u64 result;
	};
	static shifted_peripheral_fixture shifted_peripheral_case(unsigned variant, bool bsp, bool unowned = false);
	bool shifted_bsp_fixture() const;
	bool shifted_unowned_fixture() const;
	void start_shifted_peripheral(unsigned variant = 0);
	void start_shifted_peripheral_edge(unsigned variant = 0);
	void start_ldu_peripheral(unsigned variant = 0);
	void start_ldu_imr(unsigned variant = 0);
	void start_mvmd_status_irq(unsigned variant = 0);
	void start_mvmd_status_block(unsigned variant = 0);
	void start_mvmd_status_alu(unsigned variant = 0);
	virtual void machine_start() override;

	virtual void machine_reset() override;

	void program_map(address_map &map) ATTR_COLD;

	void mcbsp_control_map(address_map &map) ATTR_COLD;
	void data_map(address_map &map) ATTR_COLD;
	void plain_data_map(address_map &map) ATTR_COLD;
	void io_map(address_map &map) ATTR_COLD;
	u16 test_port_r();
	void test_port_w(u16 value);
	u16 repeat_irq_r(offs_t offset);
	void stack_mmr_w(offs_t offset, u16 value);

	static void rom4_crc_input(unsigned vector, u16 (&words)[12]);

	static u16 rom4_crc_expected(u16 const (&words)[12]);

	void start_rom4_crc_packed(unsigned index);

	static constexpr unsigned ROM4_FR_WIDTHS[] = {
			6, 6, 5, 5, 4, 4, 3, 3,
			7, 2, 2, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
			7, 2, 2, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
			7, 2, 2, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
			7, 2, 2, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
	};

	static gsm::tch_f::speech_bits rom4_fr_parameters(unsigned vector, u16 (&words)[76]);

	// GSM 06.60 parameter order, independent of the acquired descriptor tables.
	static constexpr unsigned ROM4_EFR_WIDTHS[] = {7,8,9,8,6,
		9,4,4,4,4,4,4,3,3,3,3,3,5, 6,4,4,4,4,4,4,3,3,3,3,3,5,
		9,4,4,4,4,4,4,3,3,3,3,3,5, 6,4,4,4,4,4,4,3,3,3,3,3,5};
	static_assert(std::size(ROM4_EFR_WIDTHS) == 57);
	static u8 rom4_efr_crc_reference(std::array<u8, 244> const &serial);

	static u8 rom4_efr_parameters(unsigned vector, u16 (&parameters)[57], std::array<u8, 244> *speech = nullptr);

	static std::array<u16, 17> rom4_efr_packed(std::array<u8, 244> const &speech);
	static std::array<u16, 17> rom4_efr_packed(unsigned vector);

	void start_rom4_delayed_repeat(unsigned index);

	struct staged_conditional_store_fixture
	{
		unsigned form, condition, pattern, mode, snapshot;
		u32 base;
		u16 address, final_ar, memory, output, t, brc, bk, st1, pmst;
		u64 a, b;
		bool edge;
	};
	staged_conditional_store_fixture staged_conditional_store_case(unsigned v) const;
	void start_staged_conditional_store(unsigned variant = 0);
	struct conditional_register_fixture
	{
		bool brc, take;
		unsigned x;
		u16 source, address, next;
		u64 a, b;
	};
	static conditional_register_fixture conditional_register_case(unsigned index);
	void start_conditional_register_store(unsigned index);

	struct parallel_sub_fraction_fixture
	{
		u64 a, b;
		u16 x;
		bool source_b, destination_b;
	};
	static parallel_sub_fraction_fixture parallel_sub_fraction_case(unsigned index);
	void start_native_dual_sub(unsigned index);

	void start_parallel_sub_fraction(unsigned index);

	void start_xc_barrier(unsigned index);

	struct xc_latency_fixture
	{
		unsigned gap, kind, slots, prefix;
		bool initial, polarity, staged, snapshot;
		bool take() const { return (gap < 2 ? initial : !initial) == polarity; }
		bool source() const { return slots == 2 && take() ? !polarity : !initial; }
		u16 status(bool after) const { return 0x605 | (kind < 2 ? (after ? source() : initial) << (kind ? 11 : 12) : 0); }
	};
	static xc_latency_fixture xc_latency_case(unsigned index);
	void start_xc_latency(unsigned index);

	void start_xc_status(unsigned index);

	struct xorm_fixture
	{
		u16 mode, address, next, source, mask, st1;
		unsigned words;
	};
	static xorm_fixture atomic_xorm_case(unsigned index);
	void start_atomic_xorm(unsigned index);

	void start_atomic_conditional_return(unsigned index);

	struct staged_overflow_fixture
	{
		u32 base;
		unsigned selected, producer, gap, checkpoint;
		bool delayed, take, mutate;
		u16 initial, mask, opcode, sampled;
	};
	static staged_overflow_fixture staged_overflow_case(unsigned v);
	void start_staged_overflow(unsigned variant = 0);
	void start_atomic_overflow_branch(unsigned index);

	bool rom4_codec_is(char const *name) const;
	void enable_rom4_codec_pipeline(char const *kind);
	void start_rom4_efr_encoder(unsigned frame, bool initialize);

	struct efr_decoder_fixture
	{
		char const *input, *pcm, *label;
		unsigned frames;
	};
	efr_decoder_fixture rom4_efr_decoder_fixture() const;
	void start_rom4_efr_decoder(unsigned frame, bool initialize);
	void observe_rom4_efr_state(offs_t address, u16 opcode);

	void start_rom4_efr_packing(unsigned vector, unsigned stage);

	void start_rom4_efr_crc(unsigned index);

	void start_rom4_fr_packing(unsigned vector, unsigned stage);

	void save_rom4_codec_frame(unsigned frame);
	bool replay_rom4_codec_frame(unsigned phase, unsigned milliseconds = 5);
	void trace_rom4_fr_state(char const *phase, offs_t address, u16 opcode);
	void trace_rom4_fr_input(unsigned phase, u16 entry);
	void start_rom4_fr_decoder(unsigned frame, bool initialize);

	void start_rom4_fr_encoder(unsigned frame, unsigned stage);

	void start_bsp_interrupt_case(unsigned index);

	void start_rom4_pcm_mute(unsigned index);

	void start_zero_bk_case(unsigned index);

	static constexpr u64 MINMAX_SEEDS[] = {0, 1, 0xffffffffffULL, 0x7fffffff, 0xff80000000ULL, 0x7fffffffffULL, 0x8000000000ULL, 0x123456789aULL};
	void start_minmax_case(unsigned index);

	void start_rcd_acc_case(unsigned index);

	void start_saccd_case(unsigned index);

	void start_roltc_case(unsigned index);

	static std::array<s16, 4> rom4_pair_vector(unsigned index);
	void start_rom4_pairs(unsigned index);
	static bool rom4_bitmap_bit(unsigned index, unsigned bit);
	void start_rom4_bitmap(unsigned index);
	void start_rom4_crc_helper(unsigned index);

	void rom4_program_map(address_map &map) ATTR_COLD;

	u16 rom4_loader_entry_r();

	void rom4_data_map(address_map &map) ATTR_COLD;

	void expect(bool condition, const char *message);

	void expect_opcode(u16 opcode, bool condition, const char *message);

	static constexpr u8 rom4_saved_mmr[] = {
		0x0e, 0x10, 0x11, 0x13, 0x14, 0x15,
		0x16, 0x17, 0x19, 0x1a, 0x1b, 0x1c
	};
	// Return true only when the original callback would return; order is retained.
	bool check_repeated_writa_results();
	bool check_branch_arithmetic_results();
	bool check_circular_mar_results();
	bool check_tc_return_results();
	bool check_bitt_results();
	bool check_bd_read_a_results();
	bool check_mvdd_follow_results();
	bool check_delay_bitt_results();
	bool check_repeat_store_results();
	bool check_rptz_results();
	bool check_sat_results();
	bool check_literal_guard_results();
	bool check_mmr_long_edge_results();
	bool check_shifted_store_dld_results();
	bool check_idle_nmi_result_results();
	bool check_shifted_high_store_results();
	bool check_idle_wake_results();
	bool check_rounded_multiply_results();
	bool check_stopped_timer_reload_results();
	bool check_atomic_program_results();
	bool check_far_save_results();
	bool check_bitf_offset_results();
	bool check_bio_results();
	bool check_bit_results();
	bool check_banz_offset_results();
	bool check_cmpr_results();
	bool check_carry_branch_results();
	bool check_cmpm_offset_results();
	bool check_compact_load_results();
	bool check_return_alt_results();
	bool check_software_interrupt_results();
	bool check_trn_store_results();
	bool check_parallel_load_multiply_results();
	bool check_compact_store_results();
	bool check_acc_delayed_results();
	bool check_delayed_call_results();
	bool check_asm_store_results();
	bool check_store_shift_results();
	bool check_reverse_carry_reference_results();
	bool check_intm_protection_results();
	bool check_zero_bk_results();
	bool check_saccd_results();
	bool check_bsp_interrupt_results();
	bool check_vector_prefetch_results();
	bool check_braf_control_results();
	bool check_native_address_gap_results();
	bool check_native_math_gap_results();
	bool check_native_memory_gap_results();
	bool check_native_final_gap_results();
	bool check_indirect_popm_results();
	bool check_shifted_arithmetic_selector_results();
	bool check_interrupt_entry_spacing_results();
	bool check_rounding_boundary_results();
	bool check_pipeline_frontend_results();
	bool check_multiword_pipeline_results();
	bool check_pipeline_store_results();
	bool check_pipeline_rmw_results();
	bool check_constant_dagen_results();
	bool check_peripheral_mmr_timing_audit_results();
	bool check_pipeline_immediate_store_results();
	bool check_pipeline_far_return_results();
	bool check_pipeline_far_irq_results();
	bool check_pipeline_intm_results();
	bool check_pipeline_push_sp_latency_results();
	bool check_interrupt_pop_edge_results();
	bool check_pipeline_mmr_pop_results();
	bool check_pipeline_mu4_prologue_results();
	bool check_pipeline_dst_results();
	bool check_pipeline_logical_immediate_results();
	bool check_absolute_memory_dp_results();
	bool check_pipeline_conditional_branch_results();
	bool check_pipeline_xc_irq_results();
	bool check_pipeline_xc_tc_results();
	bool check_pipeline_xc_accumulator_results();
	bool check_pipeline_delayed_return_results();
	bool check_pipeline_original_worker_roundtrip_results();
	bool check_pipeline_original_context_call_results();
	bool check_addm_expected_results();
	bool check_pipeline_data_push_results();
	bool check_pipeline_retd_data_order_results();
	bool check_pipeline_data_pop_results();
	bool check_pipeline_cmpr_results();
	bool check_pipeline_memory_logic_results();
	bool check_pipeline_mu4_full_worker_results();
	bool check_pipeline_mu4_timer_context_results();
	bool check_pipeline_absolute_load_results();
	bool check_pipeline_far_call_results();
	bool check_pipeline_idle_sp_tail_results();
	bool check_repeat_pop_exclusion_results();
	bool check_pipeline_sp_rmw_results();
	bool check_pipeline_memory_repeat_read_a_results();
	bool check_instruction_observer_results();
	bool check_mvkd_sp_results();
	bool check_pipeline_add_sp_context_results();
	bool check_pipeline_peripheral_logic_results();
	bool check_pipeline_direct_immediate_store_results();
	bool check_ts_load_results();
	bool check_extended_ar_store_results();
	bool check_accumulator_logic_results();
	bool check_data_copy_results();
	bool check_accumulator_shift_results();
	bool check_peripheral_long_store_results();
	bool check_pipeline_mvmm_sp_results();
	bool check_branch_repeat_results();
	bool check_rptbd_t_load_results();
	bool check_pipeline_repeat_nop_results();
	bool check_pipeline_xmem_bit_results();
	bool check_pipeline_port_read_results();
	bool check_long_accumulator_store_results();
	bool check_pipeline_long_move_results();
	bool check_absolute_xorm_results();
	bool check_signed_branch_taken_results();
	bool check_bitfield_address_results();
	bool check_staged_load_results();
	bool check_pipeline_extended_banz_results();
	bool check_immediate_arithmetic_results();
	bool check_memory_repeat_results();
	bool check_banzd_results();
	bool check_stlm_t_results();
	bool check_pipeline_swap_body_results();
	bool check_push_pop_same_results();
	bool check_shift_delay_edge_results();
	bool check_asm_store_results_part2();
	bool check_repeat_rearm_results();
	bool check_saved_repeat_results();
	bool check_repeat_context_results();
	bool check_mvdk_mapped_edge_results();
	bool check_clkmd_move_edge_results();
	bool check_mvdk_mapped_results();
	bool check_basic_repeat_edge_results();
	bool check_rptbd_shift_store_results();
	bool check_rptbd_layout_results();
	bool check_negate_results();
	bool check_long_mapped_original_results();
	bool check_mcbsp_control_sequence_results();
	bool check_preincrement_immediate_results();
	bool check_stack_irq_order_results();
	bool check_repeat_pop_exclusion_results_part2();
	bool check_accumulator_cmpm_results();
	bool check_staged_conditional_store_results();
	bool check_accumulator_stm_results();
	bool check_ldu_peripheral_results();
	bool check_ldu_status_value_results();
	bool check_ldu_imr_results();
	bool check_mvmd_status_block_results();
	bool check_shifted_peripheral_edge_results();
	bool check_stm_status_edge_results();
	bool check_rptbd_square_results();
	bool check_delay_copy_results();
	bool check_staged_overflow_results();
	bool check_mapped_dual_results();
	bool check_mvmd_accumulator_results();
	bool check_write_a_edge_results();
	bool check_repeat_mvdp_irq_results();
	bool check_repeat_mvkd_results();
	bool check_conditional_call_results();
	bool check_near_accumulator_results();
	bool check_extended_t_store_results();
	bool check_circular_portr_results();
	bool check_ifr_pair_results();
	bool check_mvmd_ar_results();
	bool check_poly_results();
	bool check_delayed_far_call_results();
	bool check_mvkd_delay_results();
	bool check_absolute_logic_results();
	bool check_popd_mapped_results();
	bool check_accumulator_branch_results();
	bool check_mvmd_word_register_results();
	bool check_mvdd_peripheral_results();
	bool check_source_alias_results();
	bool check_mapped_immediate_mac_results();
	bool check_block_branch_count_results();
	bool check_bd_long_accumulator_results();
	bool check_t_move_results();
	bool check_block_xc_edge_results();
	bool check_block_xc_irq_results();
	bool check_move_repeat_owner_results();
	bool check_move_brc_edge_results();
	bool check_nibble_input_results();
	bool check_software_intr_results();
	bool check_block_branch_edge_results();
	bool check_circular_load_results();
	bool check_delay_dld_results();
	bool check_asm_mapped_results();
	bool check_t_store_edge_results();
	bool check_ts_arithmetic_edge_results();
	bool check_asm_edge_results();
	bool check_delay_compare_edge_results();
	bool check_delayed_condition_return_results();
	bool check_poll_interrupt_results();
	bool check_delay_compare_results();
	bool check_asm_circular_store_results();
	bool check_minmax_results();
	bool check_indexed_mar_results();
	bool check_addc_chain_results();
	bool check_mapped_parallel_load_results();
	bool check_mvdm_acc_results();
	bool check_memory_copy_repeat_results();
	bool check_block_near_call_edge_results();
	bool check_repeated_mvdk_results();
	bool check_portr_bypass_results();
	bool check_sqdst_results();
	bool check_signed_xc_results();
	bool check_normalization_results();
	bool check_xmem_load_results();
	bool check_exponent_results();
	bool check_repeat_push_results();
	bool check_block_exit_results();
	bool check_stlm_bsp_edge_results();
	bool check_stlm_t_results_part2();
	bool check_staged_bus_stm_results();
	bool check_rom4_fr_decoder_results();
	bool check_native_dual_sub_results();
	bool check_xc_barrier_results();
	bool check_atomic_xorm_results();
	bool check_conditional_register_results();
	bool check_rom4_codec_is_results();
	bool check_roltc_results();
	bool check_rom4_crc_input_results();
	TIMER_CALLBACK_MEMBER(check_results);

	required_device<tms320c54x_device> m_cpu;
	u16 m_shifted_bus_word = 0;
	tms320c54x_bsp_state m_shifted_bsp;
	int m_bio_level = 1;
	optional_device<nokia_dspif_device> m_transport;
	util::gsm_fr_codec m_rom4_codec;
	util::gsm_fr_codec::pcm_block m_rom4_pcm{};
	std::array<u16, 76> m_rom4_parameters{};
	unsigned m_rom4_pcm_mismatches = 0;
	unsigned m_rom4_cni_nonzero_frames = 0;
	bool m_rom4_codec_replay = false;
	unsigned m_rom4_codec_replays = 0;
	unsigned m_rom4_fr_trace_count = 0;
	// Read-only bench counters stay outside the snapshot; replay emits no duplicate observations.
	unsigned m_rom4_efr_pitch_observations = 0, m_rom4_efr_code_observations = 0;
	unsigned m_rom4_homing_resets = 0;
	u32 m_rom4_checkpoint_pc = 0;
	std::vector<u16> m_rom4_replay_data, m_rom4_checkpoint_data;
	emu_timer *m_check_timer = nullptr;
	memory_passthrough_handler m_timer_probe_fetch, m_timer_probe_reload, m_timer_probe_stack, m_vector_prefetch_tap;
	memory_passthrough_handler m_vector_stack_tap;
	memory_passthrough_handler m_timer_probe_write, m_timer_probe_mask;
	memory_passthrough_handler m_timer_probe_foreground;
	unsigned m_timer_probe_foreground_fetches = 0;
	u64 m_timer_probe_foreground_first_cycle = 0;
	u64 m_timer_probe_write_cycle = 0, m_timer_probe_mask_cycle = 0;
	unsigned m_timer_probe_writes = 0, m_timer_probe_masks = 0;
	unsigned m_vector_reads = 0, m_vector_acks = 0, m_vector_case = 0;
	unsigned m_entry_stack_writes = 0;
	unsigned m_stage_restore_case = 0;
	int m_entry_ack_source = -1;
	u64 m_entry_fetch_cycle = 0, m_entry_stack_cycle = 0;
	std::stringstream m_saved_vector;
	std::stringstream m_saved_pipeline;
	std::vector<std::pair<offs_t, u16>> m_observed_instructions, m_saved_observed_instructions;
	std::vector<std::array<u64, 3>> m_repeat_load_expected;
	u64 m_pipeline_expected_cycle = 0;
	bool m_pipeline_empty_nmi = false;
	bool m_pipeline_memory_case = false;
	bool m_pipeline_memory_b = false;
	unsigned m_pipeline_operand_reads = 0;
	unsigned m_pipeline_long_case = 0;
	unsigned m_pipeline_store_case = 0;
	unsigned m_pipeline_store_writes = 0;
	unsigned m_pipeline_indirect_case = 0;
	unsigned m_pipeline_rmw_case = 0;
	unsigned m_pipeline_return_case = 0;
	unsigned m_pipeline_branch_case = 0;
	unsigned m_pipeline_stm_case = 0;
	unsigned m_mmr_timing_case = 0;
	unsigned m_pipeline_peripheral_case = 0;
	unsigned m_pipeline_extensions_reads = 0;
	memory_passthrough_handler m_pipeline_write_tap;
	unsigned m_pipeline_register_writes = 0;
	memory_passthrough_handler m_pipeline_operand_tap;
	bool m_pipeline_replay = false;
	struct rptbd_observations { unsigned reads = 0, writes = 0, fetches = 0, delay_reads = 0, delay_writes = 0, acks = 0, stack_reads = 0, stack_writes = 0; };
	rptbd_observations m_rptbd_counts, m_rptbd_saved_counts;
	repeat_context_observations m_repeat_context_counts, m_repeat_context_saved_counts;
	saved_repeat_observations m_saved_repeat_counts, m_saved_repeat_snapshot;
	unsigned m_saved_repeat_checkpoint = 0;
	fcala_delay_observations m_fcala_delay_counts, m_fcala_delay_snapshot;
	u16 m_rptbd_return = 0, m_rptbd_saved_return = 0;
	unsigned m_pipeline_drained_fetches = 0;
	memory_passthrough_handler m_pipeline_fetch_tap;
	std::array<unsigned, 9> m_recurring_irq_snapshot = {};
	unsigned m_recurring_irq_first_return_writes = ~0U, m_recurring_irq_end_writes = 0;
	unsigned m_idle_fixture_tick = 0, m_idle_fixture_saved_tick = 0;
	unsigned m_pipeline_repeat_count_reads = 0;
	u64 m_recurring_irq_checkpoint_cycle = 0, m_recurring_irq_end_cycle = 0;
	u64 m_timer_probe_reload_cycle = 0, m_timer_probe_first_entry_cycle = 0;
	unsigned m_timer_probe_entries = 0;
	u16 m_timer_probe_return_pc = 0;
	memory_passthrough_handler m_repeat_boundary_tap;
	unsigned m_repeat_boundary_requests = 0;
	memory_passthrough_handler m_xc_irq_tap;
	unsigned m_xc_irq_fetches = 0;
	memory_passthrough_handler m_operand_fetch_tap, m_operand_data_tap, m_operand_io_tap, m_operand_program_tap;
	unsigned m_operand_extension_count = 0, m_operand_extensions = 0, m_operand_reads = 0;
	bool m_operand_order_violation = false;
	unsigned m_phase = 0;
	static constexpr unsigned native_store_phase = 1'000'000; // Outside legacy phase bands.
	static constexpr unsigned mmr_long_phase = 1'000'010;
	static constexpr unsigned delay_bitt_phase = 1'000'020;
	static constexpr unsigned mvdd_follow_phase = 1'000'030;
	static constexpr unsigned bd_read_a_phase = 1'000'040;
	static constexpr unsigned mapped_immediate_mac_phase = 1'000'050;
	static constexpr unsigned push_mvmm_phase = 1'000'060;
	static constexpr unsigned mvdm_t_phase = 1'000'070;
	std::bitset<65536> m_asserted_opcodes;
	std::array<u16, 3> m_native_store_context = {};
	unsigned m_bleq_case = 0;
	static constexpr unsigned rom4_staged_cold_phase = 62000;
	static constexpr unsigned literal_guard_phase = 62010;
	static constexpr unsigned repeated_writa_phase = 62044;
	static constexpr unsigned branch_arithmetic_phase = 62040;
	static constexpr unsigned circular_mar_phase = 62038;
	static constexpr unsigned tc_return_phase = 62036;
	static constexpr unsigned bitt_phase = 62032;
	static constexpr unsigned repeat_store_phase = 62028;
	static constexpr unsigned rptz_phase = 62026;
	static constexpr unsigned rotate_phase = 62024;
	static constexpr unsigned ar_addm_phase = 62020;
	unsigned m_rom4_checks = 0;
	bool m_irq_raised = false;
	unsigned m_fast_stack_reads = 0;
	unsigned m_stack_irq_return_reads = 0, m_stack_irq_cleanup_reads = 0;
	std::array<unsigned, 2> m_stack_irq_read_snapshot = {};
	u8 m_bus_divisor = 0;
	bool m_bus_iack_enable = false;
	bool m_bus_ready = true;
	u64 m_bus_start_cycles = 0;
	unsigned m_fast_return_case = 0;
	unsigned m_repeat_reads = 0;
	u16 m_stack_mmr[2] = {};
	unsigned m_irq_trigger_read = 1;
	u64 m_irq_accumulator = 0;
	u64 m_irq_operand_cycle = 0;
	u16 m_idle_timer_counter = 0;
	u16 m_idle_timer_control = 0;
	optional_device<tms320c54x_mcbsp_device> m_idle_mcbsp;
	unsigned m_idle_mcbsp_words = 0;
	bool m_idle_mcbsp_external = false;
	optional_device<tms320c54x_dma_device> m_idle_dma;
	unsigned m_idle_dma_completions = 0;
	attotime m_idle_dma_start = attotime::zero;
	u32 m_idle_dma_rate = 0;
	u8 m_idle_dma_first_ticks = 4;
	unsigned m_port_writes_at_irq = 0;
	u64 m_first_operand_cycle = 0;
	u64 m_last_operand_cycle = 0;
	u64 m_first_port_cycle = 0;
	u64 m_middle_port_cycle = 0;
	u64 m_last_port_cycle = 0;
	unsigned m_port_reads = 0;
	unsigned m_port_writes = 0;
	u16 m_first_port_value = 0;
	u16 m_middle_port_value = 0;
	u16 m_last_port_value = 0;
	unsigned m_saved_repeat_reads = 0;
	// Immutable case inputs remain unchanged across each manual replay.
	mvdd_fixture m_mvdd_fixture = {};
	popd_fixture m_popd_fixture = {};
	accumulator_branch_fixture m_acc_branch_fixture = {};
	offset_logic_fixture m_offset_logic_fixture = {};
	unsigned m_acc_branch_mapped_writes = 0;
	u32 m_popd_write_mask = 0;
	std::stringstream m_saved_repeat;
	bool m_far_save_return = false;
	std::array<u16, 0x800> m_saved_transport = {};
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_STATE_H
