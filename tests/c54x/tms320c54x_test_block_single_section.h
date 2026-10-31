// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> block_single_cases(bool memory)
{
	std::vector<clocked_register_case> cases;
	for(unsigned outer : {0U,1U,2U,7U})for(unsigned inner : {0U,1U,2U,7U})for(unsigned kind=0;kind<3;++kind)
	for(bool endpoint : {false,true})for(u32 base : {0U,0x20000U})for(unsigned form=0;form<(memory ? 5U : 1U);++form){
		u16 const body=0x604+unsigned(memory && form==4), end=body+unsigned(kind==1)+(endpoint ? 0 : 3), st1=memory && form==1 ? 0x4800 : 0x800;
		unsigned const total=(outer+1)*(inner+1)+(memory && form==3 ? (outer+1)/2 : 0);
		auto initial=initial_case("block-and-single-repeat-own-independent-counts",{0xf072,end,0x6d94});
		for(auto &word : initial.program)word.address+=base;
		initial.program.insert(initial.program.end(),{{base+0x603,u16(memory ? std::array<u16,5>{0x4760,0x4760,0x4780,0x4790,0x47f8}[form] : 0xec00|inner)},
			{base+body,std::array<u16,3>{0xe589,0x7693,0xf495}[kind]},{base+end+1,0x770e},{base+end+2,0x1a2b},{base+end+3,0xf4e1}});
		if(memory && form==4)initial.program.push_back({base+0x604,0x1060});
		if(kind==1)initial.program.push_back({base+body+1,0x1234});
		if(!endpoint)initial.program.push_back({base+body+1+unsigned(kind==1),0x6d95});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x8820},{r::STATE_ST1,st1},{r::STATE_BRC,outer},{r::STATE_RC,0},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5555},{r::STATE_SP,0x1000}});
		for(unsigned ar=0;ar<8;++ar)initial.initial.push_back({r::STATE_AR0+ar,ar==0 && memory ? 0x1060U : ar==2 ? 0x5000U : ar==3 ? 0x6000U : ar==4 ? 0x7000U : ar==5 ? 0x7100U : 0x7200+ar});
		auto final=observation("inner-body-count-and-outer-prefix-suffix-settle",{{STATE_GENPC,base+end+4},{r::STATE_ST0,0x8820},{r::STATE_ST1,st1},{r::STATE_BRC,0},{r::STATE_RC,0},
			{r::STATE_RSA,0x602},{r::STATE_REA,end},{r::STATE_T,0x1a2b},{r::STATE_SP,0x1000},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},
			{r::STATE_AR2,0x5000+(kind==0 ? total : 0)},{r::STATE_AR3,0x6000+(kind<2 ? total : 0)},{r::STATE_AR4,0x7001+outer},{r::STATE_AR5,0x7100+(endpoint ? 0 : outer+1)}});
		if(memory)final.expected.push_back({r::STATE_AR0,0x1060+(form==3 ? outer+1 : 0)});
		for(unsigned i=0;i<total+1;++i){initial.data.insert(initial.data.end(),{{0x5000+i,u16(0x9000+i)},{0x6000+i,0xdead}});final.expected_data.push_back({0x6000+i,u16(kind<2 && i<total ? (kind==1 ? 0x1234 : 0x9000+i) : 0xdead)});}
		if(memory)for(unsigned i=0;i<=outer;++i)initial.data.push_back({0x1060+i,u16(inner+(form==3 ? i&1 : 0))});
		clocked_register_case c{{{0,std::move(initial)},{600,std::move(final)}},std::array<unsigned,4>{7,10,13,17}[(outer+inner+kind+endpoint+form+unsigned(base!=0))%4],600};
		c.data_events=std::vector<clocked_data_event>{};unsigned index=0;
		for(unsigned o=0;o<=outer;++o){
			if(memory){c.data_events->push_back({false,0x1060+(form==3 ? o : 0),u16(inner+(form==3 ? o&1 : 0)),0,false});if(form==3)c.volatile_data_reads.push_back({0x1060+o,u16(inner+(o&1))});}
			unsigned const copies=inner+1+(memory && form==3 ? o&1 : 0);
			for(unsigned j=0;j<copies;++j,++index){
				if(kind==0){
					c.data_events->push_back({false,0x5000+index,u16(0x9000+index),0,false});
					// Memory-count setup drains the first copy before cloned
					// issues overlap Read with the preceding Execute Write.
					if(memory && !j)c.data_events->push_back({true,0x6000+index,u16(0x9000+index),0,false});
					else if(j>unsigned(memory))c.data_events->push_back({true,0x6000+index-1,u16(0x9000+index-1),0,false});
				}
				if(kind==1)c.data_events->push_back({true,0x6000+index,0x1234,0,false});
			}
			if(kind==0 && (!memory || copies>1))c.data_events->push_back({true,0x6000+index-1,u16(0x9000+index-1),0,false});
		}
		c.data_events->push_back({true,0xe,0x1a2b,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
