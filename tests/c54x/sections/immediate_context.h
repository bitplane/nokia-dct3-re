// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned n = 0; n < 16; ++n)
	{
		u16 const value = std::array<u16,4>{0,1,0x8000,0xf495}[n % 4];
		unsigned const offset = n & 4 ? 0x20 : 0;
		bool const compiler = n & 8;
		u16 const target = (compiler ? 0x1000 : 0x400) + offset;
		auto initial = initial_case("immediate-context", {u16(0x7600 | offset),value});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x608},{r::STATE_ST1,compiler ? 0x4900 : 0x900}});
		initial.data = {{target,0xaaaa}};
		auto final = observation("stored", {{r::STATE_SP,compiler ? 0x1020U : 0x1000U},
			{r::STATE_ST0,compiler ? 0x608U : 0x609U},{r::STATE_ST1,compiler ? 0x900U : 0x4900U}}, {{target,value}});
		clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},4 + (n + n / 4) % 4,20};
		c.require_idle = false; c.retained_program_reads = {{0x601,value}};
		c.actions = {{4,{{r::STATE_ST1,compiler ? 0x900 : 0x4900}},false,{{0x601,0xdead}}},
			{5,{{compiler ? r::STATE_SP : r::STATE_ST0,compiler ? 0x1020 : 0x609}}}};
		c.data_events = std::vector<clocked_data_event>{{true,target,value,7}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
