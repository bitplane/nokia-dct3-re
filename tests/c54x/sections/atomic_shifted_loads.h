// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,source; bool sxm,into_a; u16 ar0,ar2,ar3; };
	constexpr row rows[]{
		{0x4593,0xd00,true,false,0xd10,0xd10,0xd01},
		{0x4592,0xd10,true,false,0xd10,0xd11,0xd01},
		{0x4592,0xd10,false,false,0xd10,0xd11,0xd01},
		{0x4582,0xd10,true,false,0xd10,0xd10,0xd01},
		{0x4582,0xd10,false,false,0xd10,0xd10,0xd01},
		{0x4488,0xd10,true,true,0xd0f,0xd10,0xd01},
		{0x4488,0xd10,false,true,0xd0f,0xd10,0xd01}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		auto initial = initial_case("LD-memory-shift16-SXM-and-pointer", {0xf495,v.opcode,0xf4e1});
		initial.data = {{v.source,0xff80}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,v.into_a ? 0U : 0x11223344U},
			{r::STATE_B,v.into_a ? 0x11223344U : 0U},{r::STATE_ST0,0xaa5},{r::STATE_ST1,v.sxm ? 0x100U : 0U},
			{r::STATE_AR0,0xd10},{r::STATE_AR2,0xd10},{r::STATE_AR3,v.opcode == 0x4593 ? 0xd00U : 0xd01U}});
		auto final = observation("signed-or-unsigned-high-word-retains-sibling", {{STATE_GENPC,0x603},
			{int(v.into_a ? r::STATE_A : r::STATE_B),v.sxm ? 0xffff800000ULL : 0xff800000ULL},
			{int(v.into_a ? r::STATE_B : r::STATE_A),0x11223344},{r::STATE_AR0,v.ar0},{r::STATE_AR2,v.ar2},
			{r::STATE_AR3,v.ar3},{r::STATE_ST0,0xaa5},{r::STATE_ST1,v.sxm ? 0x100U : 0U}},{{v.source,0xff80}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-load-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("load-running", {})},{16,std::move(final)}},1,16};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,v.source,0xff80,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
