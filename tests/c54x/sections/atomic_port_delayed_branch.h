// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned condition = 0; condition != 3; ++condition)
		for (bool taken : {false,true}) {
			u16 const opcode = condition == 0 ? 0xfa20 : condition == 1 ? 0xfa4d : 0xfa30;
			u16 const st0 = (condition == 0 ? !taken : condition == 2 && taken) ? 0x1000 : 0;
			auto initial = initial_case("atomic-delayed-condition-before-slot-effects",{0xf495,0x75f8,0x100,0x124,
				opcode,0x700,0xe801,0xe902,0xe803,0x75f8,0x101,0x124,0xf4e1});
			initial.program.insert(initial.program.end(),{{0x700,0x75f8},{0x701,0x102},{0x702,0x124},{0x703,0xf4e1}});
			initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_B,condition == 1 && !taken ? 1U : 0U},
				{r::STATE_ST0,st0},{r::STATE_ST1,0}});
			initial.data = {{0x100,0x1111},{0x101,0x2222},{0x102,0x3333}};
			auto final = observation("both-slots-execute-before-selected-continuation",{{STATE_GENPC,taken ? 0x704U : 0x60dU},
				{r::STATE_A,taken ? 1U : 3U},{r::STATE_B,2},{r::STATE_ST0,st0},{r::STATE_ST1,0}});
			final.expected_data = initial.data;
			clocked_register_case c{{{0,std::move(initial)},{1,observation("before-delay-markers",{{STATE_GENPC,0x601}})},
				{2,observation("delay-markers-running",{})},{48,std::move(final)}},3,48};
			c.pipeline_frontend = false; c.observed_data_space = AS_IO;
			c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},
				{true,0x124,u16(taken ? 0x3333 : 0x2222),0,false,{},taken ? 8U : 9U}};
			cases.push_back(std::move(c));
		}
	return cases;
}
}
