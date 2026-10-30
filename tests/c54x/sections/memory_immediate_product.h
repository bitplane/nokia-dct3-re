// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_OBSERVE_MEMORY_MULTIPLY
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add=[&](bool mac,unsigned form,unsigned p,unsigned q,unsigned flags,unsigned selectors) {
		bool const b=selectors&1, bsrc=selectors&2;
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		u16 const word=std::array<u16,8>{0,1,0xffff,0x8000,0x7fff,0x7f,0x1234,0xf495}[p];
		u16 const literal=std::array<u16,8>{0,1,0xffff,0x8000,0x7fff,0x12c,0xf495,0xff80}[q];
		u16 const mode=std::array<u16,13>{4,0x82,0x92,0x8a,0x9a,0xe2,0xea,0xf2,0xf8,0xf8,0xf8,0xf8,0xf8}[form];
		u16 const address=std::array<u16,13>{0x1404,0x140c,0x140c,0x140c,0x140d,0x140f,0x140f,0x1402,0x140c,8,9,0xe,0x13}[form];
		unsigned const access=mode>=0xe0 ? 6 : 5, read=access+1, words=mode>=0xe0 ? 3 : 2;
		u16 const extension=mode==0xf8 ? address : 3;
		u16 const st0=0x1028|(p&1 ? 0x800 : 0)|(q&1 ? 0x600 : 0);
		u16 const st1=0x800|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0)|(flags&4 ? 0x100 : 0)|(!form && base ? 0x4000 : 0);
		u64 a=0x123456789aULL, bb=0xfedcba9876ULL;
		if (address==8) a=(a&~0xffffULL)|word;
		if (address==9) a=(a&~0xffff0000ULL)|(u64(word)<<16);
		s64 product=s64(s16(word))*s16(literal)*(flags&1 ? 2 : 1);
		if (mac) {
			constexpr std::array<u64,8> seeds{0,1,0x123456789aULL,0xffffffffffULL,0xff80000000ULL,0x7fffffff,0x0080000000ULL,0xfedcba9876ULL};
			a=seeds[p]; bb=seeds[q];
			if ((flags&11)==11) product=std::clamp<s64>(product,-2147483648LL,2147483647LL);
			u64 const source=bsrc ? bb : a;
			product+=source&0x8000000000ULL ? s64(source)-0x10000000000LL : s64(source);
		}
		bool const overflow=product < -2147483648LL || product > 2147483647LL;
		u64 const result=u64(overflow && (flags&2) ? std::clamp<s64>(product,-2147483648LL,2147483647LL) : product)&0xffffffffffULL;
		auto initial=initial_case("memory-immediate-product-capture-and-publication",{u16((mac ? 0x6400 : 0x6200)|(bsrc ? 0x200 : 0)|(b ? 0x100 : 0)|mode)});
		initial.program[1].value=words==3 ? extension : literal;
		if (words==3) initial.program[2].value=literal;
		initial.program[words].value=0xf4e1;
		for (auto &entry : initial.program) entry.address+=base;
		initial.data={{address,word}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a},{r::STATE_B,bb},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,mac && (flags&8) ? 2U : 0U},
			{r::STATE_T,address==0xe ? word : 0x9abcU},{r::STATE_SP,0x1400},{r::STATE_AR2,0x140c},{r::STATE_BK,form==7 ? 13U : 0U}});
		u64 poisoned_a=a;
		if (address==8) poisoned_a=(a&~0xffffULL)|0xdead;
		if (address==9) poisoned_a=(a&~0xffff0000ULL)|0xdead0000ULL;
		auto final=observation("Execute-uses-captured-word-and-literals-not-poisoned-T",{{r::STATE_A,b ? poisoned_a : result},
			{r::STATE_B,b ? result : bb},{r::STATE_ST0,u16(st0|(overflow ? b ? 0x200 : 0x400 : 0))},
			{r::STATE_ST1,st1},{r::STATE_PMST,mac && (flags&8) ? 2U : 0U},{r::STATE_T,0xdead},
			{r::STATE_AR2,0x3333}});
		auto retired=final;
		retired.expected.push_back({STATE_GENPC,base+0x601+words});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-address-capture",{})},
			{access,observation("Access-updates-pointer-once",{{r::STATE_AR2,std::array<unsigned,13>{0x140c,0x140c,0x140d,0x140b,0x140d,0x140c,0x140f,0x1402,0x140c,0x140c,0x140c,0x140c,0x140c}[form]}})},
			{read,observation("Read-publishes-T-before-Execute",{{r::STATE_T,word},{r::STATE_A,a},{r::STATE_B,bb},{r::STATE_ST0,st0}})},
			{read+1,std::move(final)},{14,std::move(retired)}},std::array<unsigned,4>{3,access,read,read+1}[cases.size()%4],14};
		c.actions={{3,{},false,{{base+0x601,0xf495}}}};
		if (words==3) c.actions.push_back({4,{},false,{{base+0x602,0xf495}}});
		c.actions.push_back({access,{{r::STATE_AR2,0x3333}}});
		c.actions.push_back({read,{{r::STATE_T,0xdead}},false,{},{},{{address,0xdead}}});
		c.data_events=std::vector<clocked_data_event>{{false,address,word,read}};
		c.retained_program_reads={{base+0x601,u16(words==3 ? extension : literal)}};
		if (words==3) c.retained_program_reads.push_back({base+0x602,literal});
		cases.push_back(std::move(c));
	};
	// Numeric contracts use one RAM form; routing crosses representative pairs.
	for (unsigned p=0;p<8;++p) for (unsigned q=0;q<8;++q) {
		for (unsigned flags=0;flags<4;++flags) for (unsigned b=0;b<2;++b) add(false,1,p,q,flags,b);
		for (unsigned flags=0;flags<16;++flags) for (unsigned sel=0;sel<4;++sel) add(true,1,p,q,flags,sel);
	}
	for (unsigned mac=0;mac<2;++mac) for (unsigned form=0;form<(mac ? 9U : 13U);++form) if (form!=4)
	for (unsigned pair=0;pair<4;++pair) for (unsigned flags=0;flags<(mac ? 16U : 8U);++flags)
	for (unsigned sel=0;sel<(mac ? 4U : 2U);++sel) add(mac,form,pair*2,(pair*2+3)%8,flags,sel);
	return cases;
}
}
