// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 8; ++index) {
		bool const bitfield = index >= 4, match = index & 1, update = index & 2;
		u16 const value = bitfield ? 2 : 1, literal = bitfield ? match ? 2 : 1 : match ? 1 : 2;
		u16 const st0 = bitfield ? 0xaa5 : 0x2a5;
		auto initial = initial_case("offset-memory-test", {u16((bitfield ? 0x6100 : 0x6000) | (update ? 0xe9 : 0xe1)),4,literal,0xf4e1});
		initial.data = {{0x1004,value},{0x1001,4},{0x1002,4}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_AR1,0x1000},{r::STATE_ST0,st0}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("displacement-before-test-literal", {{r::STATE_AR1,update ? 0x1004U : 0x1000U},
			{r::STATE_ST0,u64(st0 | (match ? 0x1000 : 0))},{STATE_GENPC,0x604}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x1004,value,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
