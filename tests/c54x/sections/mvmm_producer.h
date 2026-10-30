// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned immediate = 0; immediate != 2; ++immediate)
	for (unsigned gap = 0; gap != 4; ++gap) {
		unsigned const read = (immediate ? 5 : 4) + gap, words = immediate ? 2 : 1;
		u16 const value = immediate || gap >= 2 ? 0x1200 : 0x1000;
		u32 const base = (gap + immediate) % 2 ? 0x20000 : 0;
		auto initial = initial_case("MVMM-observes-producer-age-and-forwards-to-load", {0xf495});
		for (auto &word : initial.program) word.address += base;
		initial.program[0].value = immediate ? 0x7711 : 0x8811;
		if (immediate) initial.program[1].value = 0x1200;
		initial.program[words + gap].value = 0xe715;
		initial.program[words + gap + 1].value = 0x1085;
		initial.program[words + gap + 2].value = 0xf4e1;
		initial.data = {{0x1000,0xaaaa},{0x1200,0xbbbb}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,8},
			{r::STATE_AR1,0x1000},{r::STATE_AR5,0x1300},{r::STATE_A,0x1200}});
		auto final = observation("exact-producer-sample-and-consumer-word", {{STATE_GENPC,base + 0x603 + words + gap},
			{r::STATE_AR1,0x1200},{r::STATE_AR5,value},{r::STATE_A,value == 0x1000 ? 0xaaaaU : 0xbbbbU}},
			{{0x1000,0xaaaa},{0x1200,0xbbbb}});
		unsigned const checkpoint = read + std::array<unsigned,3>{0,1,3}[(gap + immediate) % 3];
		clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},checkpoint,20};
		c.data_events = std::vector<clocked_data_event>{{true,0x11,0x1200,immediate ? 5U : 6U},
			{false,0x11,value,read},{true,0x15,value,read + 1},
			{false,value,u16(value == 0x1000 ? 0xaaaa : 0xbbbb),read + 3}};
		std::stable_sort(c.data_events->begin(),c.data_events->end(),[](auto const &a,auto const &b) {
			return a.cycle < b.cycle;
		});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
