// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 64; ++index) {
		unsigned const mode = index / 16;
		bool const b = index & 1, sxm = index & 2;
		u16 const word = std::array<u16,4>{0,0x7fff,0x8000,0xffff}[(index >> 2) & 3];
		u16 const address = mode == 0 ? 0x45 : 0xf66;
		u64 const source = sxm ? u64(s64(s16(word))) : word, result = ((source << 16) | 0x8000) & 0xffffffffffULL;
		auto initial = initial_case("rounded-load", {u16(0x1600 | (b ? 0x100 : 0) | std::array<u16,4>{0x45,0x80,0x88,0xf8}[mode]),
			u16(mode == 3 ? address : 0xf4e1),0xf4e1});
		initial.data = {{address,word}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0x12345678},{r::STATE_B,0x12345678},
			{r::STATE_ST0,0xe00},{r::STATE_ST1,u64(0xa00 | (sxm ? 0x100 : 0))},{r::STATE_AR0,0xf66}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("rounded-high-word", {{r::STATE_A,b ? 0x12345678 : result},
			{r::STATE_B,b ? result : 0x12345678},{r::STATE_ST0,0xe00},{r::STATE_ST1,u64(0xa00 | (sxm ? 0x100 : 0))},
			{r::STATE_AR0,mode == 2 ? 0xf65U : 0xf66U},{STATE_GENPC,mode == 3 ? 0x603U : 0x602U}})});
		c.data_events = std::vector<clocked_data_event>{{false,address,word,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
