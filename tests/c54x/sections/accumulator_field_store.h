// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 144; ++index) {
		unsigned const lane = index % 6, pattern = index / 36;
		u16 const literal = std::array<u16,6>{0,1,0x7fff,0x8000,0xff00,0xffff}[(index / 6) % 6];
		u64 const a = std::array<u64,4>{0,0xffffffffffULL,0x123456789aULL,0x8000000000ULL}[pattern], b = a ^ 0x123456789aULL;
		u64 const live_a = a ^ 0xffffffffffULL, live_b = b ^ 0xffffffffffULL;
		unsigned const shift = 16 * (lane % 3);
		u64 const mask = (lane % 3 == 2 ? 0xffULL : 0xffffULL) << shift;
		u64 final_a = live_a, final_b = live_b;
		u64 &selected = lane < 3 ? final_a : final_b;
		selected = (selected & ~mask) | ((u64(literal) << shift) & mask);
		u16 const observed = lane % 3 == 2 ? u8(literal) : literal;
		u16 const st1 = 0x800 | (((lane + pattern + index / 6) % 4) << 8);
		u32 const base = (lane + pattern) & 1 ? 0x20000 : 0;
		auto initial = initial_case("accumulator-field-store", {u16(0x7708 + lane),literal,u16(0x7108 + lane),0x1200,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_ST0,0xa600},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{4,6,7}[(lane + pattern) % 3],20};
		for (unsigned tick = 1; tick <= 20; ++tick) c.states.push_back({tick,observation("live-sibling-fields-and-guard-mask", {
			{r::STATE_A,tick < 6 ? a : tick == 6 ? live_a : final_a},
			{r::STATE_B,tick < 6 ? b : tick == 6 ? live_b : final_b},{r::STATE_ST0,0xa600},{r::STATE_ST1,st1}})});
		c.actions = {{4,{},false,{{base + 0x601,0xdead}}},{6,{{r::STATE_A,live_a},{r::STATE_B,live_b}},true}};
		c.data_events = std::vector<clocked_data_event>{{true,8 + lane,literal,7},{false,8 + lane,observed,7},
			{true,0x1200,observed,8}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
