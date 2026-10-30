// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0,1,0xffffffffffULL,0x0100000000ULL,0,1,0xffffffffffULL,1,0x0100000000ULL};
	for (unsigned index = 0; index < 9; ++index) {
		bool const overflow = index >= 4 && index != 7;
		s64 const a = values[index] & 0x8000000000ULL ? s64(values[index]) - (s64(1) << 40) : s64(values[index]);
		bool const taken = index < 4 ? values[index] != 0 : overflow && a > 0;
		auto initial = initial_case("atomic-delayed-call", {u16(index < 4 ? 0xfb44 : 0xfb76),0x700,0xe800,0xe901,0xf4e1});
		for (auto &word : initial.program) word.address += 0x10000;
		initial.program.insert(initial.program.end(), {{0x10700,0xe902},{0x10701,0xfc00}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x10600},{r::STATE_ST1,0x900},
			{r::STATE_A,values[index]},{r::STATE_ST0,overflow ? 0x400U : 0U}});
		initial.data = {{0xfff,0xbeef}};
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 4U : 1U,40};
		c.pipeline_frontend = false;
		c.states.push_back({40,observation("predicate-before-delays", {{r::STATE_A,0},{r::STATE_B,taken ? 2U : 1U},
			{r::STATE_SP,0x1000},{STATE_GENPC,0x10605},{r::STATE_XPC,1}},{{0xfff,u16(taken ? 0x604 : 0xbeef)}})});
		c.data_events = std::vector<clocked_data_event>{};
		if (taken) {
			c.data_events->push_back({true,0xfff,0x604,0,false});
			c.data_events->push_back({false,0xfff,0x604,0,false});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
