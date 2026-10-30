// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct vector { u16 opcode,t,memory,status; u64 initial,expected; bool overflow; u16 pmst = 0; };
	// Explicit results from SPRU172C MPYR/MACR/MASR arithmetic, not
	// a second implementation of the core's rounding helper.
	constexpr vector vectors[] = {
		{0x2200, 0x8000, 0x8000, 0x0040, 0, 0x0080000000ULL, true},
		{0x2200, 0x8000, 0x8000, 0x0240, 0, 0x007fffffffULL, true},
		{0x2a00, 1, 1, 0, 0x007fff7ffeULL, 0x007fff0000ULL, false},
		{0x2a00, 1, 1, 0, 0x007fff7fffULL, 0x0080000000ULL, true},
		{0x2a00, 1, 1, 0x0200, 0x007fff7fffULL, 0x007fffffffULL, true},
		{0x2e00, 0x0100, 0x0100, 0, 0xff80000000ULL, 0xff7fff0000ULL, true},
		{0x2e00, 0x0100, 0x0100, 0x0200, 0xff80000000ULL, 0xff80000000ULL, true},
		{0x2e00, 1, 1, 0, 0xff80008000ULL, 0xff80000000ULL, false},
		// SPRU131G SMUL: clamp the fractional product before MAC/MAS,
		// then apply the instruction's rounding and destination saturation.
		{0x2a00, 0x8000, 0x8000, 0x0240, 0xffffff8000ULL, 0x007fffffffULL, true, 0},
		{0x2a00, 0x8000, 0x8000, 0x0240, 0xffffff8000ULL, 0x007fff0000ULL, false, 2},
		{0x2e00, 0x8000, 0x8000, 0x0240, 0x0000007fffULL, 0xff80000000ULL, false, 0},
		{0x2e00, 0x8000, 0x8000, 0x0240, 0x0000007fffULL, 0xff80010000ULL, false, 2},
		// SMUL is inert when either OVM or FRCT is clear.
		{0x2a00, 0x8000, 0x8000, 0x0040, 0xffffff8000ULL, 0x0080000000ULL, true, 2},
		{0x2a00, 0x8000, 0x8000, 0x0200, 0, 0x0040000000ULL, false, 2},
		// SMUL controls MAC/MAS pre-accumulation, not MPYR rounding.
		{0x2200, 0x8000, 0x8000, 0x0240, 0, 0x007fffffffULL, true, 2},
		{0x2200, 0x8000, 0x8000, 0x0040, 0, 0x0080000000ULL, true, 2},
		{0x2200, 0x8000, 0x8000, 0x0200, 0, 0x0040000000ULL, false, 2},
		{0x2200, 0x8000, 0x8000, 0, 0, 0x0040000000ULL, false, 2},
		{0x2200, 1, 0x8000, 0, 0, 0, false, 2},
		{0x2200, 1, 0x8000, 0x0040, 0, 0xffffff0000ULL, false, 2},
		{0x2200, 1, 0x7fff, 0, 0, 0, false, 2},
		{0x2200, 1, 0x4000, 0x0040, 0, 0x0000010000ULL, false, 2},
		// Rounded accumulation crosses the 40-bit guard boundary; OVM
		// saturates the mathematical result before accumulator truncation.
		{0x2a00, 1, 1, 0, 0x7fffffffffULL, 0x8000000000ULL, true},
		{0x2a00, 1, 1, 0x0200, 0x7fffffffffULL, 0x007fffffffULL, true},
		{0x2e00, 0x0100, 0x0100, 0, 0x8000000000ULL, 0x7fffff0000ULL, true},
		{0x2e00, 0x0100, 0x0100, 0x0200, 0x8000000000ULL, 0xff80000000ULL, true}
	};
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 104; ++index) {
		auto const &v = vectors[index / 4];
		bool const destination_b = index & 1, absolute = index & 2, sticky = (index / 4 + index) & 1;
		u16 const opcode = v.opcode | (destination_b ? 0x100 : 0) | (absolute ? 0xf8 : 0x82);
		u16 const sibling = destination_b ? 0x400 : 0x200, overflow_flag = destination_b ? 0x200 : 0x400;
		u16 const st0 = 0x1800 | sibling | (sticky ? overflow_flag : 0);
		u16 const flags = 0x1800 | sibling | (sticky || v.overflow ? overflow_flag : 0);
		auto initial = initial_case("rounded-multiply-boundary", {opcode,u16(absolute ? 0x500 : 0xf4e1),0xf4e1});
		initial.data = {{0x500,v.memory}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,destination_b ? 0x12345678 : v.initial},
			{r::STATE_B,destination_b ? v.initial : 0x12345678},{r::STATE_AR2,0x500},{r::STATE_T,v.t},
			{r::STATE_ST0,st0},{r::STATE_ST1,u64(0x800 | v.status)},{r::STATE_PMST,v.pmst}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("manual-rounded-product-result", {{r::STATE_A,destination_b ? 0x12345678 : v.expected},
			{r::STATE_B,destination_b ? v.expected : 0x12345678},{r::STATE_AR2,0x500},{r::STATE_T,v.t},
			{r::STATE_ST0,flags},{r::STATE_ST1,u64(0x800 | v.status)},{r::STATE_PMST,v.pmst},{STATE_GENPC,absolute ? 0x603U : 0x602U}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x500,v.memory,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
