// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include <algorithm>
namespace c54x_test {
inline instruction_case bus_register_observation(bool standard,u16 waits,u16 control,u16 multiplier)
{
	auto result=observation("bus-register-mask-and-ownership",{{r::STATE_SWWSR,waits},
		{r::STATE_BSCR,standard ? 0U : control},{r::STATE_SWCR,standard ? 0U : multiplier}},
		{{0x28,waits},{0x29,control},{0x2b,multiplier}});
	return result;
}
inline std::vector<clocked_register_case> bus_register_cases(bool standard)
{
	std::vector<u16> patterns{0,0xffff,0xaaaa,0x5555,0x7fff,0x8000,0x8006,0x1000};
	for (unsigned bit=0;bit<16;++bit) for (u16 value : {u16(1U<<bit),u16(0xffff^(1U<<bit))})
		if (std::find(patterns.begin(),patterns.end(),value)==patterns.end()) patterns.push_back(value);
	std::vector<clocked_register_case> cases;
	auto append=[&](u16 raw_wait,u16 raw_control,u16 raw_multiplier) {
		auto initial=initial_case("bus-register-bit-basis-and-boundaries",{0xf4e1});
		initial.data={{0x28,raw_wait},{0x29,raw_control},{0x2b,raw_multiplier}};
		clocked_register_case c{{{0,std::move(initial)},{1,bus_register_observation(standard,
			u16(raw_wait&(standard ? 0x7fff : 0xffff)),u16(raw_control&(standard ? 0xffff : 0xf006)),
			u16(raw_multiplier&(standard ? 0xffff : 1)))}},2,4};
		c.initial_input_lines={{INPUT_LINE_HALT,ASSERT_LINE}};
		c.reset_cpu_before_case=true; c.require_idle=false; c.data_events=std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	};
	for (u16 raw : patterns) append(raw,raw,raw);
	append(0x7fff,standard ? 0x1357 : 0x8006,standard ? 0x2468 : 0);
	auto &partial=cases.back(); partial.checkpoint=1; partial.finish=8;
	clocked_register_action merge{2,{}};
	if (standard) merge.masked_data={{0x28,0,0x8000},{0x28,0xabcd,0xff}};
	else merge.masked_data={{0x29,0,0x8000},{0x29,0xffff,0xff9},{0x2b,0xffff,0xfffe},{0x28,0xabcd,0xff}};
	partial.actions={std::move(merge)};
	partial.states.push_back({3,bus_register_observation(standard,0x7fcd,standard ? 0x1357 : 6,standard ? 0x2468 : 0)});
	append(0xffff,0xffff,0xffff);
	auto &reset=cases.back(); reset.finish=8;
	clocked_register_action restart{4,{},true}; restart.reset_cpu=true; reset.actions={restart};
	reset.states.push_back({4,bus_register_observation(standard,0x7fff,standard ? 0xffff : 0xf000,standard ? 0xffff : 0)});
	if (!standard) {
		auto initial=initial_case("logical-IRQ-ack-with-physical-IACK-disabled",{0xf4e1});
		initial.program.insert(initial.program.end(),{{0x40,0xe955},{0x41,0xf4e1}});
		initial.data={{0x28,0x7fff},{0x29,0xf000},{0x2b,0}};
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST1,0},{r::STATE_IMR,1}});
		auto final=bus_register_observation(false,0x7fff,0xf000,0);
		final.expected.insert(final.expected.end(),{{r::STATE_B,0x55},{STATE_GENPC,0x42},{r::STATE_SP,0xfff}});
		clocked_register_case c{{{0,std::move(initial)},{1,bus_register_observation(false,0x7fff,0xf000,0)},
			{64,std::move(final)}},12,64};
		c.reset_cpu_before_case=true;
		c.initial_input_lines={{INPUT_LINE_HALT,CLEAR_LINE},{0,CLEAR_LINE}};
		c.actions={{16,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}}};
		c.data_events=std::vector<clocked_data_event>{{true,0xfff,0x601,0,false}};
		c.irqs={{0,0,~0U,0,0x601}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
