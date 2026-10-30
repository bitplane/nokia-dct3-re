// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> address_mode_cases()
{
	std::vector<clocked_register_case> cases;
	// Producer 0 is RSBX/SSBX; 1/2 MVMD, 3/4 STLM, 5/6 STM.
	for (unsigned producer=0;producer<7;++producer) for (unsigned set=0;set<2;++set)
	for (unsigned store=0;store<2;++store) for (unsigned dp=0;dp<(producer ? 1U : 2U);++dp)
	for (unsigned gap=(producer==0 || producer==3 || producer==4) ? 3 : 2;gap<=3;++gap) {
		bool const full=producer!=0, cmpt=producer && !(producer&1), stlm=producer==3 || producer==4, stm=producer>=5;
		unsigned const prefix=full && !stlm ? 2 : 1;
		u16 const st0=cmpt ? 0 : dp && !set ? 0xf : 0xe;
		u16 const st1=cmpt ? set ? 0x800 : 0x820 : dp || set ? 0x800 : 0x4800;
		u16 const final_st0=cmpt ? set ? 0x6000 : 0 : dp && set ? 0xf : 0xe;
		u16 const final_st1=cmpt ? set ? 0x820 : 0x800 : dp || !set ? 0x800 : 0x4800;
		u16 const first=cmpt ? 0x1400 : 0x727, second=cmpt ? 0x1500 : dp ? 0x7a7 : 0x1027;
		u16 const opcode=!full ? dp ? set ? 0xf5b0 : 0xf4b0 : set ? 0xf7be : 0xf6be : stlm ? 0x8907 : stm ? 0x7707 : 0x730b;
		auto initial=initial_case("separated-status-producer-feeds-address-mode-consumer",{opcode});
		if (prefix==2) initial.program[1].value=stm ? final_st1 : 7;
		initial.program[prefix+gap].value=cmpt ? store ? 0x8093 : 0x1093 : store ? 0x8027 : 0x1027;
		initial.data={{first,0x1111},{second,0x2222}};
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_A,0x7777}});
		if (full) initial.initial.push_back({r::STATE_B,final_st1});
		if (cmpt) initial.initial.insert(initial.initial.end(),{{r::STATE_AR0,0x1400},{r::STATE_AR3,0x1500}});
		auto final=observation("documented-gap-selects-new-address-mode-once",{{r::STATE_ST0,final_st0},{r::STATE_ST1,final_st1},
			{r::STATE_A,store ? 0x7777U : cmpt || set ? 0x2222U : 0x1111U}},
			{{first,store && !cmpt && !set ? 0x7777 : 0x1111},{second,store && (cmpt || set) ? 0x7777 : 0x2222}});
		if (cmpt) final.expected.insert(final.expected.end(),{{r::STATE_AR0,0x1400},{r::STATE_AR3,0x1501}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("address-mode-program-running",{})},{11,std::move(final)}},4+gap,11};
		c.require_idle=false; c.check_pipeline_clock=true;
		cases.push_back(std::move(c));
	}
	return cases;
}
inline std::vector<clocked_register_case> immediate_dp_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned page=0;page<512;++page) for (unsigned chain=0;chain<2;++chain) {
		u16 const first=0xea00|(chain ? page^1 : page);
		auto initial=initial_case("immediate-DP-forwards-all-nine-address-bits",{first});
		if (chain) initial.program[1].value=0xea00|page;
		initial.program[1+chain].value=0x107f;
		initial.data={{((page^1)<<7)|0x7f,0x1111},{(page<<7)|0x7f,0x2222}};
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,0x600|(page^1)},{r::STATE_ST1,0x800},{r::STATE_A,0}});
		auto before=observation("before-DP-publication",{{r::STATE_ST0,0x600|(page^1)},{r::STATE_ST1,0x800},{r::STATE_A,0}});
		auto status=observation("new-DP-before-consumer-result",{{r::STATE_ST0,0x600|page},{r::STATE_ST1,0x800},{r::STATE_A,0}});
		auto final=status; final.expected[2].value=0x2222;
		clocked_register_case c{{{0,std::move(initial)},{1,std::move(before)},{6+chain,std::move(status)},{7+chain,std::move(final)}},4+chain,10};
		c.require_idle=false; c.check_pipeline_clock=true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
