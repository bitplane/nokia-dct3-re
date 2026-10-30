// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned delay = 0; delay < 3; ++delay)
	for (unsigned masked = 0; masked < 2; ++masked)
	for (unsigned page = 0; page < 2; ++page)
	for (unsigned prefix = 0; prefix < 2; ++prefix)
	{
		bool const delayed = delay != 0, multiword = delay == 2;
		u32 const base = page ? 0x10000 : 0, target = (base ^ 0x10000) + 0x700;
		unsigned const read = (delayed ? 4 : 5) + prefix;
		unsigned const fetch = (delayed ? 6 : 8) + prefix, finish = (delayed ? 10 : 12) + prefix;
		u16 const opcode = (masked ? 0xf4e4 : 0xf4e5) | (delayed ? 0x200 : 0);
		auto initial = initial_case("far-return", {u16(prefix ? 0x8a11 : opcode)});
		for (auto &word : initial.program) { word.address += base; if (word.address != base + 0x600) word.value = 0xe933; }
		if (prefix) initial.program[1].value = opcode;
		if (delayed) {
			initial.program[1 + prefix].value = multiword ? 0xf020 : 0xe811;
			initial.program[2 + prefix].value = multiword ? 0x1234 : 0xe922;
		}
		for (unsigned i = 0; i < 20; ++i) initial.program.push_back({target + i,u16(i ? 0xf495 : 0xe955)});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_SP,0x1000 - prefix},{r::STATE_AR1,0}});
		initial.data = {{0xfff,0x42},{0x1000,u16(0x80 | (target >> 16))},{0x1001,0x700}};
		clocked_register_case c{{{0,std::move(initial)}},read - 1 + (delay + masked + page + prefix) % 3,finish};
		for (unsigned tick = 1; tick <= finish; ++tick)
			c.states.push_back({tick,observation("return-stack-and-delay", {
				{r::STATE_SP,0x1000 - prefix + unsigned(prefix && tick >= 5) + unsigned(tick >= read) + unsigned(tick > read)},
				{r::STATE_AR1,prefix && tick >= 5 ? 0x42U : 0U},
				{r::STATE_ST1,masked || tick < (delayed ? 6 : 7) + prefix ? 0x800U : 0U},
				{r::STATE_A,multiword ? tick >= 8 + prefix ? 0x1234U : 0U : delayed && tick >= 7 + prefix ? 0x11U : 0U},
				{r::STATE_B,tick >= finish ? 0x55U : delayed && !multiword && tick >= 8 + prefix ? 0x22U : 0U}})});
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,target,0xe955,fetch}};
		c.data_events = std::vector<clocked_data_event>{};
		if (prefix) {
			c.data_events->push_back({false,0xfff,0x42,5});
			c.data_events->push_back({true,0x11,0x42,5});
		}
		c.data_events->push_back({false,0x1000,u16(0x80 | (target >> 16)),read});
		c.data_events->push_back({false,0x1001,0x700,read + 1});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
