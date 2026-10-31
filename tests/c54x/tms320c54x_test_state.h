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
	bool check_rom4_fr_decoder_results();
	bool check_rom4_codec_is_results();
	bool check_rom4_crc_input_results();
	TIMER_CALLBACK_MEMBER(check_results);

	required_device<tms320c54x_device> m_cpu;
	u16 m_shifted_bus_word = 0;
	ordered_case_digest m_context_coverage;
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
	// Separate checkpoint lifetimes; host observations still rewind explicitly.
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_STATE_H
