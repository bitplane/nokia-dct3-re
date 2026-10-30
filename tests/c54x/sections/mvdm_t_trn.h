// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned destination = 0xe; destination <= 0xf; ++destination) for (unsigned source_kind = 0; source_kind < 3; ++source_kind)
	for (unsigned pattern = 0; pattern < 8; ++pattern) for (unsigned gap = 0; gap < 2; ++gap) {
		unsigned const seed = destination + source_kind + pattern + gap, flags = (pattern * 3 + destination + source_kind + gap * 7) % 16;
		u16 const source = std::array<u16,3>{0x500,0x14,0x11}[source_kind];
		u16 const word = std::array<u16,8>{0,1,0x7f,0xff,0x7fff,0x8000,0xfffe,0xffff}[pattern];
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 4 ? 0x4000 : 0) | (flags & 8 ? 0x40 : 0);
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("MVDM-T-TRN-consumer", {u16(0x7200 | destination),source,0xf495,0xf495,0xf4e1});
		initial.program[2 + gap].value = 0x4800 | destination;
		initial.program[3 + gap].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{source,word}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0xbeef},{r::STATE_TRN,0xcafe},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1}});
		auto publication = observation("mapped-destination-published", {{r::STATE_T,destination == 0xe ? unsigned(word) : 0xbeefU},
			{r::STATE_TRN,destination == 0xf ? unsigned(word) : 0xcafeU},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_B,0xfedcba9876ULL}});
		auto final = publication; final.expected.insert(final.expected.end(), {{r::STATE_A,word},{STATE_GENPC,base + 0x604 + gap}});
		clocked_register_case c{{{0,std::move(initial)},{6,std::move(publication)},{12,std::move(final)}},
			std::array<unsigned,4>{3,5,6,8}[seed % 4],12};
		c.clear_pipeline_before_restore = true;
		c.actions = {{4,{},false,{{base + 0x601,0xdead}}},{6,{},false,{},{},{{source,0xdead}}}};
		c.program_events = {{false,base + 0x601,source,0,false}};
		c.data_events = std::vector<clocked_data_event>{{false,source,word,6},{true,destination,word,6},{false,destination,word,7 + gap}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
