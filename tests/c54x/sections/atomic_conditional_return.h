// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0,1,0xffffffffffULL,0x7fffffff,0xff80000000ULL,0x0100000000ULL,0x8000000000ULL,0x7fffffffffULL};
	for (unsigned condition = 0; condition < 12; ++condition)
	for (unsigned pattern = 0; pattern < 8; ++pattern)
	{
		u64 const value = values[pattern];
		s64 const signed_value = value & 0x8000000000ULL ? s64(value) - (s64(1) << 40) : s64(value);
		bool const taken = std::array<bool,6>{signed_value >= 0,signed_value < 0,value != 0,value == 0,signed_value > 0,signed_value <= 0}[condition % 6];
		u64 const a = condition < 6 ? value : 0x23456789, b = condition < 6 ? 0x23456789 : value;
		u32 const base = (condition + pattern) & 1 ? 0x10000 : 0;
		auto initial = initial_case("atomic-conditional-return", {u16(0xfc40 | (condition < 6 ? condition + 2 : condition + 4)),0xe811,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(), {{base + 0x700,0xe822},{base + 0x701,0xf4e1}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,0x1e05}});
		initial.data = {{0x1000,0x700}};
		clocked_register_case c{{{0,std::move(initial)}},(condition + pattern) & 1 ? 5U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("conditional-stack-ownership", {{r::STATE_A,taken ? 0x22U : 0x11U},{r::STATE_B,b},
			{r::STATE_SP,0x1000 + unsigned(taken)},{STATE_GENPC,base + (taken ? 0x702U : 0x603U)},{r::STATE_XPC,base >> 16},
			{r::STATE_ST0,0x1e05},{r::STATE_ST1,0x800}})});
		c.data_events = std::vector<clocked_data_event>{};
		if (taken) c.data_events->push_back({false,0x1000,0x700,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
