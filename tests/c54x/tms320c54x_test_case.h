// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_CASE_H
#define MAME_NOKIA_TMS320C54X_TEST_CASE_H

#include "tms320c54x_test_support.h"
#include <vector>

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
};

struct clocked_register_state { unsigned tick; instruction_case observation; };
struct clocked_register_case
{
	std::vector<clocked_register_state> states;
	unsigned checkpoint, finish;
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
}

template <typename Cpu>
void check_case(Cpu &cpu, instruction_case const &test,
		unsigned port_writes, u64 port_interval)
{
	check_case_state(cpu, test);
	if (port_writes != test.port_writes || port_interval != test.port_interval)
		throw emu_fatalerror("C54x case %s: port writes=%u interval=%llu expected=%u/%u",
			test.id, port_writes, (unsigned long long)port_interval, test.port_writes, test.port_interval);
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_CASE_H
