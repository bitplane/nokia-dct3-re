// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 512; ++index) {
		unsigned const operand = (index >> 4) & 15, reg = 2 + (operand & 3);
		u64 const source = index & 256 ? 0xff87654321ULL : 0x12345678;
		u16 const word = u16(source << (index & 15));
		auto initial = initial_case("compact-low-store", {u16(0x9800 | index),0xf4e1});
		initial.data = {{0x507,0xbeef}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0x12345678},{r::STATE_B,0xff87654321ULL},
			{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x900},{r::STATE_AR0,3},{r::STATE_BK,8},{int(r::STATE_AR0 + reg),0x507}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("shifted-low-word-and-address-modification", {{r::STATE_A,0x12345678},{r::STATE_B,0xff87654321ULL},
			{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x900},{int(r::STATE_AR0 + reg),u64(0x507 + std::array<int,4>{0,-1,1,-5}[operand >> 2])},
			{STATE_GENPC,0x602}},{{0x507,word}})});
		c.data_events = std::vector<clocked_data_event>{{true,0x507,word,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
