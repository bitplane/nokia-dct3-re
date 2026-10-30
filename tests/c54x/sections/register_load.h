// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 sources[]{6,7,8,0xa,0x11,0x18,0x1e,0x7e,0x25};
	for (unsigned index = 0; index < 9; ++index)
	for (unsigned page = 0; page < (index == 6 ? 2U : 1U); ++page)
	for (unsigned flags = 0; flags < 4; ++flags) for (unsigned b = 0; b < 2; ++b)
	{
		u16 const st1 = 0x800 | ((flags & 1) << 8) | ((flags >> 1) << 14);
		u16 const value = std::array<u16,9>{0x61a,st1,0xcafe,0x12,0xffff,0x1000,u16(page),0x8001,0xabcd}[index];
		unsigned const read = index == 8 ? 6 : 5;
		auto initial = initial_case("register-load", {u16(0x4800 | (b << 8) | sources[index])});
		for (auto &word : initial.program) word.address += page * 0x10000;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,page * 0x10000 + 0x600},
			{r::STATE_ST0,0x61a},{r::STATE_ST1,st1},{r::STATE_AR1,0xffff},
			{r::STATE_A,0x123456cafeULL},{r::STATE_B,0x234567abcdULL}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_A,0x123456cafeULL},{r::STATE_B,0x234567abcdULL},
			{r::STATE_ST0,0x61a},{r::STATE_ST1,st1},{r::STATE_SP,0x1000}});
		initial.data = {{0x7e,0x8001},{0x25,0xabcd}};
		auto loaded = observation("loaded", {}); loaded.expected = initial.expected;
		loaded.expected[b ? 2 : 1].value = value;
		unsigned const point = (index + flags + b) % 2 ? read : 4;
		clocked_register_case c{{{0,std::move(initial)},{read + 1,std::move(loaded)}},point,10};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{{false,sources[index],value,read}};
		if (index == 7)
		{
			clocked_register_action poison{read,{}};
			poison.data = {{0x7e,0}}; c.actions.push_back(std::move(poison));
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
