// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u16 bits : std::array<u16,8>{56,1,7,8,9,64,255,512}) {
		u16 const bytes = (bits+7)/8;
		// Load a bit count, round up through mapped AL, and store a long count.
		auto initial = initial_case("rounded-byte-count-with-stack-relative-pointer",{
			0x10e1,5,0xf010,1,0x7102,0x0011,0x4808,0xf47d,0xf000,1,0x4808,0x4ee1,6,0xf4e1});
		u32 const base = cases.size()%2 ? 0x20000 : 0;
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0},{r::STATE_B,0x5678},
			{r::STATE_AR1,0x1800},{r::STATE_SP,0x1400},{r::STATE_ST0,0},{r::STATE_ST1,0x4900}});
		initial.data = {{0x1805,bits},{0x1402,0x1900},{0x1906,0x7777},{0x1907,0x8888}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-count-composition",{})},
			{40,observation("ceiling-divide-by-eight-and-long-store",{{r::STATE_A,bytes},{r::STATE_B,0x5678},
				{r::STATE_AR1,0x1900},{r::STATE_SP,0x1400}},{{0x1906,0},{0x1907,bytes}})}},14,40};
		c.data_events = std::vector<clocked_data_event>{{false,0x1805,bits,0,false},{false,0x1402,0x1900,0,false},
			{true,0x11,0x1900,0,false},{false,8,u16(bits-1),0,false},{false,8,bytes,0,false},
			{true,0x1906,0,0,false},{true,0x1907,bytes,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
