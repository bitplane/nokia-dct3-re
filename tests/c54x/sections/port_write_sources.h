// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned mode = 0; mode != 5; ++mode) for (unsigned pattern = 0; pattern != 4; ++pattern) for (unsigned sxm = 0; sxm != 2; ++sxm) {
		u16 const value = std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		bool const absolute = mode == 0 || mode == 1 || mode == 4;
		u16 const source = mode == 0 ? 8 : mode == 1 || mode == 2 ? 0x500 : mode == 3 ? 0x1020 : 0x25;
		u32 const base = (mode ^ pattern ^ sxm) & 1 ? 0x20000 : 0;
		auto initial = initial_case("PORTW-retained-MMR-RAM-peripheral-source",{u16(absolute ? 0x75f8 : mode == 2 ? 0x7591 : 0x7520),u16(absolute ? source : 0x124)});
		if (absolute) initial.program[2].value = 0x124;
		initial.program[absolute ? 3 : 2].value = 0xf4e1;
		for (auto &word : initial.program) word.address += base;
		u64 const a = mode == 0 ? 0x1234560000ULL | value : 0x2233445566ULL;
		u16 const st1 = 0x800 | (sxm << 8) | (mode == 3 ? 0x4000 : 0);
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_PRD,value},{r::STATE_A,a},
			{r::STATE_AR1,0x500},{r::STATE_SP,0x1000},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1}});
		initial.data = {{0x510,0xface}};
		if (mode >= 1 && mode <= 3) initial.data.push_back({source,value});
		auto final = observation("original-source-and-port-survive-poisoning",{{r::STATE_AR1,mode == 2 ? 0x501U : 0x500U},
			{r::STATE_SP,0x1000},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_A,mode == 0 ? 0x1234 : a},
			{r::STATE_PRD,mode == 4 ? 0x1234U : value}},{{0x510,0xface}});
		if (mode >= 1 && mode <= 3) final.expected_data.push_back({source,0x1234});
		unsigned const read = absolute ? mode == 4 ? 8 : 7 : 6;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("PORTW-running",{})},{20,std::move(final)}},
			std::array<unsigned,4>{4,6,7,9}[(mode+pattern+sxm)%4],20};
		c.data_events = std::vector<clocked_data_event>{{false,source,value,read}};
		c.io_events = std::vector<clocked_data_event>{{true,0x124,value,read+1}};
		c.program_events = {{false,base+0x601,u16(absolute ? source : 0x124),0,false}};
		if (absolute) c.program_events.push_back({false,base+0x602,0x124,0,false});
		c.actions = {{absolute ? 5U : 4U,{},false,{{base+(absolute ? 0x602U : 0x601U),0x125}}},{read,{}}};
		if (absolute) c.actions.front().program.push_back({base+0x601,0x510});
		if (mode == 0 || mode == 4) c.actions.back().values = {{mode == 0 ? r::STATE_A : r::STATE_PRD,0x1234}};
		else c.actions.back().data = {{source,0x1234}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
