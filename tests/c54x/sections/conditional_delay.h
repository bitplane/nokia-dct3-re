// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant < 32; ++variant)
	{
		bool const b = variant & 1, equal = variant & 2, nonzero = variant & 4, memory = variant & 16;
		bool const taken = equal ? !nonzero : nonzero;
		u32 const base = variant & 8 ? 0x10000 : 0;
		u64 const value = nonzero ? 0x0100000000ULL : 0, opposite = nonzero ? 0 : 0x0100000000ULL;
		auto initial = initial_case("conditional-delay", {u16(0xfa44 | (unsigned(b) << 3) | unsigned(equal)),0x700,
			u16(memory ? 0x467f : 0xe811),u16(memory ? 0xf495 : 0xe944),0xe966});
		for (auto &word : initial.program) word.address += base;
		for (unsigned i = 0; i < 24; ++i) initial.program.push_back({base + 0x700 + i,
			u16(i == 0 ? memory ? 0x107e : 0xe955 : i == 1 && memory ? 0xe955 : 0xf495)});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},
			{r::STATE_A,b ? 0x9999 : value},{r::STATE_B,b ? value : 0x7777}});
		if (memory) initial.data = {{0x47f,0xe012},{0x97e,0x3333}};
		unsigned const point = memory ? std::array<unsigned,3>{6,8,11}[variant % 3] :
			std::array<unsigned,3>{4,6,8}[variant % 3];
		clocked_register_case c{{{0,std::move(initial)}},point,18};
		c.actions = {{6,{{int(b ? r::STATE_B : r::STATE_A),opposite}},true}};
		for (unsigned tick = 1; tick <= 18; ++tick)
		{
			u64 const a = memory ? taken && tick >= 14 ? 0x3333 : !b && tick >= 6 ? opposite : b ? 0x9999 : value :
				tick >= 8 ? 0x11 : !b && tick >= 6 ? opposite : b ? 0x9999 : value;
			u64 const out_b = memory ? taken && tick >= 15 ? 0x55 : !taken && tick >= 13 ? 0x66 : b && tick >= 6 ? opposite : b ? value : 0x7777 :
				taken && tick >= 12 ? 0x55 : !taken && tick >= 11 ? 0x66 : tick >= 10 ? 0x44 : b && tick >= 6 ? opposite : b ? value : 0x7777;
			c.states.push_back({tick,observation("delays-and-continuation", {{r::STATE_A,a},{r::STATE_B,out_b},
				{r::STATE_ST0,memory && tick >= 10 ? 0xa612U : 0xa608U},{r::STATE_ST1,0x800}})});
		}
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,base + 0x602,u16(memory ? 0x467f : 0xe811),4},
			{false,base + 0x603,u16(memory ? 0xf495 : 0xe944),6}};
		if (taken) c.program_events.push_back({false,base + 0x700,u16(memory ? 0x107e : 0xe955),memory ? 10U : 8U});
		c.data_events = std::vector<clocked_data_event>{};
		if (memory) {
			c.data_events->push_back({false,0x47f,0xe012,7});
			if (taken) c.data_events->push_back({false,0x97e,0x3333,13});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
