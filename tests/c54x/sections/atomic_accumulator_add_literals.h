// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 7; ++variant) {
		bool const cross = variant >= 1 && variant <= 4, b = cross && variant % 2;
		bool const ovm = cross && variant >= 3;
		u16 const opcode = variant == 0 ? 0xf508 : cross ? b ? 0xf500 : 0xf600 : variant == 5 ? 0xf400 : 0xf028;
		u64 const a = variant == 0 ? 0x1234 : cross ? b ? 1 : 0x7fffffff : variant == 5 ? 0x7fffffff : 0;
		u64 const b_value = variant == 0 ? 0x10000 : cross ? b ? 0x7fffffff : 1 : 0x11223344;
		u64 const sum = cross ? ovm ? 0x7fffffff : 0x80000000 : variant == 0 ? 0x133400 : variant == 5 ? 0xfffffffeULL : 0xaa00;
		u16 const st0 = cross ? 0x1800 : 0, st1 = cross ? ovm ? 0x200 : 0 : 0x100;
		u16 const result_st0 = cross ? b ? 0x1200 : 0x1400 : variant == 5 ? 0x400 : 0;
		auto initial = initial_case("accumulator-ADD-overflow-and-shift-literals", {0xf495,opcode});
		if (variant == 6) initial.program[2].value = 0xaa;
		initial.program[variant == 6 ? 3 : 2].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b_value},{r::STATE_T,0x8888},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		auto final = observation("destination-overflow-source-T-and-TC", {{STATE_GENPC,variant == 6 ? 0x604U : 0x603U},
			{r::STATE_A,variant == 0 || b ? a : sum},{r::STATE_B,variant == 0 || b ? sum : b_value},
			{r::STATE_T,0x8888},{r::STATE_ST0,result_st0},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-add-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("add-running", {})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	return cases;
}
}
