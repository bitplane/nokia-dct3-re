// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned outer : {0U,1U,3U}) for (unsigned inner : {0U,2U,9U})
	for (unsigned destination=0;destination<2;++destination) {
		u16 const header=0xf071|(destination<<8);
		u16 const mac=0xb098|(destination ? 0x300 : 0);
		auto initial=initial_case("block-zeroing-repeat-independent-counts",{0xf072,0x606,header,u16(inner),mac,u16(0x8094|(destination<<8)),0x6d95,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_BRC,outer},{r::STATE_ST0,0x1000},{r::STATE_ST1,0x800},
			{r::STATE_A,0x123456},{r::STATE_B,0x234567},{r::STATE_AR2,0x4000},{r::STATE_AR3,0x5000},
			{r::STATE_AR4,0x6000},{r::STATE_AR5,0x7000}});
		u64 result=0;
		auto final=observation("zero-before-each-inner-loop-and-one-outer-tail",{{STATE_GENPC,0x608},
			{r::STATE_BRC,0},{r::STATE_RC,0},{r::STATE_ST0,0x1000},{r::STATE_ST1,0x800},
			{r::STATE_AR2,0x4000+(outer+1)*(inner+1)},{r::STATE_AR3,0x5000+(outer+1)*(inner+1)},
			{r::STATE_AR4,0x6001+outer},{r::STATE_AR5,0x7001+outer}});
		for (unsigned o=0;o<=outer;++o) {
			result=0;
			for (unsigned i=0;i<=inner;++i) {
				unsigned const n=o*(inner+1)+i;
				initial.data.push_back({0x4000+n,u16(n+1)});
				initial.data.push_back({0x5000+n,u16(n+2)});
				result+=(n+1)*(n+2);
			}
			final.expected_data.push_back({0x6000+o,u16(result)});
		}
		final.expected.push_back({r::STATE_A,destination ? 0x123456 : result});
		final.expected.push_back({r::STATE_B,destination ? result : 0x234567});
		clocked_register_case c{{{0,std::move(initial)},{300,std::move(final)}},
			std::array<unsigned,4>{5,8,11,15}[cases.size()%4],300};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
