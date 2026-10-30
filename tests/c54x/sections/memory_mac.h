// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add=[&](bool square,bool bk,unsigned form,unsigned selector,unsigned tp,unsigned mp,unsigned flags) {
		unsigned const operation=selector/2, read=5+unsigned(form!=0);
		bool const b=selector&1, absolute=form!=0;
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		u16 const t=std::array<u16,8>{0,1,0xffff,0x7fff,0x8000,0x8001,0x1234,0xabcd}[tp];
		u16 const memory=std::array<u16,8>{0,1,0xffff,0x7fff,0x8000,0x8001,0x5678,0xfedc}[mp];
		u64 const source=std::array<u64,8>{0,1,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x1234567833ULL,0x80aa001234ULL}[tp];
		u16 const st0=(0xa604|(tp&1 ? 0x600 : 0)|(mp&1 ? 0x800 : 0))&(bk ? 0xfe00 : 0xffff);
		u16 const st1=0x800|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0)|(flags&8 ? 0x100 : 0);
		s64 product=s64(square ? s16(memory) : s16(t))*s16(memory)*(flags&1 ? 2 : 1);
		if (!square && (flags&7)==7) product=std::clamp<s64>(product,-2147483648LL,2147483647LL);
		s64 const acc=BIT(source,39) ? s64(source)-0x10000000000LL : s64(source);
		s64 result=operation>=(square ? 1U : 2U) ? acc-product : acc+product;
		if (!square && (operation&1)) result=(result+32768)&~s64(65535);
		bool const overflow=result < -2147483648LL || result > 2147483647LL;
		u64 const stored=u64(overflow && (flags&2) ? std::clamp<s64>(result,-2147483648LL,2147483647LL) : result)&0xffffffffffULL;
		u16 const address=bk ? 0x19 : form==2 ? 0x11 : 0x500;
		u16 const opcode=((square ? 0x3800 : 0x2800)+operation*0x200)|(unsigned(b)<<8)|(absolute ? 0xf8 : bk ? 0x19 : 0x81);
		auto initial=initial_case("memory-MAC-retains-Smem-and-T-without-republication",{opcode});
		if (absolute) initial.program[1].value=address;
		// Keep the original NOP context during mapped-square admission; an
		// adjacent IDLE is a distinct control composition, not this contract.
		initial.program[5+absolute].value=0xf4e1;
		for (auto &word : initial.program) word.address+=base;
		if (address>=0x60) initial.data={{address,memory}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,b ? 0x246813579aULL : source},
			{r::STATE_B,b ? source : 0x246813579aULL},{r::STATE_T,t},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_PMST,flags&4 ? 2U : 0U},{r::STATE_AR1,address==0x11 ? memory : 0x500U},{r::STATE_BK,bk ? memory : 0U}});
		auto final=observation("captured-operands-rounding-saturation-and-sticky-flags",{{r::STATE_A,b ? 0x246813579aULL : stored},
			{r::STATE_B,b ? stored : 0x246813579aULL},{r::STATE_T,0xdead},{r::STATE_ST0,u16(st0|(overflow ? b ? 0x200 : 0x400 : 0))},
			{r::STATE_ST1,st1},{r::STATE_PMST,flags&4 ? 2U : 0U}});
		if (bk) final.expected.push_back({r::STATE_BK,0xdead});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("MAC-running",{})},
			{read,observation("square-only-T-publication-at-Read",{{r::STATE_T,square ? memory : t}})},
			{read+1,std::move(final)}},read-1+unsigned(cases.size()%3),12};
		if (absolute) c.actions.push_back({4,{},false,{{base+0x601,0xdead}}});
		c.actions.push_back({read,{{r::STATE_T,0xdead}},false,{},{},{{address,0xdead}}});
		c.data_events=std::vector<clocked_data_event>{{false,address,memory,read}};
		if (absolute) c.retained_program_reads={{base+0x601,address}};
		cases.push_back(std::move(c));
	};
	for (unsigned square=0;square<2;++square) {
		for (unsigned selector=0;selector<(square ? 4U : 8U);++selector) for (unsigned tp=0;tp<8;++tp)
		for (unsigned mp=0;mp<8;++mp) for (unsigned flags=0;flags<16;++flags) add(square,false,0,selector,tp,mp,flags);
		for (unsigned form=0;form<3;++form) for (unsigned selector=0;selector<(square ? 4U : 8U);++selector)
		for (unsigned pair=0;pair<4;++pair) for (unsigned flags=0;flags<16;++flags) add(square,false,form,selector,pair*2,(pair*2+3)%8,flags);
	}
	for (unsigned form=0;form<2;++form) for (unsigned selector=0;selector<8;++selector)
	for (unsigned pair=0;pair<4;++pair) for (unsigned flags=0;flags<16;++flags) add(false,true,form,selector,pair*2,(pair*2+3)%8,flags);
	return cases;
}
}
