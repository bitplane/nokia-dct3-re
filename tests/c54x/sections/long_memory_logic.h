// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](unsigned operation, bool b, u16 opcode, u16 literal, u16 address, u16 word,
			u64 source, u16 st0, u16 final_st0, u16 st1, u16 bk, unsigned selected, u16 next, unsigned seed, bool absolute) {
		u32 const base = seed & 1 ? 0x20000 : 0;
		u64 const result = operation == 0 ? source & word : operation == 1 ? source | word : source ^ word;
		auto initial = initial_case("retained-long-memory-logic", {opcode,literal,0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{address,word}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,b ? 0x2233445566ULL : source},
			{r::STATE_B,b ? source : 0x2233445566ULL},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_BK,bk},
			{r::STATE_SP,0x3000},{r::STATE_T,0xdead},{r::STATE_RTN,0x5555}});
		for (unsigned i = 0; i < 8; ++i) initial.initial.push_back({int(r::STATE_AR0) + int(i),absolute ? 0x2222U : 0x100c + i * 0x100});
		auto state = [&](char const *name, bool modified, bool poisoned, bool published) {
			auto check = observation(name, {{r::STATE_A,b ? 0x2233445566ULL : published ? result : source},
				{r::STATE_B,b ? (published ? result : source) : 0x2233445566ULL},{r::STATE_ST0,modified ? final_st0 : st0},
				{r::STATE_ST1,st1},{r::STATE_BK,bk},{r::STATE_SP,0x3000},{r::STATE_T,0xdead},{r::STATE_RTN,0x5555}});
			for (unsigned i = 0; i < 8; ++i) check.expected.push_back({int(r::STATE_AR0) + int(i),
				absolute ? 0x2222U : poisoned ? 0x3330 + i : modified && i == selected ? next : 0x100c + i * 0x100});
			return check;
		};
		auto final = state("retained-address-word-and-result",true,!absolute,true);
		final.expected.push_back({STATE_GENPC,base + 0x603});
		clocked_register_case c{{{0,std::move(initial)},{1,state("before-Access",false,false,false)},
			{5,state("one-Access-update",true,false,false)},{6,state("sample-before-Execute",true,!absolute,false)},
			{7,state("Execute-publication",true,!absolute,true)},{12,std::move(final)}},
			std::array<unsigned,4>{3,absolute ? 4U : 5U,6,7}[seed % 4],12};
		c.actions = {{3,{},false,{{base + 0x601,0xf495}}}};
		if (!absolute) {
			std::vector<register_setting> poison;
			for (unsigned i = 0; i < 8; ++i) poison.push_back({int(r::STATE_AR0) + int(i),0x3330 + i});
			c.actions.push_back({5,std::move(poison)});
		}
		c.actions.push_back({6,{},false,{},{},{{address,0xdead}}});
		c.program_events = {{false,base + 0x601,literal,3}};
		c.data_events = std::vector<clocked_data_event>{{false,address,word,6}};
		cases.push_back(std::move(c));
	};
	for (unsigned operation = 0; operation < 3; ++operation) for (unsigned b = 0; b < 2; ++b)
	for (unsigned mode = 0; mode < 24; ++mode) for (unsigned offset = 0; offset < 4; ++offset)
	for (unsigned pair = 0; pair < 4; ++pair) for (unsigned cmpt = 0; cmpt < 2; ++cmpt) for (unsigned linear = 0; linear < 2; ++linear) {
		unsigned const kind = mode / 8, encoded = mode % 8, selected = cmpt && !encoded ? 3 : encoded;
		unsigned const seed = operation + b + mode + offset + pair + cmpt + linear, flags = seed % 4;
		int const step = offset == 3 && kind == 2 && !linear ? 7 : std::array<int,4>{0,1,-1,0x1462}[offset];
		u16 const pointer = 0x100c + selected * 0x100;
		int const remainder = (12 + step) % 13;
		u16 const address = kind == 2 && !linear ? (pointer & 0xfff0) | (remainder < 0 ? remainder + 13 : remainder) : u16(pointer + step);
		u16 const st0 = 0x6608 | (pair & 1 ? 0x1800 : 0);
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (cmpt ? 0x20 : 0) | (pair & 2 ? 0x80 : 0);
		append(operation,b,0x1800 + operation * 0x200 + b * 0x100 + 0xe0 + mode,u16(step),address,
			std::array<u16,4>{0,1,0x8000,0xffff}[pair],std::array<u64,4>{0,0xffff,0x123456789aULL,0xff80000000ULL}[pair],
			st0,cmpt && encoded ? (st0 & 0x1fff) | (encoded << 13) : st0,st1,linear ? 0 : 13,selected,kind ? address : pointer,seed,false);
	}
	for (unsigned operation = 0; operation < 3; ++operation) for (unsigned b = 0; b < 2; ++b)
	for (u16 address : {0x62U,0x1001U,0xb582U,0xffffU}) for (unsigned pair = 0; pair < 4; ++pair) for (unsigned sxm = 0; sxm < 2; ++sxm) {
		append(operation,b,0x18f8 + operation * 0x200 + b * 0x100,address,address,
			std::array<u16,4>{0xffff,0x8000,0xff,0x8001}[pair],std::array<u64,4>{0,0xffff,0x123456789aULL,0xff80000000ULL}[pair],
			0xb601,0xb601,0x4a20 | (sxm << 8),0,0,0,operation + b + address + pair + sxm,true);
	}
	return cases;
}
}
