// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 offsets[]{0,1,0xffff,0xf495};
	constexpr u16 linear[]{0x1007,0x1008,0x1006,0x49c};
	constexpr u16 circular[]{0x4007,0x4008,0x4006,0x549c};
	constexpr u16 absolute[]{0x1500,0x1501,0x14ff,0x995};
	for (unsigned form = 0; form < 25; ++form) for (unsigned offset = 0; offset < 4; ++offset)
	for (unsigned target = 0; target < 12; ++target) for (u16 value : {0U,1U,0x8000U,0xffffU})
	for (unsigned compatibility = 0; compatibility < 2; ++compatibility)
	{
		unsigned const kind = form == 24 ? 3 : form / 8, reg = form == 24 ? 0 : form % 8;
		unsigned const selected = compatibility && !reg ? 3 : reg;
		u16 const address = kind == 3 ? absolute[offset] : kind == 2 ? circular[offset] : linear[offset];
		u16 const start = kind == 2 ? 0x4007 : 0x1007;
		u16 const next = kind == 1 || kind == 2 ? address : start;
		u16 const literal = kind == 3 ? address : offsets[offset], bk = kind == 2 ? 8192 : 16;
		u16 const destination = target < 10 ? 0x10 + target : target == 10 ? 0xf495 : address;
		u16 status = compatibility && reg && kind != 3 ? 0x608 | (reg << 13) : compatibility ? 0x6608 : 0x608;
		if (target < 8) {
			u16 const ar0 = target == 0 ? value : selected == 0 ? next : 0x1007;
			status = (status & ~0x1000) | (value == ar0 ? 0x1000 : 0);
		}
		u32 const base = reg == 6 ? 0x20000 : 0;
		auto initial = initial_case("offset-data-move", {u16(0x7100 | (kind == 3 ? 0xf8 : 0xe0 | (kind << 3) | reg)),literal,destination});
		if (target < 8) initial.program[3].value = 0xf4a8 | target;
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},
			{r::STATE_ST0,compatibility ? 0x6608 : 0x608},{r::STATE_ST1,compatibility ? 0x820 : 0x800},
			{r::STATE_SP,0x1200},{r::STATE_BK,bk},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({int(r::STATE_AR0) + int(ar),ar == selected ? start : 0x1007});
		initial.data = {{address,value}};
		if (target == 10) initial.data.push_back({destination,0xdead});
		auto final = observation("aliased-destination-published", {{r::STATE_ST0,status},{r::STATE_ST1,compatibility ? 0x820U : 0x800U},
			{r::STATE_SP,target == 8 ? value : 0x1200U},{r::STATE_BK,target == 9 ? value : unsigned(bk)},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}}, {{destination,value}});
		for (unsigned ar = 0; ar < 8; ++ar)
			final.expected.push_back({int(r::STATE_AR0) + int(ar),destination == 0x10 + ar ? value : ar == selected ? next : 0x1007U});
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},
			std::array<unsigned,4>{3,5,6,7}[(form + offset + target + value + compatibility) % 4],16};
		c.require_idle = false;
		c.actions = {{5,{},false,{{base + 0x601,0xdead},{base + 0x602,0xdead}}},{6,{}}};
		c.actions.back().data = {{address,0x7777}};
		c.program_events = {{false,base + 0x601,literal,0,false},{false,base + 0x602,destination,0,false}};
		c.data_events = std::vector<clocked_data_event>{{false,address,value,6},{true,destination,value,destination < 0x60 ? 6U : 7U}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
