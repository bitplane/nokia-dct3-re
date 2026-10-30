// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_HIGH_PROGRAM
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form != 7; ++form)
	for (unsigned reg = 0; reg != (form < 2 || form == 3 ? 1U : 8U); ++reg)
	for (unsigned pattern = 0; pattern != 4; ++pattern)
	for (bool mapped : {false,true}) {
		bool const extended = form >= 3;
		u32 const base = (form + reg + pattern + mapped) % 2 ? 0x20000 : 0;
		u32 const logical = std::array<u32,4>{0x032e00,0x03a000,0x7fff00,0xffffff}[pattern];
		u32 const physical = mapped && (logical & 0xffff) < 0x8000 ? logical & 0xffff : logical & 0x7fffff;
		u16 const source = std::array<u16,7>{0x1203,0x1203,0x1230,0x1230,0x122f,0x1231,0x1201}[form];
		u16 const value = std::array<u16,4>{0,0xf495,0xffff,0x1234}[pattern];
		u16 const mode = form < 2 ? 3 : form == 2 ? 0x90 | reg : form == 3 ? 0xf8 : 0xe0 | ((form - 4) << 3) | reg;
		auto initial = initial_case("WRITA-retains-source-PAR-and-overlay-decision", {u16(0x7f00 | mode)});
		if (extended) initial.program[1].value = form == 3 ? source : form == 4 ? 0xffff : form == 5 ? 1 : 2;
		initial.program[1 + extended].value = 0xf4e1;
		// Both physical code pages remain valid while OVLY changes mid-instruction.
		auto code = initial.program;
		for (auto const &word : code) initial.program.push_back({word.address + 0x20000,word.value});
		initial.program.push_back({physical,0xbeef}); initial.data = {{source,value}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa624},
			{r::STATE_ST1,form == 1 ? 0x4800U : 0x800U},{r::STATE_PMST,mapped ? 0x20U : 0U},
			{r::STATE_SP,0x1200},{r::STATE_BK,16},{r::STATE_A,0x1200000000ULL | logical},{r::STATE_B,0x123456789aULL}});
		auto access = observation("Access-updates-source-address-once", {});
		auto final = observation("retained-program-word-without-destination-read", {{STATE_GENPC,base + 0x602 + extended},
			{r::STATE_ST0,0xa624},{r::STATE_ST1,form == 1 ? 0x4800U : 0x800U},{r::STATE_PMST,mapped ? 0U : 0x20U},
			{r::STATE_SP,0x1200},{r::STATE_BK,16},{r::STATE_A,0x7f9000},{r::STATE_B,0x123456789aULL}},{{source,0xdead}});
		final.expected_program = {{physical,value}};
		for (unsigned ar = 0; ar != 8; ++ar) {
			u16 const pointer = ar == reg ? form == 6 ? 0x120f : 0x1230 : 0x3000 + ar;
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),pointer});
			access.expected.push_back({int(r::STATE_AR0) + int(ar),ar == reg ?
				u16(form == 6 ? 0x1201 : form == 2 || form == 5 ? 0x1231 : 0x1230) : pointer});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == reg ? 0x4000U : pointer});
		}
		clocked_register_case c{{{0,std::move(initial)},{4 + unsigned(extended),std::move(access)},
			{5 + unsigned(extended),observation("source-poison-after-Read", {})},{24,std::move(final)}},
			std::array<unsigned,4>{4,5,6,9}[(form + reg + pattern + mapped) % 4] + extended,24};
		c.actions = {{4 + unsigned(extended),{{int(r::STATE_AR0) + int(reg),0x4000}}},
			{5 + unsigned(extended),{},false,{}, {},{{source,0xdead}}},
			{6 + unsigned(extended),{{r::STATE_A,0x7f9000},{r::STATE_PMST,mapped ? 0U : 0x20U}}}};
		c.data_events = std::vector<clocked_data_event>{{false,source,value,5 + unsigned(extended)}};
		c.program_events = {{true,physical,value,10 + unsigned(extended)}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
