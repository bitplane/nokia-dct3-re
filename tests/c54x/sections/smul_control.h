// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	// Explicit MAC/MAS boundary results without FRCT, with FRCT, with SMUL.
	constexpr u64 results[4][3]{{0x3fffffff,0x7fffffff,0x7ffffffe},{0x40000000,0x80000000,0x80000000},
		{0xffc0000001ULL,0xff80000001ULL,0xff80000002ULL},{0xffc0000000ULL,0xff80000000ULL,0xff80000000ULL}};
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 192; ++index) {
		unsigned const family = index / 64, kind = (index / 16) % 4, controls = (index / 2) & 7;
		bool const destination_b = index & 1;
		u64 const seed = kind < 2 ? 0xffffffffffULL : 1;
		bool const overflow = kind == 1 && (controls & 1);
		u64 const result = overflow && (controls & 2) ? 0x7fffffff : results[kind][controls & 1 ? (controls == 7 ? 2 : 1) : 0];
		u16 const opcode = family == 0 ? 0x2893 | (kind << 9) | (destination_b ? 0x100 : 0) :
			(family == 1 ? 0xb089 : 0xd089) | ((kind & 1) << 10) | ((kind >> 1) << 11) |
			(destination_b ? 0x100 : 0) | ((family == 1 ? destination_b : !destination_b) ? 0x200 : 0);
		u16 const st0 = 0x1800 | (destination_b ? 0x400 : 0x200);
		u16 const flags = st0 | (overflow ? (destination_b ? 0x200 : 0x400) : 0);
		u16 const st1 = 0x900 | (controls & 1 ? 0x40 : 0) | (controls & 2 ? 0x200 : 0);
		auto initial = initial_case("smul-control", {opcode,0xf4e1});
		initial.data = {{0x600,0x8000},{0x610,0x8000}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,destination_b ? 0x12340000 : seed},
			{r::STATE_B,destination_b ? seed : 0x12340000},{r::STATE_T,0x8000},{r::STATE_AR2,0x600},
			{r::STATE_AR3,family == 0 ? 0x600U : 0x610U},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,controls & 4 ? 2U : 0U}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("fractional-saturating-multiply-controls", {{r::STATE_A,destination_b ? 0x12340000 : result},
			{r::STATE_B,destination_b ? result : 0x12340000},{r::STATE_T,0x8000},{r::STATE_ST0,flags},{r::STATE_ST1,st1},
			{r::STATE_PMST,controls & 4 ? 2U : 0U},{r::STATE_AR2,family == 0 ? 0x600U : 0x601U},
			{r::STATE_AR3,family == 0 ? 0x601U : 0x611U},{STATE_GENPC,0x602}},
			{{0x600,0x8000},{0x610,u16(family == 2 ? 0x1234 : 0x8000)}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x600,0x8000,0,false}};
		if (family == 1) c.data_events->push_back({false,0x610,0x8000,0,false});
		if (family == 2) c.data_events->push_back({true,0x610,0x1234,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
