// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned kind=0;kind<8;++kind)for(u32 base : {0U,0x20000U}){
		// A conditional forward exit at the endpoint has positive coverage.
		if(kind==4)continue;
		u16 const end=kind==1 || kind==4 ? 0x604 : kind==2 ? 0x606 : 0x60d;
		auto initial=initial_case("block-branch-unqualified-control-fails-closed",{0xf072,end,0x1092,
			std::array<u16,8>{0xf073,0xf073,0xf273,0xf845,0xf845,0xf945,0xf072,0xf880}[kind],
			u16(kind==0 || kind==3 ? 0x601 : kind==1 || kind==4 ? 0x700 : kind==2 ? 0x602 : 0x60a),0x7693,0x1111});
		for(auto &word : initial.program)word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0x1234},{r::STATE_B,0x2345},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},
			{r::STATE_AR2,0x5000},{r::STATE_AR3,0x5100},{r::STATE_SP,0x3000},{r::STATE_BRC,0}});
		initial.data={{0x5000,0},{0x5100,0xaaaa}};
		initial.expected.clear();
		auto final=observation("no-control-frame-or-speculative-body-write",{{r::STATE_ILLEGAL,1},{r::STATE_SP,0x3000},{r::STATE_AR2,0x5001},{r::STATE_AR3,0x5100}},{{0x5100,0xaaaa}});
		// observation() normally appends ILLEGAL=0; these are intentional limits.
		final.expected.back().value=1;
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},kind&1 ? 8U : 3U,16};c.require_idle=false;
		c.data_events=std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
