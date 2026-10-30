// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_TEST_DUAL_OPERAND_SECTION_H
#define C54X_TEST_DUAL_OPERAND_SECTION_H
#include "c54x_section_common.h"
#include <algorithm>
namespace c54x_test {
enum class dual_section_operation { multiply, arithmetic, unsigned_mac, mac, mac_round, mas, mas_round, store_mac, store_mac_round, store_mas, store_mas_round };
inline std::vector<clocked_register_case> dual_operand_cases(dual_section_operation kind)
{
	bool const arithmetic=kind==dual_section_operation::arithmetic,unsigned_mac=kind==dual_section_operation::unsigned_mac;
	bool const parallel=kind>=dual_section_operation::store_mac;
	unsigned const signed_mac=parallel ? unsigned(kind)-unsigned(dual_section_operation::store_mac) : kind>=dual_section_operation::mac ? unsigned(kind)-unsigned(dual_section_operation::mac) : 4;
	bool const signed_family=signed_mac<4,mac=unsigned_mac || signed_family;
	std::vector<clocked_register_case> cases;
	std::array<u16,8> const words{0,1,0xffff,0x8000,0x7fff,0x1234,0x5555,0xaaaa};
	std::array<u64,8> const seeds{0,1,0xffffffffffULL,0x7fffffff,0xff80000000ULL,0x0180000000ULL,0xfe7fffffffULL,0x123456789aULL};
	auto append=[&](unsigned op,unsigned x,unsigned y,unsigned layout,bool linear,unsigned destination,
		unsigned flags,unsigned context,u32 base,u16 xword,u16 yword,u64 seed,unsigned relation,unsigned asm_bits=0,bool sst=false) {
		unsigned const source=destination^relation;
		unsigned const feedback_register=parallel ? destination : source;
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
		u16 const st1=0x800|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0)|(flags&8 ? 0x100 : 0)|(x&1 ? 0x4020 : 0)|(parallel ? asm_bits : 0);
		u16 const pmst=(flags&4 ? 2 : 0)|(parallel && sst ? 1 : 0);
		s64 const left=s64(flags&8 ? s16(xword) : s32(xword))*65536;
		s64 const right=s64(flags&8 ? s16(yword) : s32(yword))*65536;
		s64 product=s64(unsigned_mac ? s32(xword) : s32(s16(xword)))*s16(yword)*(flags&1 ? 2 : 1);
		if (mac && (flags&7)==7) product=std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
		s64 const feedback=seed&0x8000000000ULL ? s64(seed)-(s64(1)<<40) : s64(seed);
		s64 number=mac ? feedback+(signed_family && (signed_mac&2) ? -product : product) : op ? (op==2 ? left-right : left+right) : product;
		if (signed_family && (signed_mac&1)) {
			s64 const biased=number+32768;
			number=(biased>=0 ? biased/65536 : -((-biased+65535)/65536))*65536;
		}
		bool const overflow=number<-2147483648LL || number>2147483647LL;
		u64 const result=u64(overflow && (flags&2) ? (number<0 ? -2147483648LL : 2147483647LL) : number)&0xffffffffffULL;
		bool const carry=op==2 ? xword>=yword : u32(xword)+yword>65535;
		u16 const final_st0=(op ? (st0&~0x800)|(carry ? 0x800 : 0) : st0)|(overflow ? destination ? 0x200 : 0x400 : 0);
		u64 const before_a=mac && !feedback_register ? seed^0xffffffffffULL : 0x123456789aULL;
		u64 const before_b=mac && feedback_register ? seed^0xffffffffffULL : 0xfeabcd9876ULL;
		u64 const a=destination ? mac && !feedback_register ? seed : before_a : result;
		u64 const b=destination ? result : mac && feedback_register ? seed : before_b;
		u64 const stored_source=source==destination ? seed : source ? before_b : before_a;
		int const shift=int(asm_bits)-(asm_bits&16 ? 32 : 0);
		u64 shifted=0;
		for (int bit=0;bit<40;++bit) {
			int const from=bit-shift;
			if ((from>=0 && from<40 && BIT(stored_source,from)) ||
				(shift<0 && from>=40 && (flags&8) && BIT(stored_source,39))) shifted|=u64(1)<<bit;
		}
		if (sst) {
			s64 const number=BIT(shifted,39) ? s64(shifted)-(s64(1)<<40) : s64(shifted);
			shifted=flags&8 ? u64(std::clamp(number,s64(-2147483648LL),s64(2147483647LL))) : std::min(shifted,u64(0xffffffff));
		}
		u16 const stored=u16(shifted>>16);
		u16 const opcode=(signed_family ? (parallel ? 0xd000 : 0xb000)|(signed_mac<<10)|(source<<9) : unsigned_mac ? 0xa600 : op ? op==2 ? 0xa200 : 0xa000 : 0xa400)|(destination<<8)|(x<<4)|y;
		auto initial=initial_case("dual-operands-address-capture-and-Execute-result",{opcode,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.data={{xaddress,xword},{yaddress,yword}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_PMST,pmst},{r::STATE_T,parallel ? yword : 0xabcdU},{r::STATE_A,before_a},{r::STATE_B,before_b},
			{r::STATE_AR0,3},{r::STATE_BK,linear ? 0U : 16U}});
		for (unsigned i=0;i<4;++i) initial.initial.push_back({r::STATE_AR2+i,initial_ar[i]});
		auto before=observation("dual-addresses-published-once",{{r::STATE_A,before_a},{r::STATE_B,before_b},{r::STATE_ST0,st0}});
		initial.expected.insert(initial.expected.end(),before.expected.begin(),before.expected.end());
		for (unsigned i=0;i<4;++i) before.expected.push_back({int(r::STATE_AR2+i),final_ar[i]});
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(before)},
			{5,observation("dual-Read-T-publication",{{r::STATE_A,before_a},{r::STATE_B,before_b},
				{r::STATE_ST0,st0},{r::STATE_T,parallel ? yword : op ? 0xabcdU : xword}})},
			{6,observation("dual-Execute-only-arithmetic",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,final_st0},{r::STATE_T,0xdead}})},
			{12,observation("dual-result-and-unchanged-context",{{r::STATE_IDLE,1},{r::STATE_A,a},{r::STATE_B,b},
				{r::STATE_ST0,final_st0},{r::STATE_ST1,parallel ? st1^31U : st1},{r::STATE_PMST,pmst},{r::STATE_T,0xdead},
				{r::STATE_AR2,0x2200},{r::STATE_AR3,0x2201},{r::STATE_AR4,0x2202},{r::STATE_AR5,0x2203}})}},3+context,12};
		c.actions={{4,{{r::STATE_AR2,0x2200},{r::STATE_AR3,0x2201},{r::STATE_AR4,0x2202},{r::STATE_AR5,0x2203},{r::STATE_BK,32}}},
			{5,{{r::STATE_T,0xdead}},false,{},{},{{xaddress,0xdead},{yaddress,0xbeef}}}};
		if (mac) c.actions.back().values.push_back({feedback_register ? r::STATE_B : r::STATE_A,seed});
		if (parallel) c.actions.back().values.push_back({r::STATE_ST1,st1^31U});
		c.data_events=std::vector<clocked_data_event>{{false,xaddress,xword,5}};
		c.data_events->push_back({parallel,yaddress,parallel ? stored : yword,parallel ? 6U : 5U});
		c.single_fetch=base+0x600;
		cases.push_back(std::move(c));
	};
	for (unsigned op=arithmetic ? 1 : 0;op<(arithmetic ? 3U : 1U);++op) {
		for (unsigned relation=0;relation<(signed_family ? 2U : 1U);++relation) {
		// MPY/ALU/MACSU retain status-varying cohorts; signed MAC distributes those contexts.
		for (unsigned context=0;context<(signed_family ? 1U : 3U);++context) for (unsigned page=0;page<(signed_family ? 1U : 2U);++page)
		for (unsigned linear=0;linear<2;++linear) for (unsigned layout=0;layout<3;++layout)
		for (unsigned destination=0;destination<2;++destination) for (unsigned y=0;y<16;++y) for (unsigned x=0;x<16;++x) {
			unsigned const flags=((y/4+4*destination+8*layout+24*linear+48*page+96*context)^(layout+3*linear+6*page+12*context))%32;
			unsigned const point=signed_family ? (x+y+layout+destination+relation)%3 : context;
			unsigned const bank=signed_family ? (x+y+layout+linear+destination+relation)%2 : page;
			append(op,x,y,layout,linear,destination,flags,point,bank ? 0x20000 : 0,words[y%8],words[(y/8+2*destination+4*layout+4*linear)%8],seeds[(layout+3*linear+6*page+12*context)%8],relation,(x+16*y)&31,(y/2)&1);
		}
		for (unsigned flags=0;flags<(signed_family ? 16U : 32U);++flags) for (unsigned destination=0;destination<2;++destination)
		for (unsigned seed=0;seed<(mac ? 8U : 1U);++seed)
		for (u16 xword : words) for (u16 yword : words) {
			// The signed product ignores SXM; parallel stores cover SXM separately below.
			unsigned const status=signed_family ? (flags&7)|((flags&8)<<1)|((xword+yword+seed+destination+relation)&1 ? 8 : 0) : flags;
			append(op,8,9,0,true,destination,status,cases.size()%3,cases.size()%2 ? 0x20000 : 0,xword,yword,seeds[seed],relation,cases.size()%32,(cases.size()/32)%2);
		}
		if (parallel) for (unsigned destination=0;destination<2;++destination) for (unsigned seed=0;seed<8;++seed)
		for (unsigned asm_bits=0;asm_bits<32;++asm_bits) for (unsigned sxm=0;sxm<2;++sxm) for (unsigned sst=0;sst<2;++sst)
			append(op,8,9,0,true,destination,(seed&7)|(sxm ? 8 : 0)|(seed&1 ? 16 : 0),cases.size()%3,
				cases.size()%2 ? 0x20000 : 0,0x8000,0x7fff,seeds[seed],relation,asm_bits,sst);
		}
	}
	return cases;
}
}
#endif
