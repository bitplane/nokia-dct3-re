// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u16 values[]{0,1,0x8000,0xffff};
	std::vector<clocked_register_case> cases;
	for (unsigned mode = 0; mode != 13; ++mode) for (unsigned pattern = 0; pattern != 4; ++pattern) {
		bool const absolute = mode <= 8, compiler = mode == 10;
		u16 const target = mode < 8 ? 8+mode : compiler ? 0x1020 : mode == 9 ? 0x4a0 : 0x500;
		u16 const value = values[pattern];
		u32 const base = (mode ^ pattern) & 1 ? 0x20000 : 0;
		auto initial = initial_case("PORTR-destination-lanes-retained-port-and-literals",{u16(absolute ? 0x74f8 : mode == 11 ? 0x7491 : mode == 12 ? 0x7489 : 0x7420),
			u16(absolute ? target : 0x125)});
		initial.program[absolute ? 3 : 2].value = 0xf4e1;
		if (absolute) initial.program[2].value = 0x125;
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x609},{r::STATE_ST1,0x900U | (compiler ? 0x4000U : 0U)},
			{r::STATE_SP,0x1000},{r::STATE_AR1,0x500},{r::STATE_A,0x1212345678ULL},{r::STATE_B,0x349abcdef0ULL},
			{r::STATE_T,0x1234},{r::STATE_TRN,0x5678}});
		if (mode >= 8) initial.data = {{u32(target-1),0xbeef},{target,0xdead},{u32(target+1),0xcafe}};
		u64 a = 0x1212345678ULL, b = 0x349abcdef0ULL;
		if (mode < 6) {
			unsigned const lane = mode % 3, shift = lane*16;
			u64 const mask = (lane == 2 ? 0xffULL : 0xffffULL) << shift;
			u64 &destination = mode < 3 ? a : b;
			destination = (destination & ~mask) | ((u64(value) << shift) & mask);
		}
		auto final = observation("retained-sample-publishes-once-without-destination-read",{{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_T,mode == 6 ? value : 0x1234U},{r::STATE_TRN,mode == 7 ? value : 0x5678U},
			{r::STATE_AR1,mode == 11 ? 0x501U : mode == 12 ? 0x4ffU : 0x500U},{r::STATE_SP,0x1000},
			{r::STATE_ST0,0x609},{r::STATE_ST1,0x900U | (compiler ? 0x4000U : 0U)}});
		if (mode >= 8) final.expected_data = {{u32(target-1),0xbeef},{target,value},{u32(target+1),0xcafe}};
		unsigned const read = absolute ? 7 : 6;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("PORTR-running",{})},{20,std::move(final)}},
			std::array<unsigned,4>{3,5,7,8}[(mode+pattern)%4],20};
		c.initial_io = {{0x125,value}};
		c.data_events = std::vector<clocked_data_event>{{true,target,value,read+1}};
		c.io_events = std::vector<clocked_data_event>{{false,0x125,value,read}};
		c.program_events = {{false,base+0x601,u16(absolute ? target : 0x125),0,false}};
		if (absolute) c.program_events.push_back({false,base+0x602,0x125,0,false});
		c.actions = {{absolute ? 5U : 4U,{},false,{{base+0x601,0xf495}}},{read,{}}};
		if (absolute) c.actions.front().program.push_back({base+0x602,0xf495});
		c.actions.back().io = {{0x125,u16(value ^ 0xffff)}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
