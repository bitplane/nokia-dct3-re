// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant < 10; ++variant) {
		bool const excluded = variant >= 8;
		unsigned const first = excluded ? 3 : std::array<unsigned,4>{0,1,3,7}[variant % 4];
		unsigned const second = variant & 4 ? 2 : 0;
		u32 const base = variant & 1 ? 0x20000 : 0;
		auto initial = initial_case("repeat-rearm", {0xf072,0x604,0x6d92,0xf495,0xf495,0x771a,u16(second),
			0xf072,0x60b,0x6d93,0xf495,0xf495,0x6d94,0xf4e1});
		if (excluded) { initial.program[1].value = 0x60b; initial.program[2].value = 0x771a; initial.program[3].value = 0; initial.expected.clear(); }
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},{r::STATE_BRC,first},
			{r::STATE_AR2,0x1200},{r::STATE_AR3,0x1300},{r::STATE_AR4,0x1400}});
		clocked_register_case c{{{0,std::move(initial)}},excluded ? 6 + variant % 4 : 10 + first * 3 + variant % 4,excluded ? 20U : 100U};
		c.data_events = std::vector<clocked_data_event>{};
		if (excluded) {
			c.require_idle = false;
			auto final = observation("in-body-brc-model-limit", {{r::STATE_ILLEGAL,1},{r::STATE_BRC,3},
				{r::STATE_RSA,0x602},{r::STATE_REA,0x60b},{r::STATE_AR2,0x1200},{r::STATE_AR3,0x1300},{r::STATE_AR4,0x1400}});
			final.expected.pop_back();
			c.states.push_back({20,std::move(final)});
		} else {
			c.states.push_back({100,observation("independent-block-counts", {{STATE_GENPC,base + 0x60e},
				{r::STATE_AR2,0x1201 + first},{r::STATE_AR3,0x1301 + second},{r::STATE_AR4,0x1401},
				{r::STATE_BRC,0},{r::STATE_RSA,0x609},{r::STATE_REA,0x60b},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800}})});
			c.data_events->push_back({true,0x1a,u16(second),0,false});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
