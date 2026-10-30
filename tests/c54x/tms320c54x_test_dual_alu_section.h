// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_TEST_DUAL_ALU_SECTION_H
#define C54X_TEST_DUAL_ALU_SECTION_H
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> dual_alu_cases(bool arithmetic)
{
	std::vector<clocked_register_case> cases;
	std::array<u16,8> const words{0,1,0xffff,0x8000,0x7fff,0x1234,0x5555,0xaaaa};
	auto append=[&](unsigned op,unsigned x,unsigned y,unsigned layout,bool linear,unsigned destination,
		unsigned flags,unsigned context,u32 base,u16 xword,u16 yword) {
		std::array<u16,4> initial_ar{},final_ar{};
		for (unsigned i=0;i<4;++i) initial_ar[i]=layout==2 ? 0x1048+i : 0x100f+(layout ? 0 : 0x100*i);
		u16 const xaddress=initial_ar[x&3],yaddress=initial_ar[y&3];
		if (xaddress==yaddress) yword=xword;
		final_ar=initial_ar;
		auto update=[&](u16 address,unsigned mode)->u16 {
			return mode==1 ? address-1 : mode==2 ? address+1 : mode==3 ?
				linear ? address+3 : (address&0xfff0)|((address+3)&15) : address;
		};
		final_ar[x&3]=update(initial_ar[x&3],x>>2);
		if ((x&3)!=(y&3)) final_ar[y&3]=update(initial_ar[y&3],y>>2);
		u16 const st0=0xa002|(flags&16 ? 0x1e00 : 0);
		u16 const st1=0x800|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0)|(flags&8 ? 0x100 : 0)|(x&1 ? 0x4020 : 0);
		u16 const pmst=flags&4 ? 2 : 0;
		s64 const left=s64(flags&8 ? s16(xword) : s32(xword))*65536;
		s64 const right=s64(flags&8 ? s16(yword) : s32(yword))*65536;
		s64 const number=op ? (op==2 ? left-right : left+right) : s64(s16(xword))*s16(yword)*(flags&1 ? 2 : 1);
		bool const overflow=number<-2147483648LL || number>2147483647LL;
		u64 const result=u64(overflow && (flags&2) ? (number<0 ? -2147483648LL : 2147483647LL) : number)&0xffffffffffULL;
		bool const carry=op==2 ? xword>=yword : u32(xword)+yword>65535;
		u16 const final_st0=(op ? (st0&~0x800)|(carry ? 0x800 : 0) : st0)|(overflow ? destination ? 0x200 : 0x400 : 0);
		u64 const a=destination ? 0x123456789aULL : result,b=destination ? result : 0xfeabcd9876ULL;
		u16 const opcode=(op ? op==2 ? 0xa200 : 0xa000 : 0xa400)|(destination<<8)|(x<<4)|y;
		auto initial=initial_case("dual-operands-address-capture-and-Execute-result",{opcode,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.data={{xaddress,xword},{yaddress,yword}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_PMST,pmst},{r::STATE_T,0xabcd},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},
			{r::STATE_AR0,3},{r::STATE_BK,linear ? 0U : 16U}});
		for (unsigned i=0;i<4;++i) initial.initial.push_back({r::STATE_AR2+i,initial_ar[i]});
		auto before=observation("dual-addresses-published-once",{{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_ST0,st0}});
		initial.expected.insert(initial.expected.end(),before.expected.begin(),before.expected.end());
		for (unsigned i=0;i<4;++i) before.expected.push_back({int(r::STATE_AR2+i),final_ar[i]});
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(before)},
			{5,observation("dual-Read-T-publication",{{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},
				{r::STATE_ST0,st0},{r::STATE_T,op ? 0xabcdU : xword}})},
			{6,observation("dual-Execute-only-arithmetic",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,final_st0},{r::STATE_T,0xdead}})},
			{12,observation("dual-result-and-unchanged-context",{{r::STATE_IDLE,1},{r::STATE_A,a},{r::STATE_B,b},
				{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},{r::STATE_T,0xdead},
				{r::STATE_AR2,0x2200},{r::STATE_AR3,0x2201},{r::STATE_AR4,0x2202},{r::STATE_AR5,0x2203}})}},3+context,12};
		c.actions={{4,{{r::STATE_AR2,0x2200},{r::STATE_AR3,0x2201},{r::STATE_AR4,0x2202},{r::STATE_AR5,0x2203},{r::STATE_BK,32}}},
			{5,{{r::STATE_T,0xdead}},false,{},{},{{xaddress,0xdead},{yaddress,0xbeef}}}};
		c.data_events=std::vector<clocked_data_event>{{false,xaddress,xword,5},{false,yaddress,yword,5}};
		c.single_fetch=base+0x600;
		cases.push_back(std::move(c));
	};
	for (unsigned op=arithmetic ? 1 : 0;op<(arithmetic ? 3U : 1U);++op) {
		// All address cohorts remain: their flags vary with page/checkpoint context.
		for (unsigned context=0;context<3;++context) for (unsigned page=0;page<2;++page)
		for (unsigned linear=0;linear<2;++linear) for (unsigned layout=0;layout<3;++layout)
		for (unsigned destination=0;destination<2;++destination) for (unsigned y=0;y<16;++y) for (unsigned x=0;x<16;++x) {
			unsigned const flags=((y/4+4*destination+8*layout+24*linear+48*page+96*context)^(layout+3*linear+6*page+12*context))%32;
			append(op,x,y,layout,linear,destination,flags,context,page ? 0x20000 : 0,words[y%8],words[(y/8+2*destination+4*layout+4*linear)%8]);
		}
		for (unsigned flags=0;flags<32;++flags) for (unsigned destination=0;destination<2;++destination)
		for (u16 xword : words) for (u16 yword : words)
			append(op,8,9,0,true,destination,flags,cases.size()%3,cases.size()%2 ? 0x20000 : 0,xword,yword);
	}
	return cases;
}
}
#endif
