// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned slot = 0; slot < 2; ++slot)
	for (unsigned form = 0; form < 4; ++form)
	for (unsigned pattern = 0; pattern < 4; ++pattern)
	for (unsigned caller = 0; caller < 2; ++caller)
	for (unsigned page = 0; page < 2; ++page)
	{
		u32 const base = caller ? 0x20000 : 0, source = (page ? 0x20000 : 0) + 0x2700;
		u16 const word = std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		unsigned const read = 7 + slot;
		u16 const st1 = 0x900 | (form == 3 ? 0x4000 : 0);
		auto initial = initial_case("delayed-program-read", {0xf273,0x700,0xf495,0xf495});
		for (auto &entry : initial.program) entry.address += base;
		if (slot) initial.program[2].value = 0x0084;
		initial.program[2 + slot].value = 0x7e00 | (form == 0 || form == 3 ? 0x20 : form == 1 ? 0x83 : 0x93);
		initial.program.insert(initial.program.end(), {{base + 0x700,0xf4e1},{source,word}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0x1080},{r::STATE_ST1,st1},
			{r::STATE_A,source - 5 * slot},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5aa5},
			{r::STATE_SP,0x4000},{r::STATE_AR3,0x4020},{r::STATE_AR4,0x5100}});
		initial.data = {{0x4020,0xdead},{0x5100,5}};
		clocked_register_case c{{{0,std::move(initial)}},
			std::array<unsigned,4>{3,read,read + 1,read + 4}[(slot + form + pattern + caller + page) % 4],32};
		c.states.push_back({read,observation("destination-captured", {{r::STATE_AR3,form == 2 ? 0x4021U : 0x4020U}})});
		c.states.push_back({read + 1,observation("program-address-captured", {{r::STATE_A,source}})});
		c.states.push_back({read + 2,observation("address-poisoned", {{r::STATE_A,0x5a5a}})});
		c.states.push_back({32,observation("retained-program-transfer", {{STATE_GENPC,base + 0x701},{r::STATE_SP,0x4000},
			{r::STATE_A,0x5a5a},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5aa5},{r::STATE_ST0,0x1080},
			{r::STATE_ST1,st1},{r::STATE_AR3,0x7000},{r::STATE_AR4,0x5100}},{{0x4020,word}})});
		c.actions = {{read,{{r::STATE_AR3,0x7000}}},{read + 1,{{r::STATE_A,0x5a5a}}},
			{read + 4,{},false,{{source,u16(~word)}}}};
		c.program_events = {{false,source,word,read + 4}};
		c.data_events = std::vector<clocked_data_event>{};
		if (slot) c.data_events->push_back({false,0x5100,5,0,false});
		c.data_events->push_back({true,0x4020,word,read + 5});
		c.clear_pipeline_before_restore = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
