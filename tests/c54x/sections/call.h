// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 parameters[]{6,0,0x8000,0xf495,0xffff,0x76e6,0x4ee6,0x7001,0x54e6};
	for (unsigned layout = 0; layout < 11; ++layout)
	for (unsigned adjacent = 0; adjacent < 2; ++adjacent)
	for (unsigned page = 0; page < 2; ++page)
	{
		bool const t_delay = layout >= 2, delayed = layout != 0;
		u16 const parameter = t_delay ? parameters[layout - 2] : 0;
		u32 const base = page ? 0x10000 : 0;
		u16 const continuation = 0x600 + adjacent + (delayed ? 4 : 2), stack = 0xfff - adjacent;
		auto initial = initial_case("call", {0xf495});
		for (auto &word : initial.program) word.address += base;
		if (adjacent) initial.program[0].value = 0x4a11;
		initial.program[adjacent].value = delayed ? 0xf274 : 0xf074;
		initial.program[1 + adjacent].value = 0x700;
		if (delayed) {
			initial.program[2 + adjacent].value = t_delay ? 0x770e : 0xe811;
			initial.program[3 + adjacent].value = t_delay ? parameter : 0xe922;
		}
		initial.program[continuation - 0x600].value = 0xe966;
		for (unsigned i = 0; i < 32; ++i) initial.program.push_back({base + 0x700 + i,
			u16(i == 0 ? t_delay ? 0x490e : 0xe955 : i == 1 ? 0xfc00 : 0xf495)});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa61b},
			{r::STATE_RTN,0x8888},{r::STATE_T,0xbeef},{r::STATE_AR1,0x7777}});
		initial.data = {{0xffe,0xaaaa},{0xfff,0xaaaa}};
		clocked_register_case c{{{0,std::move(initial)}},4 + adjacent + (layout + adjacent + page) % 3,22};
		for (unsigned tick = 1; tick <= 22; ++tick)
		{
			u64 const sp = tick < 4 + adjacent ? 0x1000 : tick >= 9 + adjacent ? 0x1000 - adjacent : 0xfff - adjacent;
			u64 const b = tick >= 16 + adjacent ? 0x66 : tick >= 10 + adjacent ? t_delay ? parameter : 0x55 :
				delayed && !t_delay && tick >= 9 + adjacent ? 0x22 : 0;
			c.states.push_back({tick,observation("call-delay-and-return", {{r::STATE_SP,sp},
				{r::STATE_RTN,tick >= 5 + adjacent ? continuation : 0x8888U},
				{r::STATE_A,delayed && !t_delay && tick >= 8 + adjacent ? 0x11U : 0U},
				{r::STATE_T,t_delay && tick >= 7 + adjacent ? unsigned(parameter) : 0xbeefU},{r::STATE_B,b}})});
		}
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,base + 0x700,u16(t_delay ? 0x490e : 0xe955),6 + adjacent}};
		c.data_events = std::vector<clocked_data_event>{};
		if (adjacent) {
			c.data_events->push_back({false,0x11,0x7777,5});
			c.data_events->push_back({true,0xfff,0x7777,6});
		}
		c.data_events->push_back({true,stack,continuation,6 + adjacent});
		if (t_delay) {
			c.data_events->push_back({true,0xe,parameter,7 + adjacent});
			c.data_events->push_back({false,0xe,parameter,9 + adjacent});
		}
		c.data_events->push_back({false,stack,continuation,10 + adjacent});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
