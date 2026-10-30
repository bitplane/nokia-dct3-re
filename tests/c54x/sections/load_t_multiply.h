// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u16 t : {0U,1U,0xffffU,0x8000U,0x7fffU,0x1234U,0xfff0U,0x8001U})
	for (u16 memory : {1U,0xffffU,0x8000U,0x7fffU}) for (unsigned b = 0; b < 2; ++b) for (unsigned flags = 0; flags < 4; ++flags) {
		s64 const product = s64(s16(t)) * s16(memory) * (flags & 1 ? 2 : 1);
		bool const overflow = product < -2147483648LL || product > 2147483647LL;
		u64 const result = u64(overflow && (flags & 2) ? std::clamp<s64>(product,-2147483648LL,2147483647LL) : product) & 0xffffffffffULL;
		u16 const status = 0x1800 | (overflow ? b ? 0x200 : 0x400 : 0), st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0);
		unsigned const seed = t + memory + b + flags;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("LD-T-to-adjacent-offset-MPY", {0x30e1,4,u16(0x20e1 | (b << 8)),3,0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{0x500b,t},{0x500a,memory}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},
			{r::STATE_AR1,0x5007},{r::STATE_T,0x9abc},{r::STATE_ST0,0x1800},{r::STATE_ST1,st1}});
		auto state = [&](char const *name, unsigned tick) {
			return observation(name, {{r::STATE_A,!b && tick >= 9 ? result : 0x12345678},
				{r::STATE_B,b && tick >= 9 ? result : 0x87654321},{r::STATE_T,tick < 6 ? 0x9abcU : tick < 9 ? t : 0xbeefU},
				{r::STATE_ST0,tick >= 9 ? status : 0x1800U},{r::STATE_ST1,st1}});
		};
		auto final = state("captured-T-and-memory-product",18); final.expected.push_back({STATE_GENPC,base + 0x605});
		clocked_register_case c{{{0,std::move(initial)},{1,state("before-producer-Read",1)},
			{6,state("T-published-at-Read",6)},{9,state("MPY-Execute",9)},{18,std::move(final)}},
			std::array<unsigned,3>{4,6,8}[seed % 3],18};
		c.actions = {{6,{},false,{},{},{{0x500b,0xdead}}},{8,{{r::STATE_T,0xbeef}},false,{},{},{{0x500a,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{{false,0x500b,t,6},{false,0x500a,memory,8}};
		c.program_events = {{false,base + 0x601,4,0,false},{false,base + 0x603,3,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
