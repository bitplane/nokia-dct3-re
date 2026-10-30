// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_CLOCK_RATES
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	auto initial=initial_case("CLKMD-rate-publication-and-peripheral-retiming",{0xf4e1});
	initial.data={{0x58,0x6004}};
	initial.initial.insert(initial.initial.end(),{{r::STATE_TIM,9999},{r::STATE_PRD,9999},{r::STATE_TCR,0}});
	for (unsigned word=0;word<4;++word) {
		initial.data.push_back({0x520+word,u16(0x1111*(word+1))});
		initial.data.push_back({0x530+word,0});
	}
	clocked_register_case c{{{0,std::move(initial)}},32000,67616};
	c.actions={{16,{},false,{}, {},{{0x58,0x97fb}}}};
	c.initial_input_lines={{INPUT_LINE_HALT,ASSERT_LINE},{6,CLEAR_LINE}};
	c.require_idle=false; c.reset_cpu_before_case=true;
	c.observation_subticks=16; c.observation_clock_hz=13'000'000;
	c.data_events=std::vector<clocked_data_event>{};
	for (unsigned block=0;block<2;++block) for (unsigned word=0;word<4;++word) {
		c.data_events->push_back({false,0x520+word,u16(0x1111*(word+1)),0,false});
		c.data_events->push_back({true,0x530+word,u16(0x1111*(word+1)),0,false});
	}
	std::vector<clocked_register_case> cases;
	cases.push_back(std::move(c));
	return cases;
}
}
