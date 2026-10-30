// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form < 7; ++form) for (unsigned kind = 0; kind < 4; ++kind)
	for (unsigned pattern = 0; pattern < (kind ? 4U : 1U); ++pattern)
	for (unsigned gap = 0; gap < (form == 6 ? 2U : 1U); ++gap) {
		unsigned const read = form == 5 ? 7 : 6, words = form == 5 ? 3 : 2;
		unsigned const seed = form + kind + pattern + gap;
		u32 const base = seed & 1 ? 0x20000 : 0;
		u16 const destination = std::array<u16,7>{0x402,0x1002,0x1000,0x1000,0x1001,0x1500,0x11}[form];
		u16 const next = form == 2 || form == 4 ? 0x1001 : form == 3 ? 0xfff : 0x1000;
		u16 const source = std::array<u16,4>{0x11,0x1400,destination,0x12}[kind];
		u16 const value = kind ? std::array<u16,4>{0,1,0x8000,0xf495}[pattern] : next;
		auto initial = initial_case("retained-data-move", {u16(0x7000 | std::array<u16,7>{2,2,0x91,0x89,0x99,0xf8,0x11}[form]),
			u16(form == 5 ? destination : source)});
		if (form == 5) initial.program[2].value = source;
		if (form == 6) initial.program[words + gap].value = 0xf4a9;
		unsigned const idle = words + (form == 6 ? gap + 1 : 0);
		initial.program[idle].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		if (destination >= 0x60) initial.data.push_back({destination,0xaaaa});
		if (source >= 0x60) initial.data.push_back({source,value});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,form == 6 ? 0xa600U : 0xa608U},
			{r::STATE_ST1,0x900U | (form == 1 ? 0x4000U : 0U)},{r::STATE_SP,0x1000},{r::STATE_AR0,value},
			{r::STATE_AR1,source == 0x11 && kind ? value : 0x1000U},{r::STATE_AR2,source == 0x12 ? value : 0U}});
		auto final = observation("one-read-one-retained-write", {{STATE_GENPC,base + 0x601 + idle}},{{destination,value}});
		if (form == 6) final.expected.push_back({int(r::STATE_ST0),gap || kind == 0 || kind == 2 ? 0x1000U : 0U,0x1000});
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},
			read - 2 + std::array<unsigned,3>{0,2,3}[seed % 3],24};
		c.actions = {{read - 2,{{r::STATE_ST1,0x900U | (form == 1 ? 0U : 0x4000U)}},false,{{base + 0x601,0xdead}}},
			{read - 1,{{r::STATE_SP,0x2000},{r::STATE_ST0,0xa609}}},
			{read,{{r::STATE_AR2,0xbeef}}}};
		if (form == 5) c.actions.front().program.push_back({base + 0x602,0xbeef});
		if (source >= 0x60) c.actions.back().data.push_back({source,0xdead});
		if (form != 6) c.actions.back().values.push_back({r::STATE_AR1,0x2000});
		c.program_events = {{false,base + 0x601,u16(form == 5 ? destination : source),0,false}};
		if (form == 5) c.program_events.push_back({false,base + 0x602,source,0,false});
		c.data_events = std::vector<clocked_data_event>{{false,source,value,read},
			{true,destination,value,read + unsigned(form != 6)}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
