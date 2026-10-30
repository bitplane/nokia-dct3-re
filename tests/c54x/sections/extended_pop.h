// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form != 25; ++form)
	for (unsigned offset = 0; offset != 4; ++offset)
	for (unsigned pattern = 0; pattern != 4; ++pattern)
	for (unsigned cmpt = 0; cmpt != 2; ++cmpt)
	for (unsigned cpl = 0; cpl != 2; ++cpl)
	for (unsigned bk : {0U,9U}) {
		if (form >= 16 && form < 24 && !bk) continue;
		unsigned const seed = form + offset + pattern + cmpt + cpl + bk;
		u32 const base = seed % 2 ? 0x20000 : 0;
		u16 const mode = form == 24 ? 0xf8 : 0xe0 + form;
		unsigned const reg = mode & 7, selected = form == 24 ? 8 : cmpt && reg == 0 ? 5 : reg;
		u16 const literal = form == 24 ? std::array<u16,4>{0x5000,0x3000,0x2fff,0xffff}[offset] :
			form >= 16 && bk && offset == 3 ? 8 : std::array<u16,4>{0xffff,0,4,0x7fff}[offset];
		int const delta = s16(literal);
		int const position = (7 + delta) % 9;
		u16 const address = form == 24 ? literal : form >= 16 && bk ? 0x5000 + (position < 0 ? position + 9 : position) : u16(0x5007 + delta);
		u16 const value = std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		u16 const status = cmpt && form != 24 && reg ? (0xb608 & 0x1fff) | (reg << 13) : 0xb608;
		u16 const st1 = 0x800 | (cpl ? 0x4000 : 0) | (cmpt ? 0x20 : 0) |
			(pattern & 1 ? 0x100 : 0) | (pattern & 2 ? 0x200 : 0) | 0x13;
		auto initial = initial_case("extended-POPD-retains-destination-and-stack-word", {u16(0x8b00 | mode),literal,0xe955});
		initial.program[15].value = 0xf4e1;
		for (auto &word : initial.program) word.address += base;
		initial.data = {{address,0xaaaa},{0x3000,value}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xb608},
			{r::STATE_ST1,st1},{r::STATE_PMST,pattern & 1},{r::STATE_SP,0x3000},{r::STATE_BK,bk},
			{r::STATE_A,0x123456789a},{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0x5aa5},{r::STATE_TRN,0x1234}});
		auto access = observation("Access-captures-destination-without-consuming-stack", {{r::STATE_ST0,status},{r::STATE_SP,0x3000}});
		auto read = observation("Read-consumes-stack-after-pointer-poison", {{r::STATE_ST0,status},{r::STATE_SP,0x3001}});
		auto final = observation("one-owned-destination-write", {{STATE_GENPC,base + 0x610},{r::STATE_SP,0x3001},
			{r::STATE_A,0x123456789a},{r::STATE_B,0x55},{r::STATE_ST0,status},{r::STATE_ST1,st1},
			{r::STATE_PMST,pattern & 1},{r::STATE_BK,bk},{r::STATE_T,0x5aa5},{r::STATE_TRN,0x1234}},{{address,value}});
		if (address != 0x3000) final.expected_data.push_back({0x3000,0xdead});
		std::vector<register_setting> poison;
		for (unsigned ar = 0; ar != 8; ++ar) {
			u16 const start = ar == selected ? 0x5007 : 0x7000 + ar;
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),start});
			access.expected.push_back({int(r::STATE_AR0) + int(ar),ar == selected && form >= 8 && form < 24 ? address : start});
			for (auto *state : {&read,&final}) state->expected.push_back({int(r::STATE_AR0) + int(ar),0x6a00 + ar});
			poison.push_back({int(r::STATE_AR0) + int(ar),0x6a00 + ar});
		}
		clocked_register_case c{{{0,std::move(initial)},{5,std::move(access)},{6,std::move(read)},
			{24,std::move(final)}},std::array<unsigned,3>{3,5,7}[seed % 3],24};
		c.actions = {{3,{},false,{{base + 0x601,0xdead}}},{5,std::move(poison)},{6,{},false,{}, {},{{0x3000,0xdead}}}};
		c.program_events = {{false,base + 0x601,literal,3}};
		c.data_events = std::vector<clocked_data_event>{{false,0x3000,value,6},{true,address,value,7}};
		cases.push_back(std::move(c));
	}
	for (unsigned kind = 0; kind != 8; ++kind)
	for (unsigned cmpt = 0; cmpt != 2; ++cmpt) {
		u32 const base = kind % 2 ? 0x20000 : 0;
		auto initial = initial_case("extended-POPD-unaccepted-composition", {0xf495});
		for (auto &word : initial.program) word.address += base;
		initial.program[0].value = kind < 3 ? 0x8beb : kind == 3 ? 0x8bf9 : 0xf495;
		initial.program[1].value = u16((kind == 0 ? 0xe : kind == 1 ? 0x18 : 0x24) - 0x5007);
		if (kind == 4 || kind == 6) {
			initial.program[0].value = kind == 4 ? 0xf273 : 0xf072; initial.program[1].value = kind == 4 ? 0x610 : 0x604;
			initial.program[2].value = 0x8be3; initial.program[3].value = 4;
		}
		if (kind == 5 || kind == 7) {
			initial.program[0].value = kind == 5 ? 0xec00 : 0xfd30; initial.program[1].value = 0x8be3; initial.program[2].value = 4;
		}
		initial.expected.clear();
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xb608},
			{r::STATE_ST1,0x813 | (cmpt ? 0x20U : 0U)},{r::STATE_SP,0x3000},{r::STATE_AR3,0x5007}});
		auto final = observation("reject-before-any-data-or-pointer-effect", {{r::STATE_SP,0x3000},{r::STATE_AR3,0x5007},{r::STATE_ST0,0xb608}});
		final.expected.back() = {r::STATE_ILLEGAL,1};
		clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},(kind + cmpt) % 2 ? 2U : 10U,20};
		c.require_idle = false; c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	return cases;
}
}
