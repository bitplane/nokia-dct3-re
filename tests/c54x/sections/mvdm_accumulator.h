// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0,1,0x7fffffff,0x80000000,0x7fffffffffULL,0x8000000000ULL,0xffffffffffULL,0x123456789aULL};
	constexpr u16 words[]{0,1,0x7f,0x80,0xff,0x7fff,0x8000,0xffff};
	for (unsigned lane = 0; lane < 6; ++lane) for (unsigned source_kind = 0; source_kind < 3; ++source_kind)
	for (unsigned pattern = 0; pattern < 8; ++pattern) for (unsigned word_index = 0; word_index < 8; ++word_index) {
		unsigned const seed = lane + source_kind + pattern + word_index, flags = (pattern * 3 + word_index + lane + source_kind) % 16;
		u16 const source = std::array<u16,3>{0x500,0x14,0x19}[source_kind], word = words[word_index];
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 4 ? 0x4000 : 0) | (flags & 8 ? 0x40 : 0);
		u64 const a = values[pattern], b = a ^ 0x5aa55aa55aULL;
		unsigned const shift = (lane % 3) * 16;
		u64 const mask = (lane % 3 == 2 ? 0xffULL : 0xffffULL) << shift;
		u64 const merged = ((lane < 3 ? a : b) & ~mask) | ((u64(word) << shift) & mask);
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("MVDM-accumulator-slice", {u16(0x7208 + lane),source,u16(lane < 3 ? 0xf040 : 0xf340),0,0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{source,word}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_T,0xbeef},{r::STATE_SP,0x3000},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1}});
		auto before = observation("before-slice-publication", {{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1}});
		auto after = observation("merged-before-immediate-consumer", {{r::STATE_A,lane < 3 ? merged : a},{r::STATE_B,lane >= 3 ? merged : b},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_T,0xbeef},{r::STATE_SP,0x3000}});
		auto final = after; final.expected.push_back({STATE_GENPC,base + 0x605});
		clocked_register_case c{{{0,std::move(initial)},{5,std::move(before)},{6,std::move(after)},{12,std::move(final)}},
			std::array<unsigned,3>{5,6,9}[seed % 3],12};
		c.actions = {{4,{},false,{{base + 0x601,0xdead}}},{6,{},false,{},{},{{source,u16(word ^ 0xffff)}}}};
		c.program_events = {{false,base + 0x601,source,0,false}};
		c.data_events = std::vector<clocked_data_event>{{false,source,word,6},{true,8 + lane,word,6}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
