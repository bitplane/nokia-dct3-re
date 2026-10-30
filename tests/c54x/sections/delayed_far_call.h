// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_HIGH_PROGRAM
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned caller_page : {0,2,0x7f}) for (unsigned target_page : {0,2,0x7f})
		for (unsigned layout = 0; layout != 3; ++layout) for (unsigned interrupt = 0; interrupt != 3; ++interrupt) {
			u32 const caller = caller_page << 16, target = target_page << 16;
			// NOP/STLM, one two-word STM, and NOP/LDM fill the two delay words.
			auto initial = initial_case("FCALLD-owned-delay-words-and-frame-roundtrip",{u16(0xfb80 | target_page),0x700,
				u16(layout == 1 ? 0x770e : 0xf495),u16(layout == 0 ? 0x891a : layout == 1 ? 0x1234 : 0x4816),0xe955,0xf4e1});
			for (auto &word : initial.program) word.address += caller;
			initial.program.insert(initial.program.end(),{{target+0x700,0xe933},{target+0x701,0xf495},{target+0x702,0xf495},
				{target+0x703,0xf4e4},{target+4,0xf4eb},{target+0x40,0xf4eb}});
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,caller+0x600},{r::STATE_A,0},{r::STATE_B,0x1234},
				{r::STATE_ST0,0xa608},{r::STATE_ST1,0},{r::STATE_T,0xbeef},{r::STATE_BRC,0x9876},
				{r::STATE_AR6,0x4567},{r::STATE_SP,0x1000},{r::STATE_IMR,1}});
			initial.data = {{0xfff,0xaaaa},{0xffe,0xbbbb},{0xffd,0xcccc}};
			// In this model the pending entry drains the callee marker/NOPs,
			// then stacks the still-unexecuted FRET address.
			u16 const irq_return = 0x703;
			auto final = observation("caller-page-and-post-delay-continuation-restored",{{STATE_GENPC,caller+0x606},{r::STATE_XPC,caller_page},
				{r::STATE_A,layout == 2 ? 0x4567U : 0U},{r::STATE_B,0x55},{r::STATE_T,layout == 1 ? 0x1234U : 0xbeefU},
				{r::STATE_BRC,layout == 0 ? 0x1234U : 0x9876U},{r::STATE_SP,0x1000},{r::STATE_ST0,0xa608},{r::STATE_ST1,0}},
				{{0xfff,0x604},{0xffe,u16(caller_page)},{0xffd,u16(interrupt ? irq_return : 0xcccc)}});
			clocked_register_case c{{{0,std::move(initial)},{1,observation("delayed-far-call-running",{})},{50,std::move(final)}},
				std::array<unsigned,3>{6,8,14}[(caller_page+target_page+layout+interrupt)%3],50};
			c.data_events = std::vector<clocked_data_event>{{true,0xfff,0x604,0,false}};
			// The two-word STM publishes between the two call-frame words.
			if (layout == 1) c.data_events->push_back({true,0xe,0x1234,0,false});
			c.data_events->push_back({true,0xffe,u16(caller_page),0,false});
			if (layout == 0) c.data_events->push_back({true,0x1a,0x1234,0,false});
			if (layout == 2) c.data_events->push_back({false,0x16,0x4567,0,false});
			c.program_events = {{false,caller+0x601,0x700,0,false}};
			if (layout == 1) c.program_events.push_back({false,caller+0x603,0x1234,0,false});
			c.program_events.push_back({false,target+0x700,0xe933,0,false});
			if (interrupt) {
				int const line = interrupt == 2 ? INPUT_LINE_NMI : 0;
				c.actions = {{5,{},false,{},{{line,ASSERT_LINE}}}};
				c.irqs = {{line,0,~0U,3}};
				c.data_events->insert(c.data_events->end(),{{true,0xffd,irq_return,0,false},{false,0xffd,irq_return,0,false}});
				c.program_events.push_back({false,target+(interrupt == 2 ? 4 : 0x40),0xf4eb,0,false});
			}
			c.data_events->insert(c.data_events->end(),{{false,0xffe,u16(caller_page),0,false},{false,0xfff,0x604,0,false}});
			c.program_events.push_back({false,caller+0x604,0xe955,0,false});
			cases.push_back(std::move(c));
		}
	return cases;
}
}
