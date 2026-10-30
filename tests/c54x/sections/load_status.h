// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_XF
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form < 3; ++form) for (unsigned b = 0; b < 2; ++b)
	for (u16 shape : {0U,0x200U,0x8000U,0x8200U,0x6900U,0x6b00U,0xfdffU,0xffffU}) {
		u16 const value = shape & ~u16(0x400), changed = (value ^ 0xffff) & ~u16(0x400);
		unsigned const read = form == 1 ? 6 : 5, words = form == 1 ? 2 : 1, seed = form + b + shape;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("unsigned-load-of-live-ST1",
			{u16(0x1200 | (b << 8) | (form == 1 ? 0xf8 : form == 2 ? 0x82 : 7))});
		if (form == 1) initial.program[1].value = 7;
		initial.program[words].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0x1600},{r::STATE_ST1,value},
			{r::STATE_SP,0},{r::STATE_A,0xabcdef1234ULL},{r::STATE_B,0x123456789aULL}});
		for (unsigned i = 0; i < 8; ++i) initial.initial.push_back({int(r::STATE_AR0) + int(i),7});
		auto final = observation("unsigned-status-sample-survives-replacement", {{STATE_GENPC,base + 0x601 + words},
			{r::STATE_A,b ? 0xabcdef1234ULL : value},{r::STATE_B,b ? value : 0x123456789aULL},
			{r::STATE_ST1,changed},{r::STATE_ST0,form == 2 && (value & 0x20) ? 0x5600U : 0x1600U},{r::STATE_SP,0}});
		clocked_register_case c{{{0,std::move(initial)},{18,std::move(final)}},
			std::array<unsigned,3>{3,read - 1,read}[seed % 3],18};
		if (form == 1) c.actions.push_back({4,{},false,{{base + 0x601,0xdead}}});
		if (form == 2) {
			std::vector<register_setting> pointers;
			for (unsigned i = 0; i < 8; ++i) pointers.push_back({int(r::STATE_AR0) + int(i),0x2222});
			c.actions.push_back({read - 1,std::move(pointers)});
		}
		c.actions.push_back({read,{{r::STATE_ST1,changed}}});
		if (form == 1) c.program_events = {{false,base + 0x601,7,0,false}};
		c.data_events = std::vector<clocked_data_event>{{false,7,value,read}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
