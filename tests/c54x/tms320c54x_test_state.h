// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_STATE_H
#define MAME_NOKIA_TMS320C54X_TEST_STATE_H

#include "emuopts.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "gsm_tch_f_l1.h"
#include "util/gsmfr.h"
#include "tms320c54x_bsp_state.h"
#include "tms320c54x_test_selection.h"
#include "tms320c54x_test_support.h"
#include "tms320c54x_test_register_sequence.h"
#include "tms320c54x_test_snapshot.h"
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
		m_cpu(*this, "maincpu")
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
	void check_control_disassembler();
	void start_pipeline_mu4_prologue(unsigned variant = 0);
	void observe_context_irq(int line);
	bool check_rom4_context_results();
	void record_context_case(unsigned bank, unsigned raw)
	{
		if (profile_is("rom4-context")) m_context_coverage.add((bank << 8) | raw);
	}
	void start_pipeline_original_worker_roundtrip(unsigned variant = 0);
	void start_pipeline_original_context_call(unsigned variant = 0, bool full_body = false);
	void start_pipeline_mu4_timer_context(unsigned variant = 0);
	void start_pipeline_mu4_recurring_timer(unsigned variant = 0);
	void start_pipeline_idle_sp_tail(unsigned variant = 0);
	void start_pipeline_original_idle(unsigned variant = 0);
	void start_instruction_observer(unsigned variant = 0);
	static unsigned circular_fixture_mask(u16 bk);
	static bool circular_fixture_domain(u16 initial, int step, u16 bk);
	static u16 circular_fixture_next(u16 initial, int step, u16 bk);
	void start_pipeline_store_mvmm(unsigned variant = 0);
	struct indexed_store_fixture;
	struct extended_banz_fixture
	{
		unsigned form, reg, selected;
		u16 initial, offset, resolved, next_ar;
		u32 base;
		u16 bk;
	};
	static extended_banz_fixture extended_banz_case(unsigned variant);
	void start_long_mapped_original(unsigned variant = 0);




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
	struct rptbd_t_load_fixture
	{
		unsigned count, slot, form, snapshot, accumulator;
		u16 value, status, mode, source;
		u32 base;
		bool branch = false;
		unsigned branch_count = 0;
		bool ordinary = false;
		unsigned control = 0, late_t = 0;
		unsigned read_clock() const { return ordinary ? 7 + unsigned(slot != 0) + unsigned(control == 5 || ((control == 1 || control == 2) && slot)) : 7 + slot; }
		u16 target() const { return accumulator ? 0x580 : 0x610; }
		u32 target_base() const { return ordinary && control == 4 ? base ^ 0x20000 : base; }
		u16 loaded_t() const { return late_t == 2 ? 0x5aa5 : late_t ? value ^ 0xffff : value; }
		u16 product() const { return u16(s32(s16(loaded_t())) * 3); }
		u16 final_sp() const { return 0x1e00 + (ordinary ? control == 3 ? -1 : control == 4 ? -2 : control == 5 ? 1 : 0 : 0); }
	};
	rptbd_t_load_fixture rptbd_t_load_case(unsigned v) const;
	void start_rptbd_t_load(unsigned variant = 0);
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
	void start_repeat_mvdp(unsigned variant = 0);
	void start_repeat_mvdp_irq(unsigned variant = 0);
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
	void check_st1_memory_rules();
	virtual void machine_start() override;

	virtual void machine_reset() override;

	void program_map(address_map &map) ATTR_COLD;

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
	bool check_instruction_observer_results();
	bool check_mvdd_source_results();
	bool check_rptbd_t_load_results();
	bool check_mvdk_producer_results();
	bool check_mvdk_mapped_results();
	bool check_long_mapped_original_results();
	bool check_staged_conditional_store_results();
	bool check_mapped_dual_results();
	bool check_repeat_mvdp_irq_results();
	bool check_popd_mapped_results();
	bool check_circular_move_results();
	bool check_rom4_fr_decoder_results();
	bool check_rom4_codec_is_results();
	bool check_rom4_crc_input_results();
	TIMER_CALLBACK_MEMBER(check_results);

	required_device<tms320c54x_device> m_cpu;
	u16 m_shifted_bus_word = 0;
	ordered_case_digest m_context_coverage;
	ordered_case_digest m_circular_move_coverage;
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
	memory_passthrough_handler m_vector_prefetch_tap;
	memory_passthrough_handler m_vector_stack_tap;
	unsigned m_vector_reads = 0, m_vector_acks = 0;
	std::stringstream m_saved_pipeline;
	std::vector<std::pair<offs_t, u16>> m_observed_instructions, m_saved_observed_instructions;
	u64 m_pipeline_expected_cycle = 0;
	bool m_pipeline_memory_case = false;
	unsigned m_pipeline_operand_reads = 0;
	unsigned m_pipeline_store_case = 0;
	unsigned m_pipeline_store_writes = 0;
	unsigned m_pipeline_indirect_case = 0;
	unsigned m_pipeline_branch_case = 0;
	unsigned m_pipeline_stm_case = 0;
	unsigned m_pipeline_peripheral_case = 0;
	unsigned m_pipeline_extensions_reads = 0;
	memory_passthrough_handler m_pipeline_write_tap;
	unsigned m_pipeline_register_writes = 0;
	memory_passthrough_handler m_pipeline_operand_tap;
	bool m_pipeline_replay = false;
	instruction_observer_fixture m_instruction_observer;
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
	memory_passthrough_handler m_operand_data_tap;
	unsigned m_phase = 0;
	bool (tms320c54x_test_state::*m_result_owner)() = nullptr;
	std::bitset<65536> m_asserted_opcodes;
	static constexpr unsigned rom4_staged_cold_phase = 62000;
	unsigned m_rom4_checks = 0;
	bool m_irq_raised = false;
	unsigned m_fast_stack_reads = 0;
	unsigned m_fast_return_case = 0;
	unsigned m_repeat_reads = 0;
	u16 m_stack_mmr[2] = {};
	unsigned m_irq_trigger_read = 1;
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
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_STATE_H
