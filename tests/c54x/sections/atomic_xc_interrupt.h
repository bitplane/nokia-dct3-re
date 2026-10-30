// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned scenario = 0; scenario != 8; ++scenario) {
		bool const loop = scenario >= 4, wide = loop ? scenario & 1 : scenario >= 2;
		bool const accepted = loop || !(scenario & 1), nmi = scenario >= 6;
		u16 const vector = nmi ? 4 : 0x48, xc = wide ? 0xff4d : 0xfd4d;
		auto initial = initial_case("atomic-XC-interrupt-protection-across-followers-or-loop-wrap",{0xf495});
		initial.program.clear();
		std::vector<u16> code = loop ? std::vector<u16>{0xf072,u16(wide ? 0x805 : 0x804),0xe809,xc,
			u16(wide ? 0xf020 : 0xe801),u16(wide ? 1 : 0xf4e1),0xf4e1} : std::vector<u16>{xc,0xe801,0xe809,0xf4e1};
		for (unsigned i = 0; i != code.size(); ++i) initial.program.push_back({0x800+i,code[i]});
		initial.program.insert(initial.program.end(),{{vector,0x0083},{vector+1,u16(loop ? 0xf4e1 : 0xf4eb)}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x800},{r::STATE_A,0},{r::STATE_B,accepted ? 0U : 1U},
			{r::STATE_AR3,0x61},{r::STATE_SP,0x1000},{r::STATE_ST0,0},{r::STATE_ST1,0},{r::STATE_PMST,0},{r::STATE_IMR,4},{r::STATE_BRC,1}});
		initial.data = {{0x61,0},{0xfff,0xaaaa}};
		u16 const continuation = loop && wide ? 0x802 : 0x803;
		u64 const entry_a = loop ? wide ? 1 : 9 : wide && !accepted ? 0 : 9;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("atomic-XC-interrupt-running",{})},
			{32,observation("two-word-protection-and-owned-return",{{r::STATE_A,entry_a},{r::STATE_SP,loop ? 0xfffU : 0x1000U},
				{STATE_GENPC,loop ? u32(vector+2) : 0x804U}},{{0xfff,continuation}})}},3,32};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{true,0xfff,continuation,0,false},{false,0x61,0,0,false}};
		c.data_events->back().registers = {{r::STATE_A,entry_a}};
		if (!loop) c.data_events->push_back({false,0xfff,continuation,0,false});
		u32 const trigger = loop ? 0x804 : 0x800;
		u16 const trigger_word = loop ? wide ? 0xf020 : 0xe801 : xc;
		c.program_events = {{false,trigger,trigger_word,0,false},{false,vector,0x0083,0,false}};
		int const line = nmi ? INPUT_LINE_NMI : 2;
		c.program_events.front().raise_input = line;
		c.irqs = {{line,0,~0U,0,continuation}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
