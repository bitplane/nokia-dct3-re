// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned lane = 0; lane < 6; ++lane) for (u16 word : {0U,1U,0x8000U,0xffffU})
	for (unsigned absolute = 0; absolute < 2; ++absolute) {
		unsigned const seed = lane + word + absolute;
		u16 const destination = 8 + lane;
		u32 const source = (seed & 1 ? 0x20000 : 0) | 0x400;
		u64 const initial_a = 0x1200000000ULL | source, live_a = 0x3400030400ULL, b = 0xfeabcd9876ULL;
		unsigned const shift = (lane % 3) * 16, read = absolute ? 10 : 9, write = read + 1;
		u64 const mask = (lane % 3 == 2 ? 0xffULL : 0xffffULL) << shift;
		u64 const merged = ((lane < 3 ? live_a : b) & ~mask) | ((u64(word) << shift) & mask);
		auto initial = initial_case("READA-accumulator-live-merge", {u16(0x7e00 | (absolute ? 0xf8 : destination)),
			absolute ? destination : u16(0xf4e1),0xf4e1});
		initial.program.insert(initial.program.end(), {{source,word},{0x30400,u16(word ^ 0xffff)}});
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,initial_a},{r::STATE_B,b},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x800}});
		auto before = observation("before-held-address-edit", {{r::STATE_A,initial_a},{r::STATE_B,b}});
		auto held = observation("new-live-A-before-retained-publication", {{r::STATE_A,live_a},{r::STATE_B,b}});
		auto after = observation("retained-program-word-merges-live-lane", {{r::STATE_A,lane < 3 ? merged : live_a},
			{r::STATE_B,lane >= 3 ? merged : b},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x800}});
		auto final = after; final.expected.push_back({STATE_GENPC,absolute ? 0x603U : 0x602U});
		clocked_register_case c{{{0,std::move(initial)},{7,std::move(before)},{8,std::move(held)},
			{write,std::move(after)},{18,std::move(final)}},std::array<unsigned,3>{5,8,12}[seed % 3],18};
		c.actions = {{7,{{r::STATE_A,live_a}}}};
		if (absolute) c.program_events.push_back({false,0x601,destination,0,false});
		c.program_events.push_back({false,source,word,read});
		c.data_events = std::vector<clocked_data_event>{{true,destination,word,write}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
