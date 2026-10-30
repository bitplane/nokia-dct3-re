// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form != 7; ++form)
	for (unsigned reg = 0; reg != (form < 2 || form == 3 ? 1U : 8U); ++reg) {
		bool const extended = form >= 3;
		u32 const base = (form + reg) % 2 ? 0x20000 : 0;
		u16 const source = std::array<u16,7>{0x1203,0x1203,0x1230,0x1230,0x122f,0x1231,0x1201}[form];
		u16 const mode = form < 2 ? 3 : form == 2 ? 0x90 | reg : form == 3 ? 0xf8 : 0xe0 | ((form - 4) << 3) | reg;
		auto initial = initial_case("MVDP-retained-source-and-instruction-page-destination", {u16(0x7d00 | mode),u16(form == 3 ? source : 0x2e00)});
		if (extended) initial.program[2].value = form == 3 ? 0x2e00 : form == 4 ? 0xffff : form == 5 ? 1 : 2;
		initial.program[2 + extended].value = 0xf4e1;
		for (auto &word : initial.program) word.address += base;
		initial.program.push_back({base + 0x2e00,0xbeef}); initial.data = {{source,0x2468}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa624},
			{r::STATE_ST1,form == 1 ? 0x4800U : 0x800U},{r::STATE_SP,0x1200},{r::STATE_BK,16}});
		auto access = observation("Access-updates-pointer-once", {});
		auto final = observation("one-source-and-one-program-write", {{STATE_GENPC,base + 0x603 + extended},
			{r::STATE_ST0,0xa624},{r::STATE_ST1,form == 1 ? 0x4800U : 0x800U},{r::STATE_SP,0x1200},{r::STATE_BK,16}},{{source,0xdead}});
		final.expected_program = {{base + 0x2e00,0x2468}};
		for (unsigned ar = 0; ar != 8; ++ar) {
			u16 const start = ar == reg ? form == 6 ? 0x120f : 0x1230 : 0x3000 + ar;
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),start});
			u16 const next = form == 6 ? 0x1201 : form == 2 || form == 5 ? 0x1231 : 0x1230;
			access.expected.push_back({int(r::STATE_AR0) + int(ar),ar == reg ? next : start});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == reg ? 0x4000U : start});
		}
		clocked_register_case c{{{0,std::move(initial)},{5 + unsigned(extended),std::move(access)},
			{6 + unsigned(extended),observation("pointer-poison-after-capture", {})},{24,std::move(final)}},
			std::array<unsigned,3>{5,6,8}[(form + reg) % 3] + extended,24};
		c.actions = {{5 + unsigned(extended),{{int(r::STATE_AR0) + int(reg),0x4000}}},
			{6 + unsigned(extended),{},false,{}, {},{{source,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{{false,source,0x2468,6 + unsigned(extended)}};
		c.program_events = {{true,base + 0x2e00,0x2468,0,false}};
		cases.push_back(std::move(c));
	}
	for (unsigned snapshot = 0; snapshot != 6; ++snapshot) {
		u32 const base = snapshot % 2 ? 0x20000 : 0;
		auto initial = initial_case("adjacent-MVDP-independent-write-ownership", {0x7d92,0x2e00,0x7d92,0x2e01,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(),{{base + 0x2e00,0xbeef},{base + 0x2e01,0xbeef}});
		initial.data = {{0x1230,0x2468},{0x1231,0x1357}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_AR2,0x1230}});
		auto final = observation("both-copies-retire-with-one-pointer-update-each", {{STATE_GENPC,base + 0x605},{r::STATE_AR2,0x1232}});
		final.expected_program = {{base + 0x2e00,0x2468},{base + 0x2e01,0x1357}};
		clocked_register_case c{{{0,std::move(initial)},{32,std::move(final)}},std::array<unsigned,6>{5,6,8,9,10,12}[snapshot],32};
		c.data_events = std::vector<clocked_data_event>{{false,0x1230,0x2468,0,false},{false,0x1231,0x1357,0,false}};
		c.program_events = {{true,base + 0x2e00,0x2468,0,false},{true,base + 0x2e01,0x1357,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
