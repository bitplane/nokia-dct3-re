// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> mapped_alu_cases(unsigned group)
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<u64,8> accs{0,0xffffffffULL,0x7fffffffULL,0xff80000000ULL,0x7fffffffffULL,0x8000000000ULL,0xffff,0x123456789aULL};
	constexpr std::array<u16,8> words{0,1,0x7fff,0x8000,0xffff,0xaaaa,0x5555,0xfffe};
	auto add=[&](unsigned op,bool mapped,u16 address,unsigned form,unsigned p,unsigned q,unsigned flags,bool b,bool late,bool source_b) {
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		u64 a=b ? 0x123456789aULL : accs[p], bb=b ? accs[p] : 0xfeabcd9876ULL;
		u16 const t=words[p], trn=words[(p+3)%8];
		std::array<u16,8> ar{}; for (unsigned i=0;i<8;++i) ar[i]=0x1200+0x100*i;
		ar[0]=1;
		u16 st0=0x1000|(mapped && !form ? 0 : 0x20)|(flags&1 ? 0x800 : 0)|
			(p&1 ? b ? 0x400 : 0x200 : 0)|(p&2 ? b ? 0x200 : 0x400 : 0);
		u16 st1=0x800|(flags&2 ? 0x100 : 0)|(flags&4 ? 0x200 : 0), mode, literal=0, bk=0;
		bool extension;
		u16 word=words[q];
		if (mapped) {
			mode=form==0 ? address : form==1 ? 0xf8 : 0x83;
			extension=form==1; literal=address;
			if (form==2) ar[3]=address;
			if ((op==3 || op==4) && source_b!=b) std::swap(a,bb);
			word=address<14 ? u16((address<11 ? a : bb)>>(16*((address-8)%3)))&((address-8)%3==2 ? 0xff : 0xffff) :
				address==14 ? t : address==15 ? trn : ar[address-16];
		} else {
			mode=std::array<u16,6>{2,2,0xf8,0x93,0xe3,0xd3}[form];
			extension=form==2 || form==4; literal=form==4 ? 2 : 0x1002;
			address=form==1 ? 0x3002 : form==5 ? 0x100c : 0x1002;
			ar[3]=form==4 ? 0x1000 : address;
			if (form==1) st1|=0x4000;
			if (form==5) bk=13;
		}
		u16 const next_ar=!mapped && form==3 ? 0x1003 : !mapped && form==5 ? 0x1000 : ar[3];
		u64 after_a=a,after_b=bb;
		if (mapped && address<14) {
			unsigned const part=(address-8)%3; u64 const mask=part==2 ? 0xff : 0xffff;
			u64 &src=address<11 ? after_a : after_b;
			src=(src&~(mask<<(16*part)))|((0xa55aULL&mask)<<(16*part));
		}
		bool const shifted=op==3 || op==4;
		if (late) ((shifted ? source_b : b) ? after_b : after_a)=accs[(p+1)%8];
		u16 const execute_st0=st0^(late ? 0x800 : 0);
		u64 const source=(shifted ? source_b : b) ? after_b : after_a;
		s64 const left=source&0x8000000000ULL ? s64(source)-0x10000000000LL : s64(source);
		s64 result; u16 final_st0=execute_st0;
		if (op>=5) result=op==5 ? source&word : op==6 ? source|word : source^word;
		else {
			s64 const right=shifted ? s64(st1&0x100 ? s16(word) : s32(word))*65536 : s64(word)+(op==0 ? (execute_st0>>11)&1 : 0);
			bool const subtract=op==2 || op==4;
			result=subtract ? left-right : left+right;
			bool const carry=subtract ? u32(source)>=u32(right) : u64(u32(source))+u32(right)>0xffffffffULL;
			if (op<3 || carry!=subtract) final_st0=(final_st0&~0x800)|(carry ? 0x800 : 0);
			if (result < -2147483648LL || result > 2147483647LL) {
				final_st0|=b ? 0x200 : 0x400;
				if (st1&0x200) result=std::clamp<s64>(result,-2147483648LL,2147483647LL);
			}
		}
		u16 const opcode=(op==0 ? 0x0600 : op==1 ? 0x0200 : op==2 ? 0x0a00 :
			op==3 ? 0x3c00 : op==4 ? 0x4000 : 0x1800+((op-5)<<9))|(b<<8)|mode|
			((op==3 || op==4) ? unsigned(source_b)<<9 : 0);
		unsigned const read=extension ? 6 : 5;
		auto initial=initial_case("mapped-ALU-retains-Smem-and-uses-Execute-live-registers",{opcode});
		if (extension) initial.program[1].value=literal;
		initial.program[1+extension].value=0xf4e1;
		for (auto &entry : initial.program) entry.address+=base;
		if (!mapped) initial.data={{address,word}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a},{r::STATE_B,bb},{r::STATE_T,t},
			{r::STATE_TRN,trn},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_SP,0x3000},{r::STATE_BK,bk}});
		for (unsigned i=0;i<8;++i) initial.initial.push_back({r::STATE_AR0+i,ar[i]});
		auto final=observation("retained-word-live-addend-and-sticky-status",{{r::STATE_A,b ? after_a : u64(result)&0xffffffffffULL},
			{r::STATE_B,b ? u64(result)&0xffffffffffULL : after_b},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},
			{r::STATE_T,address==14 ? 0xa55aU : t},{r::STATE_TRN,address==15 ? 0xa55aU : trn},
			{r::STATE_BK,bk},{r::STATE_SP,0x3000}});
		for (unsigned i=0;i<8;++i) final.expected.push_back({int(r::STATE_AR0+i),0x3330+i});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("ALU-running",{})},
			{read,observation("Read-resolves-pointer-before-poison",{{r::STATE_AR3,next_ar}})},
			{12,std::move(final)}},read-1+unsigned(cases.size()%3),12};
		c.clear_pipeline_before_restore=op>=3;
		clocked_register_action poison{read,{{r::STATE_A,after_a},{r::STATE_B,after_b},{r::STATE_ST0,execute_st0}}};
		if (!mapped) poison.data={{address,0xa55a}};
		if (address==14) poison.values.push_back({r::STATE_T,0xa55a});
		if (address==15) poison.values.push_back({r::STATE_TRN,0xa55a});
		for (unsigned i=0;i<8;++i) poison.values.push_back({r::STATE_AR0+i,0x3330+i});
		c.actions.push_back(std::move(poison));
		// The Read observation is instantaneous; later pointer poison is deliberate.
		c.states.insert(c.states.end()-1,{read+1,observation("operand-captured",{})});
		c.data_events=std::vector<clocked_data_event>{{false,address,word,read}};
		if (extension) c.retained_program_reads={{base+0x601,literal}};
		cases.push_back(std::move(c));
	};
	for (unsigned op=group==0 ? 0 : group==1 ? 3 : 5;op<(group==0 ? 3U : group==1 ? 5U : 8U);++op)
	for (unsigned sb=0;sb<(group==1 ? 2U : 1U);++sb) {
		if (group==0) {
			for (unsigned p=0;p<8;++p) for (unsigned q=0;q<8;++q) for (unsigned flags=0;flags<8;++flags)
			for (unsigned b=0;b<2;++b) for (unsigned late=0;late<2;++late) add(op,false,0,0,p,q,flags,b,late,sb);
			for (unsigned form=0;form<6;++form) for (unsigned p=0;p<4;++p) for (unsigned flags=0;flags<8;++flags)
			for (unsigned b=0;b<2;++b) add(op,false,0,form,p,p*2,flags,b,p&1,sb);
		}
		for (u16 address=8;address<24;++address) for (unsigned form=0;form<3;++form)
		for (unsigned p=0;p<4;++p) for (unsigned flags=0;flags<8;++flags) for (unsigned b=0;b<2;++b)
			add(op,true,address,form,p,0,flags,b,p&1,sb);
	}
	return cases;
}
}
