// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned family = 0; family < 6; ++family) for (unsigned taken = 0; taken < 2; ++taken) {
		bool const returning = family == 0, call = family == 2 || family == 3, delayed = family == 1 || family >= 4;
		u16 const opcode = std::array<u16,6>{0xfc4b,0xfa45,0xf947,0xf930,0x6e8f,0x6e82}[family];
		u64 const negative = 0xff00000000ULL;
		u64 const a = family == 1 ? !taken : family == 2 ? (taken ? negative : 1) : family == 3 ? 1 : 0x55;
		u64 const b = returning ? (taken ? negative : 1) : family == 3 ? 2 : 0x12345678;
		u16 const st0 = family == 3 ? 0x800 | (taken << 12) : 0;
		unsigned const words = delayed ? 4 : returning ? 1 : 2;
		auto initial = initial_case("atomic-conditional-control", {opcode});
		if (!returning) initial.program[1].value = 0x700;
		if (delayed) { initial.program[2].value = 0xe801; initial.program[3].value = 0xe802; }
		initial.program[words].value = 0xf4e1;
		initial.program.push_back({0x700,0xf4e1}); initial.program.push_back({0x5ff,0xf495});
		initial.data = {{0x3000,0x700},{0x2fff,0xbeef}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_SP,0x3000},{r::STATE_ST0,st0},{r::STATE_ST1,0x100},
			{r::STATE_AR2,family == 5 ? taken : 0x1200U},{r::STATE_AR7,family == 4 ? taken : 0x1234U}});
		auto final = observation("retained-predicate-and-owned-stack-effect", {{STATE_GENPC,taken ? 0x701U : 0x601 + words},
			{r::STATE_A,delayed ? 2 : a},{r::STATE_B,b},{r::STATE_ST0,st0},{r::STATE_ST1,0x100},
			{r::STATE_SP,0x3000U + (returning ? taken : 0) - (call ? taken : 0)},
			{r::STATE_AR2,family == 5 ? taken : 0x1200U},{r::STATE_AR7,family == 4 ? u16(taken - 1) : 0x1234U}},
			{{0x2fff,call && taken ? u16(0x602) : u16(0xbeef)}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-control-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("control-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{};
		if (taken && returning) c.data_events->push_back({false,0x3000,0x700,0,false});
		if (taken && call) c.data_events->push_back({true,0x2fff,0x602,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
