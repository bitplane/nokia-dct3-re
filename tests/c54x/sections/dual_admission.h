// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// MPY, ADD, SUB and parallel MAC each retain all four source limits.
	for (unsigned operation=0;operation<4;++operation) for (unsigned checkpoint : {3U,4U})
	for (unsigned page=0;page<2;++page) for (unsigned source=0;source<4;++source) {
		bool const arithmetic=operation==1 || operation==2,parallel=operation==3;
		std::array<u16,4> ar{0x100f,0x110f,0x120f,0x130f};
		if (source<2) ar[0]=source ? 0x20 : arithmetic ? 6 : 0xe;
		else ar[1]=source==2 ? (arithmetic ? 6 : 0xe) : 0x33;
		u32 const base=page ? 0x20000 : 0;
		u16 const st1=0x800|(source&1 ? 0x4020 : 0)|(parallel ? source+4*page+8*(checkpoint-3) : 0);
		u16 const opcode=std::array<u16,4>{0xa489,0xa089,0xa289,0xd089}[operation];
		auto initial=initial_case("dual-source-limit-before-address-or-bus-effects",{opcode,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		std::vector<register_check> preserved{{r::STATE_A,parallel ? 0 : 0x123456789aULL},
			{r::STATE_B,0xfeabcd9876ULL},{r::STATE_ST0,0xa002},{r::STATE_ST1,st1},
			{r::STATE_PMST,0},{r::STATE_T,parallel ? 0U : 0xabcdU}};
		for (unsigned i=0;i<4;++i) preserved.push_back({int(r::STATE_AR2+i),ar[i]});
		for (auto const &value : preserved) initial.initial.push_back({value.index,value.value});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_AR0,3},{r::STATE_BK,0}});
		initial.expected.insert(initial.expected.end(),preserved.begin(),preserved.end());
		auto rejected=observation("dual-limit-guard",{{r::STATE_ILLEGAL,1}});
		rejected.expected.pop_back(); rejected.expected.insert(rejected.expected.end(),preserved.begin(),preserved.end());
		auto final=rejected; final.expected.push_back({r::STATE_IDLE,0});
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(rejected)},{12,std::move(final)}},checkpoint,12};
		c.require_idle=false; c.forbid_data_access=true; c.single_fetch=base+0x600;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
