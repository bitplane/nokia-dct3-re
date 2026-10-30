// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 320; ++index) {
		bool const delayed = index >= 256;
		unsigned const v = delayed ? index - 256 : index;
		unsigned const form = delayed ? 5 + v % 2 : v % 8, selector = (v / (delayed ? 2 : 8)) % 16;
		bool const invert = delayed ? v / 32 : v / 128;
		bool const high = (selector + form) & 1, tc = (selector + form + unsigned(invert)) & 1;
		u16 const t = selector | (high ? 0xa5f0 : 0), mask = u16(1U << (15 - selector)), value = invert ? u16(~mask) : mask;
		u16 const source = form == 5 ? 0x6d1 : 0x1201;
		u32 const base = (selector + form) & 1 ? 0x20000 : 0;
		bool const extended = form >= 5;
		u16 const mode = std::array<u16,8>{1,1,0x83,0x8b,0x93,0xf8,0xe3,0xeb}[form];
		u16 const st1 = 0xb00 | (form == 1 ? 0x4000 : 0), st0 = 0xa824 | (tc ? 0x1000 : 0);
		auto initial = initial_case("bitt-retained-operands", {0xf495});
		auto put = [&](u32 address,u16 word) {
			for (auto &entry : initial.program) if (entry.address == address) { entry.value = word; return; }
			initial.program.push_back({address,word});
		};
		for (auto &word : initial.program) word.address += base;
		if (delayed) { put(base + 0x600,0xfe00); put(base + 0x601,0x3400 | mode); put(base + 0x602,form == 5 ? source : 1); put(base + 0x603,0xffff); put(base + 0x700,0xf4e1); }
		else { put(base + 0x600,0x3400 | mode); if (extended) put(base + 0x601,form == 5 ? source : 1); put(base + 0x601 + unsigned(extended),0xf4e1); }
		initial.data = {{source,value},{0x2000,0x700}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_T,t},{r::STATE_SP,delayed ? 0x2000U : 0x1200U},{r::STATE_A,0xff12345678ULL},{r::STATE_B,0x81abcdef01ULL}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({int(r::STATE_AR0 + ar),ar == 3 ? (form >= 6 ? 0x1200U : 0x1201U) : 0x4000 + ar});
		unsigned const read_tick = delayed ? 9 : extended ? 6 : 5;
		clocked_register_case c{{{0,std::move(initial)}},delayed ? std::array<unsigned,3>{4,7,10}[(selector + form) % 3] :
			std::array<unsigned,3>{4,6,7}[(selector + form) % 3],24};
		u16 const pointer = form == 3 || form == 6 ? 0x1200 : form == 4 ? 0x1202 : 0x1201;
		c.states.push_back({24,observation("retained-bit-and-address", {{r::STATE_ST0,u64(0xa824 | (invert ? 0 : 0x1000))},
			{r::STATE_ST1,st1},{r::STATE_T,u64(t ^ 15)},{r::STATE_SP,delayed ? 0x2001U : 0x1200U},{r::STATE_AR3,pointer},
			{r::STATE_A,0xff12345678ULL},{r::STATE_B,0x81abcdef01ULL},
			{STATE_GENPC,base + (delayed ? 0x701 : extended ? 0x603 : 0x602)}})});
		c.actions = {{read_tick,{{r::STATE_T,u64(t ^ 15)}},false,{}, {},{{source,0x5555}}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (delayed) c.data_events->push_back({false,0x2000,0x700,0,false});
		c.data_events->push_back({false,source,value,read_tick});
		if (extended) c.program_events = {{false,base + (delayed ? 0x602 : 0x601),u16(form == 5 ? source : 1),0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
