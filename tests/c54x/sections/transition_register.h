// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 12; ++index) {
		unsigned const mode = index % 3;
		u16 const value = std::array<u16,4>{0,0xffff,0x8001,0x1234}[index / 3], address = mode == 0 ? 0x45 : 0xf67;
		auto initial = initial_case("transition-register", {0x760f,value,0x480f,
			u16(mode == 0 ? 0x8d45 : mode == 1 ? 0x8dd2 : 0x8df8),u16(mode == 2 ? address : 0xf4e1),0xf4e1});
		initial.data = {{address,0xdead}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_TRN,0xabcd},{r::STATE_T,0x9876},{r::STATE_B,0x12345678},
			{r::STATE_ST0,0},{r::STATE_ST1,0x900},{r::STATE_BK,8},{r::STATE_AR2,0xf67}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,32};
		c.pipeline_frontend = false;
		c.states.push_back({32,observation("mapped-TRN-read-write-and-store", {{r::STATE_TRN,value},{r::STATE_A,value},
			{r::STATE_B,0x12345678},{r::STATE_T,0x9876},{r::STATE_ST0,0},{r::STATE_ST1,0x900},
			{r::STATE_AR2,mode == 1 ? 0xf60U : 0xf67U},{STATE_GENPC,mode == 2 ? 0x606U : 0x605U}},{{address,value}})});
		c.data_events = std::vector<clocked_data_event>{{true,0xf,value,0,false},{false,0xf,value,0,false},{true,address,value,0,false}};
		cases.push_back(std::move(c));
	}
	for (unsigned index = 0; index < 8; ++index) {
		unsigned const mode = index % 4;
		u16 const value = index < 4 ? 0x1234 : 0x8001;
		auto initial = initial_case("page-zero-TRN-push", {std::array<u16,4>{0x4a0f,0x4a82,0x4a8a,0x4ac0}[mode],0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_TRN,value},{r::STATE_SP,0x1000},{r::STATE_ST0,0x1ff},
			{r::STATE_ST1,0x4900},{r::STATE_AR0,0xf0f},{r::STATE_AR2,0xf0f},{r::STATE_BK,16}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("MMR-address-ignores-DP-CPL", {{r::STATE_SP,0xfff},{r::STATE_TRN,value},
			{r::STATE_AR2,mode == 1 ? 0xfU : mode == 2 ? 0xeU : 0xf0fU},{r::STATE_AR0,mode == 3 ? 0xeU : 0xf0fU},
			{r::STATE_ST0,0x1ff},{r::STATE_ST1,0x4900},{STATE_GENPC,0x602}},{{0xfff,value}})});
		c.data_events = std::vector<clocked_data_event>{{false,0xf,value,0,false},{true,0xfff,value,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
