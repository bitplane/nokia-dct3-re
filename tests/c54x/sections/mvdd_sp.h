// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	// SPRU172C 4-107: MVDD copies Xmem to Ymem without status effects.
	std::vector<clocked_register_case> cases;
	for (unsigned index=0; index<8; ++index) {
		u16 const value=std::array<u16,4>{0,1,0x8000,0xffff}[index/2];
		u16 const old_sp=index&1 ? 0xffff : 0x3000;
		auto initial=initial_case("MVDD-RAM-to-SP", {0xe51a,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_SP,old_sp},{r::STATE_AR3,0x1400},
			{r::STATE_AR4,0x18},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x0800},
			{r::STATE_A,0x12345678},{r::STATE_B,0x87654321}});
		initial.data={{0x1400,value}};
		clocked_register_case c{{{0,std::move(initial)},
			{4,observation("Address-updates-Y-pointer-only", {{r::STATE_AR3,0x1400},{r::STATE_AR4,0x19},{r::STATE_SP,old_sp}})},
			{5,observation("Read-does-not-publish-SP", {{r::STATE_SP,old_sp}})},
			{6,observation("Execute-writes-retained-source-to-SP", {{r::STATE_SP,value}})},
			{20,observation("copy-preserves-status-and-accumulators", {{r::STATE_SP,value},{r::STATE_AR3,0x2200},
				{r::STATE_AR4,0x2300},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x0800},
				{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},{STATE_GENPC,0x602}})}},4+index%3,20};
		c.actions={{4,{{r::STATE_AR3,0x2200},{r::STATE_AR4,0x2300}}},
			{5,{},false,{},{},{{0x1400,u16(value^0xffff)}}}};
		c.data_events=std::vector<clocked_data_event>{{false,0x1400,value,5},{true,0x18,value,6}};
		cases.push_back(std::move(c));
	}
	for (unsigned index=0; index<4; ++index) {
		u16 const destination=index<2 ? 0x17+index*2 : 0x18;
		auto initial=initial_case("MVDD-unqualified-mapped-destination", {u16(index==2 ? 0xec00 : index==3 ? 0xe51b : 0xe51a),0xf4e1});
		if (index==2) { initial.program[1].value=0xe51a; initial.program[2].value=0xf4e1; }
		initial.initial.insert(initial.initial.end(), {{r::STATE_AR3,0x1400},{r::STATE_AR4,destination},{r::STATE_SP,0x3000}});
		initial.expected={{r::STATE_AR3,0x1400},{r::STATE_AR4,destination},{r::STATE_SP,0x3000}};
		clocked_register_case c{{{0,std::move(initial)},
			{20,observation("reject-before-pointer-or-bus-effects", {{r::STATE_ILLEGAL,1},
				{r::STATE_AR3,0x1400},{r::STATE_AR4,destination},{r::STATE_SP,0x3000}})}},3+index%3,20};
		c.require_idle=false;
		c.forbid_data_access=true;
		for (auto &check:c.states.back().observation.expected)
			if (check.index==r::STATE_ILLEGAL) check.value=1;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
