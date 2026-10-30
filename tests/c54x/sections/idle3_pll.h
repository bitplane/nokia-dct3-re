// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_IDLE3_PLL
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	// Explicit prelocked reset profile; ordinary cold acquisition is separate.
	auto initial=initial_case("IDLE3-invalidates-retained-lock-before-software-reentry",{0xf5e1,0xf4e1});
	initial.data={{0x58,0x77fc}};
	initial.initial.push_back({r::STATE_IMR,1});
	auto divided=observation("DIV-selected-with-PLL-power-retained",{{STATE_GENPC,0x600}},{{0x58,0x77fc}});
	auto sleeping=observation("real-IDLE3-before-unexecuted-continuation",{{r::STATE_IDLE,1},{STATE_GENPC,0x601}},{{0x58,0x77fc}});
	auto awake=observation("enabled-INTM-wake-continues-in-DIV",{{r::STATE_IDLE,1},
		{STATE_GENPC,0x602},{r::STATE_IFR,1},{r::STATE_IMR,1},{r::STATE_ST1,0x800}},{{0x58,0x77fc}});
	auto locking=observation("IDLE3-prevents-warm-lock-shortcut",{{STATE_GENPC,0x602}},{{0x58,0x77fe}});
	auto locked=observation("software-reentry-completes-cold-lock-delay",{{STATE_GENPC,0x602}},{{0x58,0x77ff}});
	clocked_register_case c{{{0,std::move(initial)},{128,std::move(divided)},
		{161,observation("release-to-IDLE3",{})},{300,std::move(sleeping)},
		{321,observation("external-wake-delivered",{})},{480,std::move(awake)},
		{497,std::move(locking)},{65879,observation("fractional-lock-edge",{})},{65880,std::move(locked)}},640,65896};
	c.actions={{160,{},false,{},{{INPUT_LINE_HALT,CLEAR_LINE}}},
		{320,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}},
		{480,{},false,{},{{INPUT_LINE_HALT,ASSERT_LINE}}},
		{496,{},false,{}, {},{{0x58,0x77ff}}}};
	c.initial_input_lines={{INPUT_LINE_HALT,ASSERT_LINE},{0,CLEAR_LINE}};
	c.pipeline_frontend=false; c.reset_cpu_before_case=true;
	c.observation_subticks=16; c.observation_clock_hz=13'000'000;
	c.data_events=std::vector<clocked_data_event>{};
	std::vector<clocked_register_case> cases;
	cases.push_back(std::move(c));
	return cases;
}
}
