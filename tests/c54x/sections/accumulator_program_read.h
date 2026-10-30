// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_HIGH_PROGRAM
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned ram = 0; ram < 2; ++ram) for (unsigned page : {0U,2U,0x7fU})
	for (unsigned upper = 0; upper < 2; ++upper) for (unsigned overlay = 0; overlay < 2; ++overlay)
	{
		u32 const logical = (page << 16) | (upper ? 0x8700 : 0x700);
		u32 const physical = overlay && !upper ? logical & 0xffff : logical;
		auto initial = initial_case("accumulator-program-read", {0xf000,1,u16(ram ? 0x7e92 : 0x7ef8),u16(ram ? 0xf495 : 0x11)});
		initial.program[ram ? 5 : 6].value = ram ? 0x1083 : 0x1081;
		for (auto &word : initial.program) word.address += 0x18a00;
		initial.program.insert(initial.program.end(), {{physical,0x440},{physical + 1,0x450}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x19000},{r::STATE_PMST,overlay ? 0x20 : 0},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800},{r::STATE_AR1,0x410},{r::STATE_AR2,0x420},
			{r::STATE_AR3,0x420},{r::STATE_A,logical - 1}});
		initial.data = {{0x410,0x1111},{0x440,0x4444},{0x420,0xffff}};
		auto final = observation("consumer", {{r::STATE_AR1,ram ? 0x410U : 0x440U},
			{r::STATE_AR2,ram ? 0x421U : 0x420U},{r::STATE_A,ram ? 0x440U : 0x4444U},
			{r::STATE_ST1,0x800},{r::STATE_PMST,overlay ? 0x20U : 0U},{r::STATE_SP,0x1000}});
		clocked_register_case c{{{0,std::move(initial)},{22,std::move(final)}},
			(ram ? std::array<unsigned,4>{5,7,9,11} : std::array<unsigned,4>{6,8,10,12})[(page + upper + overlay) % 4],22};
		c.require_idle = false;
		c.actions = {{ram ? 8U : 9U,{{r::STATE_A,logical + 1}}},
			{ram ? 11U : 12U,{},false,{{physical,0x450}}}};
		c.program_events = {{false,physical,0x440,ram ? 11U : 12U}};
		// Use two NOPs for the MMR-to-indirect consumer, not the old
		// short-gap old-pointer projections; the RAM consumer has its own timing.
		c.data_events = std::vector<clocked_data_event>{{true,ram ? 0x420U : 0x11U,0x440,ram ? 12U : 13U},
			{false,ram ? 0x420U : 0x440U,u16(ram ? 0x440 : 0x4444),ram ? 14U : 15U}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
