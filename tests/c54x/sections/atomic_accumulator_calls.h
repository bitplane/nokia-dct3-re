// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 3; ++variant) {
		bool const delayed = variant == 2;
		u16 const opcode = variant == 0 ? 0xf4e3 : variant == 1 ? 0xf5e3 : 0xf6e3;
		u16 const value = variant == 0 ? 0xbeef : variant == 1 ? 0xcafe : 0x1234, address = 0x944 + variant;
		u16 const continuation = delayed ? 0x604 : 0x602;
		auto initial = initial_case("CALA-A-B-and-CALAD-captured-target",{0xf495,opcode});
		if (delayed) { initial.program[2].value = 0xe801; initial.program[3].value = 0xe902; }
		initial.program[delayed ? 4 : 2].value = 0xf4e1;
		initial.program.insert(initial.program.end(),{{0x700,0x76f8},{0x701,address},{0x702,value},{0x703,0xfc00}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,variant == 1 ? 0x1234U : 0x700U},
			{r::STATE_B,variant == 1 ? 0x700U : 3U},{r::STATE_SP,0x300},{r::STATE_ST0,0},{r::STATE_ST1,0}});
		initial.data = {{address,0},{0x2ff,0xa55a}};
		auto final = observation("target-call-frame-delay-effects-and-return",{{STATE_GENPC,u32(continuation + 1)},
			{r::STATE_SP,0x300},{r::STATE_A,delayed ? 1U : variant == 1 ? 0x1234U : 0x700U},
			{r::STATE_B,delayed ? 2U : variant == 1 ? 0x700U : 3U}},{{address,value},{0x2ff,continuation}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-call-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("call-running",{})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{true,0x2ff,continuation,0,false},{true,address,value,0,false},{false,0x2ff,continuation,0,false}};
		c.program_events = {{false,0x700,0x76f8,0,false}}; cases.push_back(std::move(c));
	}
	return cases;
}
}
