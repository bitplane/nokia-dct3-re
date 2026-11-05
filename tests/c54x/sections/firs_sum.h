// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 xs[]{0xffff,0xffff,0x7fff,0x8000}, ys[]{1,0xffff,1,0x8000};
	for (unsigned index = 0; index < 32; ++index) {
		unsigned const pair = index % 4;
		u16 const x = xs[pair], y = ys[pair];
		bool const sxm = index & 4, ovm = index & 8;
		s64 const sum = (sxm ? s64(s16(x)) : s64(x)) + (sxm ? s64(s16(y)) : s64(y));
		s64 const shifted = sum * 65536;
		bool const overflow = shifted < -2147483648LL || shifted > 2147483647LL;
		u64 const result = u64(overflow && ovm ? std::clamp(shifted,s64(-2147483648LL),s64(2147483647LL)) : shifted) & 0xffffffffffULL;
		u16 const flags = 0x1200 | (u32(x) + y > 0xffff ? 0x800 : 0) | (overflow || index >= 16 ? 0x400 : 0);
		auto initial = initial_case("firs-sum", {0xe001,0x0900,0xf4e1});
		initial.program.push_back({0x900,2});
		initial.data = {{0x600,x},{0x601,y}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_AR2,0x600},{r::STATE_AR3,0x601},
			{r::STATE_A,0x10000},{r::STATE_B,3},{r::STATE_T,0x1234},
			{r::STATE_ST0,u64(0x1200 | (pair == 2 ? 0x800 : 0) | (index >= 16 ? 0x400 : 0))},
			{r::STATE_ST1,u64((sxm ? 0x100 : 0) | (ovm ? 0x200 : 0))}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		// B accumulates old A's high word times the program-space coefficient.
		c.states.push_back({24,observation("sum-carry-and-saturation", {{r::STATE_A,result},{r::STATE_B,5},
			{r::STATE_ST0,flags},{r::STATE_T,0x1234},{r::STATE_AR2,0x600},{r::STATE_AR3,0x601},{STATE_GENPC,0x603}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x600,x,0,false},{false,0x601,y,0,false}};
		cases.push_back(std::move(c));
	}
	for (unsigned boundary : {6U,7U}) {
		auto retained = cases.front();
		retained.states.front().observation.id = boundary == 6 ? "FIRS-held-coefficient-request" : "FIRS-captured-coefficient";
		retained.states.front().observation.program.push_back({0xa00,9});
		retained.checkpoint = boundary;
		retained.actions = {{boundary,{},false,{{0x601,0xa00}},{},{{0x600,0x5a5a},{0x601,0xa5a5}}}};
		if (boundary == 7) retained.actions.front().program.push_back({0x900,0xdead});
		retained.data_events = std::vector<clocked_data_event>{{false,0x600,0xffff,6},{false,0x601,1,6}};
		retained.program_events = {{false,0x900,2,7}};
		cases.push_back(std::move(retained));
	}
	return cases;
}
}
