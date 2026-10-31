// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> repeated_mvmd_cases(bool zero)
{
	std::vector<clocked_register_case> cases;
	for(unsigned source=0;source<2;++source)for(unsigned count : {0U,1U,39U,255U})for(unsigned pattern=0;pattern<4;++pattern)for(unsigned header=0;header<2;++header){
		unsigned const seed=source+count+pattern+header,clear=zero ? header : 2;
		bool const long_header=zero || header;unsigned const first=long_header ? 7 : 6,body=long_header ? 2 : 1,finish=count+20;
		u32 const base=seed&1 ? 0x20000 : 0;u16 const destination=base ? 0x2400 : 0x122e,mmr=source ? 0xb : 8;
		u16 const value=std::array<u16,4>{0,1,0x8000,0xffff}[pattern],captured=clear==source ? 0 : value,status=0x800|((seed%4)<<8);
		u64 const selected=clear==source ? 0 : 0x1234560000ULL|value,other=clear==1-source ? 0 : 0xfeabcd9876ULL,changed=selected^0xffffULL;
		auto initial=initial_case(zero ? "RPTZ-clears-before-fixed-MMR-copy" : "repeat-resamples-fixed-MMR-and-advances-only-destination",{u16(zero ? 0xf071|(clear<<8) : long_header ? 0xf070 : 0xec00|count)});
		for(auto &word : initial.program)word.address+=base;
		if(long_header)initial.program[1].value=count;
		initial.program[body].value=0x7300|mmr;initial.program[body+1].value=destination;initial.program[body+2].value=0xf4e1;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x8620},{r::STATE_ST1,status},{r::STATE_SP,0x1e00},{r::STATE_T,0xbeef},
			{r::STATE_A,source ? 0xfeabcd9876ULL : 0x1234560000ULL|value},{r::STATE_B,source ? 0x1234560000ULL|value : 0xfeabcd9876ULL}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,0x3300+i});
		for(unsigned i=0;i<count+3;++i)initial.data.push_back({destination-1+i,0xdead});
		auto final=observation("all-copies-final-accumulators-and-guards",{{STATE_GENPC,base+0x600+body+3},{r::STATE_ST0,0x8620},{r::STATE_ST1,status},{r::STATE_SP,0x1e00},{r::STATE_T,0xbeef},{r::STATE_RC,0},
			{r::STATE_A,source ? other : changed},{r::STATE_B,source ? changed : other}},{{destination-1U,0xdead},{destination+count+1U,0xdead}});
		for(unsigned i=0;i<8;++i)final.expected.push_back({int(r::STATE_AR0+i),0x3300+i});
		for(unsigned i=0;i<=count;++i)final.expected_data.push_back({destination+i,u16(i>count/2 ? ~captured : captured)});
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},std::array<unsigned,3>{4,first,first+count/2}[seed%3],finish};
		c.program_events={{false,base+0x600+body+1,destination,0,false}};c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<=count;++i){
			c.data_events->push_back({false,mmr,u16(i>count/2 ? ~captured : captured),first+i,true});
			if(i)c.data_events->push_back({true,destination+i-1,u16(i-1>count/2 ? ~captured : captured),first+i,true});
		}
		c.data_events->push_back({true,destination+count,u16(count>count/2 ? ~captured : captured),first+count+1,true});
		c.actions.push_back({first-1,{},false,{{base+0x600+body+1,0x5000}}});
		c.actions.push_back({first+count/2,{{source ? r::STATE_B : r::STATE_A,changed}}});cases.push_back(std::move(c));
	}
	return cases;
}
}
