// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 inputs[]{0,1,0xffffffffffULL,0x7fffffff,0xff80000000ULL,
		0x0100000000ULL,0x8000000000ULL,0x7fffffffffULL};
	// GE,LT,NE,EQ,GT,LE truth vectors for zero, positive and signed40 negative.
	constexpr unsigned truth[]{0x29,0x15,0x26,0x15,0x26,0x15,0x26,0x15};
	for (unsigned condition = 0; condition < 6; ++condition)
	for (unsigned source = 0; source < 2; ++source) for (unsigned delayed = 0; delayed < 2; ++delayed)
	for (unsigned pattern = 0; pattern < 8; ++pattern) for (unsigned poison = 0; poison < 2; ++poison)
	{
		bool const taken = truth[pattern] & (1 << condition);
		u64 const changed = inputs[pattern] == 1 ? 0 : 1;
		unsigned const flags = (condition + source + delayed + pattern + poison) % 4;
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0);
		u32 const base = (condition + source + pattern) % 2 ? 0x20000 : 0;
		auto initial = initial_case("signed-condition", {u16(0xf800 | (0x42 + condition) | (source << 3) | (delayed << 9)),
			0x700,u16(source ? 0xe907 : 0xe807),u16(source ? 0xe822 : 0xe922),
			u16(source ? 0xe811 : 0xe911),0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(), {{base + 0x700,u16(source ? 0xe855 : 0xe955)},
			{base + 0x701,0xf4e1}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},
			{r::STATE_ST1,st1},{r::STATE_A,source ? 0 : inputs[pattern]},{r::STATE_B,source ? inputs[pattern] : 0}});
		u64 const compared = delayed || !taken ? 7 : poison ? changed : inputs[pattern];
		u64 const other = taken ? 0x55 : 0x11;
		auto final = observation("selected", {{r::STATE_A,source ? other : compared},{r::STATE_B,source ? compared : other},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)},{30,std::move(final)}},
			std::array<unsigned,4>{4,5,6,8}[(condition + source + delayed + pattern + poison) % 4],30};
		c.actions = {{4,{},false,{{base + 0x601,0xdead}}}};
		if (poison) c.actions.push_back({6,{{source ? r::STATE_B : r::STATE_A,changed}}});
		c.data_events = std::vector<clocked_data_event>{};
		c.program_events = {{false,base + 0x601,0x700,0,false}};
		if (taken) c.program_events.push_back({false,base + 0x700,u16(source ? 0xe855 : 0xe955),delayed ? 8U : 7U});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
