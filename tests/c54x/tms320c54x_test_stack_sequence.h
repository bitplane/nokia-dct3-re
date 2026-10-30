// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_STACK_SEQUENCE_H
#define MAME_NOKIA_TMS320C54X_TEST_STACK_SEQUENCE_H

#include "tms320c54x_test_case.h"
#include <utility>

namespace c54x_test {

// Functional FRAME/PSHM composition; observation clocks are model regressions.
template <typename R>
clocked_register_case frame_sequence(unsigned key)
{
	int const offset = (key & 255) < 128 ? int(key & 255) : int(key & 255) - 256;
	u16 const target = 0x1000 + offset - 1;
	instruction_case initial{"frame-push", u16(0xee00 | (key & 255)), {}, {{target, 0xaaaa}}, {
		{R::STATE_IDLE, 0}, {R::STATE_TCR, 0x10}, {R::STATE_PMST, 0}, {R::STATE_ILLEGAL, 0},
		{R::STATE_ST0, 0xe}, {R::STATE_ST1, 0x800}, {R::STATE_SP, 0x1000},
		{R::STATE_AR1, 0x5678}, {R::STATE_IFR, 0}, {R::STATE_IMR, 0}, {STATE_GENPC, 0x600}
	}, {{R::STATE_ILLEGAL, 0}, {R::STATE_ST0, 0xe}, {R::STATE_ST1, 0x800}, {R::STATE_SP, 0x1000}}, 0, 0};
	for (unsigned i = 0; i < 16; ++i) initial.program.push_back({0x600 + i, 0xf495});
	initial.program.push_back({0x600, initial.opcode}); initial.program.push_back({0x601, 0x4a11});
	auto allocated = initial; allocated.program.clear(); allocated.data.clear(); allocated.initial.clear();
	allocated.expected.back().value = 0x1000 + offset;
	auto pushed = allocated; pushed.expected.back().value = target;
	clocked_register_case result{{{0, std::move(initial)}, {4, std::move(allocated)}, {6, std::move(pushed)}},
		std::array<unsigned, 3>{3,4,6}[key / 256], 10};
	result.identity = key; result.require_idle = false; result.check_pipeline_clock = true;
	result.data_events = std::vector<clocked_data_event>{{false, 0x11, 0x5678, 6}, {true, target, 0x5678, 7}};
	return result;
}

// Table 7-9 requires one gap for these PSHM/POPM -> basic CPL consumers.
template <typename R>
clocked_register_case stack_sp_sequence(unsigned index)
{
	unsigned const key = index < 2 ? 2 + index : 6 + (index & 1) + (index / 2) * 4;
	bool const store = key & 1, pop = key & 8, other = key & 4;
	u16 const alternate = pop ? 0x1028 : 0x1026;
	u16 const opcode = pop ? other ? 0x8a0b : 0x8a11 : 0x4a11;
	instruction_case initial{"stack-sp", opcode, {}, {{0xfff, 0xaaaa}, {0x1000, 0x5678}, {0x1027, 0x1111}, {alternate, 0x2222}}, {
		{R::STATE_IDLE, 0}, {R::STATE_TCR, 0x10}, {R::STATE_PMST, 0}, {R::STATE_ILLEGAL, 0},
		{R::STATE_ST1, 0x4800}, {R::STATE_SP, 0x1000}, {R::STATE_AR1, 0x5678},
		{R::STATE_A, 0x7777}, {R::STATE_IFR, 0}, {R::STATE_IMR, 0}, {STATE_GENPC, 0x600}
	}, {{R::STATE_ILLEGAL, 0}}, 0, 0};
	for (unsigned i = 0; i < 16; ++i) initial.program.push_back({0x600 + i, 0xf495});
	initial.program.push_back({0x600, opcode}); initial.program.push_back({0x602, store ? 0x8027 : 0x1027});
	instruction_case final{"stack-sp", opcode, {}, {}, {}, {
		{R::STATE_ILLEGAL, 0}, {R::STATE_SP, pop ? 0x1001U : 0xfffU}, {R::STATE_A, store ? 0x7777U : 0x2222U}
	}, 0, 0, {{0xfff, pop ? 0xaaaa : 0x5678}, {0x1027, 0x1111}, {alternate, store ? 0x7777 : 0x2222}}};
	if (pop) final.expected_data.push_back({other ? 0xb : 0x11, 0x5678});
	clocked_register_case result{{{0, std::move(initial)}, {10, std::move(final)}}, 6, 10};
	result.identity = key; result.require_idle = false; result.check_pipeline_clock = true;
	return result;
}

template <typename R>
clocked_register_case stack_sequence(unsigned key)
{
	if (key < 768) return frame_sequence<R>(key);
	auto result = stack_sp_sequence<R>(key - 768);
	result.identity = 768 + *result.identity;
	return result;
}

} // namespace c54x_test
#endif // MAME_NOKIA_TMS320C54X_TEST_STACK_SEQUENCE_H
