// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](unsigned kind,unsigned reg,unsigned cmpt,u16 offset,u16 value,u16 start,u16 address,u16 bk,unsigned seed) {
		unsigned const selected = cmpt && reg == 0 ? 3 : reg;
		u32 const base = seed % 2 ? 0x20000 : 0;
		u16 const st0 = cmpt ? 0x6608 : 0x608, status = cmpt && reg ? (st0 & 0x1fff) | (reg << 13) : st0;
		auto initial = initial_case("long-ST-retains-offset-value-and-write-address", {u16(0x76e0 | (kind << 3) | reg),offset,value,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.data = {{address,0xaaaa}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},
			{r::STATE_ST1,0x800 | (cmpt << 5)},{r::STATE_BK,bk}});
		auto access = observation("pointer-update-precedes-later-register-poison", {{r::STATE_ST0,status}});
		auto final = observation("only-original-literals-and-address-publish", {{STATE_GENPC,base + 0x604},
			{r::STATE_ST0,(status & ~u16(0x1ff)) | 9U},{r::STATE_ST1,0x4800 | (cmpt << 5)},{r::STATE_BK,32}},{{address,value}});
		for (unsigned ar = 0; ar != 8; ++ar) {
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),ar == selected ? start : 0x5555U});
			access.expected.push_back({int(r::STATE_AR0) + int(ar),ar == selected ? kind ? address : start : 0x5555U});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == selected ? 0x2000U : 0x5555U});
		}
		clocked_register_case c{{{0,std::move(initial)},{7,std::move(access)},
			{8,observation("captured-address-survives-register-poison", {})},{20,std::move(final)}},std::array<unsigned,4>{4,5,7,8}[seed % 4],20};
		c.actions = {{4,{},false,{{base + 0x601,0xdead}}},
			{5,{{r::STATE_ST1,0x4800 | (cmpt << 5)}},false,{{base + 0x602,0xbeef}}},
			{7,{{int(r::STATE_AR0) + int(selected),0x2000},{r::STATE_BK,32},{r::STATE_ST0,(status & ~u16(0x1ff)) | 9U}}}};
		c.program_events = {{false,base + 0x601,offset,3},{false,base + 0x602,value,4}};
		c.data_events = std::vector<clocked_data_event>{{true,address,value,address >= 0x20 && address < 0x60 ? 9U : 8U}};
		cases.push_back(std::move(c));
	};
	for (unsigned kind = 0; kind != 3; ++kind)
	for (unsigned reg = 0; reg != 8; ++reg)
	for (unsigned cmpt = 0; cmpt != 2; ++cmpt)
	for (u16 offset : {0,1,0xffff,0xf495})
	for (u16 value : {0,1,0x8000,0xf495}) {
		u16 const start = kind == 2 ? 0x4007 : 0x1007;
		int const position = (7 + s16(offset)) % 8192;
		u16 const address = kind == 2 ? 0x4000 + (position < 0 ? position + 8192 : position) : u16(start + s16(offset));
		append(kind,reg,cmpt,offset,value,start,address,kind == 2 ? 8192 : 16,kind + reg + cmpt + offset + value);
	}
	for (unsigned kind = 0; kind != 2; ++kind)
	for (unsigned reg = 0; reg != 8; ++reg)
	for (unsigned cmpt = 0; cmpt != 2; ++cmpt)
	for (u16 offset : {0x3b,0x38,0x49})
	for (u16 value : {0,1,0x8000,0xffff})
	for (u16 start : {0,0x1400})
		append(kind,reg,cmpt,offset,value,start,start + offset,16,kind + reg + cmpt + offset + value + start);
	return cases;
}
}
