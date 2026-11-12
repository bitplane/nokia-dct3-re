// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// STM #k,TRN publishes in Execute: a live TRN replacement before then is
// overwritten, and an adjacent CMPS shifts the literal, not old TRN. The
// MVMD readback is one NOP later; same-clock mapped-read order is unasserted.
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 36; ++index) {
		unsigned const variant = index % 3;
		u16 const literal = std::array<u16,6>{0,1,0x7fff,0x8000,0xea20,0xffff}[(index / 3) % 6];
		u16 const old_trn = 0x0f0f ^ u16(index), live_trn = ~old_trn;
		// CMPS stores the larger signed half of A and shifts its decision into TRN.
		u64 const a = variant == 2 ? 0x0056781234ULL : 0x0012345678ULL;
		bool const tc = variant == 1;
		u16 const shifted = u16((literal << 1) | (tc ? 1 : 0));
		u32 const base = index / 18 ? 0x20000 : 0;
		auto initial = initial_case("trn-immediate-store-mvmd", {0x770f,literal,0xf495,0x710f,0x1200,0xf4e1});
		if (variant) initial = initial_case("trn-immediate-store-cmps", {0x770f,literal,0x8e82,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_TRN,old_trn},
			{r::STATE_AR2,0x1200},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x800}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{4,6,7}[index % 3],20};
		for (unsigned tick = 1; tick <= 20; ++tick) {
			u16 const trn = tick < 6 ? old_trn : tick == 6 ? live_trn : tick == 7 || !variant ? literal : shifted;
			u16 const st0 = variant && tick >= 8 && tc ? 0xb600 : 0xa600;
			c.states.push_back({tick,observation("trn-publishes-in-Execute",{{r::STATE_TRN,trn},{r::STATE_ST0,st0},{r::STATE_A,a}})});
		}
		c.actions = {{4,{},false,{{base + 0x601,0xdead}}},{6,{{r::STATE_TRN,live_trn}},true}};
		c.data_events = std::vector<clocked_data_event>{{true,0x0f,literal,7}};
		if (variant) c.data_events->push_back({true,0x1200,0x5678,8});
		else c.data_events->insert(c.data_events->end(), {{false,0x0f,literal,8},{true,0x1200,literal,9}});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
