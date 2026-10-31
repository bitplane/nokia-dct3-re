// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> mapped_dual_cases(unsigned kind)
{
	std::vector<clocked_register_case> cases;bool const edge=kind==1,interrupts=kind==2,low=kind==3;
	unsigned const addresses=edge ? 74 : interrupts ? 2 : low ? 4 : 16;
	for(unsigned address=0;address<addresses;++address)for(unsigned lanes=0;lanes<3;++lanes)for(unsigned selector=0;selector<(edge ? 1U : 8U);++selector)
	for(bool subtract : {false,true})for(unsigned destination=0;destination<2;++destination)for(unsigned bk_mode=0;bk_mode<(edge ? 1U : 2U);++bk_mode){
		unsigned const seed=address+lanes+selector+subtract+destination+bk_mode,flags=edge ? 0 : seed%4,pattern=edge ? 0 : (seed/2)%2;
		u32 const base=seed&1 ? 0x20000 : 0;u16 const mapped=edge ? address<2 ? address+6 : 0x18+address-2 : interrupts ? address : low ? address+2 : 8+address,bk=bk_mode ? 32 : 0;
		unsigned const selectors=edge ? 0x89 : std::array<unsigned,8>{0x01,0x0a,0x89,0x45,0xcd,0x88,0x4c,0xf2}[selector],x=selectors>>4,y=selectors&15,xar=2+(x&3),yar=2+(y&3);
		std::array<u16,8> ar{3,0x1700,0x101f,0x111f,0x121f,0x131f,0x1800,0x1900};
		if(lanes!=1)ar[xar]=mapped;
		if(lanes!=0)ar[yar]=lanes==2 && xar!=yar ? edge ? mapped==0x5f ? 6 : mapped+1 : interrupts ? (address+1)%2 : low ? 2+(address+1)%4 : 8+(address+5)%16 : mapped;
		u16 const xa=ar[xar],ya=ar[yar];auto after=ar;
		auto modify=[=](u16 value,unsigned mode)->u16{return mode==1 ? value-1 : mode==2 ? value+1 : mode==3 ? bk ? (value&0xffc0)|((value+3)&31) : value+3 : value;};
		if(!edge){after[xar]=modify(ar[xar],x>>2);if(xar!=yar)after[yar]=modify(ar[yar],y>>2);}
		u64 a=pattern ? 0x7fffff8001ULL : 0x8055557fffULL,b=pattern ? 0x8055557fffULL : 0x7fffff8001ULL;
		u16 const st0=0xa002|(pattern ? 0x800 : 0),st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0),imr=interrupts && pattern ? 0xa55a : 0,ifr=interrupts && pattern ? 0x5aa5 : 0;
		auto sample=[&](u16 at)->u16{if(at>=0x60)return pattern ? 0x7fff : 0x8001;if(at<2)return at ? ifr : imr;if(at<6)return pattern ? 0xf495 : 0x8001;if(at>=0x10 && at<=0x17)return after[at-0x10];if(at>=8 && at<=13)return (at<11 ? a : b)>>(((at-8)%3)*16);return at==14 ? 0x8001 : 0x7fff;};
		u16 const xword=sample(xa),yword=sample(ya),opcode=0xa000|(subtract ? 0x200 : 0)|(destination<<8)|selectors;
		s64 const left=s64(flags&1 ? s16(xword) : s32(xword))*65536,right=s64(flags&1 ? s16(yword) : s32(yword))*65536,result=subtract ? left-right : left+right;
		bool const overflow=result<-2147483648LL || result>2147483647LL;
		u64 const output=u64(flags&2 ? std::clamp(result,s64(-2147483648LL),s64(2147483647LL)) : result)&0xffffffffffULL;
		u16 const status=(st0&~0x800)|((subtract ? xword>=yword : u32(xword)+yword>65535) ? 0x800 : 0)|(overflow ? destination ? 0x200 : 0x400 : 0);
		auto initial=initial_case(edge ? "dual-ALU-rejects-unowned-source-before-effects" : "dual-ALU-Read-samples-mapped-sources-after-Access",{opcode,0xf4e1});
		for(auto &word : initial.program)word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_IMR,imr},{r::STATE_IFR,ifr},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0x8001},{r::STATE_TRN,0x7fff},{r::STATE_SP,0x3000},{r::STATE_BK,bk}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,ar[i]});
		for(auto [at,word] : {std::pair{xa,xword},std::pair{ya,yword}})if(at>=0x60 || (low && at>=2 && at<=5))initial.data.push_back({at,word});
		std::vector<memory_word> poison;
		if(!edge){for(u16 at : {xa,ya}){
			if(at>=2)poison.push_back({at,0x5a5a});
			if(at>=8 && at<=13){unsigned const part=(at-8)%3,shift=16*part;u64 const mask=part==2 ? 0xff : 0xffff;u64 &acc=at<11 ? a : b;acc=(acc&~(mask<<shift))|((0x5a5a&mask)<<shift);}
			if(at>=0x10 && at<=0x17)after[at-0x10]=0x5a5a;
		}(destination ? b : a)=output;}
		auto final=observation("retained-arithmetic-result-and-unselected-fields",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,edge ? st0 : status},{r::STATE_ST1,st1},{r::STATE_SP,0x3000},{r::STATE_BK,bk},
			{r::STATE_T,!edge && (xa==14 || ya==14) ? 0x5a5aU : 0x8001U},{r::STATE_TRN,!edge && (xa==15 || ya==15) ? 0x5a5aU : 0x7fffU}});
		for(unsigned i=0;i<8;++i)final.expected.push_back({int(r::STATE_AR0+i),after[i]});
		if(edge){initial.expected.clear();final.expected[8].value=1;}
		if(interrupts)final.expected.insert(final.expected.end(),{{r::STATE_IMR,0x0f0f},{r::STATE_IFR,0xf0f0}});
		clocked_register_case c{{{0,std::move(initial)},{12,std::move(final)}},edge ? 3+seed%2 : std::array<unsigned,3>{3,5,6}[seed%3],12};c.require_idle=!edge;c.clear_pipeline_before_restore=true;
		if(edge){c.states.back().tick=4;c.states.insert(c.states.begin()+1,{3,observation("before-Access-rejection",{})});}
		c.data_events=std::vector<clocked_data_event>{};
		if(!edge){c.data_events->push_back({false,xa,xword,5,true});c.data_events->push_back({false,ya,yword,5,true});if(interrupts)for(auto &event : *c.data_events)event.registers={{r::STATE_IMR,imr},{r::STATE_IFR,ifr}};clocked_register_action action{5,{},false,{},{},poison};if(interrupts)action.values={{r::STATE_IMR,0x0f0f},{r::STATE_IFR,0xf0f0}};c.actions.push_back(std::move(action));}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
