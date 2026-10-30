// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned flags = 0; flags < 4; ++flags)
	for (unsigned accumulator = 0; accumulator < 2; ++accumulator)
	for (unsigned polarity = 0; polarity < 2; ++polarity)
	for (unsigned delayed = 0; delayed < 2; ++delayed)
	{
		u16 const mask = 1U << (10 - accumulator);
		u16 const status = 0x1805 | ((flags & 1) << 10) | ((flags & 2) << 8);
		bool const taken = ((flags >> accumulator) & 1) == polarity, slots = delayed || !taken;
		auto initial = initial_case("atomic-overflow-branch", {u16(0xf860 | (delayed << 9) | (accumulator << 3) | (polarity << 4)),
			0x700,u16(0xf5b0 | (10 - accumulator)),0xe955,0xe811,0xf4e1});
		initial.program.insert(initial.program.end(), {{0x700,0xe822},{0x701,0xf4e1}});
		initial.initial.push_back({r::STATE_ST0,status});
		clocked_register_case c{{{0,std::move(initial)}},(flags + accumulator + polarity + delayed) & 1 ? 3U : 1U,32};
		c.pipeline_frontend = false;
		c.states.push_back({1,observation("selected-overflow-consumed", {{r::STATE_ST0,unsigned(status & ~mask)}})});
		c.states.push_back({2,observation("retained-transfer", {})});
		c.states.push_back({32,observation("flag-changing-delay", {{r::STATE_A,taken ? 0x22U : 0x11U},{r::STATE_B,slots ? 0x55U : 0U},
			{STATE_GENPC,taken ? 0x702U : 0x606U},{r::STATE_ST0,unsigned((status & ~mask) | (slots ? mask : 0))},{r::STATE_SP,0x1000}})});
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
