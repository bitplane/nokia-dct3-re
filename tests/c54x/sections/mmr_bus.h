// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_MMR_BUS
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	auto initial=initial_case("MMR-shared-state-masked-writes-and-reserved-RAM",{0xf4e1});
	initial.initial.insert(initial.initial.end(),{{r::STATE_AR3,0xbad},{r::STATE_IFR,0xbeef}});
	initial.data={{3,0xdead}};
	clocked_register_case c{{{0,std::move(initial)},
		{1,observation("explicit-register-and-gap-setup",{{r::STATE_AR3,0xbad},{r::STATE_IFR,0xbeef}},{{3,0xdead}})},
		{3,observation("word-and-byte-writes-share-register-state",{{r::STATE_AR3,0xabcd}},{{0x13,0xabcd}})},
		{5,observation("low-IFR-write-clears-only-selected-flags",{{r::STATE_IFR,0xbe00}})},
		{6,observation("high-IFR-write-preserves-unselected-flags",{{r::STATE_IFR,0xb000}})},
		{7,observation("reserved-gap-remains-board-RAM",{{r::STATE_AR3,0xabcd},{r::STATE_IFR,0xb000}},{{3,0x5a5a},{0x13,0xabcd}})}},1,8};
	clocked_register_action word{2,{}};
	word.masked_data={{0x13,0x1234,0xffff},{0x13,0xab00,0xff00},{0x13,0xcd,0xff}};
	clocked_register_action low{4,{}}; low.masked_data={{1,0xef,0xff}};
	clocked_register_action high{5,{}}; high.masked_data={{1,0xe00,0xff00}};
	c.actions={word,low,high,{6,{},false,{}, {},{{3,0x5a5a}}}};
	c.initial_input_lines={{INPUT_LINE_HALT,ASSERT_LINE}};
	c.require_idle=false; c.reset_cpu_before_case=true;
	c.data_events=std::vector<clocked_data_event>{};
	std::vector<clocked_register_case> cases;
	cases.push_back(std::move(c));
	return cases;
}
}
