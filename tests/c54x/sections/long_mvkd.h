// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form != 25; ++form) for (unsigned offset_index = 0; offset_index != 4; ++offset_index)
		for (unsigned cmpt = 0; cmpt != 2; ++cmpt) for (unsigned kind = 0; kind != 4; ++kind)
			for (unsigned pattern = 0; pattern != 4; ++pattern) {
				unsigned const mode = form == 24 ? 3 : form/8, reg = form == 24 ? 0 : form%8, selected = cmpt && !reg ? 3 : reg;
				if ((!kind && pattern) || (kind == 1 && selected == 2)) continue;
				s16 const offset = std::array<s16,4>{0,1,-1,s16(0xf495)}[offset_index];
				u16 const pointer = mode == 2 ? 0x4007 : 0x1007, bk = mode == 2 ? 8192 : 16;
				u16 const address = mode == 3 ? u16(0x1500+offset) : mode == 2 ? 0x4000 | ((7+offset) & 0x1fff) : u16(pointer+offset);
				u16 const next = mode == 1 || mode == 2 ? address : pointer;
				u16 const source = std::array<u16,4>{u16(0x10+selected),0x12,0x1400,address}[kind];
				u16 const value = !kind ? next : std::array<u16,4>{0,1,0x8000,0xf495}[pattern];
				u16 const literal = mode == 3 ? address : u16(offset), opcode = 0x7000 | (mode == 3 ? 0xf8 : 0xe0 | (mode << 3) | reg);
				u16 const st0 = cmpt ? 0x6608 : 0x608, st1 = cmpt ? 0x820 : 0x800;
				u16 const final_st0 = cmpt && reg && mode != 3 ? (st0 & 0x1fff) | (reg << 13) : st0;
				u32 const base = cases.size()%2 ? 0x20000 : 0;
				auto initial = initial_case("long-MVKD-captured-source-and-destination",{opcode,literal,source,0xf4e1});
				for (auto &word : initial.program) word.address += base;
				initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
					{r::STATE_SP,0x1000},{r::STATE_BK,bk}});
				for (unsigned ar = 0; ar != 8; ++ar) initial.initial.push_back({r::STATE_AR0+ar,ar == selected ? pointer : kind == 1 && ar == 2 ? value : 0x1007U});
				initial.data = {{address,0xaaaa}};
				if (kind >= 2) initial.data.push_back({source,value});
				auto final = observation("read-word-survives-source-and-pointer-poison",{{int(r::STATE_AR0+selected),0x2000},
					{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_BK,bk}},{{address,value}});
				if (kind == 1) final.expected.push_back({r::STATE_AR2,0xdead});
				if (kind == 2) final.expected_data.push_back({source,0xdead});
				clocked_register_case c{{{0,std::move(initial)},{1,observation("long-MVKD-running",{})},
					{6,observation("destination-update-before-source-read",{{int(r::STATE_AR0+selected),next}})},
					{7,observation("source-captured",{})},{8,std::move(final)}},std::array<unsigned,3>{4,7,8}[cases.size()%3],16};
				c.actions = {{5,{},false,{{base+0x601,0xdead},{base+0x602,0xbeef}}},
					{7,{{r::STATE_AR0+selected,0x2000}}}};
				// For selected-AR sources, the pointer assignment poisons both roles.
				if (kind) c.actions.back().data.push_back({source,0xdead});
				c.retained_program_reads = {{base+0x601,literal},{base+0x602,source}};
				c.data_events = std::vector<clocked_data_event>{{false,source,value,7},{true,address,value,8}};
				cases.push_back(std::move(c));
			}
	return cases;
}
}
