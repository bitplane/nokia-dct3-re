// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	// Program-table pairs are copied through one scratch word. Each adjacent
	// instruction owns its AR3 update; table addresses advance through A.
	std::vector<clocked_register_case> cases;
	for (unsigned count: {0U,1U,3U,7U}) {
		auto initial=initial_case("READA-ADD-MVDD-table-loop", {
			0x7713,0x0060,0x7683,0x0001,0xf020,0x1200,0xf030,0xffff,
			0x7ef8,0x001a,0x0083,0xf072,0x0616,
			0xf030,0xffff,0x7ef8,0x0014,0x0093,0xf030,0xffff,
			0x7e8b,0x0093,0xe552,0xf4e1});
		initial.program.push_back({0x1200,u16(count)});
		for (unsigned i=0;i<=count;++i) {
			initial.program.push_back({0x1201+2*i,u16(0x2000+i)});
			initial.program.push_back({0x1202+2*i,u16(0x1111*(i+1))});
		}
		auto final=observation("each-pair-uses-its-own-scratch-and-address", {
			{r::STATE_A,0x1203+2*count},{r::STATE_AR3,0x60},{r::STATE_AR4,0x2000+count},
			{r::STATE_BRC,0},{STATE_GENPC,0x618}},{{0x60,1},{0x61,u16(0x1111*(count+1))}});
		for (unsigned i=0;i<=count;++i) final.expected_data.push_back({0x2000+i,u16(0x1111*(i+1))});
		cases.push_back({{{0,std::move(initial)},{400,std::move(final)}},20+count,400});
	}
	return cases;
}
}
