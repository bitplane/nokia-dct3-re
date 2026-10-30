// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form != 3; ++form)
	for (unsigned move = 0; move != 2; ++move)
	{
		u16 const opcode = u16((move ? 0x7b00 : 0x7900) | (form == 0 ? 0x8b : form == 1 ? 0xf8 : 0xea));
		auto initial = initial_case(move ? "MACD-coefficient-and-successor" : "MACP-coefficient-no-store", {opcode});
		initial.program[1].value = form == 1 ? 0xf90 : 0x700;
		initial.program[2].value = form == 1 ? 0x700 : form == 2 ? 5 : 0xf4e1;
		if (form) initial.program[3].value = 0xf4e1;
		initial.program.push_back({0x5ff,0xf495});
		initial.program.push_back({0x700,0x1234});
		u32 const source = form == 2 ? 0xf05 : 0xf90;
		initial.data = {{source,0x55},{source + 1,0x66}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,0x1234},
			{r::STATE_B,0x770000},{r::STATE_T,8},{r::STATE_ST0,0x800},{r::STATE_ST1,0x100},
			{r::STATE_AR3,0xf90},{r::STATE_AR2,0xf00}});
		auto final = observation("old-B-plus-signed-coefficient-product", {{STATE_GENPC,form ? 0x604U : 0x603U},
			{r::STATE_A,0x1234},{r::STATE_B,0x7d0b44},{r::STATE_T,0x55},
			{r::STATE_AR3,form == 0 ? 0xf8fU : 0xf90U},{r::STATE_AR2,form == 2 ? 0xf05U : 0xf00U}},
			{{source,0x55},{source + 1,u16(move ? 0x55 : 0x66)}});
		final.expected_program = {{0x700,0x1234}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-MAC-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("MAC-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false;
		c.program_events = {{false,0x700,0x1234,0,false}};
		c.data_events = std::vector<clocked_data_event>{{false,source,0x55,0,false}};
		if (move) c.data_events->push_back({true,source + 1,0x55,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
