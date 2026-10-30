// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 64; ++index) {
		bool const b = index & 1, rounding = index & 2, subtract = index & 4, sxm = index & 8, fractional = index & 16, update = index & 32;
		s64 const product = s64(3) * -3 * (fractional ? 2 : 1);
		s64 sum = 0x18000 + (subtract ? -product : product);
		if (rounding) sum = (sum + 0x8000) & ~s64(0xffff);
		u64 const load = sxm ? 0xff80010000ULL : 0x80010000ULL;
		u16 const opcode = 0xa800 | ((index & 1) << 8) | ((index & 2) << 8) | ((index & 4) << 8) | (update ? 0xb9 : 0x31);
		auto initial = initial_case("parallel-load-multiply", {opcode,0xf4e1});
		initial.data = {{0x500,0x8001},{0x600,0xfffd}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,b ? 0x18000U : 0x1234U},{r::STATE_B,b ? 0x1234U : 0x18000U},
			{r::STATE_T,3},{r::STATE_ST0,0x800},{r::STATE_ST1,u64(0x800 | (sxm ? 0x100 : 0) | (fractional ? 0x40 : 0))},
			{r::STATE_AR5,0x500},{r::STATE_AR3,0x600}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("old-T-and-other-accumulator", {{r::STATE_A,b ? u64(sum) : load},
			{r::STATE_B,b ? load : u64(sum)},{r::STATE_T,3},{r::STATE_ST0,0x800},
			{r::STATE_AR5,update ? 0x501U : 0x500U},{r::STATE_AR3,update ? 0x601U : 0x600U},{STATE_GENPC,0x602}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x500,0x8001,0,false},{false,0x600,0xfffd,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
