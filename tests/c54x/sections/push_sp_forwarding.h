// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned pushes = 1; pushes <= 2; ++pushes) for (unsigned last : {0x12U,6U,7U,0x18U})
	for (unsigned gap = 0; gap < 3; ++gap) for (unsigned destination = 0; destination < 8; ++destination)
	for (u16 sp : {0x3000U,0U}) {
		unsigned const seed = pushes + last + gap + destination + sp, read = 3 + pushes + gap;
		u16 const copied = sp - pushes;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto source = [&](unsigned n) { return pushes == 2 && !n ? 0x12 : last; };
		auto value = [&](unsigned n) -> u16 { unsigned const address = source(n);
			return address == 0x18 ? u16(sp - n) : address == 6 ? 0xa020 : address == 7 ? 0x800 : 0x5002; };
		auto initial = initial_case("PSHM-to-MVMM-SP-forwarding", {0xf495});
		for (unsigned n = 0; n < pushes; ++n) initial.program[n].value = 0x4a00 | source(n);
		initial.program[pushes + gap].value = 0xe780 | destination;
		initial.program[pushes + gap + 1].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_SP,sp},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0x5aa5},{r::STATE_TRN,0x2468},{r::STATE_ST0,0xa020},{r::STATE_ST1,0x800}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,0x5000 + ar});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{3,read,read + 1}[seed % 3],24};
		c.clear_pipeline_before_restore = true;
		for (unsigned tick = 1; tick <= 24; ++tick) {
			unsigned const allocated = tick < 5 ? 0 : std::min(tick - 4,pushes);
			auto state = observation("Read-owned-allocation-and-forwarded-SP", {{r::STATE_SP,u16(sp - allocated)},
				{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0x5aa5},{r::STATE_TRN,0x2468},
				{r::STATE_ST0,0xa020},{r::STATE_ST1,0x800}});
			for (unsigned ar = 0; ar < 8; ++ar) state.expected.push_back({int(r::STATE_AR0 + ar),ar == destination && tick >= read + 1 ? unsigned(copied) : 0x5000U + ar});
			c.states.push_back({tick,std::move(state)});
		}
		c.states.back().observation.expected.push_back({STATE_GENPC,base + 0x602 + pushes + gap});
		c.actions = {{read,{},false,{{base + 0x600 + pushes + gap,0xffff}}}};
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned n = 0; n < pushes; ++n) c.data_events->push_back({false,source(n),value(n),5 + n});
		for (unsigned n = 0; n < pushes; ++n) c.data_events->push_back({true,u16(sp - 1 - n),value(n),6 + n});
		c.data_events->push_back({false,0x18,u16(copied + unsigned(!gap)),read});
		c.data_events->push_back({true,0x10 + destination,copied,read + 1});
		// At equal clocks: PSHM Read, stack Write, younger Decode SP read, MVMM publication.
		std::stable_sort(c.data_events->begin(),c.data_events->end(),[](auto const &a,auto const &b) { return a.cycle < b.cycle; });
		cases.push_back(std::move(c));
	}
	return cases;
}
}
