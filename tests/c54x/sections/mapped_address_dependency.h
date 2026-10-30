// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 128; ++index) {
		unsigned const reg = index % 8, pattern = (index / 8) % 4, gap = 2 + index / 64;
		bool const extended = (index / 32) & 1;
		u32 const base = (reg + pattern + gap) & 1 ? 0x20000 : 0;
		u16 const before = std::array<u16,4>{0x1200,0x7ff0,0xfff0,0x8000}[pattern];
		u16 const immediate = std::array<u16,4>{0x800,0x20,0x1040,0x9800}[pattern], after = before + immediate;
		auto initial = initial_case("mapped-address-dependency", {0x6bf8,u16(0x10 + reg),immediate});
		for (auto &word : initial.program) word.address += base;
		for (auto &word : initial.program) {
			if (word.address == base + 0x603 + gap) word.value = 0x1000 | (extended ? 0xe0 : 0x80) | reg;
			if (extended && word.address == base + 0x604 + gap) word.value = 0;
			if (word.address == base + 0x604 + gap + unsigned(extended)) word.value = 0xf4e1;
		}
		initial.data = {{before,0x5678},{after,u16(0x4000 + pattern)}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa024},{r::STATE_ST1,0x900}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({int(r::STATE_AR0 + ar),ar == reg ? before : 0x3000 + ar});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{6,8,10}[(reg + pattern + gap) % 3],32};
		auto final = observation("published-address-consumed", {{r::STATE_A,0x4000 + pattern},
			{STATE_GENPC,base + 0x605 + gap + unsigned(extended)}});
		for (unsigned ar = 0; ar < 8; ++ar) final.expected.push_back({int(r::STATE_AR0 + ar),ar == reg ? after : 0x3000 + ar});
		c.states.push_back({32,std::move(final)});
		c.data_events = std::vector<clocked_data_event>{{false,0x10 + reg,before,0,false},
			{true,0x10 + reg,after,0,false},{false,after,u16(0x4000 + pattern),0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
