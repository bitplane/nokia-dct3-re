// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned phase = 0; phase != 32; ++phase) for (unsigned nmi = 0; nmi != 2; ++nmi) {
		u32 const base = (phase+nmi)%2 ? 0x20000 : 0;
		auto initial = initial_case("physical-interrupt-releases-delayed-branch-poll",{0x10f8,0x1002,0x0881,0xfa45,0x602,0x10f8,0x1002,0xe955,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		u32 const vector = base+(nmi ? 4 : 0x40);
		initial.program.insert(initial.program.end(),{{vector,0xf273},{vector+1,0x700},{vector+2,0xf495},{vector+3,0xf495},
			{base+0x700,0x80f8},{base+0x701,0x1004},{base+0x702,0x76f8},{base+0x703,0x1002},{base+0x704,2},{base+0x705,0xf4eb}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0},{r::STATE_B,0},{r::STATE_ST0,0},
			{r::STATE_ST1,0x100},{r::STATE_SP,0x1000},{r::STATE_AR1,0x1003},{r::STATE_IMR,1}});
		initial.data = {{0x1002,1},{0x1003,1},{0x1004,0xdead},{0xfff,0xaaaa}};
		// Never snapshot in the callback that queues the input assertion.
		unsigned const arrival = 16+phase, checkpoint = arrival-1+std::array<unsigned,3>{0,2,6}[(phase+nmi)%3];
		auto final = observation("handler-only-release-and-balanced-return",{{STATE_GENPC,base+0x609},{r::STATE_A,2},{r::STATE_B,0x55},
			{r::STATE_SP,0x1000},{r::STATE_AR1,0x1003}},{{0x1002,2},{0x1003,1},{0x1004,1},{0xfff,0x602}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("polling-and-interrupt-running",{})},{160,std::move(final)}},checkpoint,160};
		int const line = nmi ? INPUT_LINE_NMI : 0;
		c.actions = {{arrival,{},false,{},{{line,ASSERT_LINE}}}};
		c.observe_data_reads = false;
		c.data_events = std::vector<clocked_data_event>{{true,0xfff,0x602,0,false},{true,0x1004,1,0,false},{true,0x1002,2,0,false}};
		c.irqs = {{line,arrival,159,0,0x602}};
		c.program_events = {{false,vector,0xf273,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
