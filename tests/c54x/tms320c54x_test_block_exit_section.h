// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> block_exit_cases(bool delayed)
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,2U,7U}) for(unsigned pattern=0;pattern<4;++pattern) for(unsigned form=0;form<(delayed ? 13U : 3U);++form)
	for(bool endpoint : {false,true}) for(u32 base : {0U,0x20000U}) for(unsigned reverse=0;reverse<(delayed ? 2U : 1U);++reverse){
		// Unconditional branches cannot distinguish the four exit-input patterns.
		if(!form && pattern)continue;
		bool const b=delayed ? form>=7 : form==2;
		unsigned const exit=!form || !pattern ? 0 : pattern==1 ? count+1 : pattern==2 ? count/2 : count;
		bool const taken=exit<=count;
		unsigned const reads=taken ? exit+1 : count+1, body=taken ? exit : count+1, writes=body+unsigned(taken && endpoint);
		u16 const tail=delayed ? 0x607 : 0x605, end=(delayed ? 2 : 0)+(endpoint ? 0x606 : 0x609);
		auto value=[=](bool condition)->u16 {
			if(!delayed || !form)return condition ? 0 : 1;
			switch((form-1)%6){case 0:return condition ? 0 : 0xffff;case 1:return condition ? 0xffff : 0;case 2:case 4:return condition ? 1 : 0;default:return condition ? 0 : 1;}
		};
		auto initial=initial_case("forward-block-exit-preserves-controller-until-explicit-clear",{0xf072,end});
		for(auto &word : initial.program)word.address+=base;
		for(unsigned i=0x620;i<0x720;++i)initial.program.push_back({base+i,0xf495});
		initial.program.insert(initial.program.end(),{{base+0x602,u16(b ? 0x1192 : 0x1092)},
			{base+0x603,u16(delayed ? (form ? 0xfa42+(form-1)%6+(b ? 8 : 0) : 0xf273) : (form ? (b ? 0xf84d : 0xf845) : 0xf073))},
			{base+0x604,0x700},{base+tail,0x7693},{base+tail+1,0x1111},{base+end+1,0xf4e1},
			{base+0x700,0x76f8},{base+0x701,0x5200},{base+0x702,0xcccc},{base+0x703,0xf6bf},
			{base+0x70a,0xf073},{base+0x70b,u16(endpoint ? tail : end)}});
		if(delayed)initial.program.insert(initial.program.end(),{{base+0x605+reverse,u16(b ? 0x1195 : 0x1095)},{base+0x606-reverse,u16(b ? 0x8194 : 0x8094)}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_A,0x1234},{r::STATE_B,0x2345},
			{r::STATE_SP,0x3000},{r::STATE_BRC,count},{r::STATE_AR2,0x5000},{r::STATE_AR3,0x5100},{r::STATE_AR4,0x5300},{r::STATE_AR5,0x5400}});
		auto final=observation("exact-exit-tail-and-spaced-endpoint-revisit",{{STATE_GENPC,base+end+2},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},
			{r::STATE_BRC,taken ? count-exit : 0},{r::STATE_RSA,0x602},{r::STATE_REA,end},{r::STATE_SP,0x3000},
			{r::STATE_AR2,0x5000+reads},{r::STATE_AR3,0x5100+writes},{r::STATE_AR4,0x5300+(delayed ? reads : 0)},{r::STATE_AR5,0x5400+(delayed ? reads : 0)}});
		for(unsigned i=0;i<count+2;++i){initial.data.insert(initial.data.end(),{{0x5000+i,value(i==exit)},{0x5100+i,0xaaaa},{0x5400+i,value(i!=exit)}});final.expected_data.push_back({0x5100+i,u16(i<writes ? 0x1111 : 0xaaaa)});}
		clocked_register_case c{{{0,std::move(initial)},{256,std::move(final)}},std::array<unsigned,6>{3,8,9,10,12,18}[(count+pattern+form+endpoint+reverse+unsigned(base!=0))%6],256};
		c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<reads;++i){
			clocked_data_event read{false,0x5000+i,value(i==exit),0,false};
			read.registers={{r::STATE_BRC,count-i},{r::STATE_ST1,u64(0x900|(i<count ? 0x8000 : 0))}};
			c.data_events->push_back(read);
			if(endpoint && i && i-1<body)c.data_events->push_back({true,0x5100+i-1,0x1111,0,false});
			if(delayed){
				clocked_data_event load{false,0x5400+i,value(i!=exit),0,false}, store{true,0x5300+i,value(reverse ? i==exit : i!=exit),0,false};
				if(reverse && form){c.data_events->push_back(store);c.data_events->push_back(load);}else{c.data_events->push_back(load);c.data_events->push_back(store);}
			}
			if(!endpoint && i<body)c.data_events->push_back({true,0x5100+i,0x1111,0,false});
		}
		if(endpoint && body==reads)c.data_events->push_back({true,0x5100+body-1,0x1111,0,false});
		if(taken){
			clocked_data_event marker{true,0x5200,0xcccc,0,false};marker.registers={{r::STATE_BRC,count-exit},{r::STATE_ST1,u64(0x900|(exit<count ? 0x8000 : 0))}};
			c.data_events->push_back(marker);
			if(endpoint){clocked_data_event revisit{true,0x5100+body,0x1111,0,false};revisit.registers={{r::STATE_ST1,0x900}};c.data_events->push_back(revisit);}
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
