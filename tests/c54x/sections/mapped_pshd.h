// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 sources[]{0,1,6,7,8,9,10,11,12,13,14,15};
	constexpr u16 patterns[]{0,1,0x8000,0xffff};
	for (unsigned i = 0; i != 12; ++i)
	for (unsigned p = 0; p != 4; ++p) {
		u16 const source = sources[i], value = patterns[p];
		u16 const expected = source <= 1 ? 0 : source == 6 ? 0xa600 : source == 7 ? 0x800 :
			(source == 10 || source == 13) ? value & 0xff : value;
		u32 const base = (i + p) % 2 ? 0x20000 : 0;
		unsigned const checkpoint = 4 + (i + p) % 3;
		auto initial = initial_case("mapped-PSHD-retained-register-word", {0xf495});
		initial.program = {{base + 0x600,0x4b80},{base + 0x601,0xf4e1}};
		initial.data = {{0xfff,0xbeef}};
		u64 const a = source >= 8 && source <= 10 ? u64(expected) << ((source - 8) * 16) : 0;
		u64 const b = source >= 11 && source <= 13 ? u64(expected) << ((source - 11) * 16) : 0;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa600},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,source == 14 ? value : 0U},
			{r::STATE_TRN,source == 15 ? value : 0U},{r::STATE_AR0,source}});
		clocked_register_case c{{{0,std::move(initial)},
			{4,observation("Access-allocates-stack-before-Read", {{r::STATE_SP,0xfff}})},
			{12,observation("Execute-retains-word-after-source-poison", {{STATE_GENPC,base + 0x602},
				{r::STATE_SP,0xfff},{r::STATE_AR0,0x2222}},{{0xfff,expected}})}},checkpoint,12};
		clocked_register_action poison{5,{{r::STATE_AR0,0x2222}}};
		if (source == 1) poison.values.push_back({r::STATE_IFR,0xffff});
		else if (source == 6) poison.values.push_back({r::STATE_ST0,0xa601});
		else if (source == 7) poison.values.push_back({r::STATE_ST1,0x900});
		else poison.data.push_back({source,u16(expected ^ 0xffff)});
		c.actions.push_back(std::move(poison));
		c.data_events = std::vector<clocked_data_event>{{false,source,expected,5},{true,0xfff,expected,6}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
