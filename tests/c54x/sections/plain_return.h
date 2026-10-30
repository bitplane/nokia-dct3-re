// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned delayed = 0; delayed < 2; ++delayed)
	for (unsigned prefix = delayed ? 1 : 0; prefix < 3; ++prefix)
	for (unsigned enable = 0; enable < (delayed ? 1U : 2U); ++enable)
	for (unsigned page = 0; page < 2; ++page)
	{
		bool const adjacent = prefix == 1;
		u16 const target = adjacent ? 0x720 : 0x700;
		u32 const base = page ? 0x10000 : 0;
		auto initial = initial_case("plain-return", {u16(prefix ? 0x4a11 : enable ? 0xf4eb : 0xfc00)});
		for (auto &word : initial.program) { word.address += base; if (word.address != base + 0x600) word.value = delayed ? 0xf495 : 0xe933; }
		if (prefix == 2) initial.program[1].value = 0xf495;
		if (prefix) initial.program[prefix].value = delayed ? 0xfe00 : enable ? 0xf4eb : 0xfc00;
		if (delayed) initial.program[3 + prefix].value = 0xffff;
		for (unsigned i = 0; i < 24; ++i) {
			initial.program.push_back({base + 0x700 + i,u16(i ? 0xf495 : 0xe955)});
			initial.program.push_back({base + 0x720 + i,u16(i ? 0xf495 : 0xe999)});
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa61b},{r::STATE_AR1,0x700}});
		initial.data = {{0xfff,0x720},{0x1000,0x700}};
		unsigned const finish = delayed ? 15 : 14;
		clocked_register_case c{{{0,std::move(initial)}},(delayed ? 4U : 3U) + prefix + (prefix + enable + page) % 3,finish};
		for (unsigned tick = 1; tick <= finish; ++tick)
			c.states.push_back({tick,observation("return-and-older-push", {
				{r::STATE_SP,!prefix && tick >= 4 ? 0x1001U : prefix == 2 && tick == 5 ? 0xfffU : 0x1000U},
				{r::STATE_ST0,0xa61b},{r::STATE_ST1,enable && tick >= 6 + prefix ? 0U : 0x800U},
				{r::STATE_B,tick >= 11 + prefix ? adjacent ? 0x99U : 0x55U : 0U}})});
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,base + target,u16(adjacent ? 0xe999 : 0xe955),7 + prefix}};
		c.data_events = std::vector<clocked_data_event>{};
		if (prefix) c.data_events->push_back({false,0x11,0x700,5});
		if (prefix == 2) c.data_events->push_back({true,0xfff,0x700,6});
		c.data_events->push_back({false,prefix ? 0xfffU : 0x1000U,target,5 + prefix});
		if (adjacent) c.data_events->push_back({true,0xfff,0x700,6});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
