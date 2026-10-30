// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_HIGH_PROGRAM
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (bool b : {false,true}) for (u32 caller : {0U,0x20000U,0x7f0000U}) for (u32 target : {0U,0x30000U,0x7f0000U})
	for (bool wide : {false,true}) for (unsigned source=0;source<3;++source) {
		auto initial=initial_case("delayed-accumulator-far-call-frame-and-IRQ-drain",{u16(b ? 0xf7e7 : 0xf6e7),
			u16(wide ? 0x7693 : b ? 0x890e : 0x880e),u16(wide ? 0x5aa5 : b ? 0x890f : 0x880f),0xf4e1});
		for (auto &word : initial.program) word.address+=caller;
		for (unsigned i=0;i<16;++i) initial.program.push_back({target+0x700+i,0xf495});
		initial.program.insert(initial.program.end(),{{target+0x700,u16(b ? 0x8192 : 0x8092)},{target+0x708,0xf4e4},{target+(source==2 ? 4U : 0x40U),0xf4eb}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,caller+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,source==1 ? 0x100U : 0x900U},
			{r::STATE_IMR,source==1 ? 1U : 0U},{r::STATE_AR2,0x1500},{r::STATE_AR3,0x1400},{r::STATE_AR4,0x4400},
			{r::STATE_A,b ? 0x1234 : 0x5500000700ULL|target},{r::STATE_B,b ? 0x5500000700ULL|target : 0x2345},
			{r::STATE_T,0x5aa5},{r::STATE_TRN,0x2468}});
		initial.data={{0x1400,0xdead},{0x1500,0xdead}};
		auto final=observation("retained-target-balanced-frames-and-once-only-delays",{{STATE_GENPC,caller+0x604},{r::STATE_SP,0x1000},
			{r::STATE_A,b ? 0x1234U : 0xdeadbeefU},{r::STATE_B,b ? 0xdeadbeefU : 0x2345U},
			{r::STATE_AR2,0x1501},{r::STATE_AR3,wide ? 0x1401U : 0x1400U},{r::STATE_AR4,0x4400},
			{r::STATE_T,wide ? 0x5aa5U : 0xbeefU},{r::STATE_TRN,wide ? 0x2468U : 0xbeefU},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,source ? 0x100U : 0x900U}},{{0x1500,0xbeef},{0x1400,u16(wide ? 0x5aa5 : 0xdead)}});
		clocked_register_case c{{{0,std::move(initial)},{64,std::move(final)}},std::array<unsigned,6>{3,6,7,8,10,12}[cases.size()%6],64};
		c.initial_input_lines={{0,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
		clocked_register_action poison{5,{{b ? r::STATE_B : r::STATE_A,0xdeadbeef}}};
		if (source) poison.input_lines={{source==2 ? INPUT_LINE_NMI : 0,ASSERT_LINE}};
		c.actions={poison};
		c.program_events={{false,target+0x700,u16(b ? 0x8192 : 0x8092),6}};
		c.data_events=std::vector<clocked_data_event>{{true,0xfff,0x603,5},{true,0xffe,u16(caller>>16),6}};
		c.data_events->at(0).program_events_before=0;c.data_events->at(1).program_events_before=1;
		if (wide) c.data_events->push_back({true,0x1400,0x5aa5,8});
		else {c.data_events->push_back({true,0xe,0xbeef,7});c.data_events->push_back({true,0xf,0xbeef,8});}
		c.data_events->push_back({true,0x1500,0xbeef,10});
		if (source) {
			unsigned const completed=c.data_events->size();
			clocked_data_event frame{true,0xffd,0,0,false};frame.value_range=std::array<u16,2>{0x700,0x708};frame.value_register=r::STATE_RTN;
			c.data_events->push_back(frame);frame.write=false;c.data_events->push_back(frame);
			c.irqs={{source==2 ? INPUT_LINE_NMI : 0,9,~0U,completed,{}}};
		}
		c.data_events->push_back({false,0xffe,u16(caller>>16),0,false});c.data_events->push_back({false,0xfff,0x603,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
