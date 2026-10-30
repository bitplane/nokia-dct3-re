// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 16; ++index) {
		u16 const first = (index << 12) | 0x494, replacement = (~first & 0xfff8) | 4;
		auto initial = initial_case("clock-control", {0xf495});
		unsigned pc = 0;
		auto write = [&](u16 word) { initial.program[pc++].value = 0x7758; initial.program[pc++].value = word; };
		auto capture = [&](u16 address) {
			initial.program[pc++].value = 0x4858; initial.program[pc++].value = 0x80f8; initial.program[pc++].value = address;
		};
		write(0); write(first | 2); capture(0x501);
		write(replacement | 3); capture(0x502);
		write(replacement); write(replacement | 2); capture(0x503);
		initial.program[pc].value = 0xf4e1;
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 12U : 1U,96};
		c.pipeline_frontend = false;
		c.states.push_back({96,observation("div-protected-fields", {},
			{{0x501,u16(first | 3)},{0x502,u16(first | 3)},{0x503,u16(replacement | 3)}})});
		cases.push_back(std::move(c));
	}
	auto initial = initial_case("stopped-timer-debugger-reload", {0xf4e1});
	initial.initial.insert(initial.initial.end(), {{r::STATE_PRD,0x1234},{r::STATE_TIM,0x5678}});
	clocked_register_case timer{{{0,std::move(initial)},
		{1,observation("independent-stopped-registers", {{r::STATE_TCR,0x10},{r::STATE_PRD,0x1234},{r::STATE_TIM,0x5678}})},
		{2,observation("reload-self-clears-without-starting", {{r::STATE_TCR,0x10},{r::STATE_PRD,0x1234},{r::STATE_TIM,0x1234}})}},1,16};
	timer.actions = {{2,{{r::STATE_TCR,0x30}},true}};
	// Debugger TCR reload reads PRD through the mapped peripheral bus.
	cases.push_back(std::move(timer));
	return cases;
}
}
