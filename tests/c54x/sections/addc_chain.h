// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned selector = 0; selector < 4; ++selector) for (unsigned pattern = 0; pattern < 4; ++pattern)
	for (unsigned pair = 0; pair < 4; ++pair) for (unsigned flags = 0; flags < 8; ++flags) {
		u64 const a = std::array<u64,4>{0xffffffffULL,0,0x7fffffffULL,0xff80000000ULL}[pattern];
		u64 const b = std::array<u64,4>{0x123456789aULL,0xffffffffULL,0x7fffffffULL,0x8000000000ULL}[pattern];
		auto const words = std::array<std::array<u16,2>,4>{{{1,0},{0xffff,1},{0x8000,0xffff},{0,0x8000}}}[pair];
		u16 const st0 = 0x1020 | (flags & 1 ? 0x800 : 0), st1 = 0x800 | (flags & 2 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0);
		std::array<u64,2> results{a,b}; u16 status = st0;
		for (unsigned i = 0; i < 2; ++i) {
			unsigned const destination = (selector >> i) & 1;
			u64 const raw = results[destination], increment = words[i] + ((status >> 11) & 1);
			s64 const sum = (raw & 0x8000000000ULL ? s64(raw) - 0x10000000000LL : s64(raw)) + s64(increment);
			bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
			status = (status & ~0x800) | ((raw & 0xffffffffULL) + increment >= 0x100000000ULL ? 0x800 : 0) |
				(overflow ? destination ? 0x200 : 0x400 : 0);
			results[destination] = u64(overflow && (st1 & 0x200) ? std::clamp<s64>(sum,-2147483648LL,2147483647LL) : sum) & 0xffffffffffULL;
		}
		unsigned const seed = selector + pattern + pair + flags;
		u32 const base = seed & 1 ? 0x20000 : 0;
		u16 const first = 0x0602 | ((selector & 1) << 8), second = 0x0603 | ((selector & 2) << 7);
		auto initial = initial_case("adjacent-ADDC-carry-chain", {first,second,0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{0x1002,words[0]},{0x1003,words[1]}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_T,0xbeef},{r::STATE_SP,0x3000}});
		auto final = observation("Execute-consumes-generated-carry", {{STATE_GENPC,base + 0x603},{r::STATE_A,results[0]},
			{r::STATE_B,results[1]},{r::STATE_ST0,status},{r::STATE_ST1,st1},{r::STATE_T,0xbeef},{r::STATE_SP,0x3000}});
		clocked_register_case c{{{0,std::move(initial)},{12,std::move(final)}},std::array<unsigned,4>{3,5,6,7}[seed % 4],12};
		c.actions = {{5,{},false,{},{},{{0x1002,0xa55a}}},{6,{},false,{},{},{{0x1003,0xa55a}}}};
		c.data_events = std::vector<clocked_data_event>{{false,0x1002,words[0],5},{false,0x1003,words[1],6}};
		c.program_events = {{false,base + 0x600,first,0,false},{false,base + 0x601,second,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
