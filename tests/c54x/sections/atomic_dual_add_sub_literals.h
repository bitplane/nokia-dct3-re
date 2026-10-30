// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 8; ++variant) {
		bool const memory = variant >= 6, special = variant == 4 || variant == 5, b = variant < 4 && variant % 2;
		u16 const opcode = variant < 4 ? 0xa03a + variant * 0x100 : variant == 4 ? 0xa249 : variant == 5 ? 0xa201 : variant == 6 ? 0x6b84 : 0x6b8c;
		auto initial = initial_case("dual-ADD-SUB-and-saturating-ADDM-literals",{0xf495,opcode});
		if (memory) initial.program[2].value = 0xfff8;
		initial.program[2 + memory].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x1234},{r::STATE_B,0x5678},{r::STATE_T,0x4444},
			{r::STATE_ST0,0},{r::STATE_ST1,memory ? 0x300U : 0x100U},{r::STATE_AR2,0xf94},
			{r::STATE_AR3,0xf95},{r::STATE_AR4,memory ? 0x100U : 0xf91U},{r::STATE_AR5,0xf90}});
		initial.data = memory ? std::vector<memory_word>{{0xff,0xbeef},{0x100,0x8007},{0x101,0xcafe}} :
			special ? std::vector<memory_word>{{0xf94,0xfffb},{0xf95,7}} : std::vector<memory_word>{{0xf90,0x55},{0xf91,0xaa}};
		u64 const result = special ? 0xfffff40000ULL : variant >= 2 ? 0xffffab0000ULL : 0xff0000;
		auto final = observation("fixed-shifted-sign-carry-and-address-effects",{{STATE_GENPC,memory ? 0x604U : 0x603U},
			{r::STATE_A,memory || b ? 0x1234 : result},{r::STATE_B,b ? result : 0x5678},{r::STATE_T,0x4444},
			{r::STATE_ST1,memory ? 0x300U : 0x100U},{r::STATE_AR2,variant == 4 ? 0xf93U : 0xf94U},
			{r::STATE_AR3,variant == 4 ? 0xf96U : 0xf95U},{r::STATE_AR4,memory ? variant == 7 ? 0xffU : 0x100U : special ? 0xf91U : 0xf92U},
			{r::STATE_AR5,0xf90}});
		if (special) final.expected.push_back({r::STATE_ST0,0x800,0xc00});
		if (memory) final.expected.push_back({r::STATE_ST0,0xc00});
		final.expected_data = initial.data; if (memory) final.expected_data[1].value = 0x8000;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-ALU-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("ALU-running",{})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false;
		c.data_events = memory ? std::vector<clocked_data_event>{{false,0x100,0x8007,0,false},{true,0x100,0x8000,0,false}} :
			special ? std::vector<clocked_data_event>{{false,0xf94,0xfffb,0,false},{false,0xf95,7,0,false}} :
			std::vector<clocked_data_event>{{false,0xf90,0x55,0,false},{false,0xf91,0xaa,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
