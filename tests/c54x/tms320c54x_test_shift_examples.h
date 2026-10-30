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

inline auto const &atomic_shift_examples()
{
	using r = tms320c54x_device;
	struct entry {
		const char *id;
		u16 opcode;
		u64 a_before, b_before, a_after, b_after;
		u16 st0_before, st0_after, st1 = 0x0100;
	};
	static const auto cases = [] {
		static constexpr entry references[] = {
			{ "sftl-a-right4-to-b", 0xf1fc, 0x12345678, 0, 0x12345678, 0x01234567, 0, 0x0800 },
			{ "sfta-a-left3", 0xf463, 0x1001, 0, 0x8008, 0, 0x0800, 0 },
			{ "sfta-a-right16-sign-fill", 0xf470, 0xff80000000ULL, 0, 0xffffff8000ULL, 0, 0x0800, 0 },
			{ "sfta-a-right8-to-b", 0xf578, 0x123456ff, 0, 0x123456ff, 0x123456, 0, 0x0800 },
			{ "sfta-b-left3", 0xf763, 0, 0x1234, 0, 0x91a0, 0x0800, 0 },
			{ "sfta-a-right9-sign-fill", 0xf477, 0xff80000100ULL, 0x1234, 0xffffc00000ULL, 0x1234, 0x1000, 0x1800 },
			{ "sfta-a-right9-zero-fill", 0xf477, 0xff80000100ULL, 0x1234, 0x007fc00000ULL, 0x1234, 0x1000, 0x1800, 0 },
			{ "sfta-a-left1-to-b", 0xf561, 0x1001, 0x1234, 0x1001, 0x2002, 0x1800, 0x1000 },
			{ "sfta-b-right3-to-a-sign-fill", 0xf67d, 0x1234, 0xff80000004ULL, 0xfff0000000ULL, 0xff80000004ULL, 0x1000, 0x1800 },
			{ "sfta-b-right3-to-a-zero-fill", 0xf67d, 0x1234, 0xff80000004ULL, 0x1ff0000000ULL, 0xff80000004ULL, 0x1000, 0x1800, 0 },
			{ "sfta-b-left6", 0xf766, 0x1234, 0x1234, 0x1234, 0x48d00, 0x1800, 0x1000 },
			{ "sfta-b-right6-sign-fill", 0xf77a, 0x1234, 0xff80000020ULL, 0x1234, 0xfffe000000ULL, 0x1000, 0x1800 },
			{ "sfta-b-right6-zero-fill", 0xf77a, 0x1234, 0xff80000020ULL, 0x1234, 0x03fe000000ULL, 0x1000, 0x1800, 0 },
			{ "sfta-a-left2", 0xf462, 0x1001, 0x1234, 0x4004, 0x1234, 0x1800, 0x1000 },
			{ "sfta-a-right4-sign-fill", 0xf47c, 0xff80000008ULL, 0x1234, 0xfff8000000ULL, 0x1234, 0x1000, 0x1800 },
			{ "sfta-a-right4-zero-fill", 0xf47c, 0xff80000008ULL, 0x1234, 0x0ff8000000ULL, 0x1234, 0x1000, 0x1800, 0 },
			{ "sfta-a-right2-sign-fill", 0xf47e, 0xff80000002ULL, 0x1234, 0xffe0000000ULL, 0x1234, 0x1000, 0x1800 },
			{ "sfta-a-right2-zero-fill", 0xf47e, 0xff80000002ULL, 0x1234, 0x3fe0000000ULL, 0x1234, 0x1000, 0x1800, 0 },
			{ "sfta-a-right6-to-b-sign-fill", 0xf57a, 0xff80000020ULL, 0x1234, 0xff80000020ULL, 0xfffe000000ULL, 0x1000, 0x1800 },
			{ "sfta-a-right6-to-b-zero-fill", 0xf57a, 0xff80000020ULL, 0x1234, 0xff80000020ULL, 0x03fe000000ULL, 0x1000, 0x1800, 0 },
			{ "sfta-b-right9-to-a-sign-fill", 0xf677, 0x1234, 0xff80000100ULL, 0xffffc00000ULL, 0xff80000100ULL, 0x1000, 0x1800 },
			{ "sfta-b-right9-to-a-zero-fill", 0xf677, 0x1234, 0xff80000100ULL, 0x007fc00000ULL, 0xff80000100ULL, 0x1000, 0x1800, 0 },
			{ "sfta-b-left2", 0xf762, 0x1234, 0x1001, 0x1234, 0x4004, 0x1800, 0x1000 },
			{ "sfta-b-left7", 0xf767, 0x1234, 0x1001, 0x1234, 0x80080, 0x1800, 0x1000 },
			{ "sfta-b-right4-sign-fill", 0xf77c, 0x1234, 0xff80000008ULL, 0x1234, 0xfff8000000ULL, 0x1000, 0x1800 },
			{ "sfta-b-right4-zero-fill", 0xf77c, 0x1234, 0xff80000008ULL, 0x1234, 0x0ff8000000ULL, 0x1000, 0x1800, 0 },
			{ "sfta-b-right2-sign-fill", 0xf77e, 0x1234, 0xff80000002ULL, 0x1234, 0xffe0000000ULL, 0x1000, 0x1800 },
			{ "sfta-b-right2-zero-fill", 0xf77e, 0x1234, 0xff80000002ULL, 0x1234, 0x3fe0000000ULL, 0x1000, 0x1800, 0 },
			{ "sftl-b-left6-carry", 0xf3e6, 0x1234, 0xff84000001ULL, 0x1234, 0x40, 0x1000, 0x1800 },
			{ "load-b-zero-shift-to-a", 0xf640, 0x1234, 0xff80000000ULL, 0xff80000000ULL, 0xff80000000ULL, 0x1800, 0x1800 },
			{ "max-a-guard-keeps-tc-overflow", 0xf486, 0x8000000000ULL, 0x7fffffffffULL, 0x7fffffffffULL, 0x7fffffffffULL, 0x1e00, 0x1e00 },
			{ "max-b-exports-comparison-carry", 0xf586, 1, 0, 1, 1, 0x1e00, 0x1600 },
			{ "min-a-equal-sets-carry", 0xf487, 0, 0, 0, 0, 0x1600, 0x1e00 },
			{ "min-b-signed-guard-keeps-tc", 0xf587, 0x8000000000ULL, 0x7fffffffffULL, 0x8000000000ULL, 0x8000000000ULL, 0x1e00, 0x1600 },
			{ "roltc-a-uses-tc-clears-guards", 0xf492, 0x81c0005555ULL, 0x12345, 0x8000aaab, 0x12345, 0x15a5, 0x1da5, 0x0b00 },
			{ "roltc-b-ignores-old-carry", 0xf592, 0x12345, 0xff12345678ULL, 0x12345, 0x2468acf0, 0x0da5, 0x05a5, 0x0b00 }
		};
		std::vector<instruction_case> result;
		for (auto const &row : references)
		{
			auto test = port_case(row.id, {row.opcode},
				{{r::STATE_A, row.a_before}, {r::STATE_B, row.b_before}, {r::STATE_ST0, row.st0_before},
				 {r::STATE_ST1, row.st1}, {r::STATE_AR6, 0x0a03}},
				{{r::STATE_A, row.a_after}, {r::STATE_B, row.b_after}, {r::STATE_ST0, row.st0_after}, {r::STATE_ST1, row.st1}});
			test.reset_before = result.empty(); result.push_back(std::move(test));
		}
		result.front().reset_initial = {{r::STATE_BK, 4}};
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SHIFT_EXAMPLES_H
