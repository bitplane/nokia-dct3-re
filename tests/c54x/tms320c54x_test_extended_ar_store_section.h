// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_EXTENDED_AR_STORE_SECTION_H
#define C54X_EXTENDED_AR_STORE_SECTION_H
namespace c54x_test {
inline std::vector<clocked_register_case> extended_ar_store_cases(bool consumer, bool shifted)
{
	std::vector<clocked_register_case> cases;
	for (int shift : {-16,-1,0,1,15}) {
		if (!shifted && shift) continue;
		for (unsigned form=0;form<(consumer ? 1U : 4U);++form) for (unsigned reg=0;reg<8;++reg)
		for (unsigned kind=0;kind<4;++kind) for (u64 seed : {0x123456789aULL,0x0180001200ULL,0xff7fff1200ULL,0xfffffff495ULL})
		for (unsigned flags=0;flags<4;++flags) for (unsigned placement=0;placement<2;++placement) {
			if (!consumer && !form && placement) continue; // Absolute addressing has no pointer alias choice.
			u64 const value=shifted && consumer && !BIT(seed,39) ? seed|0x7f00000000ULL : seed;
			// Bit-position oracle for the 40-bit shift, followed by signed/unsigned 32-bit saturation.
			u64 bits=0;
			for (int bit=0;bit<40;++bit) {
				int const from=bit-shift;
				if ((from>=0 && from<40 && BIT(value,from)) || (shift<0 && from>=40 && (flags&2) && BIT(value,39))) bits|=u64(1)<<bit;
			}
			u32 stored=u32(bits);
			if (flags&1) {
				if (flags&2) {
					s64 const number=BIT(bits,39) ? s64(bits)-(s64(1)<<40) : s64(bits);
					if (number < -0x80000000LL) stored=0x80000000;
					else if (number > 0x7fffffffLL) stored=0x7fffffff;
				} else if (bits>0xffffffffULL) stored=0xffffffff;
			}
			u16 const result=kind&2 ? stored>>16 : stored;
			unsigned const selected=!consumer && placement ? reg : (reg+3)%8, gap=2+placement;
			u16 const target=0x10+reg, literal=!form ? target : form==1 ? u16(target-0x40) : 1;
			u16 const pointer=!form ? 0x4000+selected : form==1 ? 0x40 : form==2 ? target-1 : 0x10+(reg+7)%8;
			u16 const mode=!form ? 0xf8 : (form==1 ? 0xe0 : form==2 ? 0xe8 : 0xf0)|selected;
			u32 const base=cases.size()&1 ? 0x20000 : 0;
			unsigned const execute=7+unsigned(shifted), read=execute+gap, access=execute-1;
			auto initial=initial_case("extended-accumulator-store-to-AR",{u16(shifted ? 0x6f00|mode : 0x8000|(kind<<8)|mode),literal});
			if (shifted) initial.program[2].value=(kind&2 ? 0x0c60 : 0x0c80)|((kind&1)<<8)|(shift&31);
			if (consumer) initial.program[2+unsigned(shifted)+gap].value=0x1280|(((kind&1)^1)<<8)|reg;
			for (auto &word : initial.program) word.address+=base;
			u16 const st1=0x4800|(flags&2 ? 0x100 : 0);
			int const source=kind&1 ? r::STATE_B : r::STATE_A, other=kind&1 ? r::STATE_A : r::STATE_B;
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_PMST,flags&1},{r::STATE_ST0,0xa608},
				{r::STATE_ST1,st1},{r::STATE_SP,0x6000},{r::STATE_BK,8},{source,consumer ? value : 0x11112222ULL},{other,0xdead}});
			auto captured=observation("retained-address-before-explicit-publication",{});
			auto final=observation("only-destination-AR-changes",{{source,value},{other,consumer ? (result ? 0xbbbbU : 0U) : 0xdeadU},
				{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_PMST,flags&1},{r::STATE_SP,0x6000},{r::STATE_BK,8}});
			for (unsigned ar=0;ar<8;++ar) {
				u16 const before=consumer && ar==reg ? 0x1000 : form && ar==selected ? pointer : 0x4000+ar;
				initial.initial.push_back({r::STATE_AR0+ar,before});
				captured.expected.push_back({int(r::STATE_AR0+ar),form && ar==selected ? (form<2 ? pointer : target) : before});
				final.expected.push_back({int(r::STATE_AR0+ar),ar==reg ? result : (consumer ? 0x4000U : 0x5000U)+ar});
			}
			if (consumer && result>=0x60) initial.data={{result,0xbbbb}};
			clocked_register_case c{{{0,std::move(initial)},{1,observation("store-running",{})}},
				std::array<unsigned,3>{4,consumer ? execute : access,consumer ? read : execute}[cases.size()%3],24};
			if (!consumer) {
				c.states.push_back({access,std::move(captured)});
				c.states.push_back({execute,observation("execute-live-source",{})});
			}
			c.states.push_back({24,std::move(final)});
			c.require_idle=false;
			c.retained_program_reads={{base+0x601,literal}};
			if (shifted) c.retained_program_reads.push_back({base+0x602,c.states.front().observation.program[2].value});
			c.data_events=std::vector<clocked_data_event>{{true,target,result,execute}};
			if (consumer) c.data_events->push_back({false,result,u16(result ? 0xbbbb : 0),read});
			c.actions={{4,{}}}; c.actions.back().program={{base+0x601,0xdead}};
			if (shifted) { c.actions.push_back({5,{}}); c.actions.back().program={{base+0x602,0xdead}}; }
			if (!consumer) {
				c.actions.push_back({access,{{source,value}}});
				for (unsigned ar=0;ar<8;++ar) c.actions.back().values.push_back({r::STATE_AR0+ar,0x5000+ar});
			} else if (result>=0x60) { c.actions.push_back({read,{}}); c.actions.back().data={{result,0xcccc}}; }
			cases.push_back(std::move(c));
		}
	}
	return cases;
}
}
#endif
