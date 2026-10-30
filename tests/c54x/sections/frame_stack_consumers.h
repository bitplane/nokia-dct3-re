// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned encoded=0;encoded<256;++encoded) {
		int const offset=encoded<128 ? int(encoded) : int(encoded)-256;
		u16 const allocated=0x1000+offset,target=allocated-1;
		auto initial=initial_case("FRAME-signed-offset-then-PSHM-owned-stack-address",{u16(0xee00|encoded),0x4a11});
		initial.data={{target,0xaaaa}};
		initial.initial.push_back({r::STATE_AR1,0x5678});
		auto state=[](char const *name,u16 sp) { return observation(name,{{r::STATE_SP,sp},{r::STATE_ST0,0xe},
			{r::STATE_ST1,0x800},{r::STATE_AR1,0x5678}}); };
		auto final=state("pushed-original-register-at-allocated-top",target); final.expected_data={{target,0x5678}};
		clocked_register_case c{{{0,std::move(initial)},{1,state("before-FRAME",0x1000)},{4,state("FRAME-allocation-visible",allocated)},
			{6,state("PSHM-decrements-once",target)},{10,std::move(final)}},std::array<unsigned,3>{3,4,6}[encoded%3],10};
		c.require_idle=false; c.check_pipeline_clock=true;
		c.data_events=std::vector<clocked_data_event>{{false,0x11,0x5678,6},{true,target,0x5678,7}};
		cases.push_back(std::move(c));
	}
	for (unsigned producer=0;producer<3;++producer) for (unsigned store=0;store<2;++store) {
		bool const pop=producer!=0, other=producer==2;
		u16 const alternate=pop ? 0x1028 : 0x1026, opcode=pop ? other ? 0x8a0b : 0x8a11 : 0x4a11;
		auto initial=initial_case("gapped-stack-update-feeds-CPL-load-or-store",{opcode,0xf495,u16(store ? 0x8027 : 0x1027)});
		initial.data={{0xfff,0xaaaa},{0x1000,0x5678},{0x1027,0x1111},{alternate,0x2222}};
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST1,0x4800},{r::STATE_AR1,0x5678},{r::STATE_A,0x7777},{r::STATE_B,0}});
		auto final=observation("one-required-gap-selects-updated-SP-not-old-address",{{r::STATE_SP,pop ? 0x1001U : 0xfffU},
			{r::STATE_A,store ? 0x7777U : 0x2222U}},{{0xfff,pop ? 0xaaaa : 0x5678},{0x1027,0x1111},{alternate,store ? 0x7777 : 0x2222}});
		if (pop) final.expected_data.push_back({u32(other ? 0xb : 0x11),0x5678});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("stack-consumer-running",{})},{10,std::move(final)}},6,10};
		c.require_idle=false; c.check_pipeline_clock=true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
