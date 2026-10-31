// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_COUNT_RETIREMENTS
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> counted_copy_cases(unsigned variant)
{
	std::vector<clocked_register_case> cases;
	for(unsigned form=0;form<24;++form)for(unsigned count : {0U,1U,7U,31U})for(unsigned kind=0;kind<3;++kind)for(unsigned irq=0;irq<(variant ? 1U : 3U);++irq){
		if(variant && form!=0 && form!=18)continue;
		unsigned const seed=form+count+kind+irq;bool const absolute=form<=18,chain=variant==1,wrap=variant==2,flags=seed&1,pattern=seed&2;
		u32 const base=seed&1 ? 0x20000 : 0;u16 const origin=wrap ? 0xfffe : 0x600,address=form<18 ? form+8 : form==18 || form==21 ? 0x4000 : form==19 ? 0x4001 : form==20 ? 0x3001 : 8;
		u16 const mode=absolute ? 0xf8 : form==19 || form==20 ? 1 : form==22 ? 8 : 0x91,st0=0xa600|(form==19 ? 0x80 : form==22 ? 0 : 8),st1=(flags ? 0x320 : 0)|(irq ? 0 : 0x800)|(form==20 ? 0x4000 : 0);
		unsigned const source_ar=address==0x12 || address==0x13 ? 4 : 2,destination_ar=source_ar+1,sentinel_ar=address==0x17 ? 6 : 7,words=kind ? 2 : 1;
		u16 const opcode=!kind ? 0xe500|((source_ar+6)<<4)|(destination_ar+6) : kind==1 ? 0x7090|destination_ar : 0x7190|source_ar;
		u32 const body=base|u16(origin+(absolute ? 2 : 1)),after=body+words;unsigned const read=absolute ? 6 : 5;
		auto payload=[=](unsigned i)->u16{return pattern ? ~(0x9000+i) : 0x9000+i;};
		auto initial=initial_case("memory-count-copy-captures-once-before-independent-body",{0xf495});initial.program.clear();
		for(unsigned i=0;i<32;++i)initial.program.push_back({base|u16(origin+i),0xf495});
		initial.program.insert(initial.program.end(),{{base|origin,u16(0x4700|mode)},{body,opcode},{after+(chain ? 3U : 0U),u16(0x6d90|sentinel_ar)},{after+(chain ? 15U : 12U),0xf4e1}});
		if(absolute)initial.program.push_back({base|u16(origin+1),address});
		if(kind)initial.program.push_back({body+1,u16(kind==1 ? 0x5000 : 0x6000)});
		if(chain)initial.program.insert(initial.program.end(),{{after,0x47f8},{after+1,0x4002},{after+2,0xf495}});
		int const line=irq==2 ? INPUT_LINE_NMI : 2;initial.program.insert(initial.program.end(),{{base+(irq==2 ? 4U : 0x48U),0xf495},{base+(irq==2 ? 5U : 0x49U),0xf4eb}});
		std::array<u16,8> ar;for(unsigned i=0;i<8;++i)ar[i]=0x7200+i;ar[source_ar]=0x5000;ar[destination_ar]=0x6000;if(form==21 || form==23)ar[1]=address;
		u64 a=0x123456789aULL,b=0xfeabcd9876ULL;u16 t=0xbeef,trn=0x1357,sp=0x3000,bk=82;
		auto count_field=[&](u16 value){
			if(address<14){unsigned const part=(address-8)%3;u64 const mask=part==2 ? 0xff : 0xffff;u64 &acc=address<11 ? a : b;acc=(acc&~(mask<<(16*part)))|((u64(value)&mask)<<(16*part));}
			else if(address==14)t=value;else if(address==15)trn=value;else if(address<24)ar[address-16]=value;else if(address==24)sp=value;else bk=value;
		};
		if(address<0x60)count_field(count);
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base|origin},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_RC,0},{r::STATE_BRC,13},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,t},{r::STATE_TRN,trn},{r::STATE_SP,sp},{r::STATE_BK,bk},{r::STATE_IMR,irq==1 ? 4U : 0U}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,ar[i]});
		if(address>=0x60)initial.data.push_back({address,u16(count)});
		if(chain)initial.data.push_back({0x4002,3});
		for(unsigned i=0;i<=count+1;++i)initial.data.insert(initial.data.end(),{{0x5000+i,payload(i)},{0x6000+i,0}});
		auto final=observation("captured-count-controls-copy-and-once-only-sentinel",{{STATE_GENPC,after+(chain ? 16U : 13U)},{r::STATE_RC,0},{r::STATE_BRC,13},{r::STATE_SP,0x3000},{r::STATE_ST1,st1},{r::STATE_ST0,u64(st0&0x1fff),0x1fff},
			{int(r::STATE_AR0+source_ar),0x5000+(kind==1 ? 0 : count+1)},{int(r::STATE_AR0+destination_ar),0x6000+(kind==2 ? 0 : count+1)},{int(r::STATE_AR0+sentinel_ar),0x7201+sentinel_ar}},{{address,u16(address==0x18 ? 0x3000 : address==10 || address==13 ? 0x5a : 0xa55a)}});
		if(form==21 || form==23)final.expected.push_back({r::STATE_AR1,address+1U});
		for(unsigned i=0;i<=count+1;++i)final.expected_data.push_back({0x6000+i,i<=count ? payload(i) : u16(0)});
		clocked_register_case c{{{0,std::move(initial)},{120,std::move(final)}},std::array<unsigned,4>{3,read,read+2,read+6}[seed%4],120};
		c.retired_instruction=retired_instruction_check{chain ? after+2 : body,u16(chain ? 0xf495 : opcode),chain ? 4U : count+1};
		clocked_register_action poison{};poison.tick=read;
		if(address>=0x60)poison.data={{address,0xa55a}};
		else {count_field(address==0x18 ? 0x3000 : 0xa55a);if(address<14)poison.values={{address<11 ? r::STATE_A : r::STATE_B,address<11 ? a : b}};
			else poison.values={{address==14 ? r::STATE_T : address==15 ? r::STATE_TRN : address<24 ? r::STATE_AR0+address-16 : address==24 ? r::STATE_SP : r::STATE_BK,address==14 ? t : address==15 ? trn : address<24 ? ar[address-16] : address==24 ? sp : bk}};}
		c.actions.push_back(poison);c.initial_input_lines={{2,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
		c.program_events={{false,body,opcode,0,false}};if(chain)c.program_events.push_back({false,after+2,0xf495,0,false});
		c.data_events=std::vector<clocked_data_event>{{false,address,u16(count),read,true}};
		for(unsigned i=0;i<=count;++i){c.data_events->push_back({false,0x5000+i,payload(i),0,false});
			if(i>unsigned(!kind)){c.data_events->push_back({true,0x6000+i-1,payload(i-1),0,false});if(i==1 && irq)c.data_events->back().raise_input=line;}
			if(!kind && !i){c.data_events->push_back({true,0x6000+i,payload(i),0,false});if(irq)c.data_events->back().raise_input=line;}}
		if(kind || count){c.data_events->push_back({true,0x6000+count,payload(count),0,false});if(!count && irq)c.data_events->back().raise_input=line;}
		if(irq){unsigned const before=c.data_events->size();clocked_data_event frame{true,0x2fff,0,0,false};frame.value_range=std::array<u16,2>{u16(after),u16(after+12)};frame.value_register=r::STATE_RTN;
			c.data_events->push_back(frame);frame.write=false;c.data_events->push_back(frame);c.irqs={{line,0,~0U,before,{},{{r::STATE_RC,0}}}};}
		if(chain)c.data_events->push_back({false,0x4002,3,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
