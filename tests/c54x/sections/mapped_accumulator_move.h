// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned status = 0; status != 2; ++status) for (unsigned store = 0; store != 2; ++store)
		for (unsigned b = 0; b != 2; ++b) for (unsigned pattern = 0; pattern != 4; ++pattern) for (unsigned gap = 0; gap != 3; ++gap) {
			u16 const value = std::array<u16,4>{0,1,0x8000,0xffff}[pattern], source = b ? 0xb : 8;
			u32 const base = (pattern ^ b ^ store ^ status) & 1 ? 0x20000 : 0;
			auto initial = initial_case("mapped-accumulator-producer-to-MVMD-RAM-or-ST1",{u16(store ? b ? 0x800b : 0x8108 : 0x8a00 | source)});
			initial.program[1+gap].value = 0x7300 | source; initial.program[2+gap].value = status ? 7 : 0x1400;
			for (auto &word : initial.program) word.address += base;
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,b && store ? value : 0x123456789aULL},
				{r::STATE_B,!b && store ? value : 0xfedcba9876ULL},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x800},{r::STATE_SP,0x1000}});
			initial.data = {{0x1000,value},{0x1400,0xaaaa}};
			u64 const selected = (b ? 0xfedcba0000ULL : 0x1234560000ULL) | 0xdead;
			u64 const other = store ? value : b ? 0x123456789aULL : 0xfedcba9876ULL;
			auto final = observation("published-low-word-retained-after-debugger-poison",{{r::STATE_A,b ? other : selected},{r::STATE_B,b ? selected : other},
				{r::STATE_SP,store ? 0x1000U : 0x1001U},{r::STATE_ST0,0xa600},{r::STATE_ST1,status ? value & ~0x400U : 0x800U}},
				{{0x1000,value},{0x1400,u16(status ? 0xaaaa : value)},{source,0xdead}});
			if (status) final.expected_data.push_back({7,u16(value & ~0x400U)});
			unsigned const read = 6+gap;
			clocked_register_case c{{{0,std::move(initial)},{1,observation("mapped-move-running",{})},{18,std::move(final)}},
				std::array<unsigned,3>{5,read,read+1}[(b+pattern+gap+store+status)%3],18};
			c.require_idle = false;
			c.data_events = std::vector<clocked_data_event>{};
			if (!store) c.data_events->push_back({false,0x1000,value,5});
			c.data_events->push_back({true,source,value,6});
			c.data_events->push_back({false,source,value,read});
			c.data_events->push_back({true,u32(status ? 7 : 0x1400),value,read+1});
			c.actions = {{read,{},false,{}, {},{{source,0xdead}}}};
			cases.push_back(std::move(c));
		}
	return cases;
}
}
