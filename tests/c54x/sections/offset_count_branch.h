// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 linear_start[]{0,1,0xffff,0,1,0x1234,7,0x8000};
	constexpr u16 circular_start[]{0,1,0x7fff,0,1,0x234,7,0};
	constexpr u16 offsets[]{0,0xffff,1,1,0,0xf495,17,0x8000};
	// Independent literal results: 16-bit addition versus a legal 0x8000-word ring.
	constexpr u16 linear[]{0,0,0,1,1,0x6c9,24,0};
	constexpr u16 circular[]{0,0,0,1,1,0x76c9,24,0};
	for (unsigned delay = 0; delay < 3; ++delay) for (unsigned form = 0; form < 3; ++form)
	for (unsigned reg = 0; reg < 8; ++reg) for (unsigned pattern = 0; pattern < 8; ++pattern)
	for (unsigned compatibility = 0; compatibility < 2; ++compatibility)
	{
		unsigned const selected = compatibility && !reg ? 3 : reg;
		u16 const start = form == 2 ? circular_start[pattern] : linear_start[pattern];
		u16 const resolved = form == 2 ? circular[pattern] : linear[pattern];
		u32 const base = (delay + form + reg + pattern + compatibility) % 2 ? 0x20000 : 0;
		auto initial = initial_case("offset-count-branch", {u16((delay ? 0x6e00 : 0x6c00) | (0xe0 + form * 8 + reg)),
			offsets[pattern],0x700,u16(delay == 2 ? 0xf020 : 0xe811),u16(delay == 2 ? 0x33 : 0xe922),0xf495});
		for (auto &word : initial.program) word.address += base;
		initial.program.push_back({base + 0x700,0xe844});
		for (unsigned at = 0x701; at < 0x740; ++at) initial.program.push_back({base + at,0xf495});
		initial.program.push_back({base + 0x740,0xe866});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},
			{r::STATE_ST0,compatibility ? 0x6608 : 0x608},{r::STATE_ST1,compatibility ? 0x820 : 0x800},
			{r::STATE_BK,form == 2 ? 0x8000 : 16}});
		for (unsigned ar = 0; ar < 8; ++ar)
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),ar == selected ? start : 0x410 + ar * 16});
		auto final = observation("continued", {{r::STATE_A,resolved ? 0x44U : delay == 2 ? 0x33U : 0x11U},
			{r::STATE_B,delay ? delay == 2 ? 0U : 0x22U : resolved ? 0U : 0x22U},
			{r::STATE_ST0,compatibility ? 0x608 | (selected << 13) : 0x608},
			{r::STATE_ST1,compatibility ? 0x820U : 0x800U},
			{int(r::STATE_AR0) + int(selected),resolved ? 0U : 1U},{r::STATE_BK,32}});
		clocked_register_case c{{{0,std::move(initial)},
			{6,observation("address-published", {{int(r::STATE_AR0) + int(selected),form ? resolved : start}})},
			{7,observation("poisoned-address", {{int(r::STATE_AR0) + int(selected),resolved ? 0U : 1U}})}},
			std::array<unsigned,3>{4,5,6}[(delay + form + reg + pattern + compatibility) % 3],18};
		if (delay) {
			c.states.push_back({10,observation("delay-effects", {{r::STATE_A,delay == 2 ? 0x33U : 0x11U},
				{r::STATE_B,delay == 2 ? 0U : 0x22U}})});
			c.states.push_back({11,observation("target-draining", {})});
		}
		c.states.push_back({18,std::move(final)});
		c.require_idle = false;
		c.actions = {{5,{},false,{{base + 0x601,0xbeef},{base + 0x602,0x740}}},
			{6,{{int(r::STATE_AR0) + int(selected),resolved ? 0 : 1},{r::STATE_BK,32}}}};
		c.data_events = std::vector<clocked_data_event>{};
		c.program_events = {{false,base + 0x601,offsets[pattern],0,false},{false,base + 0x602,0x700,0,false}};
		if (resolved) c.program_events.push_back({false,base + 0x700,0xe844,7});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
