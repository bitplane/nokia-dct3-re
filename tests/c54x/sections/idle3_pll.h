// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_IDLE3_PLL
#include "c54x_section_common.h"
namespace c54x_test {
// Sixteenth-clock observations at 13 MHz: DIV executes one CPU clock per
// 32 samples. Leave ten CPU clocks for the wake continuation to drain.
inline constexpr unsigned idle3_pll_settled_tick=736;
inline constexpr unsigned idle3_pll_checkpoint_tick=896;
inline constexpr unsigned idle3_pll_locked_tick=66136;
inline std::vector<clocked_register_case> section_cases()
{
	// Explicit prelocked reset profile; ordinary cold acquisition is separate.
	auto initial=initial_case("IDLE3-invalidates-retained-lock-before-software-reentry",{0xf5e1,0xe800,0x8800,0xf4e1});
	initial.data={{0x58,0x77fc}};
	initial.initial.push_back({r::STATE_IMR,1});
	auto divided=observation("DIV-selected-with-PLL-power-retained",{{STATE_GENPC,0x600}},{{0x58,0x77fc}});
	auto sleeping=observation("real-IDLE3-before-unexecuted-continuation",{{r::STATE_IDLE,1},{STATE_GENPC,0x601},{r::STATE_IMR,1}},{{0x58,0x77fc}});
	auto awake=observation("enabled-INTM-wake-continues-in-DIV",{{r::STATE_IDLE,1},
		{STATE_GENPC,0x604},{r::STATE_IFR,1},{r::STATE_IMR,0},{r::STATE_ST1,0x800}},{{0x58,0x77fc}});
	auto locking=observation("IDLE3-prevents-warm-lock-shortcut",{{STATE_GENPC,0x604}},{{0x58,0x77fe}});
	auto locked=observation("software-reentry-completes-cold-lock-delay",{{STATE_GENPC,0x604}},{{0x58,0x77ff}});
	clocked_register_case c{{{0,std::move(initial)},{128,std::move(divided)},
		{161,observation("release-to-IDLE3",{})},{384,std::move(sleeping)},
		{417,observation("external-wake-delivered",{})},{idle3_pll_settled_tick,std::move(awake)},
		{753,std::move(locking)},{idle3_pll_locked_tick-1,observation("fractional-lock-edge",{})},
		{idle3_pll_locked_tick,std::move(locked)}},idle3_pll_checkpoint_tick,idle3_pll_locked_tick+16};
	c.actions={{160,{},false,{},{{INPUT_LINE_HALT,CLEAR_LINE}}},
		{416,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}},
		{idle3_pll_settled_tick,{},false,{},{{INPUT_LINE_HALT,ASSERT_LINE}}},
		{752,{},false,{}, {},{{0x58,0x77ff}}}};
	c.initial_input_lines={{INPUT_LINE_HALT,ASSERT_LINE},{0,CLEAR_LINE}};
	c.pipeline_frontend = true; c.reset_cpu_before_case=true;
	c.observation_subticks=16; c.observation_clock_hz=13'000'000;
	c.data_events=std::vector<clocked_data_event>{{true,0,0,0,false}};
	c.data_events->front().registers={{r::STATE_IFR,1},{r::STATE_IMR,1}};
	std::vector<clocked_register_case> cases;
	cases.push_back(std::move(c));
	return cases;
}
}
