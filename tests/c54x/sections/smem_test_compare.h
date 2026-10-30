// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include "tms320c54x_test_smem_address.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<std::array<u16,2>,8> masks{{{0xffff,0},{1,1},{0x80,0x7f},{0x7f,0x7f},
		{0x8000,0x8000},{0x8000,0x7fff},{0xffff,0xf495},{0,0xffff}}};
	constexpr std::array<std::array<u16,2>,8> comparisons{{{0,0},{1,1},{0x80,0x7f},{0x7f,0x7f},
		{0x8000,0x8000},{0x8000,0x7fff},{0xffff,0xf495},{0xffff,0xffff}}};
	for (unsigned compare = 0; compare < 2; ++compare) for (unsigned form = 0; form < 18; ++form) {
		if (form == 5) continue; // Short MOD=3 is write-only, not a readable Smem form.
		for (unsigned reg = 0; reg < 8; ++reg) for (unsigned cmpt = 0; cmpt < 2; ++cmpt)
		for (unsigned pattern = 0; pattern < 8; ++pattern) {
			unsigned const seed = compare + form + reg + cmpt + pattern, flags = seed % 4;
			auto const f = smem_address(form,reg,cmpt,pattern,flags & 1);
			unsigned const selected = f.selected, read = f.read + 1;
			u16 const mode = f.mode, bk = f.bk, address = f.address, next = f.next;
			auto const pair = compare ? comparisons[pattern] : masks[pattern];
			bool const result = compare ? pair[0] == pair[1] : (pair[0] & pair[1]) != 0;
			bool const initial_tc = seed & 1;
			u16 const st0 = (0x804 | (cmpt ? 0x6000 : 0) | (pattern & 1 ? 0x600 : 0)) | (initial_tc ? 0x1000 : 0);
			u16 const addressed_st0 = cmpt && reg && form >= 2 && form != 17 ? (st0 & 0x1fff) | (reg << 13) : st0;
			u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) |
				(cmpt ? 0x20 : 0) | (form == 1 ? 0x4000 : 0);
			u16 const literal = f.literal;
			u32 const base = reg == 6 ? 0x20000 : 0;
			auto initial = initial_case(compare ? "Smem-compare" : "Smem-mask-test",
				{u16((compare ? 0x6000 : 0x6100) | mode),form >= 14 ? literal : pair[1],
				u16(form >= 14 ? pair[1] : 0xf4e1),0xf4e1});
			for (auto &word : initial.program) word.address += base;
			initial.data = {{address,pair[0]}};
			initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},
				{r::STATE_ST1,st1},{r::STATE_BK,bk},{r::STATE_SP,0x1000},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
			for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,0x5007});
			clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{4,read - 1,read}[seed % 3],18};
			for (unsigned tick = 1; tick <= 18; ++tick) {
				u16 status = tick >= read - 1 ? addressed_st0 : st0;
				if (tick > read) status = (status & ~0x1000) | (result ? 0x1000 : 0);
				auto state = observation("address-source-and-TC-publication", {{r::STATE_ST0,status},{r::STATE_ST1,st1},
					{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
				for (unsigned ar = 0; ar < 8; ++ar) state.expected.push_back({int(r::STATE_AR0 + ar),ar != selected || tick < read - 1 ?
					0x5007U : tick == read - 1 ? unsigned(next) : 0x2222U});
				c.states.push_back({tick,std::move(state)});
			}
			c.actions = {{form >= 14 ? 5U : 4U,{},false,{{base + 0x601,0xdead}}},
				{read - 1,{{int(r::STATE_AR0 + selected),0x2222}}},{read,{},false,{},{},{{address,0xdead}}}};
			if (form >= 14) c.actions[0].program.push_back({base + 0x602,0xdead});
			c.program_events = {{false,base + 0x601,form >= 14 ? literal : pair[1],0,false}};
			if (form >= 14) c.program_events.push_back({false,base + 0x602,pair[1],0,false});
			c.data_events = std::vector<clocked_data_event>{{false,address,pair[0],read}};
			cases.push_back(std::move(c));
		}
	}
	return cases;
}
}
