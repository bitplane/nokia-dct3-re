// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0x7f80002468ULL,0xff7fff1357ULL};
	for (unsigned form = 0; form < 4; ++form)
	for (unsigned mode = 0; mode < 4; ++mode)
	for (unsigned reg = 0; reg < 8; ++reg)
	for (unsigned setup = 0; setup < 2; ++setup)
	for (unsigned flags = 0; flags < 4; ++flags)
	{
		unsigned const seed = form + mode + reg + setup + flags;
		unsigned const count = std::array<unsigned,4>{0,1,7,255}[seed % 4], pattern = (seed / 2) & 1;
		u32 const base = seed & 1 ? 0x20000 : 0;
		u16 const st1 = 0x800 | (flags & 2 ? 0x100 : 0);
		auto initial = initial_case("repeated-store", {u16(setup ? 0xf071 | ((form & 1) << 8) : 0xf070),u16(count),
			u16(0x8080 | (form << 8) | (mode << 3) | reg),0xf4e1});
		for (auto &word : initial.program) word.address += base + 0x8a00;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x9000},{r::STATE_ST0,0xa824},{r::STATE_ST1,st1},
			{r::STATE_PMST,flags & 1},{r::STATE_A,form & 1 ? 0x123456789aULL : values[pattern]},
			{r::STATE_B,form & 1 ? values[pattern] : 0x123456789aULL}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,ar == reg ? 0x1200U : 0x4000 + ar});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{5,10,17}[seed % 3],count + 32};
		auto final = observation("count-plus-one-live-stores", {{r::STATE_RC,0},{r::STATE_ST0,0xa824},{r::STATE_ST1,st1},
			{int(form & 1 ? r::STATE_A : r::STATE_B),0x123456789aULL}});
		for (unsigned ar = 0; ar < 8; ++ar) final.expected.push_back({int(r::STATE_AR0 + ar),ar != reg ? 0x4000 + ar :
			mode == 0 ? 0x1200U : mode == 1 ? 0x1200 - count - 1 : 0x1200 + count + 1});
		c.states.push_back({count + 32,std::move(final)});
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned n = 0; n <= count; ++n) {
			u64 const source = setup && !n ? 0 : values[pattern ^ (n & 1)];
			s64 const signed_source = source & 0x8000000000ULL ? s64(source) - (s64(1) << 40) : s64(source);
			u32 const narrowed = !(flags & 1) ? u32(source) : flags & 2 ?
				u32(std::clamp(signed_source,s64(-2147483648LL),s64(2147483647LL))) : u32(std::min(source,u64(0xffffffff)));
			unsigned const address = mode == 0 ? 0x1200 : mode == 1 ? 0x1200 - n : 0x1200 + n + unsigned(mode == 3);
			c.data_events->push_back({true,address,u16(narrowed >> (form & 2 ? 16 : 0)),8 + n});
			c.actions.push_back({8 + n,{{int(form & 1 ? r::STATE_B : r::STATE_A),values[pattern ^ ((n + 1) & 1)]}}});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
