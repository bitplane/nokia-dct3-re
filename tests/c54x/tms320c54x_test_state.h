// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_STATE_H
#define MAME_NOKIA_TMS320C54X_TEST_STATE_H

#include "emuopts.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "gsm_tch_f_l1.h"
#include "util/gsmfr.h"
#include "tms320c54x_mcbsp.h"
#include "tms320c54x_dma.h"
#include "tms320c54x_bsp_state.h"
#include "tms320c54x_test_selection.h"
#include "tms320c54x_test_support.h"
#include "tms320c54x_test_register_sequence.h"
#include "tms320c54x_test_repeat_state.h"
#include "tms320c54x_test_snapshot.h"
#include "tms320c54x_test_dual_input.h"
#include "tms320c54x_test_normalization_input.h"
#include "tms320c54x_test_pipeline_state.h"
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
		m_idle_mcbsp(*this, "idle_mcbsp"),
		m_idle_dma(*this, "idle_dma")
	{
	}

	void test(machine_config &config);
	void rom4(machine_config &config);

private:
	using cpu_regs = tms320c54x_device;

	bool profile_is(std::string_view name) const
	{
		return name == machine().options().bios();
	}

	void xf_w(int state);
	bool m_xf_pin = true;
	void start_repeat_endpoint_case(unsigned index);
	void start_loop_xc_case(unsigned index);
	void start_xc_irq_case(unsigned index);
	void check_control_disassembler();
	void start_idle_nmi_case(unsigned index);
	void check_idle_nmi_result(unsigned index);
	void start_idle_dma_case(unsigned index);
	void start_idle_external_serial_case(unsigned index);
	void start_idle_timer_case(unsigned index);
	void start_idle_wake_case(unsigned index);
	void start_stopped_timer_reload_case(unsigned index);
	void start_timer_prescaler_divider_case(unsigned divider);
	void start_timer_counter_write_case(unsigned divider);
	void check_timer_reserved_fields();
	void start_pipeline_mu4_prologue(unsigned variant = 0);
	void observe_context_irq(int line);
	bool check_rom4_context_results();
	void record_context_case(unsigned bank, unsigned raw)
	{
		if (profile_is("rom4-context")) m_context_coverage.add((bank << 8) | raw);
	}
	void start_pipeline_original_worker_roundtrip(unsigned variant = 0);
	void start_pipeline_original_context_call(unsigned variant = 0, bool full_body = false);
	static std::array<u16, 2> addm_inputs(unsigned variant);
	static std::array<u16, 2> addm_expected(unsigned variant);
	void start_pipeline_mu4_timer_context(unsigned variant = 0);
	void start_pipeline_mu4_recurring_timer(unsigned variant = 0);
	void start_pipeline_idle_sp_tail(unsigned variant = 0);
	unsigned pipeline_idle_kind(unsigned variant) const;
	void start_pipeline_idle1(unsigned variant = 0, bool modes = false);
	void start_pipeline_original_idle(unsigned variant = 0);
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
	void start_instruction_observer(unsigned variant = 0);
	struct memory_arithmetic_fixture
	{
		unsigned form, reg, selected, read_cycle;
		u16 opcode, literal, address, next_ar, memory, st0, st1, result_st0;
		u64 source, result;
		u16 bk = 16;
	};
	struct extended_ar_store_fixture
	{
		unsigned form, reg, selected, kind, flags, gap;
		int shift;
		bool shifted;
		u16 literal, initial_ar, next_ar, result;
		u64 value;
		u32 base;
	};
	static extended_ar_store_fixture extended_ar_store_case(unsigned variant, bool consumer, bool shifted = false);
	void start_extended_ar_store(unsigned variant = 0);
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
	void start_pipeline_store_mvmm(unsigned variant = 0);
	struct indexed_store_fixture;
	static indexed_store_fixture circular_portr_case(unsigned v);
	void start_circular_portr(unsigned variant = 0);
	struct circular_portr_block_fixture
	{
		unsigned count, size, reg, selected, snapshot, index;
		bool compatibility;
		u16 buffer, st0;
		u32 base;
	};
	static circular_portr_block_fixture circular_portr_block_case(unsigned v);
	void start_circular_portr_block(unsigned variant = 0);
	static memory_arithmetic_fixture memory_arithmetic_case(unsigned variant, bool unsigned_memory = false, unsigned shift16_selector = 8);
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
	void start_pipeline_memory_multiply(unsigned variant = 0);
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
	struct basic_repeat_fixture
	{
		unsigned form, count, irq, checkpoint, selected, modification;
		bool compiler, compatibility, absolute;
		u32 base, body;
		u16 mode, address, st0, st1;
	};
	static basic_repeat_fixture basic_repeat_case(unsigned v);
	void start_basic_repeat(unsigned variant = 0);
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
	void start_move_repeat_owner(unsigned variant = 0);
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
	void check_mmr_bus();
	void start_timer_boundary_probe(unsigned index, bool staged = false);




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
	struct block_near_call_fixture
	{
		unsigned count, layout, depth, interrupt, checkpoint;
		u32 base;
		u16 call, end, st1;
		bool delayed, delayed_return;
	};
	static block_near_call_fixture block_near_call_case(unsigned v);
	void start_block_near_call(unsigned variant = 0);
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
	struct delay_compare_fixture
	{
		unsigned kind, form, interrupt, prefix, read, literal_clock, checkpoint;
		bool bitfield, initial_tc, taken, final_tc;
		u16 memory, literal;
		u32 base;
	};
	static delay_compare_fixture delay_compare_case(unsigned v);
	void start_delay_compare(unsigned variant = 0);
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
	static indexed_store_fixture circular_load_case(unsigned v);
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
	static parallel_load_fixture parallel_load_case(unsigned variant);
	static parallel_load_fixture mapped_parallel_load_case(unsigned v);
	static parallel_load_fixture parallel_arithmetic_case(unsigned variant);
	static parallel_load_fixture parallel_multiply_case(unsigned variant);
	static parallel_load_fixture bsp_parallel_load_case(unsigned variant);
	static parallel_load_fixture peripheral_parallel_load_case(unsigned variant);
	void start_parallel_load(unsigned variant = 0);
	struct sqdst_fixture
	{
		dual_multiply_fixture memory;
		u64 a, b, result_a, result_b;
		u16 result_st0;
	};
	static sqdst_fixture sqdst_case(unsigned variant, bool lms);
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
	static circular_move_fixture circular_move_case(unsigned variant);
	static circular_move_fixture indexed_move_case(unsigned variant);
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
	static repeated_mvdk_fixture repeated_mvdk_case(unsigned variant, bool indexed = false);
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
		bool extension, late;
	};
	static std::array<u64, 2> addc_expected(u64 source, u16 word, u16 st0, u16 st1, bool b, unsigned family = 0);
	static addc_fixture addc_case(unsigned v, unsigned family = 0);
	static addc_fixture mapped_shift16_case(unsigned variant);
	static addc_fixture mapped_logic_case(unsigned variant);
	void start_addc(unsigned variant = 0);
	void start_staged_bus_stm(unsigned variant = 0);
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
	struct mvkd_peripheral_fixture
	{
		u16 destination, source, value;
		u32 base;
		unsigned form, read, write, checkpoint;
	};
	static mvkd_peripheral_fixture mvkd_peripheral_case(unsigned variant);
	void start_mvkd_peripheral(unsigned variant = 0);
	struct t_peripheral_fixture
	{
		u16 source, destination, value;
		u32 base;
		unsigned gap, write, checkpoint;
		bool postincrement;
	};
	static t_peripheral_fixture t_peripheral_case(unsigned variant);
	void start_t_peripheral(unsigned variant = 0);
	using fcala_delay_fixture = c54x_test::fcala_delay_fixture;
	static fcala_delay_fixture fcala_delay_case(unsigned v);
	void start_fcala_delay(unsigned variant = 0);
	void start_repeat_mvdp(unsigned variant = 0);
	void start_repeat_mvdp_irq(unsigned variant = 0);
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
	static u16 block_bcd_source(unsigned variant, unsigned iteration);
	static bool block_bcd_taken(unsigned variant, u16 word);
	static extended_banz_fixture block_banz_case(unsigned v);
	static u16 block_banz_resolved(unsigned v, unsigned iteration);
	void start_block_bcd(unsigned variant = 0);
	using saved_repeat_fixture = c54x_test::saved_repeat_fixture;
	static saved_repeat_fixture saved_repeat_case(unsigned v);
	u16 saved_repeat_word(saved_repeat_fixture const &f, unsigned slot) const;
	void start_saved_repeat(unsigned variant = 0);
	static saved_repeat_fixture body_repeat_case(unsigned v);
	u16 body_repeat_word(saved_repeat_fixture const &f, unsigned slot) const;
	void start_body_repeat(unsigned variant = 0);
	using repeat_context_fixture = c54x_test::repeat_context_fixture;
	static repeat_context_fixture repeat_context_case(unsigned v);
	u16 repeat_context_word(repeat_context_fixture const &f, unsigned part) const;
	void start_repeat_context(unsigned variant = 0);
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
	static mvdd_source_fixture mvdd_source_case(unsigned variant, bool repeated);
	void start_mvdd_source(unsigned variant = 0);
	void start_mvdd_peripheral(unsigned variant = 0);
	struct mvdk_mapped_fixture
	{
		u16 source, destination, value;
		unsigned form, access, checkpoint;
		u32 base;
	};
	static mvdk_mapped_fixture mvdk_mapped_case(unsigned v, bool accumulator_destination = false, bool clock = false);
	void start_mvdk_mapped(unsigned variant = 0);
	struct mvdk_producer_fixture
	{
		u16 source, destination, value, expected;
		unsigned form, producer, prefix, access, checkpoint;
		u32 base;
	};
	static mvdk_producer_fixture mvdk_producer_case(unsigned v, bool accumulator_destination = false);
	void start_mvdk_producer(unsigned variant = 0);
	void start_mcbsp_control_sequence(unsigned variant = 0);
	void check_st1_memory_rules();
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
	void start_ldu_peripheral(unsigned variant = 0);
	void start_ldu_imr(unsigned variant = 0);
	void start_mvmd_status_irq(unsigned variant = 0);
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

	// Return true only when the original callback would return; order is retained.
	bool check_repeated_writa_results();
	bool check_idle_nmi_result_results();
	bool check_idle_wake_results();
	bool check_stopped_timer_reload_results();
	bool restart_pipeline_observer(unsigned phase, u64 next_cycle)
	{
		// This replaces the deadline with one CPU clock; timed restores keep theirs.
		m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
		m_pipeline_expected_cycle = next_cycle;
		m_phase = phase;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	void capture_pipeline_at(unsigned checkpoint, std::initializer_list<unsigned *> observers, const char *message)
	{
		if (m_idle_fixture_tick != checkpoint || m_pipeline_replay) return;
		expect(observers.size() && observers.size() <= m_recurring_irq_snapshot.size(), "Explicit observer checkpoint capacity");
		for (auto *observer : observers) expect(observer != nullptr, "Explicit observer checkpoint binding");
		m_recurring_irq_snapshot.fill(0);
		m_pipeline_counter_count = 0;
		for (auto *observer : observers)
		{
			m_pipeline_counter_bindings[m_pipeline_counter_count] = observer;
			m_recurring_irq_snapshot[m_pipeline_counter_count++] = *observer;
		}
		expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, message);
	}
	bool replay_pipeline_at(unsigned phase, unsigned checkpoint, const char *message)
	{
		if (m_pipeline_replay) return false;
		expect(m_pipeline_counter_count != 0, "Replay has captured observer bindings");
		m_pipeline_replay = true;
		m_idle_fixture_tick = checkpoint;
		for (unsigned i = 0; i < m_pipeline_counter_count; ++i)
			*m_pipeline_counter_bindings[i] = m_recurring_irq_snapshot[i];
		return restore_pipeline_at(phase, message);
	}
	bool restore_pipeline_at(unsigned phase, const char *message)
	{
		// Callers rewind host observations explicitly before loading guest state.
		expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, message);
		m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
		m_phase = phase;
		m_check_timer->enable(true);
		return true;
	}
	bool check_bsp_interrupt_results();
	bool check_idle_pll_results();
	bool check_xc_interrupt_results();
	bool check_repeat_endpoints_results();
	bool check_pipeline_idle_results();
	bool check_instruction_observer_results();
	bool check_block_page_results();
	bool check_mvdd_source_results();
	bool check_extended_ar_store_results();
	bool check_accumulator_shift_results();
	bool check_branch_repeat_results();
	bool check_rptbd_t_load_results();
	bool check_staged_load_results();
	bool check_immediate_arithmetic_results();
	bool check_memory_repeat_results();
	bool check_asm_store_results_part2();
	bool check_fcala_delay_results();
	bool check_body_repeat_results();
	bool check_saved_repeat_results();
	bool check_repeat_context_results();
	bool check_delayed_call_replay_results();
	bool check_mvdk_producer_results();
	bool check_mvdk_mapped_results();
	bool check_basic_repeat_results();
	bool check_rptbd_shift_store_results();
	bool check_long_mapped_original_results();
	bool check_mcbsp_control_sequence_results();
	bool check_staged_conditional_store_results();
	bool check_ldu_peripheral_results();
	bool check_ldu_imr_results();
	bool check_shifted_peripheral_results();
	bool check_mapped_dual_results();
	bool check_repeat_mvdp_irq_results();
	bool check_circular_portr_results();
	bool check_poly_results();
	bool check_popd_mapped_results();
	bool check_mvdd_peripheral_results();
	bool check_memory_immediate_results();
	bool check_block_branch_count_results();
	bool check_bd_long_accumulator_results();
	bool check_block_xc_results();
	bool check_block_xc_irq_results();
	bool check_move_repeat_owner_results();
	bool check_block_branch_edge_results();
	bool check_delay_dld_results();
	bool check_delay_compare_results();
	bool check_mapped_parallel_load_results();
	bool check_mapped_alu_results();
	bool check_memory_copy_repeat_results();
	bool check_block_near_call_results();
	bool check_repeated_mvdk_results();
	bool check_sqdst_results();
	bool check_block_exit_results();
	bool check_staged_bus_stm_results();
	bool check_rom4_fr_decoder_results();
	bool check_rom4_codec_is_results();
	bool check_rom4_crc_input_results();
	TIMER_CALLBACK_MEMBER(check_results);

	required_device<tms320c54x_device> m_cpu;
	u16 m_shifted_bus_word = 0;
	ordered_case_digest m_shifted_bus_coverage;
	ordered_case_digest m_context_coverage;
	ordered_case_digest m_idle_coverage;
	bool m_idle_mode_bank = false;
	ordered_case_digest m_circular_move_coverage;
	ordered_case_digest m_parallel_coverage;
	unsigned m_circular_move_excluded = 0;
	tms320c54x_bsp_state m_shifted_bsp;
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
	unsigned m_vector_reads = 0, m_vector_acks = 0;
	std::stringstream m_saved_pipeline;
	std::vector<std::pair<offs_t, u16>> m_observed_instructions, m_saved_observed_instructions;
	std::vector<std::array<u64, 3>> m_repeat_load_expected;
	u64 m_pipeline_expected_cycle = 0;
	bool m_pipeline_memory_case = false;
	unsigned m_pipeline_operand_reads = 0;
	unsigned m_pipeline_store_case = 0;
	unsigned m_pipeline_store_writes = 0;
	unsigned m_pipeline_indirect_case = 0;
	ordered_case_digest m_register_bank_coverage;
	unsigned m_register_bank_replays = 0;
	ordered_case_digest m_shifted_coverage;
	ordered_case_digest m_memory_immediate_coverage;
	unsigned m_memory_immediate_restores = 0;
	unsigned m_shifted_replays = 0;
	unsigned m_pipeline_branch_case = 0;
	unsigned m_pipeline_stm_case = 0;
	unsigned m_pipeline_peripheral_case = 0;
	unsigned m_pipeline_extensions_reads = 0;
	memory_passthrough_handler m_pipeline_write_tap;
	unsigned m_pipeline_register_writes = 0;
	memory_passthrough_handler m_pipeline_operand_tap;
	bool m_pipeline_replay = false;
	instruction_observer_fixture m_instruction_observer;
	rptbd_observations m_rptbd_counts, m_rptbd_saved_counts;
	replay_fixture<saved_repeat_observations> m_saved_irq_repeat, m_body_irq_repeat;
	replay_fixture<repeat_context_observations> m_repeat_context;
	replay_fixture<fcala_delay_observations> m_fcala_delay;
	u16 m_rptbd_return = 0, m_rptbd_saved_return = 0;
	unsigned m_pipeline_drained_fetches = 0;
	memory_passthrough_handler m_pipeline_fetch_tap;
	std::array<unsigned, 9> m_recurring_irq_snapshot = {};
	// Host observation bindings, deliberately outside the guest save stream.
	std::array<unsigned *, 9> m_pipeline_counter_bindings = {};
	unsigned m_pipeline_counter_count = 0;
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
	memory_passthrough_handler m_operand_data_tap;
	unsigned m_phase = 0;
	bool (tms320c54x_test_state::*m_result_owner)() = nullptr;
	std::bitset<65536> m_asserted_opcodes;
	static constexpr unsigned rom4_staged_cold_phase = 62000;
	static constexpr unsigned repeated_writa_phase = 62044;
	unsigned m_rom4_checks = 0;
	bool m_irq_raised = false;
	unsigned m_fast_stack_reads = 0;
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
	// Immutable case inputs remain unchanged across each manual replay.
	popd_fixture m_popd_fixture = {};
	u32 m_popd_write_mask = 0;
	// Separate checkpoint lifetimes; host observations still rewind explicitly.
	std::stringstream m_idle_nmi_saved, m_idle_timer_saved, m_idle_dma_saved;
	std::stringstream m_timer_prescaler_saved, m_bsp_saved, m_pll_saved, m_clock_rates_saved;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_STATE_H
