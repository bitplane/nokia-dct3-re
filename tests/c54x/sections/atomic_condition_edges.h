// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned control = 0; control != 3; ++control)
	for (unsigned taken = 0; taken != 2; ++taken) {
		u64 const a = control == 0 ? taken ? 0 : 0xff00000000ULL : control == 1 ? taken ? 0 : 1 : taken ? 1 : 0;
		auto initial = initial_case("BC-AGEQ-CC-AEQ-RC-ANEQ", {std::array<u16,3>{0xf842,0xf945,0xfc44}[control]});
		if (control != 2) initial.program[1].value = 0x700;
		initial.program[control == 2 ? 1 : 2].value = 0xf4e1;
		initial.program.insert(initial.program.end(),{{0x5ff,0xf495},{0x700,0xf4e1}});
		initial.data = {{0x2ff,u16(control == 2 ? 0x700 : 0xa55a)}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,a},{r::STATE_SP,control == 2 ? 0x2ffU : 0x300U}});
		auto final = observation("only-taken-call-return-owns-stack", {{STATE_GENPC,taken ? 0x701U : control == 2 ? 0x602U : 0x603U},
			{r::STATE_A,a},{r::STATE_SP,control == 1 && taken ? 0x2ffU : control == 2 && !taken ? 0x2ffU : 0x300U}},
			{{0x2ff,u16(control == 1 && taken ? 0x602 : control == 2 ? 0x700 : 0xa55a)}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-condition-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("condition-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{};
		if (control == 1 && taken) c.data_events->push_back({true,0x2ff,0x602,0,false});
		if (control == 2 && taken) c.data_events->push_back({false,0x2ff,0x700,0,false});
		if (taken) c.program_events = {{false,0x700,0xf4e1,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
