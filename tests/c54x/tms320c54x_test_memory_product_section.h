// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_MEMORY_PRODUCT_SECTION_H
#define C54X_MEMORY_PRODUCT_SECTION_H
namespace c54x_test {
enum class memory_product { accumulator, unsigned_t, signed_t, square, load_t };
inline std::vector<clocked_register_case> memory_product_cases(memory_product family)
{
	std::vector<clocked_register_case> cases;
	bool const load_t=family==memory_product::load_t, square=family==memory_product::square;
	bool const keeps_t=family==memory_product::unsigned_t || family==memory_product::signed_t;
	// RAM: DP, SP, stationary, decrement, increment, offset, preincrement, circular, absolute.
	// Mapped: AL/AH/AG, BL/BH/BG, T/TRN and AR0..7, sampled before T publication.
	for (unsigned endpoint=0;endpoint<34;++endpoint) for (unsigned operation=0;operation<(load_t ? 1U : 4U);++operation)
	for (unsigned pattern=0;pattern<8;++pattern) for (unsigned flags=0;flags<8;++flags) {
		bool const mapped=endpoint>=18;
		unsigned const form=mapped ? 8 : endpoint/2, reg=mapped || !(endpoint&1) ? 1 : 6;
		int const offset=std::array<s16,8>{0,1,-1,17,s16(0xf495),0x7fff,s16(0x8000),2}[pattern];
		u16 const bk=form==7 ? 0x8000 : 16;
		u16 const mode=form<2 ? 0xb+unsigned(reg==6) : form==2 ? 0x80|reg : form==3 ? 0x88|reg :
			form==4 ? 0x90|reg : form==5 ? 0xe0|reg : form==6 ? 0xe8|reg : form==7 ? 0xf0|reg : 0xf8;
		u16 address=mapped ? 8+endpoint-18 : form<2 ? (form ? 0x1000 : 0x200)+mode :
			form==8 ? u16(0x1500+offset) : form>=5 ? u16(0x5007+offset) : 0x5007;
		if (!mapped && form==7) {
			int const index=(0x5007+offset)%0x8000;
			address=index<0 ? index+0x8000 : index;
		}
		u16 const next=form==3 ? 0x5006 : form==4 ? 0x5008 : form==6 || form==7 ? address : 0x5007;
		u16 const literal=form==8 ? address : u16(offset);
		int const high=std::array<int,8>{0,1,-1,32767,-32768,65535,-65536,0x1234}[pattern];
		u64 const a=(pattern&1 ? 0xfe00000000ULL : 0)|((u64(high)&0x1ffff)<<16)|(0x1234+pattern);
		u64 const b=std::array<u64,8>{0,1,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x1234567833ULL,0x80aa001234ULL}[pattern];
		u16 const t=keeps_t || square ? (operation&2 ? 0xffff : 0x8000) : 0x9abc;
		u16 sample=std::array<u16,8>{0,1,0xffff,0x7fff,0x8000,0x8000,0xffff,0x5678}[pattern];
		if (mapped) sample=address<11 ? u16(a>>((address-8)*16)) : address<14 ? u16(b>>((address-11)*16)) :
			address==14 ? t : address==15 ? 0x1357 : 0x5007;
		bool const destination=family==memory_product::accumulator || (operation&1);
		s64 product;
		if (square) product=s64(s16(sample))*s16(sample);
		else if (family==memory_product::unsigned_t) product=s64(t)*sample;
		else if (family==memory_product::signed_t) product=s64(s16(t))*s16(sample);
		else product=s64(high)*s16(sample);
		if (flags&1) product*=2;
		if (family==memory_product::accumulator && operation && flags==7)
			product=std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
		s64 const addend=BIT(b,39) ? s64(b)-(s64(1)<<40) : s64(b);
		s64 result=family==memory_product::accumulator && operation ? operation==1 ? addend-product : addend+product : product;
		if ((family==memory_product::accumulator && operation==3) || (family==memory_product::signed_t && (operation&2)))
			result=(result+32768)&~s64(65535);
		bool const overflow=result<-2147483648LL || result>2147483647LL;
		u64 const stored=u64(overflow && (flags&2) ? std::clamp(result,s64(-2147483648LL),s64(2147483647LL)) : result)&0xffffffffffULL;
		u16 const st0=0x1004|(pattern&1 ? 0x600 : 0)|(pattern&2 ? 0x800 : 0);
		u16 const st1=0x800|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0)|((endpoint+operation+pattern+flags)&1 ? 0x100 : 0)|(!mapped && form==1 ? 0x4000 : 0);
		u16 const opcode=(load_t ? 0x3000 : square ? 0x2600|((operation&1)<<8) : family==memory_product::unsigned_t ? 0x2400|((operation&1)<<8) :
			family==memory_product::signed_t ? (operation&2 ? 0x2200 : 0x2000)|((operation&1)<<8) : 0x3100+operation*0x200)|mode;
		u32 const base=(endpoint+operation+pattern+flags)&1 ? 0x20000 : 0;
		unsigned const read=form>=5 ? 6 : 5;
		auto initial=initial_case("memory-product-or-T-load-with-retained-Read",{opcode});
		if (form>=5) initial.program[1].value=literal;
		for (auto &entry : initial.program) entry.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,t},{r::STATE_TRN,0x1357},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,flags&4 ? 2U : 0U},{r::STATE_SP,0x1000},{r::STATE_BK,bk}});
		for (unsigned i=0;i<8;++i) initial.initial.push_back({r::STATE_AR0+i,0x5007});
		if (!mapped) initial.data={{address,sample}};
		auto before=observation("memory-product-before-Read",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,t},{r::STATE_ST0,st0}});
		before.expected.push_back({int(r::STATE_AR0+reg),next});
		auto sampled=observation("T-publication-is-Read-owned",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,keeps_t ? t : sample},{r::STATE_ST0,st0}});
		auto final=observation("Execute-preserves-newer-T",{{r::STATE_A,!load_t && !destination ? stored : a},
			{r::STATE_B,!load_t && destination ? stored : b},{r::STATE_T,0xbeef},{r::STATE_TRN,0x1357},
			{r::STATE_ST0,u16(st0|(!load_t && overflow ? destination ? 0x200 : 0x400 : 0))},
			{r::STATE_ST1,st1},{r::STATE_PMST,flags&4 ? 2U : 0U},{r::STATE_SP,mapped ? 0x1000U : 0x2222U},{r::STATE_BK,bk}});
		for (unsigned i=0;i<8;++i) final.expected.push_back({int(r::STATE_AR0+i),!mapped && i==reg ? 0x2222U : 0x5007U});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("product-running",{})},{read-1,std::move(before)},
			{read,std::move(sampled)},{read+1,final},{18,std::move(final)}},std::array<unsigned,3>{3,read-1,read}[cases.size()%3],18};
		c.require_idle=false; c.single_fetch=base+0x600;
		c.data_events=std::vector<clocked_data_event>{{false,address,sample,read}};
		if (form>=5) { c.retained_program_reads={{base+0x601,literal}}; c.actions={{4,{}}}; c.actions.back().program={{base+0x601,0xdead}}; }
		if (!mapped) c.actions.push_back({read-1,{{r::STATE_AR0+reg,0x2222},{r::STATE_SP,0x2222}}});
		c.actions.push_back({read,{{r::STATE_T,0xbeef}}});
		if (!mapped) c.actions.back().data={{address,0xdead}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
#endif
