// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned operation = 0; operation < 3; ++operation) for (unsigned producer = 0; producer < 4; ++producer)
	for (unsigned gap : {0U,1U,3U}) for (unsigned high = 0; high < 2; ++high)
	for (unsigned compiler = 0; compiler < 2; ++compiler)
	{
		bool const copy = operation == 2;
		u16 const start = high ? 0x9000 : 0x1000, operand = start + std::array<int,4>{-4,-1,1,-8}[producer];
		unsigned const prefix = producer == 3 ? 2 : 1, read = 6 + prefix + gap;
		auto initial = initial_case("mapped-stack-operand", {std::array<u16,4>{0xeefc,0x4a11,0x8a11,0x7718}[producer]});
		if (producer == 3) initial.program[1].value = operand;
		initial.program[prefix + gap].value = copy ? 0x7092 : operation ? 0x01f8 : 0x00f8;
		initial.program[prefix + gap + 1].value = 0x18;
		initial.program[prefix + gap + 2].value = 0xee02;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xa008},{r::STATE_ST1,compiler ? 0x4900 : 0x900},
			{r::STATE_SP,start},{r::STATE_AR1,0x1357},{r::STATE_AR2,0x1400},
			{r::STATE_A,operation == 1 ? 0x2233445566ULL : 0},{r::STATE_B,operation == 1 ? 0 : 0x2233445566ULL}});
		initial.data = {{start,0xbeef},{0x1400,0xaaaa}};
		u64 const value = copy ? 0 : operand & 0x8000 ? 0xffffff0000ULL | operand : operand;
		auto final = observation("retained-before-younger-frame", {{r::STATE_A,operation == 1 ? 0x2233445566ULL : value},
			{r::STATE_B,operation == 1 ? value : 0x2233445566ULL},{r::STATE_SP,0x2468},
			{r::STATE_AR1,producer == 2 ? 0xbeefU : 0x1357U},{r::STATE_AR2,copy ? 0x1401U : 0x1400U},
			{r::STATE_ST0,0xa008},{r::STATE_ST1,compiler ? 0x4900U : 0x900U}});
		if (copy) final.expected_data = {{0x1400,operand}};
		clocked_register_case c{{{0,std::move(initial)},
			{read + 2,observation("younger-frame", {{r::STATE_SP,u16(operand + 2)}})},
			{read + 3,std::move(final)}},std::array<unsigned,3>{4,read,read + 2}[(operation + producer + gap + high + compiler) % 3],24};
		c.require_idle = false;
		c.actions = {{read + 2,{{r::STATE_SP,0x2468}}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (producer == 1) {
			c.data_events->push_back({false,0x11,0x1357,5});
			c.data_events->push_back({true,u16(start - 1),0x1357,6});
		} else if (producer == 2) {
			c.data_events->push_back({false,start,0xbeef,5});
			c.data_events->push_back({true,0x11,0xbeef,5});
		} else if (producer == 3) c.data_events->push_back({true,0x18,operand,5});
		c.data_events->push_back({false,0x18,operand,read});
		if (copy) c.data_events->push_back({true,0x1400,operand,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
