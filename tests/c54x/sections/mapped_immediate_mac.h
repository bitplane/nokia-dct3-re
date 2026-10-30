// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add=[&](u16 address,unsigned form,unsigned pattern,unsigned q,unsigned flags,unsigned selectors) {
		bool const bdst=selectors&1, bsrc=selectors&2;
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		unsigned const pointer=address==0x17 ? 6 : 7, words=form>=2 ? 3 : 2;
		unsigned const access=words==3 ? 6 : 5, read=access+1;
		u16 word=std::array<u16,4>{0,0x7fff,0x8000,0xffff}[pattern];
		if (address==0xa || address==0xd) word&=0xff;
		u16 const literal=std::array<u16,8>{0,1,0xffff,0x8000,0x7fff,0x12c,0xf495,0xf38b}[q];
		u16 const mode=form==0 ? address : form==1 ? 0x80+pointer : form==2 ? 0xf8 : 0xe0+pointer;
		u16 const extension=form==2 ? address : 0, st0=0x1000|(pattern&1 ? 0x800 : 0);
		u16 const st1=0x800|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0)|(flags&4 ? 0x100 : 0);
		u64 a=0x123456789aULL, b=0xfedcba9876ULL;
		if (address<=0xd) {
			u64 &acc=address>=0xb ? b : a;
			unsigned const shift=((address-8)%3)*16;
			u64 const mask=u64(shift==32 ? 0xff : 0xffff)<<shift;
			acc=(acc&~mask)|(u64(word)<<shift);
		}
		u64 const late_a=std::array<u64,4>{0,1,0x7fffffff,0xff80000000ULL}[pattern];
		u64 const late_b=std::array<u64,4>{0xffffffffffULL,0x0080000000ULL,0xff80000001ULL,0x123456789aULL}[pattern];
		s64 product=s64(s16(word))*s16(literal)*(flags&1 ? 2 : 1);
		if ((flags&11)==11) product=std::clamp<s64>(product,-2147483648LL,2147483647LL);
		u64 const source=bsrc ? late_b : late_a;
		s64 const total=product+(source&0x8000000000ULL ? s64(source)-0x10000000000LL : s64(source));
		bool const overflow=total < -2147483648LL || total > 2147483647LL;
		u64 const result=u64(overflow && (flags&2) ? std::clamp<s64>(total,-2147483648LL,2147483647LL) : total)&0xffffffffffULL;
		auto initial=initial_case("mapped-MAC-Read-word-and-Execute-live-addend",{u16(0x6400|(bsrc ? 0x200 : 0)|(bdst ? 0x100 : 0)|mode)});
		initial.program[1].value=words==3 ? extension : literal;
		if (words==3) initial.program[2].value=literal;
		initial.program[words].value=0xf4e1;
		for (auto &entry : initial.program) entry.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_T,address==0xe ? word : 0x9abcU},{r::STATE_TRN,address==0xf ? word : 0x7badU},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,flags&8 ? 2U : 0U}});
		for (unsigned i=0;i<8;++i) initial.initial.push_back({r::STATE_AR0+i,
			i==pointer ? address : address==0x10+i ? word : 0x4000+i});
		auto final=observation("captured-source-not-poisoned-register-field",{{r::STATE_A,bdst ? late_a : result},
			{r::STATE_B,bdst ? result : late_b},{r::STATE_T,0x5a5a},{r::STATE_TRN,0xdead},
			{r::STATE_ST0,u16(st0|(overflow ? bdst ? 0x200 : 0x400 : 0))},{r::STATE_ST1,st1},
			{r::STATE_PMST,flags&8 ? 2U : 0U}});
		for (unsigned i=0;i<8;++i) final.expected.push_back({int(r::STATE_AR0+i),0x7000+i});
		auto retired=final; retired.expected.push_back({STATE_GENPC,base+0x601+words});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("mapped-MAC-running",{})},
			{read,observation("Read-publishes-original-T",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,word},{r::STATE_ST0,st0}})},
			{read+1,std::move(final)},{16,std::move(retired)}},
			std::array<unsigned,3>{3,read,read+1}[cases.size()%3],16};
		c.clear_pipeline_before_restore=true;
		c.actions={{3,{},false,{{base+0x601,0xf495}}}};
		if (words==3) c.actions.push_back({4,{},false,{{base+0x602,0xf495}}});
		c.actions.push_back({access,{{r::STATE_AR0+pointer,0x7000+pointer}}});
		clocked_register_action poison{read,{{r::STATE_A,late_a},{r::STATE_B,late_b},{r::STATE_T,0x5a5a},{r::STATE_TRN,0xdead}}};
		for (unsigned i=0;i<8;++i) poison.values.push_back({r::STATE_AR0+i,0x7000+i});
		c.actions.push_back(std::move(poison));
		c.data_events=std::vector<clocked_data_event>{{false,address,word,read}};
		c.retained_program_reads={{base+0x601,u16(words==3 ? extension : literal)}};
		if (words==3) c.retained_program_reads.push_back({base+0x602,literal});
		cases.push_back(std::move(c));
	};
	for (unsigned pattern=0;pattern<4;++pattern) for (unsigned q=0;q<8;++q)
	for (unsigned flags=0;flags<16;++flags) for (unsigned sel=0;sel<4;++sel) add(8,0,pattern,q,flags,sel);
	for (u16 address=8;address<24;++address) for (unsigned form=0;form<4;++form)
	for (unsigned pattern=0;pattern<4;++pattern) for (unsigned flags=0;flags<16;++flags)
	for (unsigned sel=0;sel<4;++sel) add(address,form,pattern,(address+form+pattern+flags+sel)%8,flags,sel);
	return cases;
}
}
