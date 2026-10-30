// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned encoded = 0; encoded < 8; ++encoded)
	for (unsigned modification = 0; modification < 3; ++modification)
	for (unsigned compatibility = 0; compatibility < 2; ++compatibility)
	for (unsigned two_word = 0; two_word < 2; ++two_word)
	for (u16 value : {0U,1U,0xffffU,0x8000U})
	{
		unsigned const selected = compatibility && !encoded ? 3 : encoded;
		u16 const next = value + (modification == 1 ? -1 : modification == 2 ? 1 : 0);
		u16 const opcode = 0x6e00 | (modification == 0 ? 0x80 : modification == 1 ? 0x88 : 0x90) | encoded;
		u32 const base = (encoded + modification + compatibility + two_word + value) % 2 ? 0x20000 : 0;
		auto initial = initial_case("delayed-count-branch", {opcode,0x700,
			u16(two_word ? 0xf020 : 0xe811),u16(two_word ? 0x33 : 0xe922),0xe866,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(), {{base + 0x700,0xe844},
			{base + 0x701,0xf4e1},{base + 0x740,0xe8aa}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},
			{r::STATE_ST0,compatibility ? 0x6608 : 0x608},
			{r::STATE_ST1,compatibility ? 0x820 : 0x800},{r::STATE_A,0},{r::STATE_B,0x1234}});
		auto modified = observation("modified", {});
		for (unsigned ar = 0; ar < 8; ++ar) {
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),ar == selected ? value : 0x410 + ar * 0x10});
			modified.expected.push_back({int(r::STATE_AR0) + int(ar),ar == selected ? next : 0x410 + ar * 0x10});
		}
		auto final = observation("continued", {{r::STATE_A,value ? 0x44U : 0x66U},
			{r::STATE_B,two_word ? 0x1234U : 0x22U},
			{int(r::STATE_AR0) + int(selected),0x2222},
			{r::STATE_ST0,0x608 | (compatibility ? selected << 13 : 0)},
			{r::STATE_ST1,compatibility ? 0x820U : 0x800U},{r::STATE_SP,0x1000}});
		clocked_register_case c{{{0,std::move(initial)},{5,std::move(modified)},
			{6,observation("poisoned-count", {{int(r::STATE_AR0) + int(selected),0x2222}})},
			{8,observation("first-delay", {{r::STATE_A,two_word ? 0U : 0x11U}})},
			{9,observation("second-delay", {{r::STATE_A,two_word ? 0x33U : 0x11U},
				{r::STATE_B,two_word ? 0x1234U : 0x22U}})},
			{10,std::move(final)}},
			std::array<unsigned,4>{4,5,8,9}[(encoded + modification + compatibility + two_word + value) % 4],22};
		c.actions = {{4,{},false,{{base + 0x601,0x740}}},
			{5,{{int(r::STATE_AR0) + int(selected),0x2222}}}};
		c.data_events = std::vector<clocked_data_event>{};
		c.program_events = {{false,base + 0x601,0x700,0,false}};
		if (value) c.program_events.push_back({false,base + 0x700,0xe844,6});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
