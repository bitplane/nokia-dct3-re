// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned control = 0; control < 7; ++control) for (unsigned form = 0; form < 8; ++form)
	for (unsigned count : {0U,1U,2U,7U,127U}) for (unsigned delayed = 0; delayed < 2; ++delayed)
	for (unsigned gap = 0; gap < (control ? 1U : 2U); ++gap) {
		if (control && (form == 1 || form == 3 || form == 4)) continue;
		unsigned const kind = control - 1, seed = control + form + count + delayed + gap;
		unsigned const read = !control ? (form == 3 || form == 4 ? 6 : 5) :
			7 + unsigned(kind == 1 || kind == 2 || kind == 5);
		u32 const base = seed & 1 ? 0x20000 : 0;
		u32 const target = control && kind == 4 ? base ^ 0x20000 : base;
		u16 const source = std::array<u16,8>{0x1202,0x3002,0x1400,0x1400,0x1403,0x14,8,8}[form];
		u16 const st1 = 0x800 | (form == 1 ? 0x4000 : 0);
		auto initial = initial_case("move-BRC-before-block-repeat", {0xf495});
		unsigned cursor = control ? (kind == 5 ? 1 : 2) : 0;
		if (control) {
			initial.program[0].value = kind == 4 ? 0xfb80 | (target >> 16) :
				std::array<u16,6>{0xf273,0xfa30,0xfa30,0xf274,0,0xfe00}[kind];
			if (kind != 5) initial.program[1].value = 0x700;
		}
		initial.program[cursor++].value = std::array<u16,8>{0x7102,0x7102,0x7192,0x71f8,0x71e2,0x7314,0x7308,0x7108}[form];
		if (form == 3 || form == 4) initial.program[cursor++].value = form == 3 ? 0x1400 : 3;
		initial.program[cursor++].value = 0x1a;
		for (auto &entry : initial.program) entry.address += base;
		if (control && kind != 2) {
			cursor = 0x100;
			for (unsigned i = 0; i < 16; ++i) initial.program.push_back({target + 0x700 + i,0xf495});
		} else if (control) cursor = 4;
		cursor += gap;
		// Replace the explicit NOP runway with RPTB[D], its end address, and a load/store body.
		auto put = [&](unsigned at, u16 word) {
			u32 const address = target + 0x600 + at;
			for (auto &entry : initial.program) if (entry.address == address) { entry.value = word; return; }
			initial.program.push_back({address,word});
		};
		put(cursor++,delayed ? 0xf272 : 0xf072);
		u16 const end = 0x600 + cursor + 2 + (delayed ? 2 : 0);
		put(cursor++,end);
		if (delayed) cursor += 2;
		put(cursor++,0x1096); put(cursor++,0x8097); put(cursor,0xf4e1);
		initial.data = {{0x3000,0x700}};
		if (source >= 0x60) initial.data.push_back({source,u16(count)});
		for (unsigned i = 0; i <= count; ++i) {
			initial.data.push_back({0x5000 + i,u16(0x6000 + i)});
			initial.data.push_back({0x5100 + i,0xdead});
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,count},{r::STATE_B,0xfedcba9876ULL},
			{r::STATE_ST0,(form == 7 ? 0xa000U : 0xa024U) | (control && kind == 1 ? 0x1000U : 0U)},
			{r::STATE_ST1,st1},{r::STATE_SP,0x3000},{r::STATE_AR2,0x1400},{r::STATE_AR4,count},
			{r::STATE_AR6,0x5000},{r::STATE_AR7,0x5100},{r::STATE_BRC,count ^ 1}});
		unsigned const finish = 80 + count * 4;
		auto final = observation("retained-count-plus-one-copies", {{STATE_GENPC,target + 0x601 + cursor},
			{r::STATE_A,0x6000 + count},{r::STATE_BRC,0},{r::STATE_ST1,st1},
			{r::STATE_AR2,form == 2 ? 0x1401U : 0x1400U},{r::STATE_AR6,0x5001 + count},{r::STATE_AR7,0x5101 + count},
			{r::STATE_SP,!control ? 0x3000U : kind == 4 ? 0x2ffeU : kind == 3 ? 0x2fffU : kind == 5 ? 0x3001U : 0x3000U}});
		for (unsigned i = 0; i <= count; ++i) final.expected_data.push_back({0x5100 + i,u16(0x6000 + i)});
		clocked_register_case c{{{0,std::move(initial)},
			{read,observation("zero-latency-BRC-publication", {{r::STATE_BRC,count}})},
			{read + 1,observation("repeat-owns-its-counter", {})},{finish,std::move(final)}},read - 1 + seed % 3,finish};
		if (source >= 0x60) c.actions = {{read,{},false,{},{},{{source,0xdead}}}};
		else c.actions = {{read,{{source == 8 ? r::STATE_A : r::STATE_AR4,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (control && (kind == 3 || kind == 4)) {
			c.data_events->push_back({true,0x2fff,0x604,0,false});
		}
		if (control && kind == 5) c.data_events->push_back({false,0x3000,0x700,0,false});
		c.data_events->push_back({false,source,u16(count),read});
		c.data_events->push_back({true,0x1a,u16(count),read});
		if (control && kind == 4) c.data_events->push_back({true,0x2ffe,u16(base >> 16),0,false});
		for (unsigned i = 0; i <= count; ++i) {
			c.data_events->push_back({false,0x5000 + i,u16(0x6000 + i),0,false});
			// The next load reaches Read before the older store reaches Write.
			if (i) c.data_events->push_back({true,0x5100 + i - 1,u16(0x6000 + i - 1),0,false});
		}
		c.data_events->push_back({true,0x5100 + count,u16(0x6000 + count),0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
