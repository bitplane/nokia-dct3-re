// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u64 sources[]{0x12345678,0x0180000000ULL,0xff7fffffffULL,0xffffffffffULL};
	// Independent 32-bit store results: rows are source boundaries, columns SST/SXM.
	constexpr u32 stored[4][4]{{0x12345678,0x12345678,0x12345678,0x12345678},
		{0x80000000,0xffffffff,0x80000000,0x7fffffff},
		{0x7fffffff,0xffffffff,0x7fffffff,0x80000000},{0xffffffff,0xffffffff,0xffffffff,0xffffffff}};
	std::vector<clocked_register_case> cases;
	for (unsigned immediate = 0; immediate != 2; ++immediate)
	for (unsigned selector = 0; selector != (immediate ? 1U : 4U); ++selector)
	for (unsigned reg = 0; reg != 8; ++reg)
	for (unsigned flags = 0; flags != 4; ++flags)
	for (unsigned pattern = 0; pattern != 4; ++pattern)
	for (unsigned cmpt = 0; cmpt != 2; ++cmpt) {
		u16 const address = std::array<u16,4>{0x1400,0xfffe,0xffff,0}[pattern];
		u16 const word = immediate ? std::array<u16,4>{0,1,0x8000,0xffff}[pattern] : u16(stored[pattern][flags] >> ((selector & 2) ? 16 : 0));
		unsigned const selected = cmpt && reg == 0 ? 3 : reg;
		u16 const st0 = cmpt ? 0x6608 : 0x608, st1 = 0x800 | ((flags & 2) ? 0x100 : 0) | (cmpt ? 0x20 : 0);
		u16 const final_st0 = cmpt && reg ? (st0 & 0x1fff) | (reg << 13) : st0;
		unsigned const mutate = immediate ? 6 : 5, write = immediate ? 7 : 6;
		u32 const base = (selector + reg + flags + pattern + cmpt + immediate) % 2 ? 0x20000 : 0;
		u16 const opcode = immediate ? 0x7698 | reg : 0x8000 | (selector << 8) | 0x98 | reg;
		auto initial = initial_case("preincrement-store-retains-address-and-owned-source", {opcode});
		for (auto &entry : initial.program) entry.address += base;
		if (immediate) initial.program[1].value = word;
		initial.program[immediate ? 2 : 1].value = 0xf4e1;
		initial.data = {{address,0xaaaa}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_PMST,flags & 1},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_A,selector & 1 ? 0xabcdef1234ULL : sources[pattern] ^ 0xffff},
			{r::STATE_B,selector & 1 ? sources[pattern] ^ 0xffff : 0xabcdef1234ULL}});
		auto before = observation("only-selected-pointer-preincrements", {});
		auto final = observation("source-poison-cannot-retarget-write", {{STATE_GENPC,base + (immediate ? 0x603 : 0x602)},
			{r::STATE_A,selector & 1 ? 0xabcdef1234ULL : sources[pattern]},
			{r::STATE_B,selector & 1 ? sources[pattern] : 0xabcdef1234ULL},
			{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_PMST,flags & 1}},{{address,word}});
		for (unsigned ar = 0; ar != 8; ++ar) {
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),u16(address - 1)});
			before.expected.push_back({int(r::STATE_AR0) + int(ar),ar == selected ? address : u16(address - 1)});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == selected ? 0x2222U : u16(address - 1)});
		}
		unsigned const checkpoint = (immediate ? std::array<unsigned,3>{4,6,7} : std::array<unsigned,3>{4,5,6})[(selector + reg + flags + pattern + cmpt) % 3];
		clocked_register_case c{{{0,std::move(initial)},{mutate,std::move(before)},
			{mutate + 1,observation("address-retained-after-pointer-poison", {})},{18,std::move(final)}},checkpoint,18};
		if (immediate) {
			c.actions.push_back({4,{},false,{{base + 0x601,0xdead}}});
			c.program_events = {{false,base + 0x601,word,0,false}};
		}
		c.actions.push_back({mutate,{{int(r::STATE_AR0) + int(selected),0x2222},
			{int(selector & 1 ? r::STATE_B : r::STATE_A),sources[pattern]}}});
		c.data_events = std::vector<clocked_data_event>{{true,address,word,write}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
