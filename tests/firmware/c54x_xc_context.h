// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned layout = 0; layout < 4; ++layout)
	{
		bool const composed = layout != 0, branch = layout >= 2, nonzero = layout == 3;
		u32 const origin = branch ? 0x20600 : 0x600;
		constexpr u16 sequence[]{0xfd00,0x4a07,0xfd00,0xf7bb,0x7192,0x0010,0x1092,0xf844,0x700};
		auto initial = initial_case("xc-context-sequence", {0xf495});
		for (auto &word : initial.program) word.address += origin - 0x600;
		for (unsigned i = 0; i < (branch ? 9U : composed ? 7U : 4U); ++i) initial.program[i].value = sequence[i];
		if (branch) for (unsigned i = 0; i < 24; ++i) initial.program.push_back({0x20700 + i,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,origin},{r::STATE_ST0,0xa608},{r::STATE_ST1,0},
			{r::STATE_SP,0x500},{r::STATE_AR0,0x5678},{r::STATE_AR2,0x510},{r::STATE_A,0x1234}});
		initial.data = {{0x510,0xbeef},{0x511,u16(branch ? nonzero : 0x1357)}};
		unsigned const finish = branch ? 21 : 16;
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,4>{3,9,11,13}[layout],finish};
		for (unsigned tick = 1; tick <= finish; ++tick) {
			auto state = observation("context-prefix-effects", {{r::STATE_SP,tick >= 6 ? 0x4ffU : 0x500U},
				{r::STATE_ST1,tick >= 9 ? 0x800U : 0U}});
			if (composed) state.expected.insert(state.expected.end(), {
				{r::STATE_AR0,tick >= 9 ? 0xbeefU : 0x5678U},{r::STATE_AR2,tick >= 10 ? 0x512U : tick >= 9 ? 0x511U : 0x510U},
				{r::STATE_A,tick >= 12 ? branch ? tick >= 14 ? unsigned(!nonzero) : unsigned(nonzero) : 0x1357U : 0x1234U}});
			c.states.push_back({tick,std::move(state)});
		}
		if (composed) c.actions.push_back({9,{},false,{}, {},{{0x510,0}}});
		if (branch) {
			c.actions.push_back({11,{},false,{}, {},{{0x511,0xdead}}});
			c.actions.push_back({13,{{r::STATE_A,unsigned(!nonzero)}}});
			c.program_events = {{false,nonzero ? 0x20700U : origin + 11,0xf495,14}};
		}
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_ranges = {{{0x4ff,0x4ff}},{{0x510,0x511}}};
		c.data_events = std::vector<clocked_data_event>{{true,0x4ff,0,7}};
		if (composed) {
			c.data_events->push_back({false,0x510,0xbeef,9});
			c.data_events->push_back({false,0x511,u16(branch ? nonzero : 0x1357),11});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
