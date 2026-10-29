// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SHIFT_EXAMPLES_H
#define MAME_NOKIA_TMS320C54X_TEST_SHIFT_EXAMPLES_H

#include "tms320c54x_test_port_case.h"

namespace c54x_test {

// Ordered scalar/memory-shift examples. The MIN tie reads its predecessor's
// B, and partial setup/status masks remain partial throughout the group.
inline auto const &shift_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, u16 opcode, std::vector<register_setting> initial,
			std::vector<register_check> expected, std::vector<memory_word> data = {}) {
			auto test = port_case(id, {opcode}, std::move(initial), std::move(expected), std::move(data));
			test.reset_before = result.empty();
			result.push_back(std::move(test));
		};
		add("add-smem-ts-a", 0x0483,
			{{r::STATE_A, 0x10}, {r::STATE_T, 4}, {r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0x0100}, {r::STATE_AR3, 0x0200}},
			{{r::STATE_A, 0x1010}, {r::STATE_ST0, 0, 0x0800}}, {{0x0200, 0x0100}});
		add("add-smem-ts-negative-b", 0x0583,
			{{r::STATE_A, 0x12345678}, {r::STATE_B, 0x10000}, {r::STATE_T, 0xabff}, {r::STATE_ST0, 0}},
			{{r::STATE_B, 0xc000}, {r::STATE_A, 0x12345678}, {r::STATE_AR3, 0x0200}, {r::STATE_T, 0xabff}}, {{0x0200, 0x8001}});
		add("sub-smem-ts-no-borrow", 0x0d83, {{r::STATE_T, 4}, {r::STATE_B, 0x1000}, {r::STATE_ST0, 0}},
			{{r::STATE_B, 0}, {r::STATE_ST0, 0x0800, 0x0a00}}, {{0x0200, 0x0100}});
		add("sub-smem-ts-logical-a", 0x0c83,
			{{r::STATE_A, 0x10000}, {r::STATE_B, 0x12345678}, {r::STATE_T, 0xabff}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0xc000}, {r::STATE_B, 0x12345678}, {r::STATE_ST0, 0x0800, 0x0c00}, {r::STATE_AR3, 0x0200}, {r::STATE_T, 0xabff}}, {{0x0200, 0x8001}});
		add("load-smem-ts-signed-b", 0x1583, {{r::STATE_T, 8}, {r::STATE_ST1, 0x0100}, {r::STATE_B, 0}},
			{{r::STATE_B, 0xfffffedc00ULL}}, {{0x0200, 0xfedc}});
		add("load-smem-ts-negative-a", 0x1483, {{r::STATE_T, 0x003f}, {r::STATE_A, 0}},
			{{r::STATE_A, 0xffffffc000ULL}}, {{0x0200, 0x8001}});
		add("add-xmem-shift-postincrement", 0x9083, {{r::STATE_A, 1}, {r::STATE_AR2, 0x0300}},
			{{r::STATE_A, 17}, {r::STATE_AR2, 0x0301}}, {{0x0300, 2}});
		add("sub-xmem-shift-signed-b", 0x9304,
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0}, {r::STATE_AR2, 0x0300}, {r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x7fff0}, {r::STATE_ST0, 0, 0x0800}, {r::STATE_AR2, 0x0300}}, {{0x0300, 0x8001}});
		add("max-a-signed-negative", 0xf486, {{r::STATE_A, 0xfffffffff6ULL}, {r::STATE_B, 0xffffffffcbULL}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0xfffffffff6ULL}, {r::STATE_B, 0xffffffffcbULL}, {r::STATE_ST0, 0, 0x0800}});
		add("max-b-larger", 0xf586, {{r::STATE_A, 0x55}, {r::STATE_B, 0x1234}, {r::STATE_ST0, 0}},
			{{r::STATE_A, 0x55}, {r::STATE_B, 0x1234}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("min-a-inherited-b-tie", 0xf487, {{r::STATE_A, 0x1234}, {r::STATE_ST0, 0}},
			{{r::STATE_A, 0x1234}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("min-b-signed-guard", 0xf587, {{r::STATE_A, 0xff00000000ULL}, {r::STATE_B, 0x7fffffff}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0xff00000000ULL}, {r::STATE_B, 0xff00000000ULL}, {r::STATE_ST0, 0, 0x0800}});
		add("exp-a-negative", 0xf48e, {{r::STATE_A, 0xffffffffcbULL}, {r::STATE_T, 0}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_T, 0x19}, {r::STATE_A, 0xffffffffcbULL}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("exp-b-guard", 0xf58e, {{r::STATE_B, 0x0785432105ULL}},
			{{r::STATE_T, 0xfffc}, {r::STATE_B, 0x0785432105ULL}});
		add("norm-a-left", 0xf48f, {{r::STATE_A, 0xfffffff001ULL}, {r::STATE_T, 0x13}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xff80080000ULL}});
		add("norm-b-to-a-right", 0xf68f, {{r::STATE_A, 0xfffffff001ULL}, {r::STATE_B, 0x210a0a0a0aULL}, {r::STATE_T, 0xfff9}},
			{{r::STATE_A, 0x0042141414ULL}, {r::STATE_B, 0x210a0a0a0aULL}});
		add("norm-b-left-preserves-status", 0xf78f,
			{{r::STATE_A, 0x12345678}, {r::STATE_B, 0xfffffff001ULL}, {r::STATE_T, 0x13}, {r::STATE_ST0, 0x1800}},
			{{r::STATE_A, 0x12345678}, {r::STATE_B, 0xff80080000ULL}, {r::STATE_T, 0x13}, {r::STATE_ST0, 0x1800}, {r::STATE_ST1, 0x0100}});
		add("norm-b-right-preserves-status", 0xf78f,
			{{r::STATE_A, 0x12345678}, {r::STATE_B, 0x210a0a0a0aULL}, {r::STATE_T, 0xfff9}, {r::STATE_ST0, 0x1800}},
			{{r::STATE_A, 0x12345678}, {r::STATE_B, 0x0042141414ULL}, {r::STATE_T, 0xfff9}, {r::STATE_ST0, 0x1800}, {r::STATE_ST1, 0x0100}});
		add("exp-zero", 0xf48e, {{r::STATE_A, 0}, {r::STATE_T, 0x1234}}, {{r::STATE_T, 0}});
		add("norm-ovm-carry", 0xf48f, {{r::STATE_A, 0x40000000}, {r::STATE_T, 1}, {r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0x0300}},
			{{r::STATE_A, 0x7fffffff}, {r::STATE_ST0, 0x0c00, 0x0c00}});
		add("rnd-a-to-b-one-cycle", 0xf59f, {{r::STATE_A, 0xffffffffffULL}, {r::STATE_B, 1}, {r::STATE_ST0, 0x0a00}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0xffffffffffULL}, {r::STATE_B, 0x7fff}, {r::STATE_ST0, 0x0a00, 0x0a00}});
		add("rnd-ovm-one-cycle", 0xf49f, {{r::STATE_A, 0x7fffffff}, {r::STATE_ST1, 0x0200}},
			{{r::STATE_A, 0x7fffffff}, {r::STATE_ST0, 0x0a00, 0x0a00}});
		add("sat-b-positive-guard", 0xf583, {{r::STATE_B, 0x7123456789ULL}, {r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0}},
			{{r::STATE_B, 0x7fffffff}, {r::STATE_ST0, 0x0a00, 0x0a00}});
		add("sat-a-negative-guard", 0xf483, {{r::STATE_A, 0xf812345678ULL}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0xff80000000ULL}, {r::STATE_ST0, 0x0c00, 0x0c00}});
		add("sat-b-in-range-clear-overflow", 0xf583, {{r::STATE_B, 0x00123456}, {r::STATE_ST0, 0x0a00}},
			{{r::STATE_B, 0x00123456}, {r::STATE_ST0, 0x0800, 0x0a00}});
		result.front().reset_initial = {{r::STATE_AR6, 0x0a03}, {r::STATE_BK, 4}};
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SHIFT_EXAMPLES_H
