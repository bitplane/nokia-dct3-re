// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline bool mvdd_mapped_owned(u16 address)
{ return address<=1 || (address>=6 && address<=0x1e) || (address>=0x24 && address<=0x26) || address==0x58; }
inline u16 mvdd_mapped_word(u16 address,unsigned pattern,u32 base,u16 ar2,u16 ar3)
{
	if(address==0)return 0x1234;
	if(address==1)return 0;
	if(address==6)return 0xa608;
	if(address==7)return 0x900;
	if(address>=8 && address<=0xd){u64 const value=address<0xb ? std::array<u64,4>{0,0x123456789aULL,0xff80000001ULL,0xffffffffffULL}[pattern] : std::array<u64,4>{0xffffffffffULL,0xabcdef0123ULL,0x007fffffffULL,0}[pattern];return value>>(((address-8)%3)*16);}
	if(address==0xe)return 0x7000|pattern;
	if(address==0xf)return 0xa500|pattern;
	if(address>=0x10 && address<=0x17)return address==0x10 ? 3 : address==0x12 ? ar2 : address==0x13 ? ar3 : 0x4000+address-0x10;
	if(address==0x18)return 0x6000;
	if(address==0x1e)return base>>16;
	if(address>=0x19 && address<=0x1d)return 0;
	if(address==0x24)return 0x1234;
	if(address==0x25)return 0x5678;
	if(address==0x26)return 0x10;
	if(address==0x58)return 0;
	return std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
}
inline std::vector<clocked_register_case> mvdd_source_cases(bool repeated)
{
	std::vector<clocked_register_case> cases;
	for(unsigned layout=0;layout<(repeated ? 5U : 96U);++layout)for(unsigned pattern=0;pattern<4;++pattern)
	for(unsigned x=0;x<(repeated ? 2U : 4U);++x)for(unsigned y=0;y<(repeated ? 1U : 4U);++y){
		unsigned const seed=layout+pattern+x+y,prefix=repeated ? x+1 : 0,count=repeated ? (!layout ? 128 : 4) : 1;
		bool const fixed=repeated && layout==4;unsigned const xm=repeated ? fixed ? 0 : 2 : x,ym=repeated ? 2 : y;
		u16 const source=repeated ? std::array<u16,5>{0,0x1f,0x5f,0x12,0x33}[layout] : layout;
		u32 const base=seed&1 ? 0x20000 : 0;u16 const opcode=0xe500|((1|(xm<<2))<<4)|(ym<<2);
		auto initial=initial_case("MVDD-mapped-Read-ownership-and-peripheral-wait",{0xf495});
		for(auto &word : initial.program)word.address+=base;
		if(prefix)initial.program[0].value=prefix==1 ? 0xec00|(count-1) : 0xf070;
		if(prefix==2)initial.program[1].value=count-1;
		initial.program[prefix].value=opcode;initial.program[prefix+1].value=0xf4e1;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_IMR,0x1234},{r::STATE_TIM,0x1234},{r::STATE_PRD,0x5678},
			{r::STATE_A,std::array<u64,4>{0,0x123456789aULL,0xff80000001ULL,0xffffffffffULL}[pattern]},{r::STATE_B,std::array<u64,4>{0xffffffffffULL,0xabcdef0123ULL,0x007fffffffULL,0}[pattern]},
			{r::STATE_T,0x7000|pattern},{r::STATE_TRN,0xa500|pattern},{r::STATE_SP,0x6000},{r::STATE_BK,0},{r::STATE_BRC,0},{r::STATE_RSA,0},{r::STATE_REA,0}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,i==0 ? 3U : i==2 ? 0x1000U : i==3 ? source : 0x4000+i});
		for(unsigned at=0;at<128;++at)if(!mvdd_mapped_owned(at))initial.data.push_back({at,mvdd_mapped_word(at,pattern,base,0,0)});
		initial.data.push_back({0x58,0});initial.data.push_back({0x58,0});
		std::vector<unsigned> reads;std::vector<memory_word> samples;unsigned waits=0;
		for(unsigned i=0;i<count;++i){u16 const at=source+(fixed ? 0 : i);waits+=at>=0x20 && at<0x60;unsigned const issued=std::min(i+2,count);
			reads.push_back(5+prefix+i+waits);samples.push_back({at,fixed && i ? u16(0) : mvdd_mapped_word(at,pattern,base,repeated ? 0x1000+issued : 0x2222,repeated ? source+(fixed ? 0 : issued) : 0x3333)});}
		unsigned const finish=reads.back()+13;auto final=observation("all-retained-copies-and-no-late-retarget",{{STATE_GENPC,base+0x602+prefix},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_AR2,repeated ? 0x1000+count : 0x4444},{r::STATE_AR3,repeated ? source+(fixed ? 0 : count) : 0x5555}});
		for(unsigned i=0;i<count;++i)final.expected_data.push_back({0x1000+i,samples[i].value});
		unsigned const first_peripheral=layout==0 ? 32 : layout==1 ? 1 : 0;
		unsigned const checkpoint=repeated ? std::array<unsigned,5>{4,reads[first_peripheral]-1,reads[first_peripheral],reads.back(),reads.back()+1}[seed%5] : 3+seed%4;
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},checkpoint,finish};c.program_events={{false,base+0x600+prefix,opcode,0,false}};c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<count;++i){c.data_events->push_back({false,samples[i].address,samples[i].value,reads[i],true});if(i)c.data_events->push_back({true,0x1000+i-1,samples[i-1].value,reads[i],true});
			if(samples[i].address>=0x20 && samples[i].address<0x60 && !mvdd_mapped_owned(samples[i].address))c.actions.push_back({reads[i],{},false,{},{},{{samples[i].address,0}}});}
		c.data_events->push_back({true,0x1000+count-1,samples.back().value,reads.back()+1,true});
		if(!repeated){auto modify=[](u16 value,unsigned mode)->u16{return value+(mode==1 ? -1 : mode==2 ? 1 : mode==3 ? 3 : 0);};
			auto access=observation("Access-modifies-both-pointers-once",{{r::STATE_AR2,modify(0x1000,ym)},{r::STATE_AR3,modify(source,xm)}});
			auto read=observation("Read-retains-source-before-later-pointer-change",{});c.states.insert(c.states.begin()+1,{{4,std::move(access)},{5,std::move(read)}});
			c.actions.push_back({4,{{r::STATE_AR2,0x2222},{r::STATE_AR3,0x3333}}});
			clocked_register_action poison{reads[0],{{r::STATE_AR2,0x4444},{r::STATE_AR3,0x5555}}};if(source==0xe)poison.values.push_back({r::STATE_T,0xdead});c.actions.push_back(std::move(poison));}
		std::stable_sort(c.actions.begin(),c.actions.end(),[](auto const &a,auto const &b){return a.tick<b.tick;});
		for(unsigned i=1;i<c.actions.size();)if(c.actions[i-1].tick==c.actions[i].tick){auto &old=c.actions[i-1];auto const &next=c.actions[i];old.values.insert(old.values.end(),next.values.begin(),next.values.end());old.data.insert(old.data.end(),next.data.begin(),next.data.end());c.actions.erase(c.actions.begin()+i);}else ++i;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
