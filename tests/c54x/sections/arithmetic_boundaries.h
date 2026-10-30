// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr s64 before[]{0x7fff7fff,0x7fff8000,-1,-32769};
	for (unsigned index = 0; index < 80; ++index) {
		bool const poly = index >= 16;
		unsigned const v = poly ? index - 16 : index;
		u64 a, b;
		u16 flags;
		if (poly) {
			// POLY rounds old B plus the old-A-high/T product to a word boundary.
			s64 const rounded = (before[v % 4] + (v & 4 ? 2 : 1) + 32768) & ~s64(65535);
			bool const overflow = rounded > 2147483647LL || rounded < -2147483648LL;
			a = u64(overflow && (v & 8) ? 2147483647LL : rounded) & 0xffffffffffULL;
			b = v & 16 ? 0xffffff0000ULL : 0xffff0000ULL;
			flags = 0x1a00 | (overflow || (v & 32) ? 0x400 : 0);
		} else {
			// Signed-17 old A is -65536; FRCT doubles its absolute distance.
			s64 const sum = (v & 4 ? -1LL : 2147483647LL) + (v & 1 ? 131072 : 65536);
			bool const overflow = sum > 2147483647LL;
			a = 0; b = overflow && (v & 2) ? 2147483647ULL : u64(sum);
			flags = 0x1c00 | (overflow || (v & 8) ? 0x200 : 0);
		}
		auto initial = initial_case(poly ? "poly-boundary" : "abdst-boundary", {u16(poly ? 0x3682 : 0xe301),0xf4e1});
		initial.data = {{0x600,u16(poly ? 0xffff : 0)},{0x601,0}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_AR2,0x600},{r::STATE_AR3,0x601},
			{r::STATE_A,poly ? 0x10000ULL : 0xff00000000ULL},
			{r::STATE_B,poly ? u64(before[v % 4]) & 0xffffffffffULL : v & 4 ? 0xffffffffffULL : 0x7fffffffULL},
			{r::STATE_T,poly ? 1U : 0x1234U},
			{r::STATE_ST0,u64(poly ? 0x1a00 | (v & 32 ? 0x400 : 0) : 0x1400 | (v & 8 ? 0x200 : 0))},
			{r::STATE_ST1,u64(poly ? (v & 4 ? 0x40 : 0) | (v & 8 ? 0x200 : 0) | (v & 16 ? 0x100 : 0) :
				0x100 | (v & 1 ? 0x40 : 0) | (v & 2 ? 0x200 : 0))}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("arithmetic-boundary-result", {{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_ST0,flags},{r::STATE_T,poly ? 1U : 0x1234U},{r::STATE_AR2,0x600},{r::STATE_AR3,0x601},{STATE_GENPC,0x602}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x600,u16(poly ? 0xffff : 0),0,false}};
		if (!poly) c.data_events->push_back({false,0x601,0,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
