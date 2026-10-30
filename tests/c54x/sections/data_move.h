// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned mode = 0; mode < 5; ++mode) for (unsigned target = 0; target < 4; ++target)
	for (u16 value : {0x123U,0xbeefU})
	{
		bool const indirect = mode == 2 || mode == 3, advance = mode == 2, conflict = mode == 4;
		bool const compiler = mode == 1 || mode == 3;
		u16 const destination = indirect ? std::array<u16,4>{0x10,0x12,0x18,0x700}[target] :
			std::array<u16,4>{0x12,0x18,0x19,0x700}[target];
		u16 const source = indirect ? 0x510 : conflict ? 0x7f : compiler ? 0x531 : 0x431;
		unsigned const read = conflict ? 6 : 5, write = conflict ? 7 : destination < 0x60 ? 5 : 6;
		auto initial = initial_case("data-move", {u16(conflict ? 0x8011 : indirect ? advance ? 0x7192 : 0x7182 : 0x7131)});
		if (conflict) initial.program[1].value = 0x717f;
		initial.program[1 + unsigned(conflict)].value = destination;
		u16 const st0 = conflict ? 0xa600 : 0xa608, st1 = compiler ? 0x4800 : 0x800;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_SP,0x500},
			{r::STATE_AR0,0x5678},{r::STATE_AR2,indirect ? 0x510 : 0x1234},{r::STATE_BK,0x40},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
		initial.data = {{source,value},{0x700,0xaaaa}};
		u16 const before = indirect ? std::array<u16,4>{0x5678,0x510,0x500,0xaaaa}[target] :
			std::array<u16,4>{0x1234,0x500,0x40,0xaaaa}[target];
		initial.expected_data = {{destination,before}};
		initial.expected.insert(initial.expected.end(), {{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
		auto final = observation("destination-published", {{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}}, {{destination,value}});
		if (indirect && destination != 0x12) final.expected.push_back({r::STATE_AR2,advance ? 0x511U : 0x510U});
		clocked_register_case c{{{0,std::move(initial)},{write,std::move(final)}},
			(conflict ? 5 : 4) + (mode + target + value) % 3,10};
		if (indirect && write > 5) c.states.insert(c.states.begin() + 1,
			{5,observation("source-pointer-published", {{r::STATE_AR2,advance ? 0x511U : 0x510U}})});
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions = {{read,{}}}; c.actions.front().data = {{source,0}};
		c.data_events = std::vector<clocked_data_event>{{false,source,value,read}};
		if (conflict) c.data_events->push_back({true,0x11,0x789a,6});
		c.data_events->push_back({true,destination,value,write});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
