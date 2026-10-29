// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_NEAR_FLOW_H
#define MAME_NOKIA_TMS320C54X_TEST_NEAR_FLOW_H

#include "tms320c54x_test_case.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <array>
#include <utility>

namespace c54x_test {

// Literal taken/fall-through outcomes. Delay words overwrite the accumulator
// used by the condition, distinguishing decision capture from late sampling.
inline auto const &near_flow_cases()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		struct row { const char *id; u16 opcode; bool b; u64 input, output; unsigned cycles; u16 sp; };
		constexpr row rows[] = {
			{"rcd-a-taken", 0xfe44, false, 1, 2, 7, 0x0301},
			{"rcd-a-fallthrough", 0xfe44, false, 0, 3, 8, 0x0300},
			{"rcd-b-taken", 0xfe4c, true, 1, 2, 7, 0x0301},
			{"rcd-b-fallthrough", 0xfe4c, true, 0, 3, 8, 0x0300},
			{"bcd-a-negative", 0xfa43, false, 0xffffffffffULL, 2, 7, 0},
			{"bcd-a-positive", 0xfa43, false, 1, 3, 8, 0},
			{"bcd-b-negative", 0xfa4f, true, 0xffffffffffULL, 2, 7, 0},
			{"bcd-b-zero", 0xfa4f, true, 0, 2, 7, 0},
			{"bcd-b-positive", 0xfa4f, true, 1, 3, 8, 0}
		};
		std::array<instruction_case, 10> result;
		for (unsigned i = 0; i < std::size(rows); ++i)
		{
			auto const &v = rows[i];
			int const accumulator = v.b ? r::STATE_B : r::STATE_A;
			instruction_case test{v.id, v.opcode,
				{{0x05e0, 0x75d6}, {0x05e1, 0x0124}, {0x05e2, v.opcode},
				 {0x05ec, 0x75d6}, {0x05ed, 0x0124}, {0x05ee, 0xf5e1}}, {},
				{{accumulator, v.input}, {r::STATE_AR6, 0x0a03}, {r::STATE_BK, 4}, {r::STATE_PC, 0x05e0}, {r::STATE_IDLE, 0}},
				{{accumulator, v.output}}, 2, v.cycles};
			bool const returning = i < 4;
			unsigned pc = 0x05e3;
			if (returning)
			{
				test.initial.emplace_back(r::STATE_SP, 0x0300);
				test.expected.push_back({r::STATE_SP, v.sp});
				test.data.emplace_back(0x0300, 0x05ec);
			}
			else test.program.emplace_back(pc++, 0x05ec);
			for (unsigned value : {returning ? 0U : 1U, 2U, 3U})
				test.program.emplace_back(pc++, (v.b ? 0xe900 : 0xe800) | value);
			for (u16 word : {0x75d6, 0x0124, 0xf5e1}) test.program.emplace_back(pc++, word);
			result[i < 4 ? i : i + 1] = std::move(test);
		}
		// MMR 0x60/0x61 belong to an interrupting peripheral in the legacy
		// fixture. Use 0x62/0x63 to test seven-bit operands without that peer.
		result[4] = {"push-pop-high-mmr", 0x4a62,
			{{0x05e0, 0x75d6}, {0x05e1, 0x0124}, {0x05e2, 0x4a62}, {0x05e3, 0x4a63},
			 {0x05e4, 0x8a62}, {0x05e5, 0x8a63}, {0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1}},
			{{0x0062, 0x1234}, {0x0063, 0x5678}},
			{{r::STATE_SP, 0x0300}, {r::STATE_AR6, 0x0a03}, {r::STATE_BK, 4}, {r::STATE_PC, 0x05e0}, {r::STATE_IDLE, 0}},
			{{r::STATE_SP, 0x0300}}, 2, 6,
			{{0x02ff, 0x1234}, {0x02fe, 0x5678}, {0x0062, 0x5678}, {0x0063, 0x1234}}};
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_NEAR_FLOW_H
