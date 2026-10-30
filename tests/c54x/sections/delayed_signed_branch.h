// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr s64 values[]{-1,0,1,0x0100000000LL,-549755813888LL};
	for (unsigned index = 0; index < 60; ++index) {
		unsigned const condition = index / 5;
		s64 const value = values[index % 5];
		bool const taken = std::array<bool,6>{value >= 0,value < 0,value != 0,value == 0,value > 0,value <= 0}[condition % 6];
		u32 const base = index & 1 ? 0x10000 : 0;
		auto initial = initial_case("signed-delayed-branch", {u16(0xfa42 + condition % 6 + (condition >= 6 ? 8 : 0)),
			0x700,0xe900,0xe800,0xe801,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(), {{base + 0x700,0xe802},{base + 0x701,0xf4e1}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST1,0x900},
			{r::STATE_A,condition < 6 ? u64(value) & 0xffffffffffULL : 1},
			{r::STATE_B,condition >= 6 ? u64(value) & 0xffffffffffULL : 1}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{3,5,7}[index % 3],32};
		c.states.push_back({32,observation("predicate-retained-across-both-delays", {{r::STATE_A,taken ? 2U : 1U},
			{r::STATE_B,0},{r::STATE_ST1,0x900},{STATE_GENPC,base + (taken ? 0x702 : 0x606)}})});
		c.forbid_data_access = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
