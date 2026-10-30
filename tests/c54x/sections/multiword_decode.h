// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 opcodes[]{0xf020,0xf120,0xf02f,0xf162}, literals[]{0xe833,0xf495,0x7fff,0x8001};
	constexpr u64 results[]{0xe833,0xfffffff495ULL,0x3fff8000,0x80010000};
	for (unsigned index = 0; index < 5; ++index) {
		bool const stream = index == 4, destination_b = index == 1 || index == 3;
		auto initial = stream ? initial_case("multiword-stream", {0xf020,0xe833,0xf162,0xf495,0xe877}) :
			initial_case("multiword-pending-IRQ", {opcodes[index],literals[index],0xe822});
		initial.initial.push_back({r::STATE_ST1,index == 1 || index == 2 ? 0x100U : 0U});
		if (!stream) {
			initial.initial.push_back({r::STATE_IMR,1});
			initial.data = {{0xfff,0xbeef}};
			for (unsigned at = 0x40; at < 0x50; ++at) initial.program.push_back({at,u16(at == 0x40 ? 0xe955 : 0xf495)});
		}
		unsigned const finish = stream ? 10 : 11;
		clocked_register_case c{{{0,std::move(initial)}},3,finish};
		for (unsigned tick = 1; tick <= finish; ++tick) {
			u64 const a = stream ? tick >= 10 ? 0x77 : tick >= 7 ? 0xe833 : 0 :
				tick < 7 || destination_b ? 0 : results[index];
			u64 const b = stream ? tick >= 9 ? 0xf4950000ULL : 0 :
				tick >= 11 ? 0x55 : tick >= 7 && destination_b ? results[index] : 0;
			auto state = observation("complete-decode-before-interrupt-entry", {{r::STATE_A,a},{r::STATE_B,b}});
			if (!stream) {
				state.expected.push_back({r::STATE_SP,tick < 6 ? 0x1000U : 0xfffU});
				state.expected_data = {{0xfff,u16(tick < 8 ? 0xbeef : 0x602)}};
			}
			c.states.push_back({tick,std::move(state)});
		}
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,0x601,stream ? u16(0xe833) : literals[index],3}};
		c.data_events = std::vector<clocked_data_event>{};
		if (stream) c.program_events.push_back({false,0x603,0xf495,5});
		else {
			c.actions = {{2,{},false,{},{{0,ASSERT_LINE}}}};
			c.irqs = {{0,4,7,0,0x602}};
			c.program_events.push_back({false,0x40,0xe955,7});
			c.data_events->push_back({true,0xfff,0x602,8});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
