// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0,1,0x8000000000ULL,0xffffffffffULL,0x7fffffffffULL,0x0100000000ULL,0x0080000000ULL,0xff80000000ULL};
	for (unsigned condition = 0; condition < 6; ++condition)
	for (unsigned b = 0; b < 2; ++b)
	for (unsigned pattern = 0; pattern < 8; ++pattern)
	for (unsigned layout = 0; layout < 3; ++layout)
	{
		u64 const value = values[pattern];
		s64 const signed_value = value & 0x8000000000ULL ? s64(value) - (s64(1) << 40) : s64(value);
		bool const taken = std::array<bool,6>{signed_value >= 0,signed_value < 0,value != 0,value == 0,signed_value > 0,signed_value <= 0}[condition];
		u64 const opposite = taken ? std::array<u64,6>{0x8000000000ULL,0,0,1,0,1}[condition] :
			std::array<u64,6>{0,0x8000000000ULL,1,0,1,0}[condition];
		bool const multiword = layout == 2;
		u32 const base = (condition + b + pattern + layout) & 1 ? 0x20000 : 0;
		auto initial = initial_case("signed-xc", {u16(0xfd42 + condition + (b ? 8 : 0) + (layout ? 0x200 : 0)),
			u16(multiword ? 0x7600 : 0x8c00),u16(multiword ? 0xfd43 : 0x8c01),u16(multiword ? 0x8c01 : 0xf4e1),0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0x060e},
			{r::STATE_T,0x5aa5},{r::STATE_A,b ? 0x1234 : opposite},{r::STATE_B,b ? opposite : 0x3456}});
		clocked_register_case c{{{0,std::move(initial)}},
			std::array<unsigned,4>{3,4,5,8}[(condition + b + pattern + layout) % 4],16};
		c.states.push_back({1,observation("preserved-modes", {{r::STATE_ST0,0x060e},{r::STATE_ST1,0x800},{r::STATE_T,0x5aa5}})});
		auto final = observation("retained-signed-decision", {{r::STATE_A,b ? 0x1234 : opposite},{r::STATE_B,b ? opposite : 0x3456},
			{r::STATE_ST0,0x060e},{r::STATE_ST1,0x800},{r::STATE_T,0x5aa5}});
		c.states.push_back({16,std::move(final)});
		c.actions = {{3,{{int(b ? r::STATE_B : r::STATE_A),value}}},{4,{{int(b ? r::STATE_B : r::STATE_A),opposite}}}};
		c.program_events = {{false,base + 0x601,u16(multiword ? 0x7600 : 0x8c00),0,false},
			{false,base + 0x602,u16(multiword ? 0xfd43 : 0x8c01),0,false}};
		c.data_events = std::vector<clocked_data_event>{};
		if (taken) c.data_events->push_back({true,0x700,u16(multiword ? 0xfd43 : 0x5aa5),7 + unsigned(multiword)});
		if (taken || layout != 1) c.data_events->push_back({true,0x701,0x5aa5,8 + unsigned(multiword)});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
