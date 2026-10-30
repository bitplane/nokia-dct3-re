// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 28; ++index) {
		bool const dp = index >= 16;
		unsigned const v = dp ? index - 16 : index, shift = std::array<unsigned,4>{0,5,8,15}[(v / 4) % 4];
		bool const b = v & 1, sxm = v & 2, cpl = v & 1;
		u16 const value = std::array<u16,6>{0,1,0xff,0x100,0x1fe,0x1ff}[(v / 2) % 6];
		u64 const source = sxm ? u64(s64(s16(0x8123))) : 0x8123, result = (source << shift) & 0xffffffffffULL;
		u16 const st1 = dp ? cpl ? 0x4800 : 0x900 : sxm ? 0x100 : 0;
		auto initial = initial_case(dp ? "immediate-DP" : "compact-Xmem-load", {u16(dp ? 0xea00 | value : 0x9480 | (b ? 0x100 : 0) | shift),0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,dp ? 0x123456789aULL : 0x1234},
			{r::STATE_B,dp ? 0x9876543210ULL : 0x1234},{r::STATE_ST0,dp ? u64(0xfe00 | (value ^ 0x1ff)) : 0},
			{r::STATE_ST1,st1},{r::STATE_AR2,0x1000}});
		if (!dp) initial.data = {{0x1000,0x8123}};
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("compact-load-result", {{r::STATE_A,dp ? 0x123456789aULL : b ? 0x1234 : result},
			{r::STATE_B,dp ? 0x9876543210ULL : b ? result : 0x1234},{r::STATE_ST1,st1},
			{r::STATE_AR2,dp ? 0x1000U : 0x1001U},{STATE_GENPC,0x602}})});
		if (dp) c.states.back().observation.expected.push_back({r::STATE_ST0,u64(0xfe00 | value)});
		c.data_events = std::vector<clocked_data_event>{};
		if (!dp) c.data_events->push_back({false,0x1000,0x8123,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
