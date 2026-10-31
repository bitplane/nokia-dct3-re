// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add=[&](unsigned count,unsigned form,unsigned kind,int shift,unsigned flags,bool negative,bool compiler,unsigned irq,unsigned selector){
		u32 const base=selector&1 ? 0x20000 : 0;u16 const mode=std::array<u16,5>{3,0x83,0x8b,0x93,0x13}[form];
		u16 const st0=0xa600|(form==4 ? 0 : 8),st1=(compiler ? 0x4000 : 0)|(flags&2 ? 0x100 : 0)|(irq ? 0 : 0x800);
		u16 const address=form>=1 && form<=3 ? 0x5000 : compiler ? 0x3000+mode : (st0&0x1ff)*128+mode,initial_ar3=form>=1 && form<=3 ? 0x5000 : 0x7103;
		u64 const source=negative ? 0xff7fff1200ULL : 0x0180001200ULL;
		s64 const input=(flags&2) && (source&0x8000000000ULL) ? s64(source)-0x10000000000LL : s64(source),divisor=s64(1)<<unsigned(shift<0 ? -shift : 0);
		u64 const bits=u64(shift>=0 ? input*(s64(1)<<shift) : input/divisor-(input<0 && input%divisor!=0))&0xffffffffffULL;
		s64 const signed_bits=bits&0x8000000000ULL ? s64(bits)-0x10000000000LL : s64(bits);
		u32 const stored=!(flags&1) ? u32(bits) : flags&2 ? u32(std::clamp(signed_bits,s64(-2147483648LL),s64(2147483647LL))) : u32(std::min(bits,u64(0xffffffff)));
		u16 const value=stored>>(kind&2 ? 16 : 0),final_ar3=address==0x13 ? value : initial_ar3+(form==3 ? 1 : form==2 ? -1 : 0);
		u16 const extension=(kind&2 ? 0x0c60 : 0x0c80)|((kind&1)<<8)|(shift&31);
		int const line=irq==2 || irq==4 ? INPUT_LINE_NMI : 2;
		auto initial=initial_case("RPTBD-shift-store-captures-address-but-consumes-live-source",{0xf272,0x605,u16(0x6f00|mode),extension,u16(0xe800|((kind&1)<<8)),0x6d94});
		for(auto &word : initial.program)word.address+=base;
		initial.program.insert(initial.program.end(),{{base+0x60f,0xf4e1},{base+(line==INPUT_LINE_NMI ? 4U : 0x48U),0xf495},{base+(line==INPUT_LINE_NMI ? 5U : 0x49U),0xf4eb}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_PMST,flags&1},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_A,kind&1 ? 0x123456789aULL : 0x11112222ULL},{r::STATE_B,kind&1 ? 0x11112222ULL : 0xfedcba9876ULL},
			{r::STATE_BRC,count},{r::STATE_SP,0x3000},{r::STATE_IMR,irq && line==2 ? 4U : 0U},{r::STATE_T,0x5aa5},{r::STATE_BK,13}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,i==3 ? initial_ar3 : i==4 ? 0x6000U : 0x7000+i});
		if(address>=0x60)initial.data={{address,0xdead}};
		auto before=observation("source-before-Execute-replacement",{{r::STATE_A,kind&1 ? 0x123456789aULL : 0x11112222ULL},{r::STATE_B,kind&1 ? 0x11112222ULL : 0xfedcba9876ULL}});
		auto live=observation("delay-publication-retains-Execute-live-source",{{r::STATE_A,kind&1 ? 0x123456789aULL : source},{r::STATE_B,kind&1 ? source : 0xfedcba9876ULL}});
		auto final=observation("one-delay-publication-then-count-plus-one-body-iterations",{{STATE_GENPC,base+0x610},{r::STATE_AR3,final_ar3},{r::STATE_AR4,0x6001+count},
			{r::STATE_A,kind&1 ? 0x123456789aULL : 0},{r::STATE_B,kind&1 ? 0 : 0xfedcba9876ULL},{r::STATE_BRC,0},{r::STATE_RSA,0x604},{r::STATE_REA,0x605},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_SP,0x3000},{r::STATE_T,0x5aa5},{r::STATE_BK,13}});
		if(address>=0x60)final.expected_data={{address,value}};
		clocked_register_case c{{{0,std::move(initial)},{8,std::move(before)},{9,std::move(live)},{10,observation("body-or-ISR-continuation",{})},{96,std::move(final)}},std::array<unsigned,3>{4,8,12}[selector%3],96};
		c.initial_input_lines={{2,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
		for(unsigned tick : {3U,5U,8U,10U}){clocked_register_action change{};change.tick=tick;
			if(tick==3 || tick==5)change.program={{base+(tick==3 ? 0x601U : 0x603U),0xdead}};
			if(tick==8)change.values={{kind&1 ? r::STATE_B : r::STATE_A,source}};
			if(irq && tick==(irq<=2 ? 5U : 10U))change.input_lines={{line,ASSERT_LINE}};
			c.actions.push_back(change);
		}
		c.program_events={{false,base+0x601,0x605,0,false},{false,base+0x602,u16(0x6f00|mode),0,false},{false,base+0x603,extension,0,false}};
		c.data_events=std::vector<clocked_data_event>{{true,address,value,9,true}};
		c.data_events->front().registers={{int(kind&1 ? r::STATE_B : r::STATE_A),source}};
		if(irq){clocked_data_event frame{true,0x2fff,0,0,false};frame.value_range=std::array<u16,2>{0x604,0x60f};frame.value_register=r::STATE_RTN;
			c.data_events->push_back(frame);frame.write=false;c.data_events->push_back(frame);c.irqs={{line,9,~0U,1,{}}};}
		cases.push_back(std::move(c));
	};
	// Numeric store boundaries are crossed at a representative outer count.
	for(unsigned form=0;form<5;++form)for(unsigned kind=0;kind<4;++kind)for(int shift : {-16,-1,15})for(unsigned flags=0;flags<4;++flags)
	for(bool negative : {false,true})for(bool compiler : {false,true})add(2,form,kind,shift,flags,negative,compiler,0,form+kind+shift+16+flags+negative+compiler);
	// Count/interrupt composition gets independent representative numeric inputs.
	for(unsigned count : {0U,1U,2U,7U})for(unsigned form=0;form<5;++form)for(unsigned kind=0;kind<4;++kind)for(unsigned irq=0;irq<5;++irq){
		if(count==2 && !irq)continue;
		unsigned const seed=count+form+kind+irq;add(count,form,kind,std::array<int,3>{-16,-1,15}[seed%3],seed%4,seed&1,seed&2,irq,seed);
	}
	return cases;
}
}
