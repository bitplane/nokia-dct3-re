// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned extended = 0; extended < 2; ++extended)
	for (unsigned count_index = 0; count_index < 5; ++count_index)
	for (unsigned interrupt = 0; interrupt < 3; ++interrupt)
	{
		unsigned const count = std::array<unsigned,5>{0,1,3,32,255}[count_index], first = extended ? 8 : 7, last = first + count;
		unsigned const seed = extended + count_index + interrupt;
		u32 const base = seed & 1 ? 0x20000 : 0, body = base + 0x601 + extended, continuation = body + 1;
		int const line = interrupt == 2 ? INPUT_LINE_NMI : 0;
		auto initial = initial_case("repeat-nop", {u16(extended ? 0xf070 : 0xec00 | count)});
		for (auto &word : initial.program) word.address += base;
		if (extended) initial.program[1].value = count;
		initial.program[2 + extended].value = 0xe855; initial.program[3 + extended].value = 0xf4e1;
		if (interrupt) {
			u32 const vector = base + (interrupt == 2 ? 4 : 0x40);
			initial.program.insert(initial.program.end(), {{vector,0x4a1e},{vector + 1,0xf495},{vector + 2,0xf4e5}});
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0x608},
			{r::STATE_ST1,interrupt == 1 ? 0U : 0x800U},{r::STATE_B,0x1234},{r::STATE_IMR,interrupt == 1 ? 1U : 0U}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,4>{3,first,first + count / 2,last}[seed % 4],last + 30};
		for (unsigned tick = 1; tick <= last + 30; ++tick) {
			unsigned const remaining = tick < first - 1 ? 0 : tick <= first ? count : tick <= last ? last - tick : 0;
			auto state = observation("repeat-counter-cadence", {{r::STATE_RC,remaining}});
			if (interrupt == 1 && tick == 3) state.expected.push_back({r::STATE_IFR,1});
			if (tick == last + 30) state.expected.insert(state.expected.end(), {{r::STATE_A,0x66},{r::STATE_B,0x1234},{r::STATE_SP,0x1000}});
			c.states.push_back({tick,std::move(state)});
		}
		if (interrupt) c.actions.push_back({2,{},false,{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}});
		c.actions.push_back({extended ? 4U : 3U,{},false,{{body,0xe811}}});
		c.actions.push_back({extended ? 5U : 4U,{},false,{{continuation,0xe866}}});
		c.program_events = {{false,body,0xf495,0,false},{false,continuation,0xe855,0,false},
			{false,continuation,0xe866,0,false}};
		if (interrupt && !count) c.program_events.push_back({false,continuation,0xe866,0,false});
		c.data_events = std::vector<clocked_data_event>{};
		if (interrupt) {
			u16 const pc = continuation & 0xffff;
			c.irqs = {{line,last + 1,~0U,0,pc}};
			*c.data_events = {{true,0xfff,pc,0,false},{false,0x1e,u16(base >> 16),0,false},
				{true,0xffe,u16(base >> 16),0,false},{false,0xffe,u16(base >> 16),0,false},{false,0xfff,pc,0,false}};
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
