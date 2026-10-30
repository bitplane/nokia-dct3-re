// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_XF
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned bank = 0; bank < 2; ++bank) for (unsigned bit = 0; bit < 16; ++bit)
	for (unsigned set = 0; set < 2; ++set) for (u16 before : {0U,0xffffU})
	{
		u16 const mask = 1U << bit, after = set ? before | mask : before & ~mask;
		auto initial = initial_case("status-bit", {u16(0xf4b0 | (bank << 9) | (set << 8) | bit)});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,before},{r::STATE_ST1,before}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_ST0,before},{r::STATE_ST1,unsigned(before & ~0x400)}});
		auto final = observation("execute-publication", {{r::STATE_ST0,bank ? before : after},
			{r::STATE_ST1,unsigned((bank ? after : before) & ~0x400)}});
		clocked_register_case c{{{0,std::move(initial)},{6,std::move(final)}},5,9};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
