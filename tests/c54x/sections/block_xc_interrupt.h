// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned endpoint = 0; endpoint != 2; ++endpoint) for (unsigned count : {0,1,3,7})
		for (unsigned wide = 0; wide != (endpoint ? 1U : 2U); ++wide) for (unsigned gap = 0; gap != (endpoint ? 1U : 3U); ++gap)
			for (unsigned nmi = 0; nmi != 2; ++nmi) {
				u32 const base = (endpoint+count+wide+gap+nmi)%2 ? 0x20000 : 0, vector = base+(nmi ? 4 : 0x40);
				u16 const end = endpoint ? 0x603 : 0x604+gap;
				auto initial = initial_case("overlapping-XC-spans-hold-pending-interrupt-through-loop",{0xf072,end,
					u16(wide ? 0xff00 : 0xfd00),0x8c93,0x8d94});
				initial.program[end+9-0x600].value = 0xf4e1;
				for (auto &word : initial.program) word.address += base;
				initial.program.push_back({vector,0xf4e1});
				initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x100},
					{r::STATE_T,0xbeef},{r::STATE_TRN,0x5aa5},{r::STATE_AR3,0x5100},{r::STATE_AR4,0x5200},
					{r::STATE_SP,0x3000},{r::STATE_BRC,count},{r::STATE_IMR,1}});
				auto final = observation("all-protected-stores-before-one-vector-and-frame",{{r::STATE_BRC,0},{r::STATE_SP,0x2fff},
					{r::STATE_AR3,0x5101+count},{r::STATE_AR4,endpoint ? 0x5201U : 0x5201+count},
					{r::STATE_T,0xbeef},{r::STATE_TRN,0x5aa5},{r::STATE_ST1,0x900}});
				for (unsigned i = 0; i <= count; ++i) final.expected_data.push_back({0x5100+i,0xbeef});
				for (unsigned i = 0; i != (endpoint ? 1U : count+1); ++i) final.expected_data.push_back({0x5200+i,0x5aa5});
				clocked_register_case c{{{0,std::move(initial)},{1,observation("short-XC-repeat-running",{})},{120,std::move(final)}},
					std::array<unsigned,4>{3,7,10,14}[(endpoint+count+wide+gap+nmi)%4],120};
				c.clear_pipeline_before_restore = endpoint;
				c.data_events = std::vector<clocked_data_event>{};
				for (unsigned i = 0; i <= count; ++i) {
					c.data_events->push_back({true,0x5100+i,0xbeef,0,false});
					if (!endpoint) c.data_events->push_back({true,0x5200+i,0x5aa5,0,false});
				}
				if (endpoint) c.data_events->push_back({true,0x5200,0x5aa5,0,false});
				unsigned const stores = c.data_events->size();
				int const line = nmi ? INPUT_LINE_NMI : 0;
				c.data_events->front().raise_input = line;
				c.data_events->push_back({true,0x2fff,0,0,false});
				c.data_events->back().value_range = std::array<u16,2>{u16(end+1),u16(end+9)};
				c.irqs = {{line,0,~0U,stores}};
				c.program_events = {{false,vector,0xf4e1,0,false}};
				cases.push_back(std::move(c));
			}
	return cases;
}
}
