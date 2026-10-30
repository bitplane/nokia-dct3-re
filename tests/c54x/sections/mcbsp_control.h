// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_MCBSP_CONTROL
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned page=0;page<2;++page) for (unsigned sxm=0;sxm<2;++sxm) for (unsigned checkpoint : {4,9,12,13}) {
		u32 const base=page ? 0x20000 : 0;
		auto initial=initial_case("McBSP-real-selector-status-and-reset",{0x76e1,0x38,1,0x68e1,0x39,0xfffe,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		u16 const st1=0x800|(sxm ? 0x100 : 0);
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_AR1,0},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xabcdef1234ULL},{r::STATE_ST0,0x1600},{r::STATE_ST1,st1}});
		auto final=observation("captured-ready-word-clears-XRST-after-TX-busy",{{r::STATE_AR1,0},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xabcdef1234ULL},{r::STATE_ST0,0x1600},{r::STATE_ST1,st1}},{{0x38,1},{0x39,0}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("McBSP-sequence-running",{})},{32,std::move(final)}},checkpoint,32};
		// Regression clocks locate the four semantic replay boundaries; no silicon timing claim.
		c.data_events=std::vector<clocked_data_event>{{true,0x38,1,9},{false,0x39,3,12},{true,0x39,2,13}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
