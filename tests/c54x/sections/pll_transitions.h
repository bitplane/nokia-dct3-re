// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_PLL_TRANSITIONS
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	// Six reference clocks + 3.5 PLL clocks, plus 255*16 clocks to lock.
	// At x8 the switch is 4086 + 7/16 reference clocks; at x10, 4086.35.
	// Sixteen observations/reference clock distinguish either side of both.
	auto initial=initial_case("PLL-lock-protection-cancellation-and-retained-lock",{0xf4e1});
	initial.data={{0x58,0x6004}};
	clocked_register_case c{{{0,std::move(initial)}},32000,131216};
	auto status=[&](unsigned tick,u16 value) {
		c.states.push_back({tick,observation("CLKMD-selected-versus-requested-source",{},{{0x58,value}})});
	};
	auto boundary=[&](unsigned tick) { c.states.push_back({tick,observation("source-switch-edge",{})}); };
	status(17,0x77fe); boundary(65399); status(65400,0x77ff);
	status(65417,0x77fd); status(65418,0x77ff);
	status(65465,0x77fd); boundary(65567); status(65568,0x77fc);
	status(65585,0x77fe); boundary(65687); status(65688,0x77ff);
	status(65705,0x77fd); boundary(65807); status(65808,0x77fc);
	status(65825,0x97fa); boundary(131206); status(131207,0x97fb);
	c.actions={{16,{},false,{}, {},{{0x58,0x77ff}}},
		{32016,{},false,{}, {},{{0x58,0x77ff}}},
		{65416,{},false,{}, {},{{0x58,0x6004}}},
		{65417,{},false,{}, {},{{0x58,0x97fb}}},
		{65464,{},false,{}, {},{{0x58,0x6004}}},
		{65584,{},false,{}, {},{{0x58,0x77ff}}},
		{65704,{},false,{}, {},{{0x58,0x6004}}},
		{65824,{},false,{}, {},{{0x58,0x97fb}}}};
	c.initial_input_lines={{INPUT_LINE_HALT,ASSERT_LINE}};
	c.reset_cpu_before_case=true; c.require_idle=false; c.observation_subticks=16;
	c.data_events=std::vector<clocked_data_event>{};
	std::vector<clocked_register_case> cases;
	cases.push_back(std::move(c));
	return cases;
}
}
