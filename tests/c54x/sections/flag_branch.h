// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// Four TC producers and one carry producer; arithmetic carry production
	// is tested separately, not implied by the status-bit carry cases here.
	for (unsigned producer = 0; producer < 5; ++producer)
	for (unsigned condition = 0; condition < 2; ++condition) for (unsigned delayed = 0; delayed < 2; ++delayed)
	for (unsigned value = 0; value < 2; ++value) for (unsigned gap = 0; gap < 3; ++gap)
	for (unsigned poison = 0; poison < 2; ++poison)
	{
		bool const carry = producer == 4, taken = value == condition;
		u16 const flag = carry ? 0x800 : 0x1000;
		u16 const operation = std::array<u16,5>{0x9634,0xf4aa,u16(value ? 0xf5bc : 0xf4bc),0xf5aa,u16(value ? 0xf5bb : 0xf4bb)}[producer];
		u32 const base = (producer + condition + delayed + value + gap) % 2 ? 0x20000 : 0;
		auto initial = initial_case("flag-branch", {operation});
		initial.program[1 + gap].value = (carry ? 0xf808 | (condition << 2) : 0xf820 | (condition << 4)) | (delayed << 9);
		initial.program[2 + gap].value = 0x700;
		initial.program[3 + gap].value = carry ? value ? 0xf4bb : 0xf5bb : value ? 0xf4bc : 0xf5bc;
		initial.program[4 + gap].value = 0xe922;
		initial.program[5 + gap].value = 0xe811;
		initial.program[6 + gap].value = 0xf4e1;
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(), {{base + 0x700,0xe855},{base + 0x701,0xf4e1}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,unsigned(0xa608 | (value ? 0 : flag))},
			{r::STATE_ST1,0x800},{r::STATE_AR0,0x1200},{r::STATE_AR5,0x1000},
			{r::STATE_AR2,producer == 3 ? value ? 0x11ffU : 0x1200U : value ? 0x1200U : 0x1201U}});
		initial.data = {{0x1000,value ? 0x800U : 0U}};
		bool const final_flag = delayed || !taken || poison ? !value : value;
		auto final = observation("retained-condition", {{r::STATE_A,taken ? 0x55U : 0x11U},
			{r::STATE_B,delayed || !taken ? 0x22U : 0U},{r::STATE_ST0,unsigned(0xa608 | (final_flag ? flag : 0))},
			{r::STATE_ST1,0x800},{r::STATE_AR5,0x1000}});
		clocked_register_case c{{{0,std::move(initial)},{32,std::move(final)}},
			std::array<unsigned,4>{4,6,7,10}[(producer + condition + delayed + value + poison) % 4] + gap,32};
		if (poison) c.actions = {{7 + gap,{{r::STATE_ST0,0xa608 | (value ? 0 : flag)}}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (!producer) c.data_events->push_back({false,0x1000,u16(value ? 0x800 : 0),5});
		c.program_events = {{false,base + 0x602 + gap,0x700,0,false}};
		if (taken) c.program_events.push_back({false,base + 0x700,0xe855,(delayed ? 9U : 8U) + gap});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
