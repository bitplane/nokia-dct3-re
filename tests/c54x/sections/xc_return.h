// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned zero = 0; zero != 2; ++zero) for (unsigned invert = 0; invert != 2; ++invert)
		for (unsigned wide = 0; wide != 2; ++wide) for (unsigned source = 0; source != (zero != invert ? 3U : 1U); ++source) {
			bool const selected = zero != invert;
			u32 const base = (zero+invert+wide+source)%2 ? 0x20000 : 0;
			auto initial = initial_case("XC-return-protection-through-resumed-word",{u16(invert ? 0xfd4c : 0xfd4d),0xfc00,0xf495,0xf4e1});
			for (auto &word : initial.program) word.address += base;
			for (unsigned i = 0; i != 9; ++i) initial.program.push_back({base+0x700+i,0xf495});
			initial.program.insert(initial.program.end(),{{base+0x700,u16(wide ? 0x7660 : 0x8c60)},
				{base+0x701,u16(wide ? 0xbeef : 0xf495)},{base+0x706,0x7707},{base+0x707,0x0740},
				{base+0x708,0xf4e1},{base+4,0xf4e1},{base+0x40,0xf4e1}});
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa020},{r::STATE_ST1,0x100},
				{r::STATE_A,0x123456789aULL},{r::STATE_B,zero ? 0U : 1U},{r::STATE_T,0xbeef},{r::STATE_SP,0x3000},{r::STATE_IMR,1}});
			initial.data = {{0x3000,0x700},{0x1060,0xdead}};
			// Model-selected continuation after the resumed word and padding NOPs.
			u16 const continuation = 0x704+wide;
			auto final = observation("one-return-sample-bounded-XC-and-owned-frame",{{r::STATE_SP,selected && !source ? 0x3001U : 0x3000U},
				{r::STATE_ST1,!selected ? 0x100U : source ? 0x900U : 0x340U},{r::STATE_ST0,0xa020},
				{r::STATE_A,0x123456789aULL},{r::STATE_B,zero ? 0U : 1U},{r::STATE_T,0xbeef}},
				{{0x1060,u16(selected ? 0xbeef : 0xdead)},{0x3000,u16(!selected ? 0x700 : source ? continuation : 0xdead)}});
			clocked_register_case c{{{0,std::move(initial)},{1,observation("XC-return-running",{})},{32,std::move(final)}},
				std::array<unsigned,4>{3,5,8,12}[(zero+invert+wide+source)%4],32};
			c.clear_pipeline_before_restore = true;
			c.data_events = std::vector<clocked_data_event>{};
			if (selected) {
				c.data_events->push_back({false,0x3000,0x700,0,false});
				c.data_events->push_back({true,0x1060,0xbeef,0,false});
				c.actions = {{6,{},false,{}, {},{{0x3000,0xdead}}}};
				if (source) {
					int const line = source == 2 ? INPUT_LINE_NMI : 0;
					c.actions.push_back({7,{},false,{},{{line,ASSERT_LINE}}});
					c.irqs = {{line,0,~0U,2}};
					c.data_events->push_back({true,0x3000,continuation,0,false});
					c.program_events = {{false,base+(source == 2 ? 4 : 0x40),0xf4e1,0,false}};
				} else c.data_events->push_back({true,7,0x0740,0,false});
			}
			cases.push_back(std::move(c));
		}
	return cases;
}
}
