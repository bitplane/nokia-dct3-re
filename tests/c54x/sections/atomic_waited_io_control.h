// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_C5416_BUS
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned context=0;context<3;++context) for (unsigned count=0;count<(context ? 1U : 4U);++count)
	for (unsigned write=0;write<2;++write) {
		unsigned const transfers=context ? 1 : count+1, body=context==1 ? 2 : 1;
		auto initial=initial_case("waited-port-control-defers-pending-IRQ",{u16(context==0 ? 0xec00|count : context==1 ? 0xfa45 : 0xf6bb)});
		initial.program.clear();
		for (unsigned i=0;i<64;++i) for (u32 base : {0U,0x600U,0x700U}) initial.program.push_back({base+i,0xf495});
		// Keep instruction construction separate from the vector/NOP padding.
		initial.program.push_back({0x600,u16(context==0 ? 0xec00|count : context==1 ? 0xfa45 : 0xf6bb)});
		if (context==1) initial.program.push_back({0x601,0x700});
		initial.program.insert(initial.program.end(),{{0x600+body,u16(write ? 0x7591 : 0x7491)},{0x601+body,0x125}});
		initial.data={{0x1400,0x1234},{0x1401,0x1234},{0x1402,0x1234},{0x1403,0x1234},{0x28,0x7000},{0x2b,0}};
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_ST0,0x1804},
			{r::STATE_ST1,context==2 ? 0x800U : 0U},{r::STATE_AR1,0x1400},{r::STATE_IMR,1}});
		auto final=observation("all-transfers-before-single-IRQ-at-owned-continuation",{{r::STATE_AR1,0x1400+transfers},
			{r::STATE_RTN,context==1 ? 0x700U : 0x603U}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("atomic-control-running",{})},{64,std::move(final)}},4,64};
		c.pipeline_frontend=false;
		c.require_idle=false;
		c.initial_io={{0x125,0x1234}};
		c.observed_data_space=AS_IO;
		c.data_events=std::vector<clocked_data_event>(transfers,{bool(write),0x125,0x1234,0,false});
		c.irqs={{0,0,0,transfers}};
		c.actions={{2,{},false,{},{{0,ASSERT_LINE}}}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
