// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_CASE_H
#define MAME_NOKIA_TMS320C54X_TEST_CASE_H

#include "tms320c54x_test_support.h"
#include <array>
#include <vector>
#include <optional>

namespace c54x_test {

struct register_check
{
	int index;
	u64 value, mask = ~u64(0);
};

struct instruction_case
{
	const char *id;
	u16 opcode;
	std::vector<memory_word> program, data;
	std::vector<register_setting> initial;
	std::vector<register_check> expected;
	unsigned port_writes, port_interval;
	std::vector<memory_word> expected_data{};
	bool reset_before = true;
	std::vector<register_setting> reset_initial{};
	std::vector<memory_word> reset_data{};
	std::optional<std::array<unsigned, 2>> split_port_intervals{}; // First/middle and middle/last writes.
	std::vector<u16> additional_opcodes{}; // Independent words retained in the legacy assertion trace.
	std::vector<memory_word> expected_program{};
};

struct clocked_register_state { unsigned tick; instruction_case observation; };
struct clocked_register_action
{
	unsigned tick;
	std::vector<register_setting> values;
	bool before_observation = false;
	std::vector<memory_word> program{};
	std::vector<register_setting> input_lines{};
	std::vector<memory_word> data{};
	bool reset_cpu = false;
	std::optional<bool> bio_level{};
	std::vector<memory_word> io{};
};
struct clocked_line_sample { unsigned cycle; bool level; bool check_cycle = true; };
struct clocked_data_event
{
	bool write; std::uint32_t address; u16 value; unsigned cycle; bool check_cycle = true;
	std::optional<int> raise_input{}; // Assert a physical input after accepting this bus event.
	std::optional<unsigned> cycles_from_previous{}; // CPU clocks between selected bus events, including atomic execution.
	std::optional<unsigned> cycles_from_first{};
	std::vector<register_check> registers{}; // State visible inside the selected bus callback.
	std::optional<unsigned> minimum_program_events{}; // Required prior program events, e.g. instruction extensions.
	std::optional<std::array<u16,2>> value_range{}; // Inclusive bounds when the contract deliberately leaves the exact word open.
	std::optional<int> value_register{}; // Relational bus contract, e.g. an interrupt frame contains RTN.
};
struct clocked_irq_expectation
{
	int line;
	unsigned earliest, latest, data_events_before;
	std::optional<u16> return_pc{};
	std::vector<register_check> registers{};
	std::optional<unsigned> io_events_before{};
};

inline bool accept_data_event(std::vector<clocked_data_event> const &events, unsigned &cursor, clocked_data_event actual)
{
	if (cursor >= events.size()) return false;
	auto const &expected = events[cursor];
	bool const value_matches = expected.value_range ?
		actual.value >= (*expected.value_range)[0] && actual.value <= (*expected.value_range)[1] : actual.value == expected.value;
	if (actual.write != expected.write || actual.address != expected.address ||
		!value_matches ||
		(expected.check_cycle && actual.cycle != expected.cycle)) return false;
	++cursor;
	return true;
}

struct clocked_register_case
{
	std::vector<clocked_register_state> states;
	unsigned checkpoint, finish;
	std::vector<clocked_register_action> actions{};
	std::optional<std::uint32_t> identity{}, single_fetch{};
	bool forbid_data_access = false;
	std::optional<std::vector<clocked_data_event>> data_events{};
	std::vector<memory_word> retained_program_reads{};
	bool require_idle = true;
	bool check_pipeline_clock = false;
	std::optional<unsigned> final_pipeline_clock{};
	bool clear_pipeline_before_restore = false;
	std::vector<clocked_data_event> program_events{}; // Reads at listed addresses; any write expectation enables whole-program write observation.
	std::vector<std::array<u32,2>> data_ranges{}; // Empty means the whole data bus.
	std::vector<clocked_irq_expectation> irqs{};
	bool pipeline_frontend = true;
	std::vector<register_setting> initial_input_lines{};
	bool extended_program = true;
	std::optional<std::vector<bool>> xf_events{}; // Instruction/debugger publications; setup and post-load re-drive are separate.
	int observed_data_space = AS_DATA; // AS_IO selects port transactions using the same ordered-event policy.
	std::vector<memory_word> initial_io{};
	std::optional<std::vector<clocked_data_event>> io_events{}; // Additional IO observation when data_events covers RAM/MMRs.
	bool initial_bio = true;
	std::optional<std::vector<clocked_line_sample>> bio_samples{};
	bool observe_data_reads = true; // Write-only contracts can leave variable polling reads unconstrained.
};

// No implicit reset: the standalone runner owns reset_* metadata.
// Legacy adapters ignore it and retain their checked predecessor.
template <typename Cpu>
void prepare_case(Cpu &cpu, instruction_case const &test)
{
	for (auto const &word : test.program) cpu.space(AS_PROGRAM).write_word(word.address, word.value);
	for (auto const &word : test.data) cpu.space(AS_DATA).write_word(word.address, word.value);
	for (auto const &setting : test.initial) cpu.set_state_int(setting.index, setting.value);
}

template <typename Cpu>
void check_case_state(Cpu &cpu, instruction_case const &test)
{
	for (auto const &expected : test.expected)
	{
		u64 const actual = cpu.state_int(expected.index);
		if ((actual & expected.mask) != (expected.value & expected.mask))
			throw emu_fatalerror("C54x case %s: register %d actual=%llx expected=%llx mask=%llx",
				test.id, expected.index, (unsigned long long)actual,
				(unsigned long long)expected.value, (unsigned long long)expected.mask);
	}
	for (auto const &word : test.expected_data)
	{
		u16 const actual = cpu.space(AS_DATA).read_word(word.address);
		if (actual != word.value)
			throw emu_fatalerror("C54x case %s: data[%x]=%x expected=%x",
				test.id, word.address, actual, word.value);
	}
	for (auto const &word : test.expected_program)
	{
		u16 const actual = cpu.space(AS_PROGRAM).read_word(word.address);
		if (actual != word.value)
			throw emu_fatalerror("C54x case %s: program[%x]=%x expected=%x",
				test.id, word.address, actual, word.value);
	}
}

template <typename Cpu>
void check_case(Cpu &cpu, instruction_case const &test,
		unsigned port_writes, u64 port_interval,
		std::optional<std::array<u64, 2>> split_port_intervals = {})
{
	check_case_state(cpu, test);
	if (port_writes != test.port_writes || port_interval != test.port_interval)
		throw emu_fatalerror("C54x case %s: port writes=%u interval=%llu expected=%u/%u",
			test.id, port_writes, (unsigned long long)port_interval, test.port_writes, test.port_interval);
	if (test.split_port_intervals && (port_writes != 3 || !split_port_intervals ||
		(*split_port_intervals)[0] != (*test.split_port_intervals)[0] ||
		(*split_port_intervals)[1] != (*test.split_port_intervals)[1]))
		throw emu_fatalerror("C54x case %s: missing or incorrect intermediate port intervals", test.id);
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_CASE_H
