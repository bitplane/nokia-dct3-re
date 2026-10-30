// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form != 6; ++form)
	for (unsigned reg = 0; reg != (form < 3 ? 1U : 8U); ++reg)
	for (unsigned loop = 0; loop != 2; ++loop)
	for (unsigned pattern = 0; pattern != 4; ++pattern)
	for (unsigned gap = 1; gap <= (loop ? 3U : 1U); ++gap)
	for (unsigned address_mode = 0; address_mode != 8; ++address_mode) {
		bool const extended = address_mode & 4, overlay = address_mode & 2;
		u32 const logical = (address_mode & 1 ? 0x20000 : 0) | 0x400;
		u32 const physical = !extended || overlay ? logical & 0xffff : logical;
		u16 const value = loop ? std::array<u16,4>{0,1,2,7}[pattern] : std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		u16 const mode = form == 0 ? 0x1a : form == 1 ? 0xa : form == 2 ? 0xf8 :
			(form == 3 ? 0x80 : form == 4 ? 0x90 : 0x88) | reg;
		unsigned const body_reg = (reg + 1) % 8, words = form == 2 ? 2 : 1, read = 8 + words;
		u32 const setup = 0x600 + words + gap;
		unsigned const writes = loop ? value + 1 : 0;
		u16 const st1 = form == 1 ? 0x4800 : 0x800;
		auto initial = initial_case("READA-retains-PAR-and-publishes-BRC-before-RPTB", {u16(0x7e00 | mode)});
		if (form == 2) initial.program[1].value = 0x1a;
		if (loop) {
			initial.program[setup - 0x600].value = 0xf072;
			initial.program[setup + 1 - 0x600].value = setup + 3;
			initial.program[setup + 2 - 0x600].value = 0x7690 | body_reg;
			initial.program[setup + 3 - 0x600].value = 0x2222;
			initial.program[setup + 4 - 0x600].value = 0xf4e1;
		} else initial.program[words + 6].value = 0xf4e1;
		initial.program.insert(initial.program.end(),{{physical,value},{0x480,u16(value ^ 0xffff)},{0x30480,u16(value ^ 0xffff)}});
		initial.initial.insert(initial.initial.end(), {{r::STATE_PMST,overlay ? 0x20U : 0U},{r::STATE_ST0,0xa600},
			{r::STATE_ST1,st1},{r::STATE_A,0x1200000000ULL | logical},{r::STATE_B,0xfeabcd9876ULL},
			{r::STATE_SP,form == 1 ? 0x10U : 0x3000U},{r::STATE_BRC,0x55aa}});
		auto final = observation("new-count-drives-only-owned-loop-stores", {{STATE_GENPC,loop ? setup + 5 : 0x607 + words},
			{r::STATE_BRC,loop ? 0U : value},{r::STATE_A,0x3400030480ULL},{r::STATE_B,0xfeabcd9876ULL},
			{r::STATE_ST0,0xa600},{r::STATE_ST1,st1},{r::STATE_SP,form == 1 ? 0x10U : 0x3000U}});
		for (unsigned ar = 0; ar != 8; ++ar) {
			u16 const start = ar == body_reg ? 0x1400 : form >= 3 && ar == reg ? 0x1a : 0x1500 + ar;
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),start});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == body_reg ? 0x1400 + writes :
				form >= 3 && ar == reg ? unsigned(0x1a + (form == 4 ? 1 : form == 5 ? -1 : 0)) : 0x1500 + ar});
		}
		for (unsigned i = 0; i < writes; ++i) {
			initial.data.push_back({0x1400 + i,0}); final.expected_data.push_back({0x1400 + i,0x2222});
		}
		final.expected_program = {{physical,u16(value ^ 0xffff)}};
		unsigned const checkpoint = std::array<unsigned,4>{6,9,11,16}[(form + reg + loop + pattern + gap + address_mode) % 4];
		clocked_register_case c{{{0,std::move(initial)},{96,std::move(final)}},checkpoint,96};
		c.extended_program = extended;
		if (form == 2) {
			c.actions.push_back({5,{},false,{{0x601,0x1b}}});
			c.program_events.push_back({false,0x601,0x1a,0,false});
		}
		c.actions.push_back({7,{{r::STATE_A,0x3400030480ULL}}});
		c.actions.push_back({read,{},false,{{physical,u16(value ^ 0xffff)}}});
		c.program_events.push_back({false,physical,value,read});
		c.data_events = std::vector<clocked_data_event>{{true,0x1a,value,read + 1}};
		for (unsigned i = 0; i < writes; ++i) c.data_events->push_back({true,0x1400 + i,0x2222,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
