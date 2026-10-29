// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_LONG_REFERENCE_H
#define MAME_NOKIA_TMS320C54X_TEST_LONG_REFERENCE_H

#include "tms320c54x_test_case.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <array>
#include <utility>

namespace c54x_test {

// Literal TI long-word arithmetic examples. C16 changes the low-lane carry
// boundary; the final two DADD rows distinguish AR2/AR5 from decoy AR3 data.
// Masks preserve the original checks: T-based rows assert carry only, and
// DSADT/C16 intentionally makes no carry assertion.
inline auto const &long_reference_cases()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::array<instruction_case, 18> result;
		struct long_row { const char *id; u16 opcode; u64 a; u16 st1, high, low; u64 out_a, out_b; u16 ar3, st0; unsigned cycles; };
		constexpr long_row rows[] = {
			{"dadd-postincrement", 0x5193, 0x56788933, 0x0100, 0x1534, 0x3456, 0x56788933, 0x6bacbd89, 0x0f92, 0, 3},
			{"dadd-c16-postdecrement", 0x518b, 0x56783933, 0x0180, 0x1534, 0x3456, 0x56783933, 0x6bac6d89, 0x0f8e, 0, 3},
			{"dsub-postincrement", 0x5493, 0x56788933, 0x0100, 0x1534, 0x3456, 0x414454dd, 0, 0x0f92, 0x0800, 3},
			{"dsub-c16-postdecrement", 0x548b, 0x56783933, 0x0180, 0x1534, 0x3456, 0x414404dd, 0, 0x0f8e, 0x0800, 3},
			{"dadd-absolute-saturate", 0x50f8, 0x7fffffff, 0x0300, 0, 1, 0x7fffffff, 0, 0x0f90, 0x0400, 4},
			{"dadd-c16-lane-carry", 0x5083, 0x7fffffff, 0x0380, 1, 1, 0x80000000, 0, 0x0f90, 0, 3},
			{"dsub-c16-lane-borrow", 0x5483, 0x00010000, 0x0380, 0, 1, 0x0001ffff, 0, 0x0f90, 0x0800, 3},
			{"drsub-postincrement", 0x5893, 0x56788933, 0x0100, 0x1534, 0x3456, 0xffbebbab23ULL, 0, 0x0f92, 0, 3},
			{"drsub-c16-postdecrement", 0x588b, 0x56783933, 0x0180, 0x1534, 0x3456, 0xffbebcfb23ULL, 0, 0x0f8e, 0, 3},
			{"dadd-ar2-not-ar3", 0x5082, 0x00010000, 0x0100, 1, 0xffff, 0x0002ffff, 0, 0x0f90, 0, 3},
			{"dadd-ar5-not-ar3", 0x5085, 0x00010000, 0x0100, 1, 0xffff, 0x0002ffff, 0, 0x0f90, 0, 3}
		};
		auto program = [](u16 opcode, bool absolute) {
			std::vector<memory_word> words{{0x05e0, 0x75d6}, {0x05e1, 0x0124}, {0x05e2, opcode}};
			unsigned pc = 0x05e3;
			if (absolute) words.emplace_back(pc++, 0x0f90);
			for (u16 word : {0x75d6, 0x0124, 0xf5e1}) words.emplace_back(pc++, word);
			return words;
		};
		for (unsigned i = 0; i < std::size(rows); ++i)
		{
			auto const &v = rows[i];
			instruction_case test{v.id, v.opcode, program(v.opcode, v.opcode == 0x50f8),
				{{0x0f90, u16(i >= 9 ? 2 : v.high)}, {0x0f91, u16(i >= 9 ? 3 : v.low)}},
				{{r::STATE_A, v.a}, {r::STATE_B, 0}, {r::STATE_AR3, 0x0f90},
				 {r::STATE_ST0, v.opcode == 0x588b ? 0x0800U : 0U}, {r::STATE_ST1, v.st1},
				 {r::STATE_AR6, 0x0a03}, {r::STATE_PC, 0x05e0}, {r::STATE_IDLE, 0}},
				{{r::STATE_A, v.out_a}, {r::STATE_B, v.out_b}, {r::STATE_AR3, v.ar3}, {r::STATE_ST0, v.st0}},
				2, v.cycles};
			if (i >= 9)
			{
				test.initial.insert(test.initial.end(), {{r::STATE_AR2, 0x0f92}, {r::STATE_AR5, 0x0f92}});
				test.data.insert(test.data.end(), {{0x0f92, v.high}, {0x0f93, v.low}});
				test.expected.insert(test.expected.end(), {{r::STATE_AR2, 0x0f92}, {r::STATE_AR5, 0x0f92}});
				test.expected_data = test.data;
			}
			result[i] = std::move(test);
		}
		struct t_row { const char *id; u16 opcode, st1; u64 a, b; u16 ar3; int carry; unsigned cycles; };
		constexpr t_row t_rows[] = {
			{"dadst-c16-postdecrement", 0x5a8b, 0x0180, 0x38791111, 0, 0x0f8e, 0, 3},
			{"dadst-postincrement", 0x5a93, 0x0100, 0x3879579b, 0, 0x0f92, 0, 3},
			{"dsadt-postincrement", 0x5e93, 0x0100, 0xfff1ef1111ULL, 0, 0x0f92, 0, 3},
			{"dsadt-c16-postdecrement", 0x5e8b, 0x0180, 0xfff1ef579bULL, 0, 0x0f8e, -1, 3},
			{"dsubt-postincrement", 0x5c93, 0x0100, 0xfff1ef1111ULL, 0, 0x0f92, 0, 3},
			{"dsubt-c16-postdecrement", 0x5c8b, 0x0180, 0xfff1ef1111ULL, 0, 0x0f8e, 0, 3},
			{"dadst-b-absolute", 0x5bf8, 0x0180, 0, 0x38791111, 0x0f90, 0, 4}
		};
		for (unsigned i = 0; i < std::size(t_rows); ++i)
		{
			auto const &v = t_rows[i];
			instruction_case test{v.id, v.opcode, program(v.opcode, v.opcode == 0x5bf8),
				{{0x0f90, 0x1534}, {0x0f91, 0x3456}},
				{{r::STATE_A, 0}, {r::STATE_B, 0}, {r::STATE_T, 0x2345}, {r::STATE_AR3, 0x0f90},
				 {r::STATE_ST0, 0}, {r::STATE_ST1, v.st1}, {r::STATE_AR6, 0x0a03}, {r::STATE_PC, 0x05e0}, {r::STATE_IDLE, 0}},
				{{r::STATE_A, v.a}, {r::STATE_B, v.b}, {r::STATE_T, 0x2345}, {r::STATE_AR3, v.ar3}}, 2, v.cycles};
			if (v.carry >= 0) test.expected.push_back({r::STATE_ST0, v.carry ? 0x0800U : 0U, 0x0800});
			result[11 + i] = std::move(test);
		}
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_LONG_REFERENCE_H
