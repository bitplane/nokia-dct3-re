// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 inputs[]{0,1,0x1200,0xffff,0x7fffffff,0xff80000000ULL,0x0180001200ULL,0xff7fff1200ULL};
	constexpr u16 plain[]{0,1,0x1200,0xffff,0xffff,0,0x1200,0x1200};
	constexpr u16 unsigned_sat[]{0,1,0x1200,0xffff,0xffff,0xffff,0xffff,0xffff};
	constexpr u16 signed_sat[]{0,1,0x1200,0xffff,0xffff,0,0xffff,0};
	for (unsigned source = 0; source < 2; ++source)
	for (unsigned pattern = 0; pattern < 8; ++pattern)
	for (unsigned flags = 0; flags < 4; ++flags)
	{
		bool const sxm = flags & 1, sst = flags & 2;
		u16 const value = sst ? sxm ? signed_sat[pattern] : unsigned_sat[pattern] : plain[pattern];
		u16 const st0 = 0x1804 | (pattern & 1 ? 0x600 : 0);
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (pattern & 2 ? 0x200 : 0) |
			(pattern & 4 ? 0x4040 : 0);
		auto initial = initial_case("stlm-peripheral", {u16(0x8821 | (source << 8))});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_PMST,unsigned(sst)},{r::STATE_A,source ? 0xdead : 0x1111},
			{r::STATE_B,source ? 0x1111 : 0xdead}});
		initial.data = {{0x21,0xa55a}};
		auto final = observation("published", {{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_PMST,unsigned(sst)},{r::STATE_A,source ? 0xdead : inputs[pattern]},
			{r::STATE_B,source ? inputs[pattern] : 0xdead}}, {{0x21,value}});
		clocked_register_case c{{{0,std::move(initial)},{12,std::move(final)}},
			std::array<unsigned,3>{5,6,8}[(source + pattern + flags) % 3],12};
		c.require_idle = false;
		c.actions = {{5,{{source ? r::STATE_B : r::STATE_A,inputs[pattern]}}}};
		// Model ordering: this destination adds one clock; no silicon phase claim.
		c.data_events = std::vector<clocked_data_event>{{true,0x21,value,7}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
