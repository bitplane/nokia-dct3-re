// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned spec=0;spec<17;++spec)for(unsigned pattern=0;pattern<4;++pattern)for(unsigned prefix : {1U,2U})
	for(bool compatibility : {false,true})for(unsigned irq=0;irq<3;++irq){
		unsigned const form=spec<15 ? spec/3 : spec-10,count=spec<15 ? std::array<unsigned,3>{0,1,15}[spec%3] : spec==15 ? 4 : 11,seed=spec+pattern+prefix+compatibility+irq;
		u32 const base=seed&1 ? 0x20000 : 0;u16 const pointer=std::array<u16,7>{0x4000,0x400f,0x4020,0x3001,0x2fff,0x1d,0x16}[form];
		u16 const mode=form==2 ? 0x20 : form==1 || form>=5 ? 0x8f : 0x97,st0=0xa600|(form==2 ? 0x80 : 0),st1=0x100|(compatibility ? 0x20 : 0)|(irq==1 ? 0 : 0x800);
		int const step=!(mode&0x80) ? 0 : (mode&0x78)==8 ? -1 : 1;
		auto payload=[=](unsigned i)->u16{return std::array<u16,4>{0,1,0x8000,0xffff}[pattern]^(i*0x111);};
		auto address=[=](unsigned i)->u16{return pointer+i*step;};
		auto write_clock=[=](unsigned i){u16 const dest=address(i);return 5+prefix+i+unsigned(dest<0x10 || dest>0x19);};
		u16 const pmst=form==5 ? payload(0) : 0;int const line=irq==2 ? INPUT_LINE_NMI : 0;
		auto initial=initial_case("repeated-POPD-alias-restoration-and-deferred-interrupt",{0xf495});initial.program.clear();
		for(u32 page : {0U,0x20000U}){
			for(unsigned i=0;i<32;++i)initial.program.push_back({page+0x600+i,0xf495});
			initial.program.insert(initial.program.end(),{{page+0x600,u16(prefix==2 ? 0xf070 : 0xec00|count)},{page+0x600+prefix,u16(0x8b00|mode)},{page+0x601+prefix,0xf4e1},{page+(pmst&0xff80)+(irq==2 ? 4U : 0x40U),0xf4eb}});
			if(prefix==2)initial.program.push_back({page+0x601,u16(count)});
		}
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_SP,0x3000},{r::STATE_BK,0},
			{r::STATE_A,0x0222012200ULL},{r::STATE_B,0x0522042203ULL},{r::STATE_T,0x2206},{r::STATE_TRN,0x2207},{r::STATE_BRC,0x2211},{r::STATE_RSA,0x2212},{r::STATE_REA,0x2213},{r::STATE_IMR,irq==1 ? 1U : 0U}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,i==7 ? pointer : 0x2208+i});
		for(unsigned i=0;i<=count;++i)if(address(i)>=0x60)initial.data.push_back({address(i),0xa55a});
		for(unsigned i=0;i<=count;++i)initial.data.push_back({0x3000+i,payload(i)});
		auto final=observation("completed-restoration-and-balanced-interrupt-frame",{{STATE_GENPC,base+0x602+prefix},{r::STATE_SP,0x3001+count},{r::STATE_ST0,compatibility && (mode&0x80) ? (st0&0x1fff)|0xe000U : st0},{r::STATE_ST1,u16(st1&~(irq ? 0x800 : 0))}});
		for(unsigned i=0;i<21;++i){u16 const reg=i<16 ? 8+i : 0x19+i-16;u16 expected=i==2 ? 2 : i==5 ? 5 : reg==0x19 || reg==0x1d ? 0 : reg==0x17 ? u16(pointer+step*(count+1)) : 0x2200+i;
			for(unsigned n=0;n<=count;++n)if(reg==address(n))expected=payload(n)&(reg==0xa || reg==0xd ? 0xff : 0xffff);
			final.expected_data.push_back({reg,expected});}
		clocked_register_case c{{{0,std::move(initial)},{96,std::move(final)}},std::array<unsigned,4>{3+prefix,4+prefix,5+prefix+(count+1)/2,6+prefix+count}[seed%4],96};
		c.initial_input_lines={{0,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};if(irq)c.actions.push_back({3,{},{},{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}});
		if(prefix==2)c.actions.push_back({3,{},false,{{0x601,0xffff},{0x20601,0xffff}}});
		if(prefix==2)c.program_events.push_back({false,base+0x601,u16(count),0,false});
		c.program_events.push_back({false,base+0x600+prefix,u16(0x8b00|mode),0,false});
		c.data_events=std::vector<clocked_data_event>{};std::map<u16,u16> ram;
		for(unsigned i=0;i<=count;++i){c.data_events->push_back({false,0x3000+i,payload(i),5+prefix+i,true});c.data_events->push_back({true,address(i),payload(i),write_clock(i),true});}
		std::stable_sort(c.data_events->begin(),c.data_events->end(),[](auto const &a,auto const &b){
			if(a.cycle!=b.cycle)return a.cycle<b.cycle;
			auto priority=[](auto const &e){return !e.write ? 0 : e.address>=0x10 && e.address<=0x19 ? 1 : 2;};return priority(a)<priority(b);
		});
		// Poison consumed stack words only after the whole clock's guest bus work.
		for(unsigned tick=5+prefix;tick<=6+prefix+count;++tick){for(auto const &event : *c.data_events)if(event.write && event.cycle==tick && event.address>=0x60)ram[event.address]=event.value;
			if(tick<=5+prefix+count){u16 const at=0x3000+tick-(5+prefix);ram[at]=0xdead;c.actions.push_back({tick,{},false,{},{},{{at,0xdead}}});}}
		if(irq){unsigned const before=c.data_events->size();u16 const at=0x3000+count,ret=0x601+prefix;c.data_events->push_back({true,at,ret,0,false});c.data_events->push_back({false,at,ret,0,false});ram[at]=ret;c.irqs={{line,7+prefix+count,~0U,before,ret,{}}};}
		for(auto const &[at,word] : ram)c.states.back().observation.expected_data.push_back({at,word});
		std::stable_sort(c.actions.begin(),c.actions.end(),[](auto const &a,auto const &b){return a.tick<b.tick;});
		for(unsigned i=1;i<c.actions.size();)if(c.actions[i-1].tick==c.actions[i].tick){auto &old=c.actions[i-1];auto const &next=c.actions[i];old.program.insert(old.program.end(),next.program.begin(),next.program.end());old.input_lines.insert(old.input_lines.end(),next.input_lines.begin(),next.input_lines.end());c.actions.erase(c.actions.begin()+i);}else ++i;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
