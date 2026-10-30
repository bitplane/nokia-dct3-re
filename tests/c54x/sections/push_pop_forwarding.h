// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned source = 8; source < 16; ++source) for (unsigned target = 0; target < 4; ++target)
	for (u16 shape : {0U,1U,0x8000U,0xffffU}) for (unsigned odd = 0; odd < 2; ++odd) {
		u16 const value = source == 0xa || source == 0xd ? shape & 0xff : shape;
		u16 const destination = std::array<u16,4>{0xe,0x13,0x19,0x18}[target], sp = 0x3000 + odd;
		unsigned const seed = source + target + shape + odd;
		u32 const base = seed & 1 ? 0x20000 : 0;
		u64 a = 0x123456789aULL, b = 0xfedcba9876ULL;
		if (source < 14) {
			unsigned const lane = (source - 8) % 3, shift = lane * 16;
			u64 const mask = u64(lane == 2 ? 0xff : 0xffff) << shift;
			u64 &accumulator = source < 11 ? a : b;
			accumulator = (accumulator & ~mask) | (u64(value) << shift);
		}
		auto initial = initial_case("adjacent-PSHM-POPM", {u16(0x4a00 | source),u16(0x8a00 | destination),0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{u32(sp - 1),0xbeef}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_SP,sp},
			{r::STATE_T,source == 14 ? value : 0U},{r::STATE_TRN,source == 15 ? value : 0U}});
		auto final = observation("forwarded-stack-word-and-balanced-pair", {{STATE_GENPC,base + 0x603},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_SP,destination == 0x18 ? value : sp}},{{destination,value}});
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},4 + seed % 4,16};
		// Replace the source after PSHM Read, and the stack backing word after POPM Read.
		c.actions = {{5,{},false,{},{},{{source,0xdead}}},{6,{},false,{},{},{{u32(sp - 1),0xbeef}}}};
		c.data_events = std::vector<clocked_data_event>{{false,source,value,5},{true,u32(sp - 1),value,6},
			{false,u32(sp - 1),value,6},{true,destination,value,destination == 0xe ? 7U : 6U}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
