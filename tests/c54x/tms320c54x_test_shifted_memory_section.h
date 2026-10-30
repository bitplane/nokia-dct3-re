// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_SHIFTED_MEMORY_SECTION_H
#define C54X_SHIFTED_MEMORY_SECTION_H
namespace c54x_test {
inline std::vector<clocked_register_case> shifted_memory_cases(bool subc, bool bk_source)
{
	std::vector<clocked_register_case> cases;
	for (unsigned endpoint=bk_source ? 20 : 0;endpoint<(bk_source ? 21U : 20U);++endpoint)
	for (unsigned selector=0;selector<(subc ? 2U : 8U);++selector)
	for (unsigned pattern=0;pattern<(subc ? 8U : 4U);++pattern)
	for (unsigned memory_pattern=0;memory_pattern<(subc ? 8U : 1U);++memory_pattern)
	for (unsigned flags=0;flags<4;++flags) for (int shift=-16;shift<16;++shift) {
		if (subc && shift) continue;
		// Full shift domain at postincrement RAM, AR0 and BK; other routes retain signed boundaries and zero.
		if (!subc && endpoint!=4 && endpoint!=12 && endpoint!=20 && shift!=-16 && shift!=-1 && shift!=0 && shift!=15) continue;
		bool const mapped=endpoint>=12, destination=selector&1;
		unsigned const form=mapped ? 5 : endpoint/2, reg=mapped || !(endpoint&1) ? 1 : 6;
		int const offset=std::array<s16,8>{0,1,-1,17,s16(0xf495),0x7fff,s16(0x8000),2}[pattern];
		u16 const mode=form<2 ? reg*16+pattern+(flags&1 ? 8 : 0) : form==2 ? 0x90|reg : form==3 ? 0xe0|reg : form==4 ? 0xe8|reg : 0xf8;
		u16 const address=mapped ? endpoint==20 ? 0x19 : 0x10+endpoint-12 : form<2 ? (form ? 0x1000 : 0x200)+mode :
			form==5 ? u16(0x1500+offset) : form>=3 ? u16(0x5007+offset) : 0x5007;
		u16 const next=form==2 ? 0x5008 : form==4 ? address : 0x5007, literal=form==5 ? address : u16(offset);
		u16 const sample=subc ? std::array<u16,8>{0,1,0x7fff,0x8000,0xffff,7,2,0x1234}[memory_pattern] :
			std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		u64 const source=subc ? std::array<u64,8>{0,1,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x1fffffff,0xffffffffULL}[pattern] : 0;
		u64 const a=subc ? destination ? 0x12345678 : source : std::array<u64,4>{0,0x7fffffff,0xff80000000ULL,0x017fffffffULL}[pattern];
		u64 const b=subc ? destination ? source : 0x12345678 : std::array<u64,4>{0xffffffffffULL,0xff7fffffffULL,0x12345678,0xff80000000ULL}[pattern];
		u64 const chosen=subc ? source : selector&2 ? b : a;
		s64 const left=BIT(chosen,39) ? s64(chosen)-(s64(1)<<40) : s64(chosen);
		s64 const operand=flags&1 ? s16(sample) : s64(sample);
		u64 shifted=0;
		if (!subc) {
			u64 const bits=u64(operand)&0xffffffffffULL;
			for (int bit=0;bit<40;++bit) {
				int const from=bit-shift;
				if ((from>=0 && from<40 && BIT(bits,from)) || (from>=40 && (flags&1) && BIT(bits,39))) shifted|=u64(1)<<bit;
			}
		}
		s64 const right=subc ? operand*32768 : BIT(shifted,39) ? s64(shifted)-(s64(1)<<40) : s64(shifted);
		s64 const sum=subc || (selector&4) ? left-right : left+right;
		bool const carry=subc ? sum>=0 : selector&4 ? u32(chosen)>=u32(shifted) : u64(u32(chosen))+u32(shifted)>0xffffffffULL;
		bool const overflow=sum<-2147483648LL || sum>2147483647LL;
		u64 const result=u64(subc ? (carry ? sum*2+1 : left*2) : overflow && (flags&2) ?
			std::clamp(sum,s64(-2147483648LL),s64(2147483647LL)) : sum)&0xffffffffffULL;
		u16 const st0=0x1804|(pattern&1 ? 0x600 : 0), final_st0=(st0&~0x800)|(carry ? 0x800 : 0)|(overflow ? destination ? 0x200 : 0x400 : 0);
		u16 const st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0)|(!mapped && form==1 ? 0x4000 : 0);
		u16 const extension=0x0c00|((selector&3)<<8)|((selector&4)<<3)|(shift&31);
		u32 const base=(endpoint+selector+pattern+memory_pattern+flags+unsigned(shift+16))&1 ? 0x20000 : 0;
		unsigned const read=(form>=3 ? 6 : 5)+unsigned(!subc);
		auto initial=initial_case(subc ? "SUBC-retained-memory-and-live-dividend" : "shifted-memory-ADD-SUB-Execute-live-accumulator",
			{u16((subc ? 0x1e00|(selector<<8) : 0x6f00)|mode)});
		unsigned cursor=1;
		if (form>=3) initial.program[cursor++].value=literal;
		if (!subc) initial.program[cursor].value=extension;
		for (auto &word : initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0x1122},{r::STATE_B,0x3344},{r::STATE_T,0x9abc},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_BK,endpoint==20 ? sample : 16U}});
		for (unsigned i=0;i<8;++i) initial.initial.push_back({r::STATE_AR0+i,mapped && endpoint!=20 && address==0x10+i ? sample : 0x5007U});
		if (!mapped) initial.data={{address,sample}};
		auto before=observation("memory-arithmetic-before-Read",{{r::STATE_A,0x1122},{r::STATE_B,0x3344},{r::STATE_ST0,st0},{r::STATE_T,0x9abc}});
		if (!mapped) before.expected.push_back({int(r::STATE_AR0+reg),next});
		auto final=observation("Execute-uses-retained-word-and-late-accumulators",{{r::STATE_A,destination ? a : result},{r::STATE_B,destination ? result : b},
			{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_T,0x9abc},{r::STATE_PMST,0},
			{r::STATE_SP,mapped ? 0x1000U : 0x2222U},{r::STATE_BK,endpoint==20 ? 0xdeadU : 16U}});
		for (unsigned i=0;i<8;++i) final.expected.push_back({int(r::STATE_AR0+i),!mapped && i==reg ? 0x2222U :
			mapped && endpoint!=20 && address==0x10+i ? 0xdeadU : 0x5007U});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("memory-arithmetic-running",{})},{read-1,std::move(before)},
			{read,observation("Read-does-not-publish-accumulator",{{r::STATE_A,0x1122},{r::STATE_B,0x3344},{r::STATE_ST0,st0}})},
			{read+1,final},{18,std::move(final)}},std::array<unsigned,3>{!subc && form>=3 ? 4U : 3U,read-1,read}[cases.size()%3],18};
		c.require_idle=false; c.single_fetch=base+0x600;
		c.data_events=std::vector<clocked_data_event>{{false,address,sample,read}};
		if (form>=3) c.retained_program_reads.push_back({base+0x601,literal});
		if (!subc) c.retained_program_reads.push_back({base+0x601+unsigned(form>=3),extension});
		if (!c.retained_program_reads.empty()) {
			c.actions={{4,{}}};
			for (auto const &word : c.retained_program_reads) c.actions.back().program.push_back({word.address,0xdead});
		}
		if (!mapped) c.actions.push_back({read-1,{{r::STATE_AR0+reg,0x2222},{r::STATE_SP,0x2222}}});
		c.actions.push_back({read,{{r::STATE_A,a},{r::STATE_B,b}}});
		if (!mapped) c.actions.back().data={{address,0xdead}};
		else c.actions.back().values.push_back({endpoint==20 ? r::STATE_BK : r::STATE_AR0+address-0x10,0xdead});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
#endif
