// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct entry { char const *id; u16 opcode; u64 a_before,b_before,a_after,b_after; u16 st0_before,st0_after,st1 = 0x100; };
	constexpr entry references[]{
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
	std::vector<clocked_register_case> cases;
	for (auto const &v : references) {
		auto initial = initial_case(v.id,{v.opcode,0xf4e1});
		initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,v.a_before},{r::STATE_B,v.b_before},
			{r::STATE_ST0,v.st0_before},{r::STATE_ST1,v.st1}});
		auto final = observation(v.id, {{STATE_GENPC,0x602},{r::STATE_A,v.a_after},{r::STATE_B,v.b_after},
			{r::STATE_ST0,v.st0_after},{r::STATE_ST1,v.st1}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-shift-snapshot", {{r::STATE_A,v.a_before},{r::STATE_B,v.b_before}})},
			{2,observation("shift-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.forbid_data_access = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
