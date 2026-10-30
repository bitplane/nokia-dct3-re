// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_COMPOSITION_LIMITS_H
#define MAME_NOKIA_TMS320C54X_TEST_COMPOSITION_LIMITS_H

#include "tms320c54x_test_case.h"
#include <string_view>

namespace c54x_test {

enum class composition_limit { none, interrupt_pop, clock_move };
inline composition_limit select_composition_limit(std::string_view name)
{
	return name == "pop-irq-edge" ? composition_limit::interrupt_pop :
		name == "clock-mvdk-edge" ? composition_limit::clock_move : composition_limit::none;
}

// These are explicit staged-model admission limits, not silicon traps.
// Preserve the original raw0..23 identities and checkpoints2/3 in each bank.
template<typename R>
clocked_register_case composition_limit_case(unsigned raw, composition_limit kind)
{
	bool const pop = kind == composition_limit::interrupt_pop;
	unsigned const form = raw % (pop ? 2 : 3), context = (raw / (pop ? 2 : 3)) % (pop ? 3 : 2);
	u32 const base = (raw / 6) % 2 ? (pop ? 0x10000 : 0x20000) : 0;
	instruction_case input{pop ? "interrupt-pop-limit" : "clock-move-limit", 0, {}, {}, {
		{R::STATE_IDLE, 0}, {R::STATE_ILLEGAL, 0}, {R::STATE_TCR, 0x10}, {R::STATE_PMST, 0},
		{R::STATE_ST0, pop ? 0x000eULL : 0x0628ULL}, {R::STATE_ST1, pop ? 0x800ULL : 0x900ULL},
		{R::STATE_SP, pop ? 0x1000ULL : 0x3000ULL}, {R::STATE_IFR, 0}, {R::STATE_IMR, pop ? 0x1357ULL : 0},
		{R::STATE_A, 0x123456789aULL}, {R::STATE_B, pop ? 0x2233445566ULL : 0xfedcba9876ULL},
		{STATE_GENPC, base + 0x600}
	}, {}, 0, 0};
	for (unsigned i = 0; i < 16; ++i) input.program.push_back({base + 0x600 + i, 0xf495});
	unsigned at = 0;
	auto word = [&](u16 value) { input.program[at++].value = value; };
	word(context ? 0xf273 : 0xec01);
	if (context) word(0x680);
	if (pop)
	{
		if (context == 2) word(0xf495);
		word(0x8a00 | form);
		input.data = {{0x1000, 0x5a5a}};
	}
	else
	{
		word(form == 0 ? 0x7120 : form == 1 ? 0x7196 : 0x71f8);
		word(form == 2 ? 0x1420 : 0x58);
		if (form == 2) word(0x58);
		input.initial.push_back({R::STATE_BK, 13});
		for (unsigned i = 0; i < 8; ++i) input.initial.push_back({int(R::STATE_AR0) + int(i), i == 6 && form == 1 ? 0x1420ULL : 0x1400ULL + i});
		input.data = {{0x1420, 0x3146}, {0x58, 0}, {0x58, 0}};
	}
	clocked_register_case result{{{0, std::move(input)}}, 2 + raw / 12, 12};
	auto &final = result.states.front().observation;
	final.expected = pop ? std::vector<register_check>{{R::STATE_SP, 0x1000}, {R::STATE_IMR, 0x1357}, {R::STATE_IFR, 0}} :
		std::vector<register_check>{{R::STATE_AR6, form == 1 ? 0x1420ULL : 0x1406ULL}};
	if (!pop) final.expected_data = {{0x58, 0}};
	// Only require the final rejection latch: the old tests did not specify
	// its intermediate clock. Preservation and absent bus effects hold throughout.
	result.states.push_back({12, final});
	result.states.back().observation.expected.push_back({R::STATE_ILLEGAL, 1});
	result.require_idle = false;
	result.forbid_data_access = true;
	result.identity = raw;
	return result;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_COMPOSITION_LIMITS_H
