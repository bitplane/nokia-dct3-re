// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,st0; u64 b; bool taken; };
	constexpr row rows[]{
		{0xfd30,0x1000,0,true},{0xfd30,0,0,false},
		{0xfd4b,0,0xff00000000ULL,true},{0xfd4b,0,0,false},
		{0xfd4d,0,0,true},{0xfd4d,0,1,false},
		{0xfd20,0x800,0x12345678,true},{0xfd20,0x1800,0x12345678,false}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		auto initial = initial_case("XC-one-slot-TC-BLT-BEQ-NTC", {0xf495,v.opcode,0x6d91,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x1234},{r::STATE_B,v.b},{r::STATE_ST0,v.st0},
			{r::STATE_ST1,0x100},{r::STATE_AR1,0x120}});
		auto final = observation("one-slot-executed-or-annulled-with-flags-preserved", {{STATE_GENPC,0x604},
			{r::STATE_A,0x1234},{r::STATE_B,v.b},{r::STATE_ST0,v.st0},{r::STATE_ST1,0x100},{r::STATE_AR1,v.taken ? 0x121U : 0x120U}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-XC-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("XC-running", {})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	return cases;
}
}
