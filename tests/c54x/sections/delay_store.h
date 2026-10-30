// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned control = 0; control < 5; ++control) for (unsigned selector = 0; selector < 4; ++selector)
	for (unsigned form = 0; form < 4; ++form) for (u16 shape : {1U,0xffffU,0xf495U,0xfc43U,0xf7d9U,0xf4e2U}) {
		unsigned const seed = control + selector + form + shape, delay = control == 4 ? 1 : 2;
		unsigned const literal_clock = control == 4 ? 4 : control == 1 || control == 2 ? 6 : 5;
		unsigned const write = control == 0 || control == 3 ? 9 : 10;
		u16 const literal = form == 3 && shape == 1 ? 0x5001 : shape;
		int const offset = s16(literal);
		// SPRU131G5.5.3.4 requires |step|<=BK. Retain opcode-shaped offsets in a legal larger ring.
		u16 const bk = form == 2 && std::abs(offset) > 13 ? 0x8000 : 13;
		int const position = bk == 13 ? 5 : 0x5005;
		int const residue = (position + offset) % int(bk);
		u16 const address = form == 3 ? literal : form == 2 ? (bk == 13 ? 0x5000 : 0) + (residue < 0 ? residue + bk : residue) : u16(0x5005 + offset);
		u16 const next = form == 1 || form == 2 ? address : 0x5005;
		u64 const a = control == 1 ? 0 : control == 2 ? 1 : 0x123456789aULL, b = 0xfedcba9876ULL;
		u16 const word = u16((selector & 1 ? b : a) >> (selector & 2 ? 16 : 0));
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("long-store-in-control-delay", {std::array<u16,5>{0xf273,0xfa45,0xfa45,0xf274,0xfe00}[control]});
		if (control != 4) initial.program[1].value = 0x700;
		initial.program[delay].value = 0x8000 | (selector << 8) | std::array<u16,4>{0xe2,0xea,0xf2,0xf8}[form];
		initial.program[delay + 1].value = literal; initial.program[delay + 2].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.program.push_back({base + 0x700,0xf4e1});
		initial.data = {{0x3000,0x700},{address,0xdead}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_BK,bk},{r::STATE_AR2,0x5005},{r::STATE_SP,0x3000},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900}});
		auto final = observation("one-retained-store-and-control-continuation", {{STATE_GENPC,base + (control == 2 ? 0x605U : 0x701U)},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_AR2,0x6000},
			{r::STATE_SP,control == 3 ? 0x2fffU : control == 4 ? 0x3001U : 0x3000U}},{{address,word}});
		clocked_register_case c{{{0,std::move(initial)},
			{write - 1,observation("one-address-modification", {{r::STATE_AR2,next}})},
			{write,observation("live-pointer-not-rewritten", {{r::STATE_AR2,0x6000}})},{24,std::move(final)}},
			std::array<unsigned,3>{3,control == 4 ? 4U : 5U,write - 1}[seed % 3],24};
		c.actions = {{literal_clock,{},false,{{base + 0x601 + delay,0x1234}}},{write - 1,{{r::STATE_AR2,0x6000}}}};
		c.program_events = {{false,base + 0x601 + delay,literal,literal_clock}};
		if (control != 2) c.program_events.push_back({false,base + 0x700,0xf4e1,0,false});
		c.data_events = std::vector<clocked_data_event>{};
		if (control == 3) c.data_events->push_back({true,0x2fff,0x604,0,false});
		if (control == 4) c.data_events->push_back({false,0x3000,0x700,0,false});
		c.data_events->push_back({true,address,word,write});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
