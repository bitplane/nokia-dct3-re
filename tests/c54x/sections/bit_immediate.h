// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 512; ++index) {
		unsigned const operand = index / 32, bit = (index / 2) & 15, reg = 2 + (operand & 3);
		bool const set = index & 1;
		u16 const mask = u16(1) << (15 - bit), value = set ? mask : u16(~mask);
		auto initial = initial_case("bit-immediate", {u16(0x9600 | (operand << 4) | bit),0xf4e1});
		initial.data = {{0x507,value}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,u64(0xaa5 | (set ? 0 : 0x1000))},
			{r::STATE_AR0,3},{int(r::STATE_AR0 + reg),0x507},{r::STATE_BK,8},{r::STATE_A,0x12345},{r::STATE_B,0x6789}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("bit-position-and-address-mode", {{r::STATE_ST0,u64(0xaa5 | (set ? 0x1000 : 0))},
			{int(r::STATE_AR0 + reg),std::array<u16,4>{0x507,0x506,0x508,0x502}[operand >> 2]},
			{r::STATE_A,0x12345},{r::STATE_B,0x6789},{STATE_GENPC,0x602}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x507,value,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
