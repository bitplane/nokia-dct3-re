// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_TEST_PAIRED_ACCUMULATOR_SECTION_H
#define C54X_TEST_PAIRED_ACCUMULATOR_SECTION_H
#include "c54x_section_common.h"
#include <algorithm>
namespace c54x_test {
inline std::vector<clocked_register_case> paired_accumulator_cases(bool lms)
{
	std::vector<clocked_register_case> cases;
	std::array<u16,8> const words{0,1,0xffff,0x8000,0x7fff,0x1234,0x5555,0xaaaa};
	std::array<u64,8> const aa{0,0x007fff0000ULL,0x0080000000ULL,0x00ffff0000ULL,0x0100000000ULL,0x017fff0000ULL,0xff80000000ULL,0xffffffffffULL};
	std::array<u64,8> const bb{0,1,0x7fffffff,0xff80000000ULL,0x0180000000ULL,0xfe7fffffffULL,0x123456789aULL,0xffffffffffULL};
	auto append=[&](unsigned x,unsigned y,unsigned layout,bool linear,unsigned flags,u16 xword,u16 yword,u64 a,u64 b,bool address_bank) {
		std::array<u16,4> ar{},after_ar{};
		for (unsigned i=0;i<4;++i) ar[i]=!address_bank ? 0x1000+0x100*i : layout==2 ? 0x1048+i : 0x100f+(layout ? 0 : 0x100*i);
		u16 const xa=ar[x&3],ya=ar[y&3]; if (xa==ya) yword=xword;
		after_ar=ar;
		auto modify=[&](u16 at,unsigned mode)->u16 { return mode==1 ? at-1 : mode==2 ? at+1 : mode==3 ? linear ? at+3 : (at&0xfff0)|((at+3)&15) : at; };
		after_ar[x&3]=modify(ar[x&3],x>>2);
		if ((x&3)!=(y&3)) after_ar[y&3]=modify(ar[y&3],y>>2);
		u16 const st0=0xa002|(flags&16 ? 0x1e00 : 0);
		u16 const st1=0x800|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0)|(flags&8 ? 0x100 : 0)|(address_bank && (x&1) ? 0x4020 : 0);
		u16 const pmst=flags&4 ? 2 : 0;
		auto signed40=[](u64 value)->s64 { return BIT(value,39) ? s64(value)-(s64(1)<<40) : s64(value); };
		s64 const memory_x=flags&8 ? s16(xword) : s32(xword),memory_y=flags&8 ? s16(yword) : s32(yword);
		s64 const high=s64((a>>16)&0xffff)-(BIT(a,32) ? 65536 : 0);
		s64 product=lms ? s64(s16(xword))*s16(yword) : high*high;
		product*=flags&1 ? 2 : 1;
		if ((flags&7)==7) product=std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
		s64 const operand=memory_x*65536+32768;
		s64 const result_a=lms ? signed40(a)+operand : (memory_x-memory_y)*65536;
		s64 const result_b=signed40(b)+product;
		bool const carry=lms ? u64(u32(a))+u32(operand)>0xffffffffULL : xword>=yword;
		u16 final_st0=(st0&~0x800)|(carry ? 0x800 : 0)|
			(result_a<-2147483648LL || result_a>2147483647LL ? 0x400 : 0)|
			(result_b<-2147483648LL || result_b>2147483647LL ? 0x200 : 0);
		u64 final_a=u64(flags&2 ? std::clamp(result_a,s64(-2147483648LL),s64(2147483647LL)) : result_a)&0xffffffffffULL;
		u64 final_b=u64(flags&2 ? std::clamp(result_b,s64(-2147483648LL),s64(2147483647LL)) : result_b)&0xffffffffffULL;
		// Literal witnesses for the 17-bit square and the SXM-sensitive LMS add.
		if (xword==0x8000 && yword==0x8000 && !b) {
			if (!lms && a==0x0100000000ULL && (flags==0 || flags==7)) {
				final_a=0; final_b=flags ? 0x7fffffff : 0x0100000000ULL; final_st0=flags ? 0xa802 : 0xaa02;
			}
			if (lms && !a && (flags==0 || flags==8)) {
				final_a=flags ? 0xff80008000ULL : 0x0080008000ULL; final_b=0x40000000; final_st0=flags ? 0xa002 : 0xa402;
			}
		}
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		u16 const opcode=(lms ? 0xe100 : 0xe200)|(x<<4)|y;
		auto initial=initial_case("paired-accumulator-Read-and-Execute-ownership",{opcode,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.data={{xa,xword},{ya,yword}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a^0xffffffffffULL},
			{r::STATE_B,b^0xffffffffffULL},{r::STATE_T,0xbeef},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_PMST,pmst},{r::STATE_AR0,3},{r::STATE_BK,linear ? 0U : 16U}});
		for (unsigned i=0;i<4;++i) initial.initial.push_back({r::STATE_AR2+i,ar[i]});
		auto captured=observation("paired-addresses-captured-once",{{r::STATE_A,a^0xffffffffffULL},{r::STATE_B,b^0xffffffffffULL},{r::STATE_ST0,st0}});
		initial.expected.insert(initial.expected.end(),captured.expected.begin(),captured.expected.end());
		for (unsigned i=0;i<4;++i) captured.expected.push_back({int(r::STATE_AR2+i),after_ar[i]});
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(captured)},
			{5,observation("paired-Read-preserves-T-and-old-accumulators",{{r::STATE_A,a^0xffffffffffULL},
				{r::STATE_B,b^0xffffffffffULL},{r::STATE_ST0,st0},{r::STATE_T,0xbeef}})},
			{6,observation("paired-Execute-publishes-both-results",{{r::STATE_A,final_a},{r::STATE_B,final_b},{r::STATE_ST0,final_st0},{r::STATE_T,0xdead}})},
			{12,observation("paired-result-and-replay-context",{{r::STATE_IDLE,1},{r::STATE_A,final_a},{r::STATE_B,final_b},
				{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},{r::STATE_T,0xdead},
				{r::STATE_AR2,0x2200},{r::STATE_AR3,0x2201},{r::STATE_AR4,0x2202},{r::STATE_AR5,0x2203}})}},3+unsigned(cases.size()%3),12};
		c.actions={{4,{{r::STATE_AR2,0x2200},{r::STATE_AR3,0x2201},{r::STATE_AR4,0x2202},{r::STATE_AR5,0x2203},{r::STATE_BK,32}}},
			{5,{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0xdead}},false,{},{},{{xa,0xdead},{ya,0xbeef}}}};
		c.data_events=std::vector<clocked_data_event>{{false,xa,xword,5},{false,ya,yword,5}};
		c.single_fetch=base+0x600;
		c.clear_pipeline_before_restore=lms;
		cases.push_back(std::move(c));
	};
	for (unsigned linear=0;linear<2;++linear) for (unsigned layout=0;layout<3;++layout)
	for (unsigned x=0;x<16;++x) for (unsigned y=0;y<16;++y)
		append(x,y,layout,linear,(x+3*y+layout+linear)%32,words[y%8],words[(x+y)%8],aa[x%8],bb[y%8],true);
	for (unsigned flags=0;flags<32;++flags) {
		if (lms) {
			for (unsigned a=0;a<8;++a) for (unsigned x=0;x<8;++x)
				append(0,1,0,true,flags,words[x],1,aa[a],bb[(a+x)%8],false);
			for (unsigned b=0;b<8;++b) for (unsigned x=0;x<8;++x) for (unsigned y=0;y<8;++y)
				append(0,1,0,true,flags,words[x],words[y],aa[(b+x+y)%8],bb[b],false);
		} else for (unsigned x=0;x<8;++x) for (unsigned y=0;y<8;++y)
			append(0,1,0,true,flags,words[x],words[y],aa[(x+y)%8],bb[(x+3*y)%8],false);
		// Keep cross-accumulator/overflow interactions without crossing all memory pairs too.
		for (u64 a : aa) for (u64 b : bb) append(0,1,0,true,flags,0x8000,0x8000,a,b,false);
	}
	return cases;
}
}
#endif
