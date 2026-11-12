// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// MPY/MAC[R] Xmem,Ymem with X on a mapped AR or on the accumulator the
// instruction neither accumulates from nor writes. Read samples X once and
// publishes it to T; replacing the mapped source afterwards cannot change it.
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	static constexpr std::array<u16,8> sources{0x10,0x11,0x14,0x15,0x16,0x17,0x0b,0x0c};
	static constexpr std::array<u16,4> ys{0x7fff,0x8001,0x0003,0xfffe};
	for (unsigned op=0; op<5; ++op) for (unsigned source=0; source<8; ++source) for (unsigned xmode=0; xmode<2; ++xmode)
	for (unsigned pattern=0; pattern<4; ++pattern) for (unsigned frct=0; frct<2; ++frct) {
		// MPY A, MPY B, MAC A->A, MACR A->A, MAS B->B.
		bool const use_b=op==1 || op==4, accumulate=op>=2, round=op==3, subtract=op==4;
		u16 mapped=sources[source];
		if (mapped>=0x0b && mapped<=0x0d && use_b) mapped-=3; // read the other accumulator
		u16 const opcode=op<2 ? u16(0xa400|(use_b<<8)|(xmode<<7)|1) :
			u16(0xb000|(round ? 0x400 : 0)|(subtract ? 0x800 : 0)|(use_b ? 0x300 : 0)|(xmode<<7)|1);
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		std::array<u16,8> ar{0x1234,0x8001,mapped,0x1800,0x7ffe,0x0005,0xff00,0x00ff};
		u64 a=pattern&1 ? 0x00fedc8000ULL : 0xff80001234ULL, b=pattern&2 ? 0x0012348001ULL : 0xffffff7fffULL;
		auto sample=[&](u16 at)->u16 {
			if (at>=0x10) return ar[at-0x10];
			u64 const acc=at<0x0b ? a : b; unsigned const part=(at-8)%3;
			return part==2 ? u16(u8(acc>>32)) : u16(acc>>(16*part));
		};
		u16 const xword=sample(mapped), yword=ys[pattern];
		u16 const st1=0x800|(frct ? 0x40 : 0);
		auto initial=initial_case("dual-multiply-Read-samples-mapped-X",{opcode,0xf4e1});
		for (auto &word:initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x1800},{r::STATE_ST1,st1},
			{r::STATE_PMST,0},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0xbeef},{r::STATE_BK,0}});
		for (unsigned i=0; i<8; ++i) initial.initial.push_back({r::STATE_AR0+i,ar[i]});
		initial.data.push_back({0x1800,yword});
		auto signed40=[](u64 value) { return BIT(value,39) ? s64(value)-(s64(1)<<40) : s64(value); };
		s64 product=s64(s16(xword))*s16(yword)*(frct ? 2 : 1);
		u64 &destination=use_b ? b : a;
		s64 result=accumulate ? signed40(destination)+(subtract ? -product : product) : product;
		if (round) result=(result+0x8000)&~s64(0xffff);
		// Poison after Read: a mapped AR, or the other accumulator's whole value.
		clocked_register_action poison{5,{}};
		u64 const other=use_b ? a : b;
		if (mapped>=0x10) { poison.values={{int(r::STATE_AR0+mapped-0x10),0x5a5a}}; ar[mapped-0x10]=0x5a5a; }
		else { poison.values={{use_b ? r::STATE_A : r::STATE_B,other^0x5a5a5a5a5aULL}}; (use_b ? a : b)=other^0x5a5a5a5a5aULL; }
		destination=u64(result)&0xffffffffffULL;
		if (xmode && mapped!=0x12) ar[2]=mapped+1; // *AR2+ after the original-address read
		auto final=observation("retained-mapped-X-product-and-T",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,xword},{r::STATE_ST1,st1}});
		for (unsigned i=0; i<8; ++i) final.expected.push_back({int(r::STATE_AR0+i),ar[i]});
		clocked_register_case c{{{0,std::move(initial)},{12,std::move(final)}},std::array<unsigned,3>{3,5,6}[cases.size()%3],12};
		c.clear_pipeline_before_restore=true;
		c.data_events=std::vector<clocked_data_event>{{false,mapped,xword,5,true},{false,0x1800,yword,5,true}};
		c.actions.push_back(std::move(poison));
		cases.push_back(std::move(c));
	}
	return cases;
}
}
