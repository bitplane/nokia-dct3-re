// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> modified_mvdk_cases(bool indexed)
{
	std::vector<clocked_register_case> cases;
	auto append=[&](unsigned encoding,bool compatibility,unsigned kind,u16 source,u16 bk,std::array<u16,8> ar,u16 word,unsigned seed){
		unsigned const encoded=encoding&7,selected=compatibility && !encoded ? 4 : encoded,operation=encoding>>3;
		u16 const mode=indexed ? (operation&2 ? 0xb0 : 0xa8)|encoded : 0xc0|encoding,destination=kind<10 ? 0x10+kind : kind<18 ? kind-2 : kind==18 ? 0x1a : kind==19 ? 0x1800 : source;
		bool const rejected=source<0x60 && (source<8 || source>0x19),bad_destination=destination<0x60 && (destination<8 || destination>0x1a),excluded=rejected || bad_destination;
		int const step=operation&1 ? s16(ar[0]) : 1,delta=indexed ? operation&2 ? int(s16(ar[0])) : -int(s16(ar[0])) : operation<2 ? -step : step;
		u16 next=source+delta;
		if(!indexed && bk){unsigned mask=1;while(mask<=bk)mask*=2;--mask;int const offset=(int(source&mask)+delta)%bk;next=(source&~mask)|u16(offset<0 ? offset+bk : offset);}
		u64 a=0x123456789aULL,b=0xfeabcd9876ULL;u16 t=0xbeef,trn=0x1357,sp=0x2200,final_bk=bk,brc=0;
		if(source<0x60){if(source>=8 && source<=13)word=(source<11 ? a : b)>>(((source-8)%3)*16);else if(source==14)word=t;else if(source==15)word=trn;else if(source>=16 && source<=23)word=ar[source-16];else if(source==24)word=sp;else if(source==25)word=bk;}
		auto after=ar;if(!excluded){after[selected]=next;
			if(destination>=8 && destination<=13){unsigned const part=(destination-8)%3,shift=part*16;u64 const mask=(part==2 ? 0xffULL : 0xffffULL)<<shift;u64 &acc=destination<11 ? a : b;acc=(acc&~mask)|((u64(word)<<shift)&mask);}
			else if(destination==14)t=word;else if(destination==15)trn=word;else if(destination<24)after[destination-16]=word;else if(destination==24)sp=word;else if(destination==25)final_bk=word;else if(destination==26)brc=word;
		}
		u32 const base=seed&1 ? 0x20000 : 0;u16 const st1=0x800|((seed%4)<<8)|(compatibility ? 0x20 : 0),pmst=(seed/4)&1,status=!excluded && compatibility && encoded ? 0x608|(encoded<<13) : 0x8608;
		auto initial=initial_case(indexed ? "indexed-MVDK-sample-modify-publish-alias-order" : "circular-MVDK-legal-modulo-and-alias-order",{u16(0x7100|mode),destination,0xf4e1});
		for(auto &entry : initial.program)entry.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x8608},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0xbeef},{r::STATE_TRN,0x1357},{r::STATE_SP,0x2200},{r::STATE_BK,bk},{r::STATE_BRC,0}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,ar[i]});
		if(source>=0x60)initial.data.push_back({source,word});
		auto access=observation("sample-precedes-pointer-and-DAGEN-destination-update",{{r::STATE_ST0,status}});
		for(unsigned i=0;i<8;++i)access.expected.push_back({int(r::STATE_AR0+i),after[i]});
		auto final=observation("no-recapture-or-repeated-update-after-poison",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,t},{r::STATE_TRN,trn},{r::STATE_SP,sp},{r::STATE_BK,final_bk},{r::STATE_BRC,brc},{r::STATE_ST0,status},{r::STATE_ST1,st1},{r::STATE_PMST,pmst}});
		for(unsigned i=0;i<8;++i)final.expected.push_back({int(r::STATE_AR0+i),!excluded && i==selected ? 0x2200U : after[i]});
		if(!excluded && destination>=0x60)final.expected_data.push_back({destination,word});
		if(excluded){initial.expected.clear();final.expected[10].value=1;}
		clocked_register_case c{{{0,std::move(initial)},{12,std::move(final)}},4+seed%3,12};c.require_idle=!excluded;c.clear_pipeline_before_restore=indexed;
		if(!excluded){c.states.insert(c.states.begin()+1,{{5,std::move(access)},{6,observation("after-pointer-poison",{})}});clocked_register_action poison{5,{{r::STATE_AR0+selected,0x2200}}};if(source>=0x60)poison.data.push_back({source,u16(word^0xffff)});c.actions.push_back(std::move(poison));}
		else{unsigned const fail=bad_destination ? 4 : 5;c.states.back().tick=fail;c.states.insert(c.states.begin()+1,{fail-1,observation("before-rejection",{})});}
		c.actions.insert(c.actions.begin(),{3,{},false,{{base+0x601,0xdead}}});c.program_events={{false,base+0x601,destination,0,false}};c.data_events=std::vector<clocked_data_event>{};
		if(!excluded){c.data_events->push_back({false,source,word,5,true});c.data_events->push_back({true,destination,word,unsigned(destination>=0x10 && destination<=0x1a ? 5 : 6),true});}
		cases.push_back(std::move(c));
	};
	unsigned const encodings=indexed ? 16 : 32;
	for(unsigned code=0;code<encodings;++code)for(bool compatibility : {false,true}){
		unsigned const encoding=indexed ? (code<8 ? 8 : 24)|(code&7) : code,selected=compatibility && !(code&7) ? 4 : code&7;
		for(unsigned block=0;block<5;++block)for(unsigned index=0;index<4;++index)for(unsigned position=0;position<2;++position){
			if(!selected && position)continue; // AR0 is both pointer and step; keep its legal fixed position.
			u16 bk=std::array<u16,5>{0,1,7,16,82}[block];if(!selected && bk)bk=0x2000;
			std::array<u16,8> ar;for(unsigned i=0;i<8;++i)ar[i]=0x1000+0x100*i;
			ar[0]=std::array<u16,4>{0,1,0xffff,bk ? u16(-bk) : u16(0x8000)}[index];u16 const source=block==1 && index==3 ? 0x20 : 0x1000+0x100*selected+(bk && position ? bk-1 : 0);ar[selected]=source;
			unsigned const seed=code+compatibility+block+index+position;append(encoding,compatibility,seed%21,source,bk,ar,std::array<u16,4>{0,1,0x8000,0xffff}[seed%4],seed);
		}
		for(unsigned source=8;source<=0x19;++source)for(unsigned kind=0;kind<21;++kind){
			std::array<u16,8> ar;for(unsigned i=0;i<8;++i)ar[i]=0x1000+0x100*i;ar[0]=1;ar[selected]=source;
			append(encoding,compatibility,kind,source,selected ? 64 : 0x2000,ar,0,code+compatibility+source+kind);
		}
	}
	for(unsigned kind=0;kind<21;++kind)for(unsigned pattern=0;pattern<8;++pattern){unsigned const code=(kind+pattern)%encodings,encoding=indexed ? (code<8 ? 8 : 24)|(code&7) : code;bool const compatibility=pattern&1;unsigned const selected=compatibility && !(code&7) ? 4 : code&7;std::array<u16,8> ar;for(unsigned i=0;i<8;++i)ar[i]=0x1000+0x100*i;ar[0]=1;ar[selected]=0x1041;append(encoding,compatibility,kind,0x1041,selected ? 82 : 0x2000,ar,std::array<u16,8>{0,1,0x8000,0xffff,0x1eff,0x00d0,0xabcd,0xf495}[pattern],kind+pattern);}
	return cases;
}
}
