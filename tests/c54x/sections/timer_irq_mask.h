// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	auto initial=initial_case("timer-IRQ-far-return-drains-direct-IMR-mask",{
		0x7711,0x25,0x7681,1,0x7711,0x26,0x7681,0x20,
		0xf495,0xf495,0xf495,0xf495,0x68f8,0,0xfff7,0xf495,
		0x76f8,0x501,1,0xf073,0x613});
	for (auto &word : initial.program) word.address+=0x10000;
	initial.program.insert(initial.program.end(),{{0x1004c,0x4a1e},{0x1004d,0xf4e5}});
	initial.data={{0x501,0}};
	initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x10600},{r::STATE_IMR,8},{r::STATE_ST1,0}});
	auto final=observation("foreground-marker-after-timer-service-and-mask",{{r::STATE_IMR,0},{r::STATE_SP,0x1000}},{{0x501,1}});
	clocked_register_case c{{{0,std::move(initial)},{1,observation("timer-stream-running",{})},{200,std::move(final)}},18,200};
	c.require_idle=false;
	c.data_ranges={{0x501,0x501}};
	c.data_events=std::vector<clocked_data_event>{{true,0x501,1,0,false}};
	c.irqs={{3,0,199,0}};
	std::vector<clocked_register_case> cases;
	cases.push_back(std::move(c));
	return cases;
}
}
