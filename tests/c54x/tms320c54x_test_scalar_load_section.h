// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_SCALAR_LOAD_SECTION_H
#define C54X_SCALAR_LOAD_SECTION_H
namespace c54x_test {
enum class scalar_load { offset, unsigned_word, shift16, variable };
inline std::vector<clocked_register_case> scalar_load_cases(scalar_load family)
{
	std::vector<clocked_register_case> cases;
	bool const variable=family==scalar_load::variable;
	auto append=[&](unsigned form,unsigned reg,unsigned destination,unsigned pattern,unsigned flags,bool compatibility,int shift,int source=-1,unsigned kind=0) {
		bool const mapped=source>=0;
		unsigned const selected=compatibility && !reg ? 3 : reg;
		int const offset=std::array<s16,8>{0,1,-1,17,s16(0xf495),0x7fff,s16(0x8000),2}[pattern];
		u16 const bk=!mapped && (form==11 || form==13 || form==16) ? 0x8000 : 16;
		u16 mode=form<2 ? reg*16+pattern+(flags&1 ? 8 : 0) : form==17 ? 0xf8 : 0x80|((form-2)<<3)|reg;
		u16 address=form==0 ? 0x200+mode : form==1 ? 0x1000+mode : form>=14 && form<17 ? u16(0x5007+offset) :
			form==17 ? u16(0x1500+offset) : 0x5007;
		if (!mapped && form==16) { int const index=(0x5007+offset)%0x8000; address=index<0 ? index+0x8000 : index; }
		u16 next=0x5007;
		if (form>=14 && form<17) next=form==14 ? 0x5007 : address;
		else if (form>=2 && form<14) next=std::array<u16,12>{0x5007,0x5006,0x5008,0x5008,0,0,0xa00e,0x2803,0x5006,0,0x5008,0x200e}[form-2];
		u16 literal=form==17 ? address : u16(offset);
		u16 sample=family==scalar_load::offset ? std::array<u16,8>{1,1,1,0xffff,0x8000,0xffff,0x1234,0xffff}[pattern] :
			std::array<u16,8>{0,1,0x7fff,0x8000,0xffff,0xf495,0xff,0xff00}[pattern];
		u64 old=std::array<u64,8>{0,0xffffffff,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x56783933,0x80aa001234ULL}[pattern];
		u16 st0=0x1804|(compatibility ? 0x6000 : 0)|(pattern&1 ? 0x600 : 0);
		if (mapped) {
			address=0x10+source; mode=kind==0 ? address : kind==1 ? 0xf8 : 0xe7;
			literal=kind==2 ? 0xb009+source : address;
			sample=source==7 ? 0x5007 : std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
			old=0xdeadbeef; st0=family==scalar_load::offset ? 0x1804 : 0x1800;
		}
		u16 const st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0)|(compatibility ? 0x20 : 0)|(!mapped && form==1 ? 0x4000 : 0);
		u16 const final_st0=compatibility && reg && form>=2 && form!=17 ? (st0&0x1fff)|(reg<<13) : st0;
		s64 const operand=family!=scalar_load::unsigned_word && (flags&1) ? s16(sample) : s64(sample);
		u64 const bits=u64(operand)&0xffffffffffULL;
		int const amount=variable ? shift : family==scalar_load::shift16 ? 16 : 0;
		u64 result=0;
		for (int bit=0;bit<40;++bit) {
			int const from=bit-amount;
			if ((from>=0 && from<40 && BIT(bits,from)) || (from>=40 && (flags&1) && BIT(bits,39))) result|=u64(1)<<bit;
		}
		u32 const base=(cases.size()+form+reg)&1 ? 0x20000 : 0;
		unsigned const read=(form>=14 ? 6 : 5)+unsigned(variable);
		u16 const opcode=(variable ? 0x6f00 : (family==scalar_load::offset ? 0x1000 : family==scalar_load::unsigned_word ? 0x1200 : 0x4400)|(destination<<8))|mode;
		u16 const extension=0x0c40|(destination<<8)|(shift&31);
		auto initial=initial_case("scalar-memory-load-retains-Read-word",{opcode});
		unsigned cursor=1;
		if (form>=14) initial.program[cursor++].value=literal;
		if (variable) initial.program[cursor].value=extension;
		for (auto &word : initial.program) word.address+=base;
		u64 const a=destination ? 0x123456789aULL : old, b=destination ? old : 0x123456789aULL;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_BK,bk}});
		for (unsigned i=0;i<8;++i) initial.initial.push_back({r::STATE_AR0+i,mapped && int(i)==source ? sample : 0x5007U});
		if (!mapped) initial.data={{address,sample}};
		auto before=observation("load-Access-updates-ARP-and-pointer",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,final_st0},{int(r::STATE_AR0+selected),next}});
		auto final=observation("load-Execute-publishes-without-resampling",{{r::STATE_A,destination ? a : result},{r::STATE_B,destination ? result : b},
			{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_BK,32},{r::STATE_SP,0x2222},{r::STATE_PMST,0}});
		for (unsigned i=0;i<8;++i) final.expected.push_back({int(r::STATE_AR0+i),mapped && int(i)==source ? 0xdeadU : i==selected && address!=0x17 ? 0x2222U : 0x5007U});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("load-running",{})},{read-1,std::move(before)},
			{read,observation("load-Read-preserves-old-accumulators",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,final_st0}})},
			{read+1,final},{18,std::move(final)}},std::array<unsigned,3>{variable && form>=14 ? 4U : 3U,read-1,read}[cases.size()%3],18};
		c.require_idle=false; c.single_fetch=base+0x600;
		c.data_events=std::vector<clocked_data_event>{{false,address,sample,read}};
		if (form>=14) c.retained_program_reads.push_back({base+0x601,literal});
		if (variable) c.retained_program_reads.push_back({base+0x601+unsigned(form>=14),extension});
		if (!c.retained_program_reads.empty()) {
			c.actions={{4,{}}};
			for (auto const &word : c.retained_program_reads) c.actions.back().program.push_back({word.address,0xdead});
		}
		c.actions.push_back({read-1,{{r::STATE_SP,0x2222},{r::STATE_BK,32}}});
		if (address!=0x17) c.actions.back().values.push_back({r::STATE_AR0+selected,0x2222});
		c.actions.push_back({read,{}}); c.actions.back().data={{address,0xdead}};
		cases.push_back(std::move(c));
	};
	for (unsigned form=family==scalar_load::offset ? 14 : 0;form<18;++form) {
		if (form==5) continue;
		for (unsigned rindex=0;rindex<(variable ? 2U : 8U);++rindex)
		for (unsigned destination=0;destination<2;++destination) for (unsigned pattern=0;pattern<(variable ? 4U : 8U);++pattern)
		for (unsigned flags=0;flags<4;++flags) for (unsigned compatibility=0;compatibility<(variable ? 1U : 2U);++compatibility)
		for (int shift=variable ? -16 : 0;shift<(variable ? 16 : 1);++shift) {
			unsigned const reg=variable ? rindex ? 6 : 1 : rindex;
			if (variable && !(form==4 && reg==1) && shift!=-16 && shift!=-1 && shift!=0 && shift!=15) continue;
			append(form,reg,destination,pattern,flags,compatibility,shift);
		}
	}
	if (!variable) for (unsigned source=0;source<8;++source) for (unsigned destination=0;destination<2;++destination)
	for (unsigned pattern=0;pattern<4;++pattern) for (unsigned flags=0;flags<(family==scalar_load::offset ? 2U : 4U);++flags)
	for (unsigned kind=family==scalar_load::offset ? 2U : 0U;kind<3;++kind)
		append(kind==0 ? 0 : kind==1 ? 17 : 14,7,destination,pattern,flags,false,0,source,kind);
	return cases;
}
}
#endif
