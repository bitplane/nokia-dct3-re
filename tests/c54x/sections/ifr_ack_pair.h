// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned mode = 0; mode != 4; ++mode) for (unsigned b = 0; b != 2; ++b) for (unsigned pattern = 0; pattern != 4; ++pattern) {
		u16 const initial_flags = std::array<u16,4>{0,8,0x438,0xffff}[pattern], sampled = initial_flags | (mode == 2 ? 0x1000 : 0);
		auto initial = initial_case("LDM-STLM-IFR-sampled-acknowledgement",{u16(b ? 0x4901 : 0x4801),u16(b ? 0x8901 : 0x8801)});
		initial.program[16].value = 0xf4e1;
		initial.program.insert(initial.program.end(),{{4,0xf4eb},{0x70,0xf4eb}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_IFR,sampled},{r::STATE_IMR,0},{r::STATE_ST0,0},{r::STATE_ST1,mode >= 2 ? 0U : 0x800U},
			{r::STATE_SP,0x1000},{r::STATE_A,0x12345678},{r::STATE_B,0x23456789}});
		initial.data = {{0xfff,0xaaaa}};
		u16 const remaining = mode == 1 ? 0x1000 & ~initial_flags : 0, continuation = 0x604;
		auto final = observation("only-sampled-flags-cleared-and-selected-entry-survives",{{r::STATE_IFR,remaining},{r::STATE_SP,0x1000},
			{r::STATE_A,b ? 0x12345678U : sampled},{r::STATE_B,b ? sampled : 0x23456789U},{r::STATE_ST1,mode >= 2 ? 0U : 0x800U}},
			{{0xfff,u16(mode >= 2 ? continuation : 0xaaaa)}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("IFR-pair-running",{})},{40,std::move(final)}},
			std::array<unsigned,3>{4,6,8}[(mode+b+pattern)%3],40};
		c.data_events = std::vector<clocked_data_event>{{false,1,sampled,5},{true,1,sampled,7}};
		if (mode) {
			clocked_register_action action{5,{}};
			if (mode == 2) action.values = {{r::STATE_IMR,0x1000}};
			else { int const line = mode == 3 ? INPUT_LINE_NMI : 12; action.input_lines = {{line,ASSERT_LINE},{line,CLEAR_LINE}}; }
			c.actions.push_back(std::move(action));
		}
		if (mode >= 2) {
			int const line = mode == 3 ? INPUT_LINE_NMI : 12;
			c.irqs = {{line,0,~0U,2,continuation}};
			c.data_events->insert(c.data_events->end(),{{true,0xfff,continuation,0,false},{false,0xfff,continuation,0,false}});
			c.program_events = {{false,u32(mode == 3 ? 4 : 0x70),0xf4eb,0,false}};
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
