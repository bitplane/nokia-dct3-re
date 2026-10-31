// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned predicate=0;predicate<17;++predicate)for(unsigned layout=0;layout<4;++layout)for(unsigned count : {0U,1U,2U,7U})
	for(bool endpoint : {false,true})for(u32 base : {0U,0x20000U})for(unsigned rotation=0;rotation<2;++rotation){
		bool const b=predicate>=6 && predicate<12;
		auto word=[=](unsigned i)->u16{return std::array<u16,8>{0,1,0x8000,0xffff,0x7fff,2,0x8001,0xfffe}[(i+rotation*4)%8];};
		auto selected=[=](unsigned i){
			if(predicate>=12)return predicate==14 || ((predicate==13 || predicate==15) ? bool(rotation) : !rotation);
			int const value=s16(word(i));
			switch(predicate%6){case 0:return value>=0;case 1:return value<0;case 2:return value!=0;case 3:return value==0;case 4:return value>0;default:return value<=0;}
		};
		u16 const end=endpoint ? (layout==3 ? 0x606 : 0x607) : 0x60b, st0=0xa608|(rotation ? 0x1800 : 0);
		u16 const opcode=predicate<12 ? 0xfd42+predicate%6+(b ? 8 : 0) : predicate==12 ? 0xfd20 : predicate==13 ? 0xfd30 : predicate==15 ? 0xfd0c : predicate==16 ? 0xfd08 : 0xfd00;
		auto initial=initial_case("block-XC-retains-predicate-and-selects-complete-slots",{0xf072,end,u16(b ? 0x1192 : 0x1092)});
		for(auto &entry : initial.program)entry.address+=base;
		initial.program.insert(initial.program.end(),{{base+0x605,u16(opcode|((layout==1 || layout==2) ? 0x200 : 0))},
			{base+0x606,u16(layout==2 ? 0x7693 : (b ? 0x8193 : 0x8093))},{base+0x607,u16(layout==2 ? 0xfd43 : (b ? 0x8194 : 0x8094))},
			{base+end+9,0xf4e1}});
		if(layout==2)initial.program.push_back({base+0x608,u16(b ? 0x8194 : 0x8094)});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,0x900},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_BRC,count},{r::STATE_AR2,0x5000},{r::STATE_AR3,0x5100},{r::STATE_AR4,0x5200}});
		unsigned first=0;for(unsigned i=0;i<=count;++i)first+=selected(i);
		unsigned const second=layout==1 ? first : layout>=2 && endpoint ? 1 : count+1;
		u64 const result=u64(s64(s16(word(count))))&0xffffffffffULL;
		auto final=observation("selected-slot-counts-and-final-controller-state",{{STATE_GENPC,base+end+10},{r::STATE_ST0,st0},{r::STATE_ST1,0x900},
			{r::STATE_BRC,0},{r::STATE_RSA,0x602},{r::STATE_REA,end},{r::STATE_AR2,0x5001+count},{r::STATE_AR3,0x5100+first},{r::STATE_AR4,0x5200+second},
			{r::STATE_A,b ? 0x123456789aULL : result},{r::STATE_B,b ? result : 0xfedcba9876ULL}});
		for(unsigned i=0;i<=count;++i){initial.data.insert(initial.data.end(),{{0x5000+i,word(i)},{0x5100+i,0xaaaa},{0x5200+i,0xbbbb}});}
		clocked_register_case c{{{0,std::move(initial)},{120+count*12,std::move(final)}},std::array<unsigned,4>{4,11,13,24}[(predicate+layout+count+endpoint+rotation+unsigned(base!=0))%4],120+count*12};
		c.data_events=std::vector<clocked_data_event>{};unsigned a=0,d=0;
		for(unsigned i=0;i<=count;++i){
			clocked_data_event read{false,0x5000+i,word(i),0,false};read.registers={{r::STATE_BRC,count-i},{r::STATE_ST1,u64(0x900|(i<count ? 0x8000 : 0))}};c.data_events->push_back(read);
			if(layout<2 && endpoint && i && (layout==0 || selected(i-1))){c.data_events->push_back({true,0x5200+d,word(i-1),0,false});c.states.back().observation.expected_data.push_back({0x5200+d++,word(i-1)});}
			if(layout>=2 && endpoint && i && selected(i-1)){u16 const value=layout==2 ? 0xfd43 : word(i-1);c.data_events->push_back({true,0x5100+a,value,0,false});c.states.back().observation.expected_data.push_back({0x5100+a++,value});}
			if(selected(i) && !(layout>=2 && endpoint)){u16 const value=layout==2 ? 0xfd43 : word(i);c.data_events->push_back({true,0x5100+a,value,0,false});c.states.back().observation.expected_data.push_back({0x5100+a++,value});}
			if((layout==0 && !endpoint) || (layout==1 && !endpoint && selected(i)) || (layout>=2 && !endpoint)){
				c.data_events->push_back({true,0x5200+d,word(i),0,false});c.states.back().observation.expected_data.push_back({0x5200+d++,word(i)});
			}
		}
		if(layout<2 && endpoint && (layout==0 || selected(count))){c.data_events->push_back({true,0x5200+d,word(count),0,false});c.states.back().observation.expected_data.push_back({0x5200+d++,word(count)});}
		if(layout>=2 && endpoint && selected(count)){u16 const value=layout==2 ? 0xfd43 : word(count);c.data_events->push_back({true,0x5100+a,value,0,false});c.states.back().observation.expected_data.push_back({0x5100+a++,value});}
		if(layout>=2 && endpoint){c.data_events->push_back({true,0x5200,word(count),0,false});c.states.back().observation.expected_data.push_back({0x5200,word(count)});}
		// Untouched output tails distinguish rejected slots from wrong destinations.
		for(unsigned i=first;i<=count;++i)c.states.back().observation.expected_data.push_back({0x5100+i,0xaaaa});
		for(unsigned i=second;i<=count;++i)c.states.back().observation.expected_data.push_back({0x5200+i,0xbbbb});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
