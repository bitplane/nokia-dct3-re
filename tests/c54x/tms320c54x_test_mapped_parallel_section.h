// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_TEST_MAPPED_PARALLEL_SECTION_H
#define C54X_TEST_MAPPED_PARALLEL_SECTION_H
#include "c54x_section_common.h"
#include <algorithm>
namespace c54x_test {
inline std::vector<clocked_register_case> mapped_parallel_cases(bool peripheral=false,bool bsp=false)
{
	std::vector<clocked_register_case> cases;
	for (unsigned form=0;form<6;++form) for (unsigned location=0;location<(bsp ? 1U : peripheral ? 64U : 32U);++location)
	for (unsigned selectors : {0x01,0x0a,0x45,0xcd,0xf2,0x83,0xbc,0xef})
	for (unsigned pattern=0;pattern<2;++pattern) for (unsigned flags=0;flags<8;++flags) {
		unsigned const address=location+(peripheral ? 0x20 : 0),read_clock=peripheral ? 6 : 5;
		unsigned const x=selectors>>4,y=selectors&15,source=form>=4 ? form-4 : form/2,destination=form&1;
		u32 const base=(form+address+selectors+pattern+flags)%2 ? 0x20000 : 0;
		u64 const a=pattern ? 0xfe7fffffffULL : 0x123456789aULL,b=a^0x5aa55aa55aULL;
		u64 const initial_a=a^0x1111111111ULL,initial_b=b^0x1111111111ULL;
		u16 const st0=0xa002|(pattern ? 0x800 : 0),st1=0x800|(flags&1 ? 0x100 : 0)|(flags&4 ? 0x200 : 0)|(pattern ? 31 : 0);
		u16 const pmst=flags&2 ? 1 : 0,imr=pattern ? 0xa55a : 0,ifr=pattern ? 0x5aa5 : 0;
		std::array<u16,4> ar{0x401f,0x411f,0x421f,0x431f}; ar[x&3]=address;
		u16 const target=ar[y&3]; auto after=ar;
		auto modify=[](u16 at,unsigned mode)->u16 { return at+(mode==1 ? -1 : mode==2 ? 1 : mode==3 ? 3 : 0); };
		after[x&3]=modify(ar[x&3],x>>2); after[y&3]=modify(ar[y&3],y>>2);
		u16 word=0;
		if (peripheral) word=pattern ? 0x8001 : 0x7ffe;
		else if (address==0) word=imr;
		else if (address==1) word=ifr;
		else if (address<6 || address==0x1f) word=pattern ? 0x7fff : 0x8001;
		else if (address==6) word=st0;
		else if (address==7) word=st1;
		else if (address<=0xa) word=initial_a>>((address-8)*16);
		else if (address<=0xd) word=initial_b>>((address-11)*16);
		else if (address==0xe) word=0xbeef;
		else if (address==0xf) word=0x7fff;
		else if (address==0x10) word=3;
		else if (address==0x11) word=0x1700;
		else if (address<=0x15) word=after[address-0x12];
		else if (address<=0x17) word=address==0x16 ? 0x1800 : 0x1900;
		else word=std::array<u16,7>{0x3000,0,7,0x4321,0x5678,pmst,u16(base>>16)}[address-0x18];
		u64 const old_source=source ? b : a;
		int const shift=pattern ? -1 : 0;
		u64 shifted=0;
		for (int bit=0;bit<40;++bit) {
			int const from=bit-shift;
			if ((from>=0 && from<40 && BIT(old_source,from)) || (from>=40 && (flags&1) && BIT(old_source,39))) shifted|=u64(1)<<bit;
		}
		if (pmst&1) {
			s64 const number=BIT(shifted,39) ? s64(shifted)-(s64(1)<<40) : s64(shifted);
			shifted=flags&1 ? u64(std::clamp(number,s64(-2147483648LL),s64(2147483647LL))) : std::min(shifted,u64(0xffffffff));
		}
		u64 const loaded=u64(s64(flags&1 ? s16(word) : s32(word))*65536)&0xffffffffffULL;
		u64 const final_a=form<4 && !destination ? loaded : a,final_b=form<4 && destination ? loaded : b;
		u16 const opcode=(form>=4 ? 0xe400 : 0xc800|(destination<<8))|(source<<9)|(x<<4)|y;
		auto initial=initial_case(bsp ? "parallel-BSP-read-acknowledges-one-frame" : peripheral ? "parallel-volatile-peripheral-read-once" : "parallel-mapped-Read-versus-live-store-source",{opcode,0xf4e1});
		for (auto &p : initial.program) p.address+=base;
		initial.data={{target,0xdead}};
		if ((address>=2 && address<=5) || address==0x1f) initial.data.push_back({address,word});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,initial_a},{r::STATE_B,initial_b},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},{r::STATE_T,0xbeef},{r::STATE_TRN,0x7fff},
			{r::STATE_IMR,imr},{r::STATE_IFR,ifr},{r::STATE_SP,0x3000},{r::STATE_RC,0},{r::STATE_BRC,7},
			{r::STATE_RSA,0x4321},{r::STATE_REA,0x5678},{r::STATE_BK,0},{r::STATE_AR0,3},
			{r::STATE_AR1,0x1700},{r::STATE_AR6,0x1800},{r::STATE_AR7,0x1900}});
		for (unsigned i=0;i<4;++i) initial.initial.push_back({r::STATE_AR2+i,ar[i]});
		auto read=observation("mapped-read-does-not-acknowledge-interrupts",{{r::STATE_IMR,imr},{r::STATE_IFR,ifr},{r::STATE_T,form>=4 ? word : 0xbeefU}});
		for (unsigned i=0;i<4;++i) read.expected.push_back({int(r::STATE_AR2+i),after[i]});
		clocked_register_case c{{{0,std::move(initial)},{read_clock,std::move(read)},
			{read_clock+1,observation("mapped-sample-retained-through-store",{{r::STATE_A,final_a},{r::STATE_B,final_b},{r::STATE_T,0x55aa}})},
			{12,observation("mapped-parallel-completion",{{r::STATE_IDLE,1},{r::STATE_A,final_a},{r::STATE_B,final_b},
				{r::STATE_T,0x55aa},{r::STATE_ST0,st0},{r::STATE_ST1,st1^31U},{r::STATE_PMST,pmst},{r::STATE_SP,0x3000},
				{r::STATE_IMR,peripheral ? imr : 0x0f0fU},{r::STATE_IFR,peripheral ? ifr : 0xf0f0U},{r::STATE_AR2,0x2200},{r::STATE_AR3,0x2201},
				{r::STATE_AR4,0x2202},{r::STATE_AR5,0x2203}})}},4+unsigned(cases.size()%(peripheral ? 4 : 3)),12};
		c.actions={{read_clock,{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0x55aa},{r::STATE_ST1,st1^31U},
			{r::STATE_AR2,0x2200},{r::STATE_AR3,0x2201},
			{r::STATE_AR4,0x2202},{r::STATE_AR5,0x2203}}}};
		if ((address>=2 && address<=5) || address==0x1f) c.actions.front().data={{address,u16(word^0xffff)}};
		if (!peripheral) c.actions.front().values.insert(c.actions.front().values.end(),{{r::STATE_IMR,0x0f0f},{r::STATE_IFR,0xf0f0}});
		else {
			if (!bsp) c.volatile_data_reads={{address,word}};
			c.states.back().observation.expected_data={{address,u16(word^0xffff)}};
		}
		c.data_events=std::vector<clocked_data_event>{{false,address,word,read_clock},{true,target,u16(shifted>>16),read_clock+1}};
		c.single_fetch=base+0x600; c.clear_pipeline_before_restore=true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
#endif
