// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 inputs[]{0x1200,0x0180001200ULL,0xff7fff1200ULL,0xfffffff495ULL};
	constexpr u16 plain[]{0x1200,0x1200,0x1200,0xf495};
	constexpr u16 unsigned_sat[]{0x1200,0xffff,0xffff,0xffff}, signed_sat[]{0x1200,0xffff,0,0xf495};
	// SPRU131G table7-7: two NOPs satisfy the stricter Category-I ARx separation.
	for (unsigned reg = 0; reg < 8; ++reg) for (unsigned source = 0; source < 2; ++source)
	for (unsigned pattern = 0; pattern < 4; ++pattern) for (unsigned flags = 0; flags < 4; ++flags)
	{
		bool const sxm = flags & 1, sst = flags & 2;
		u16 const pointer = sst ? sxm ? signed_sat[pattern] : unsigned_sat[pattern] : plain[pattern];
		u16 const sample = pointer ? 0xbbbb : 0;
		u64 const loaded = sxm && sample ? 0xffffffbbbbULL : sample;
		auto initial = initial_case("stlm-address", {u16(0x8810 | (source << 8) | reg),0xf495,0xf495,
			u16(0x1080 | ((source ^ 1) << 8) | reg),0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x608},{r::STATE_ST1,sxm ? 0x4900 : 0x4800},
			{r::STATE_PMST,unsigned(sst)},{r::STATE_A,source ? 0xdead : inputs[pattern]},
			{r::STATE_B,source ? inputs[pattern] : 0xdead}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,ar == reg ? 0x1000 : 0x4000 + ar});
		initial.data = {{0x1000,0xaaaa}};
		if (pointer) initial.data.push_back({pointer,0xbbbb});
		auto final = observation("consumer", {{r::STATE_A,source ? loaded : inputs[pattern]},
			{r::STATE_B,source ? inputs[pattern] : loaded},{r::STATE_PMST,unsigned(sst)},
			{r::STATE_ST0,0x608},{r::STATE_ST1,sxm ? 0x4900U : 0x4800U}});
		for (unsigned ar = 0; ar < 8; ++ar) final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == reg ? u64(pointer) : 0x4000 + ar});
		clocked_register_case c{{{0,std::move(initial)},
			{6,observation("published", {{int(r::STATE_AR0) + int(reg),pointer}})},
			{24,std::move(final)}},std::array<unsigned,3>{3,5,6}[(reg + source + pattern + flags) % 3],24};
		c.data_events = std::vector<clocked_data_event>{{true,0x10 + reg,pointer,6},{false,pointer,sample,8}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
