// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_TEST_PARALLEL_DATA_SECTION_H
#define C54X_TEST_PARALLEL_DATA_SECTION_H
#include "c54x_section_common.h"
#include <algorithm>
namespace c54x_test {
enum class parallel_data_operation { load, arithmetic, multiply };
inline std::vector<clocked_register_case> parallel_data_cases(parallel_data_operation kind)
{
	bool const arithmetic=kind==parallel_data_operation::arithmetic,multiply=kind==parallel_data_operation::multiply;
	unsigned const forms=arithmetic ? 8 : multiply ? 4 : 6;
	std::vector<clocked_register_case> cases;
	std::array<u16,8> const words{0,1,0x7fff,0x8000,0xffff,0x1234,0x5555,0xaaaa};
	std::array<u64,8> const seeds{0,1,0xffffffffffULL,0x7fffffff,0xff80000000ULL,0x0180000000ULL,0xfe7fffffffULL,0x123456789aULL};
	auto append=[&](unsigned form,unsigned x,unsigned y,unsigned layout,bool linear,u16 word,u64 a,u16 t,u16 st0,u16 st1,u16 pmst) {
		unsigned const source=arithmetic ? (form%4)/2 : form>=4 ? form-4 : form/2,destination=form&1;
		u64 const b=a^0x5aa55aa55aULL;
		std::array<u16,4> ar{},after{};
		for (unsigned i=0;i<4;++i) ar[i]=layout==3 ? 0x1000+i*0x100 : layout==2 ? 0x1048+i : 0x100f+(layout ? 0 : i*0x100);
		u16 const xa=ar[x&3],ya=ar[y&3]; after=ar;
		auto modify=[&](u16 at,unsigned mode)->u16 { return mode==1 ? at-1 : mode==2 ? at+1 : mode==3 ? linear ? at+3 : (at&0xfff0)|((at+3)&15) : at; };
		after[x&3]=modify(ar[x&3],x>>2);
		if ((x&3)!=(y&3)) after[y&3]=modify(ar[y&3],y>>2);
		u64 const old_source=source ? b : a;
		int const shift=int(st1&31)-(st1&16 ? 32 : 0);
		u64 shifted=0;
		for (int bit=0;bit<40;++bit) {
			int const from=bit-shift;
			if ((from>=0 && from<40 && BIT(old_source,from)) ||
				(shift<0 && from>=40 && (st1&0x100) && BIT(old_source,39))) shifted|=u64(1)<<bit;
		}
		if (pmst&1) {
			s64 const number=BIT(shifted,39) ? s64(shifted)-(s64(1)<<40) : s64(shifted);
			shifted=st1&0x100 ? u64(std::clamp(number,s64(-2147483648LL),s64(2147483647LL))) : std::min(shifted,u64(0xffffffff));
		}
		u16 const stored=u16(shifted>>16);
		u64 final_a=a,final_b=b; u16 final_st0=st0;
		s64 const memory=s64(st1&0x100 ? s16(word) : s32(word))*65536;
		if (arithmetic || multiply) {
			u64 const other_word=destination ? a : b;
			s64 const other=BIT(other_word,39) ? s64(other_word)-(s64(1)<<40) : s64(other_word);
			s64 const result=multiply ? s64(s16(word))*s16(t)*(st1&0x40 ? 2 : 1) : form>=4 ? memory-other : other+memory;
			bool const overflow=result<-2147483648LL || result>2147483647LL;
			if (arithmetic) {
				bool const carry=form>=4 ? u32(memory)>=u32(other_word) : u64(u32(memory))+u32(other_word)>0xffffffffULL;
				final_st0=(st0&~0x800)|(carry ? 0x800 : 0);
			}
			final_st0|=overflow ? destination ? 0x200 : 0x400 : 0;
			u64 const value=u64(st1&0x200 ? std::clamp(result,s64(-2147483648LL),s64(2147483647LL)) : result)&0xffffffffffULL;
			(destination ? final_b : final_a)=value;
		} else if (form<4) (destination ? final_b : final_a)=u64(memory)&0xffffffffffULL;
		u16 const opcode=(multiply ? 0xcc00|(destination<<8) : arithmetic ? 0xc000|((form/4)<<10)|(destination<<8) : form>=4 ? 0xe400 : 0xc800|(destination<<8))|(source<<9)|(x<<4)|y;
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		auto initial=initial_case("parallel-RAM-Read-versus-Execute-ownership",{opcode,0xf4e1});
		for (auto &p : initial.program) p.address+=base;
		initial.data={{xa,word}}; if (ya!=xa) initial.data.push_back({ya,0xdead});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a^0x1111111111ULL},
			{r::STATE_B,b^0x1111111111ULL},{r::STATE_T,t},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_PMST,pmst},{r::STATE_SP,0x3000},{r::STATE_AR0,3},{r::STATE_BK,linear ? 0U : 16U}});
		for (unsigned i=0;i<4;++i) initial.initial.push_back({r::STATE_AR2+i,ar[i]});
		auto read=observation("parallel-RAM-retained-source-and-T",{{r::STATE_A,a^0x1111111111ULL},
			{r::STATE_B,b^0x1111111111ULL},{r::STATE_ST0,st0},{r::STATE_T,!arithmetic && !multiply && form>=4 ? word : t}});
		for (unsigned i=0;i<4;++i) read.expected.push_back({int(r::STATE_AR2+i),after[i]});
		clocked_register_case c{{{0,std::move(initial)},{5,std::move(read)},
			{6,observation("parallel-RAM-store-and-result",{{r::STATE_A,final_a},{r::STATE_B,final_b},{r::STATE_ST0,final_st0},{r::STATE_T,0x55aa}})},
			{12,observation("parallel-RAM-completion",{{r::STATE_IDLE,1},{r::STATE_A,final_a},{r::STATE_B,final_b},
				{r::STATE_ST0,final_st0},{r::STATE_ST1,st1^31U},{r::STATE_PMST,pmst},{r::STATE_T,0x55aa},{r::STATE_SP,0x3000},
				{r::STATE_AR2,0x2200},{r::STATE_AR3,0x2201},{r::STATE_AR4,0x2202},{r::STATE_AR5,0x2203}})}},4+unsigned(cases.size()%3),12};
		c.actions={{5,{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0x55aa},{r::STATE_ST1,st1^31U},
			{r::STATE_AR2,0x2200},{r::STATE_AR3,0x2201},{r::STATE_AR4,0x2202},{r::STATE_AR5,0x2203}},false,{},{},{{xa,u16(word^0xffff)}}}};
		c.data_events=std::vector<clocked_data_event>{{false,xa,word,5},{true,ya,stored,6}};
		c.single_fetch=base+0x600; c.clear_pipeline_before_restore=arithmetic || multiply;
		cases.push_back(std::move(c));
	};
	for (unsigned form=0;form<forms;++form) {
		for (unsigned layout=0;layout<3;++layout) for (unsigned linear=0;linear<2;++linear)
		for (unsigned x=0;x<16;++x) for (unsigned y=0;y<16;++y)
			append(form,x,y,layout,linear,words[y%8],seeds[(x+y+layout)%8],0xbeef,0xa002,
				0x800|((x+y)&1 ? 0x100 : 0)|((x+y)&2 ? 0x200 : 0)|(x&1 ? 0x4020 : 0)|((x+16*y)&31),(y/2)&1);
		for (unsigned seed=0;seed<8;++seed) for (unsigned shift=0;shift<32;++shift)
		for (unsigned sxm=0;sxm<2;++sxm) for (unsigned sst=0;sst<2;++sst)
			append(form,8,9,3,true,0x8000,seeds[seed],0x7fff,0xa002|(seed&1 ? 0x1e00 : 0),
				0x800|(sxm ? 0x100 : 0)|(seed&2 ? 0x200 : 0)|shift,sst);
		if (multiply) {
			for (u16 word : words) for (u16 t : words) for (unsigned flags=0;flags<16;++flags)
				append(form,8,9,3,true,word,seeds[(word+t)%8],t,0xa002|(flags&8 ? 0x1e00 : 0),
					0x800|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0),flags&4 ? 2 : 0);
		} else if (arithmetic) {
			for (u16 word : words) for (u64 seed : seeds) for (unsigned flags=0;flags<8;++flags)
				append(form,8,9,3,true,word,seed,0xbeef,0xa002|(flags&4 ? 0x1e00 : 0),
					0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0),0);
		} else for (u16 word : words) for (unsigned flags=0;flags<4;++flags)
			append(form,8,9,3,true,word,seeds[word%8],0xbeef,0xa002|(flags&2 ? 0x800 : 0),0x800|(flags&1 ? 0x100 : 0),0);
	}
	return cases;
}
}
#endif
