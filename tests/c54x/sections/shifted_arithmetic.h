// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr int shifts[]{-16,-1,0,1,15};
	constexpr s64 unsigned_operands[]{0,32764,65528,131056,2147221504};
	constexpr s64 signed_operands[]{-1,-4,-8,-16,-262144};
	for (unsigned index = 0; index < 104; ++index) {
		unsigned const selectors = index % 8;
		int const shift = index < 24 ? int(index / 8) - 1 : shifts[((index - 24) / 8) % 5];
		s64 const operand = index < 24 ? std::array<s64,3>{4,8,16}[index / 8] :
			(index >= 64 ? signed_operands : unsigned_operands)[((index - 24) / 8) % 5];
		s64 const source = selectors & 2 ? 30 : 10;
		u64 const result = u64(source + (selectors & 4 ? -operand : operand)) & 0xffffffffffULL;
		u16 const word = index < 24 ? 8 : 0xfff8;
		u16 const extension = 0xc00 | (selectors & 4 ? 0x20 : 0) | (selectors & 2 ? 0x200 : 0) |
			(selectors & 1 ? 0x100 : 0) | (shift & 31);
		auto initial = initial_case("shifted-arithmetic", {0x6f82,extension,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_AR2,0x600},{r::STATE_A,10},{r::STATE_B,30},
			{r::STATE_ST0,0},{r::STATE_ST1,index >= 64 ? 0x100U : 0U}});
		initial.data = {{0x600,word}};
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("independent-alu-selectors", {{r::STATE_A,selectors & 1 ? 10 : result},
			{r::STATE_B,selectors & 1 ? result : 30},{r::STATE_AR2,0x600},{STATE_GENPC,0x603}},{{0x600,word}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x600,word,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
