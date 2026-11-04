// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned length : {0U,1U,8U,12U}) for (unsigned checkpoint : {4U,9U,16U,25U}) {
		// Original InitDisk 491e..492a, including its unmodified BANZD target.
		auto initial=initial_case("original-MU4-name-copy",{0x4a11,0x8812,0xf495,0x7102,0x0011,
			0x7191,0x0013,0x6e83,0x4923,0x7092,0x0013,0x8a11,0xfc00});
		for (auto &word : initial.program) word.address+=0x491e - 0x600;
		initial.program.push_back({0xf00,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x491e},{r::STATE_SP,0x1000},
			{r::STATE_ST1,0x4900},{r::STATE_ST0,0xb601},{r::STATE_A,0x112},
			{r::STATE_AR1,0x3344},{r::STATE_AR2,0x7788},{r::STATE_AR3,0x5566}});
		initial.data={{0x1000,0xf00},{0x1001,0x1fd},{0x111,0x1357},{0x113+length,0x2468}};
		auto final=observation("original-copy-NUL-terminates-and-restores-caller",{{r::STATE_SP,0x1001},
			{r::STATE_AR1,0x3344},{r::STATE_ST0,0xb601},{r::STATE_ST1,0x4900}},{{0x111,0x1357},{0x113+length,0x2468}});
		for (unsigned i=0;i<=length;++i) {
			u16 const value=i==length ? 0 : u16(0x41+i);
			initial.data.insert(initial.data.end(),{{0x1fd+i,value},{0x112+i,0xdead}});
			final.expected_data.push_back({0x112+i,value});
		}
		clocked_register_case c{{{0,std::move(initial)},{256,std::move(final)}},checkpoint,256};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
