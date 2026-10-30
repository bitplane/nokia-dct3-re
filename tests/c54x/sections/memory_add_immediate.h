// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u16 pairs[8][2]{{0,2},{0x7fff,1},{0x8007,0xfff8},{0xffff,1},
		{0x8000,0x8000},{0x7fff,0x7fff},{0xffff,0xffff},{0x1234,0xffff}};
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 256; ++index) {
		unsigned const flags = index % 8, pair = (index / 8) % 8, form = index / 64;
		bool const absolute = form == 3;
		u16 const before = pairs[pair][0], immediate = pairs[pair][1];
		s32 const left = flags & 1 ? s16(before) : s32(before), right = flags & 1 ? s16(immediate) : s32(immediate), sum = left + right;
		bool const overflow = sum < -32768 || sum > 32767;
		u16 const result = u16(overflow && (flags & 2) ? std::clamp(sum,s32(-32768),s32(32767)) : sum);
		u16 const st0 = 0xaa08 | (flags & 4 ? 0x400 : 0), st1 = 0x800 | ((flags & 1) << 8) | ((flags & 2) << 8);
		u16 const final_st0 = (st0 & ~0x800) | (u64(u32(left)) + u32(right) > 0xffffffffULL ? 0x800 : 0) | (overflow ? 0x400 : 0);
		auto initial = initial_case("memory-add-immediate", {std::array<u16,4>{0x6b20,0x6b82,0x6b92,0x6bf8}[form],
			u16(absolute ? 0x420 : immediate),u16(absolute ? immediate : 0xf495)});
		initial.data = {{0x420,before}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_AR2,0x420},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
		unsigned const read = 6 + unsigned(absolute);
		clocked_register_case c{{{0,std::move(initial)}},read - 1 + (pair + flags + form) % 3,10};
		for (unsigned tick = 1; tick <= 10; ++tick) c.states.push_back({tick,observation("retained-read-and-arithmetic-flags", {
			{r::STATE_ST0,tick > read ? final_st0 : st0},{r::STATE_ST1,st1},
			{r::STATE_AR2,form == 2 && tick >= 5 ? 0x421U : 0x420U},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}})});
		c.actions = {{read,{},false,{}, {},{{0x420,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{{false,0x420,before,read},{true,0x420,result,read + 1}};
		c.require_idle = false; c.check_pipeline_clock = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
