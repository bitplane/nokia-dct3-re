// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 inputs[]{0,0xffffffffffULL,0x7fffffff,0x80000000,0xff80000000ULL,0x7fffffffffULL,0x8000000000ULL,0x0123456789ULL};
	// SPRU172C RND adds8000h: explicit 40-bit wrap and signed32 saturation.
	constexpr u64 raw[]{0x8000,0x7fff,0x80007fff,0x80008000,0xff80008000ULL,0x8000007fffULL,0x8000008000ULL,0x012345e789ULL};
	constexpr u64 saturated[]{0x8000,0x7fff,0x7fffffff,0x7fffffff,0xff80008000ULL,0x7fffffff,0xff80000000ULL,0x7fffffff};
	for (unsigned selectors = 0; selectors < 4; ++selectors)
	for (unsigned pattern = 0; pattern < 8; ++pattern)
	for (unsigned ovm = 0; ovm < 2; ++ovm)
	{
		u64 const a = selectors & 2 ? 0x123456789aULL : inputs[pattern];
		u64 const b = selectors & 2 ? inputs[pattern] : 0x123456789aULL;
		u64 const result = (ovm ? saturated : raw)[pattern];
		u16 const status = (selectors + pattern + ovm) & 1 ? 0xabcd : 0;
		auto initial = initial_case("rounding", {u16(0xf49f | ((selectors & 2) << 8) | ((selectors & 1) << 8)),0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,status},{r::STATE_ST1,ovm ? 0x200U : 0U}});
		clocked_register_case c{{{0,std::move(initial)}},selectors & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("rounding-boundary", {{r::STATE_A,selectors & 1 ? a : result},
			{r::STATE_B,selectors & 1 ? result : b},{r::STATE_ST0,status},{r::STATE_ST1,ovm ? 0x200U : 0U},{STATE_GENPC,0x602}})});
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
