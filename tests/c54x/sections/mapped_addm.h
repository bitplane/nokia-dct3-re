// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned shape = 0; shape < 27; ++shape)
	for (unsigned pair = 0; pair < 16; ++pair)
	for (unsigned flags = 0; flags < 4; ++flags) {
		bool const t = shape == 24 || shape == 25, brc = shape == 26;
		unsigned const reg = shape % 8, form = t ? shape - 24 : brc ? 0 : shape / 8;
		bool const short_form = form != 0, cmpt = (pair + shape + flags) & 1;
		unsigned const pointer = reg == 4 ? 5 : 4;
		u16 const address = t ? 0xe : brc ? 0x1a : 0x10 + reg;
		u16 const before = std::array<u16,4>{0,0x7fff,0x8000,0xffff}[pair % 4];
		u16 const immediate = std::array<u16,4>{1,0x800,0x7fff,0xffff}[pair / 4];
		s32 const left = flags & 1 ? s16(before) : before, right = flags & 1 ? s16(immediate) : immediate, sum = left + right;
		bool const overflow = sum < -32768 || sum > 32767;
		bool const carry = u64(u32(left)) + u32(right) > 0xffffffffULL;
		u16 const result = u16((flags & 2) && overflow ? std::clamp(sum,s32(-32768),s32(32767)) : sum);
		u16 const st0 = (short_form ? 0xa000 : 0xa024) | (pair & 1 ? 0x800 : 0) | (pair & 2 ? 0x400 : 0);
		u16 final_st0 = (st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? 0x400 : 0);
		if (form == 2 && cmpt) final_st0 = (final_st0 & ~0xe000) | (pointer << 13);
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (cmpt ? 0x20 : 0);
		u32 const base = (shape + pair + flags) & 1 ? 0x20000 : 0;
		u16 const opcode = short_form ? 0x6b00 | (form == 2 ? 0x80 | pointer : address) : 0x6bf8;
		auto initial = initial_case("mapped-addm", {opcode,u16(short_form ? immediate : address),u16(short_form ? 0xf4e1 : immediate),0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({int(r::STATE_AR0 + ar),
			!t && !brc && ar == reg ? before : form == 2 && ar == pointer ? address : 0x3000 + ar});
		int const destination = t ? r::STATE_T : brc ? r::STATE_BRC : int(r::STATE_AR0 + reg);
		if (t || brc) initial.initial.push_back({destination,before});
		unsigned const read = short_form ? 6 : 7;
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{read - 1,read,read + 1}[(shape + pair + flags) % 3],24};
		auto final = observation("retained-mapped-arithmetic", {{destination,result},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{STATE_GENPC,base + (short_form ? 0x603 : 0x604)}});
		for (unsigned ar = 0; ar < 8; ++ar) if (t || brc || ar != reg) final.expected.push_back({int(r::STATE_AR0 + ar),form == 2 && ar == pointer ? address : 0x3000 + ar});
		c.states.push_back({24,std::move(final)});
		c.actions = {{read - 1,{},false,{{base + (short_form ? 0x601 : 0x602),0xdead}}},
			{read,{{destination,0x55aa}},false}};
		c.data_events = std::vector<clocked_data_event>{{false,address,before,read},{true,address,result,read + 1}};
		c.program_events = {{false,base + 0x601,u16(short_form ? immediate : address),0,false}};
		if (!short_form) c.program_events.push_back({false,base + 0x602,immediate,0,false});
		c.clear_pipeline_before_restore = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
