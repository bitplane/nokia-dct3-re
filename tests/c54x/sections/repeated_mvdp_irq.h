// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,7U,255U})for(unsigned prefix : {1U,2U})for(bool nmi : {false,true})
	for(unsigned arrival_index=0;arrival_index<3;++arrival_index)for(u32 base : {0U,0x20000U}){
		unsigned const arrival=std::array<unsigned,3>{3,6,count>=7 ? 12U : 8U}[arrival_index],seed=count+prefix+nmi+arrival_index+(base!=0),finish=count+100;
		int const line=nmi ? INPUT_LINE_NMI : 0;u32 const vector=base+(nmi ? 4 : 0x40);
		auto initial=initial_case("repeated-MVDP-drains-before-IRQ-frame",{0xf495});
		for(auto &word : initial.program)word.address+=base;
		initial.program[0].value=prefix==2 ? 0xf070 : 0xec00|count;if(prefix==2)initial.program[1].value=count;
		initial.program[prefix].value=0x7d92;initial.program[prefix+1].value=0x2e00;initial.program[prefix+2].value=0xf4e1;
		initial.program.insert(initial.program.end(),{{vector,0x4a1e},{vector+1,0xe955},{vector+2,0xf4e5}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa624},{r::STATE_ST1,nmi ? 0x800U : 0U},{r::STATE_SP,0x1200},{r::STATE_BK,16},{r::STATE_AR2,0x1230},{r::STATE_B,0},{r::STATE_IMR,nmi ? 0U : 1U}});
		auto final=observation("balanced-ISR-and-all-transfers",{{r::STATE_SP,0x1200},{r::STATE_AR2,0x1231+count},{r::STATE_B,0x55}});
		for(unsigned i=0;i<=count;++i){initial.data.push_back({0x1230+i,u16(0x2468+i)});initial.program.push_back({base+0x2e00+i,0xbeef});final.expected_program.push_back({base+0x2e00+i,u16(0x2468+i)});}
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},std::array<unsigned,3>{4,9,14}[seed%3],finish};
		c.initial_input_lines={{0,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};c.actions={{arrival,{},{},{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}}};
		c.data_events=std::vector<clocked_data_event>{};for(unsigned i=0;i<=count;++i)c.data_events->push_back({false,0x1230+i,u16(0x2468+i),0,false});
		clocked_data_event frame{true,0x11ff,u16(0x602+prefix),0,false};frame.value_register=r::STATE_RTN;frame.minimum_program_events=count+1;
		c.data_events->push_back(frame);c.data_events->push_back({false,0x1e,u16(base>>16),0,false});c.data_events->push_back({true,0x11fe,u16(base>>16),0,false});c.data_events->push_back({false,0x11fe,u16(base>>16),0,false});frame.write=false;c.data_events->push_back(frame);
		for(unsigned i=0;i<=count;++i)c.program_events.push_back({true,base+0x2e00+i,u16(0x2468+i),9+prefix+i,true});
		c.irqs={{line,9+prefix+count,~0U,count+1,u16(0x602+prefix),{}}};cases.push_back(std::move(c));
	}
	return cases;
}
}
