// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SHIFT_EXAMPLES_H
#define MAME_NOKIA_TMS320C54X_TEST_SHIFT_EXAMPLES_H

#include "tms320c54x_test_port_case.h"

namespace c54x_test {


// These atomic examples are not duplicates of the staged ALU matrices:
// PORTW brackets retain instruction-cost checks and partial predecessor state.
inline auto const &register_bit_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, std::vector<u16> words, std::vector<register_setting> initial,
			std::vector<register_check> expected, unsigned interval = 3) {
			auto test = port_case(id, std::move(words), std::move(initial), std::move(expected), {}, interval);
			test.reset_before = result.empty(); result.push_back(std::move(test));
		};
		add("ror-a-carry-in-guard-clear", {0xf490},
			{{r::STATE_A, 0xab80000002ULL}, {r::STATE_B, 0x1234}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0xc0000001}, {r::STATE_B, 0x1234}, {r::STATE_ST0, 0, 0x0800}});
		add("ror-b-inherited-clear-carry", {0xf590}, {{r::STATE_B, 0xff00000001ULL}},
			{{r::STATE_B, 0}, {r::STATE_A, 0xc0000001}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("or-b-right-zero-fill", {0xf3b0},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0xff80000001ULL}, {r::STATE_ST0, 0x0800}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_B, 0xff80ff8001ULL}, {r::STATE_A, 0x1234}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("sftl-b-left-clears-guard", {0xf3e8},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0xff80000081ULL}, {r::STATE_ST0, 0x0800}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_B, 0x8100}, {r::STATE_A, 0x1234}, {r::STATE_ST0, 0, 0x0800}});
		add("sftl-b-right-outgoing-carry", {0xf3f8},
			{{r::STATE_B, 0xff80000081ULL}, {r::STATE_ST0, 0}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_B, 0x800000}, {r::STATE_A, 0x1234}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("and-immediate-b-zero-extend", {0xf330, 0x0ff0},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0xffab12cd34ULL}, {r::STATE_ST0, 0x0800}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_B, 0x0d30}, {r::STATE_A, 0x1234}, {r::STATE_ST0, 0x0800, 0x0800}}, 4);
		add("and-immediate-cross-accumulator", {0xf130, 0xf0f0},
			{{r::STATE_A, 0xffabcd1234ULL}, {r::STATE_B, 0x5678}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_B, 0x1030}, {r::STATE_A, 0xffabcd1234ULL}, {r::STATE_ST0, 0x0800, 0x0800}}, 4);
		add("xor-b-left-truncates-forty-bits", {0xf3c8},
			{{r::STATE_A, 0x12345678}, {r::STATE_B, 0x8080000001ULL}, {r::STATE_ST0, 0x0800}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_B, 0x0080000101ULL}, {r::STATE_A, 0x12345678}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("and-immediate-a", {0xf030, 0x00f0}, {{r::STATE_A, 0x1234}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_A, 0x30}}, 4);
		add("or-immediate-a-preserves-guard", {0xf040, 0x00f0},
			{{r::STATE_A, 0xff00000000ULL}, {r::STATE_AR6, 0x0a03}}, {{r::STATE_A, 0xff000000f0ULL}}, 4);
		add("xor-immediate-high-word-a", {0xf065, 0x00ff},
			{{r::STATE_A, 0xff00000000ULL}, {r::STATE_AR6, 0x0a03}}, {{r::STATE_A, 0xff00ff0000ULL}}, 4);
		add("complement-a-forty-bits", {0xf493},
			{{r::STATE_A, 0xff12345678ULL}, {r::STATE_B, 0x5678}, {r::STATE_ST0, 0x0800}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_A, 0x00edcba987ULL}, {r::STATE_B, 0x5678}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("sftl-a-left8-inherited-b", {0xf0e8},
			{{r::STATE_A, 0xff81000081ULL}, {r::STATE_ST0, 0}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_A, 0x8100}, {r::STATE_B, 0x5678}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("sftl-a-right8-carry", {0xf0f8},
			{{r::STATE_A, 0xff12348180ULL}, {r::STATE_AR6, 0x0a03}, {r::STATE_ST0, 0}},
			{{r::STATE_A, 0x00123481}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("xor-a-left8-full-width", {0xf0c8}, {{r::STATE_A, 0x0001020304ULL}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_A, 0x0103010704ULL}});
		add("sftl-a-right1-carry", {0xf0ff}, {{r::STATE_A, 0x80000003}, {r::STATE_ST0, 0}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_A, 0x40000001}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("xor-a-to-b-preserves-guard-status", {0xf1c0},
			{{r::STATE_A, 0xff0000000fULL}, {r::STATE_B, 0x00abcd0000ULL}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_B, 0xffabcd000fULL}, {r::STATE_A, 0xff0000000fULL}, {r::STATE_ST0, 0x0800}});
		add("or-a-right8-to-b-preserves-status", {0xf1b8},
			{{r::STATE_A, 0xff80000000ULL}, {r::STATE_B, 0x1234}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_B, 0x00ff801234ULL}, {r::STATE_A, 0xff80000000ULL}, {r::STATE_ST0, 0x0800}});
		add("sfta-a-left8-overflow-unsaturated", {0xf468},
			{{r::STATE_A, 0x800000}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0x80000000}, {r::STATE_ST0, 0x0400, 0x0400}});
		add("or-b-to-a-preserves-guard-status", {0xf2a0},
			{{r::STATE_A, 0xf0}, {r::STATE_B, 0xff0000000fULL}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0xff000000ffULL}, {r::STATE_B, 0xff0000000fULL}, {r::STATE_ST0, 0x0800}});
		result.front().reset_initial = {{r::STATE_AR6, 0x0a01}, {r::STATE_BK, 4}, {r::STATE_ST1, 0x0100}};
		return result;
	}();
	return cases;
}


} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SHIFT_EXAMPLES_H
