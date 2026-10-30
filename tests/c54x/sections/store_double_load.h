// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 192; ++index) {
		unsigned const reg = index % 8, pattern = (index / 8) % 4, gap = (index / 32) % 3;
		bool const shifted = index / 96;
		u32 const base = (reg + pattern + gap) & 1 ? 0x20000 : 0;
		u64 const source = std::array<u64,4>{0x11223344,0xffffff6b76ULL,0x7fff8000,0xff80000000ULL}[pattern];
		u32 const loaded = std::array<u32,4>{0x1234abcd,0x80000000,0xffff0001,0x7fff8000}[pattern];
		u64 const expected = u64(s64(s32(loaded))) & 0xffffffffffULL;
		auto initial = initial_case("store-double-load", {0xf495});
		for (auto &word : initial.program) word.address += base;
		unsigned cursor = 0;
		auto put = [&](u16 word) { initial.program[cursor++].value = word; };
		if (shifted) { put(0x6f90 | reg); put(0xc60); put(0x6f90 | reg); put(0xc80); }
		else { put(0x8290 | reg); put(0x8090 | reg); }
		initial.program[cursor + gap].value = 0x5790 | reg;
		initial.data = {{0x1000,0x5a5a},{0x1001,0x5a5a},{0x1002,u16(loaded >> 16)},{0x1003,u16(loaded)},
			{0x1004,0x5a5a},{0x1005,0x5a5a}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,source},{r::STATE_B,0},
			{r::STATE_ST0,0},{r::STATE_ST1,0x6100}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({int(r::STATE_AR0 + ar),ar == reg ? 0x1000U : 0x2000U});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{4,8,12}[(reg + pattern + gap) % 3],30};
		c.states.push_back({30,observation("ordered-store-and-double-read-addresses", {{int(r::STATE_AR0 + reg),0x1004},
			{r::STATE_A,source},{r::STATE_B,expected}})});
		unsigned const first_write = shifted ? 7 : 6, second_write = shifted ? 9 : 7, read = second_write + gap;
		c.data_events = std::vector<clocked_data_event>{{true,0x1000,u16(source >> 16),first_write,false},
			{true,0x1001,u16(source),second_write,false},{false,0x1002,u16(loaded >> 16),read,false},
			{false,0x1003,u16(loaded),read,false}};
		std::stable_sort(c.data_events->begin(),c.data_events->end(),[](auto const &a,auto const &b) {
			return a.cycle < b.cycle || (a.cycle == b.cycle && !a.write && b.write);
		});
		c.require_idle = false;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
