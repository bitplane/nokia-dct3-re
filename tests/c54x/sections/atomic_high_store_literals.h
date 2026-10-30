// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 5; ++variant) {
		bool const b = variant % 2 && variant != 4, ar2 = variant >= 2, circular = variant != 4;
		u16 const opcode = (b ? 0x8300 : 0x8200) | (circular ? 0xd0 : 0x90) | (ar2 ? 2 : 3);
		u16 const address = circular ? 0xf93 : 0xf95, word = b ? 0x8765 : 0x1234;
		auto initial = initial_case("STH-high-half-pointer-and-neighbor-literals", {0xf495,opcode,0xf4e1});
		initial.data = {{address,0},{address - 1,0xbeef},{address + 1,0xcafe}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0x12345678},{r::STATE_B,0xff87654321ULL},
			{r::STATE_AR2,ar2 ? address : 0x3002U},{r::STATE_AR3,ar2 ? 0xf90U : address},
			{r::STATE_BK,4},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100}});
		auto final = observation("only-selected-word-and-pointer-change", {{STATE_GENPC,0x603},
			{r::STATE_A,0x12345678},{r::STATE_B,0xff87654321ULL},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100},
			{r::STATE_BK,4},{r::STATE_AR2,ar2 ? circular ? 0xf90U : 0xf96U : 0x3002U},{r::STATE_AR3,0xf90}},
			{{address,word},{address - 1,0xbeef},{address + 1,0xcafe}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-store-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("store-running", {})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{{true,address,word,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
