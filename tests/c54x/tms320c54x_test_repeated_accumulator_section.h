// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_REPEATED_ACCUMULATOR_SECTION_H
#define C54X_REPEATED_ACCUMULATOR_SECTION_H
namespace c54x_test {
enum class repeated_accumulator { load_numeric, load_counts, arithmetic, block };
inline std::vector<clocked_register_case> repeated_accumulator_cases(repeated_accumulator family)
{
	std::vector<clocked_register_case> cases;
	bool const arithmetic=family==repeated_accumulator::arithmetic, block=family==repeated_accumulator::block;
	auto append=[&](unsigned selector,int shift,unsigned flags,unsigned pattern,unsigned count,unsigned setup,unsigned interrupt,bool asm_form,unsigned outer=0) {
		u32 const base=cases.size()&1 ? 0x20000 : 0;
		bool const extended=setup==1 || setup>=4;
		u32 const body=base+(block ? 0x603 : extended ? 0x602 : 0x601);
		unsigned const first=block ? 11 : setup==0 ? 7 : setup==1 ? 8 : setup<=3 ? 9 : 10;
		unsigned const total=(count+1)*(outer+1), last=first+count, finish=block ? total+12*(outer+1)+80 : last+64;
		u16 const word=arithmetic ? 0xf460|(shift&31)|((selector&3)<<8) :
			(asm_form ? 0xf482 : 0xf440|(shift&31))|((selector&2)<<8)|((selector&1)<<8);
		u16 st0=0xb808|(pattern&1 ? 0x600 : 0);
		if (arithmetic && !(pattern&2)) st0&=~0x800;
		u16 const st1=(setup==3 ? 0x4000 : 0)|(interrupt ? 0 : 0x800)|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0)|(asm_form ? shift&31 : 11);
		u16 const address=setup==3 ? 0x3000 : setup==5 ? 0x15 : 0x400;
		u64 const source=std::array<u64,8>{0,1,0x007fffffffULL,0x0080000000ULL,0xff80000000ULL,0xffffffffffULL,0x7fffffffffULL,0x8000000000ULL}[pattern];
		std::vector<std::array<u64,3>> expected{{selector&2 ? 0x123456789aULL : source,selector&2 ? source : 0xfedcba9876ULL,st0}};
		for (unsigned n=0;n<total;++n) {
			auto state=expected.back(); u64 const operand=state[(selector>>1)&1]; u64 result=0;
			// Bit-position shift oracle; no processor arithmetic helpers or host signed shifts.
			for (int bit=0;bit<40;++bit) {
				int const from=bit-shift;
				if ((from>=0 && from<40 && BIT(operand,from)) || (from>=40 && (flags&1) && BIT(operand,39))) result|=u64(1)<<bit;
			}
			if (arithmetic && shift) state[2]=(state[2]&~0x800ULL)|
				(BIT(operand,shift<0 ? -shift-1 : 39-shift) ? 0x800 : 0);
			if (shift>=0 && (!arithmetic || shift)) {
				s64 const number=BIT(operand,39) ? s64(operand)-(s64(1)<<40) : s64(operand);
				s64 const full=number*(s64(1)<<shift);
				if (full<-2147483648LL || full>2147483647LL) {
					state[2]|=selector&1 ? 0x200 : 0x400;
					if (flags&2) result=full<0 ? 0xff80000000ULL : 0x007fffffffULL;
				}
			}
			state[selector&1]=result; expected.push_back(state);
		}
		auto initial=initial_case(arithmetic ? "repeated-SFTA-live-feedback" : "repeated-accumulator-load-live-feedback",
			{u16(block ? 0xf072 : setup==0 ? 0xec00|count : setup==1 ? 0xf070 : setup<=3 ? 0x4700 : 0x47f8)});
		if (block) { initial.program[1].value=0x607; initial.program[2].value=0xec00|count; }
		else if (extended) initial.program[1].value=setup==1 ? count : address;
		initial.program[body-base-0x600].value=block ? 0xe8ff : word;
		initial.program[body-base-0x600+1].value=0x6d94;
		initial.program[block ? 8 : 15].value=0xf4e1; initial.opcode=word;
		for (auto &entry : initial.program) entry.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,expected.front()[0]},{r::STATE_B,expected.front()[1]},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_SP,0x3000},{r::STATE_RC,0},{r::STATE_BRC,block ? outer : 13U},
			{r::STATE_T,0x5aa5},{r::STATE_BK,17},{r::STATE_IMR,interrupt==1 ? 4U : 0U}});
		auto final=observation("one-fresh-continuation-per-outer-iteration",{{STATE_GENPC,base+(block ? 0x609 : 0x610)},
			{r::STATE_A,expected.back()[0]},{r::STATE_B,expected.back()[1]},{r::STATE_ST0,expected.back()[2]},
			{r::STATE_ST1,st1},{r::STATE_SP,0x3000},{r::STATE_BRC,block ? 0U : 13U},{r::STATE_RC,0},{r::STATE_T,0x5aa5},{r::STATE_BK,17}});
		for (unsigned i=0;i<8;++i) {
			initial.initial.push_back({r::STATE_AR0+i,i==4 ? 0x6000U : i==5 && setup==5 ? count : 0x7000+i});
			final.expected.push_back({int(r::STATE_AR0+i),i==4 ? 0x6000+outer+1 : i==5 && setup==5 ? 0xdeadU : 0x7000+i});
		}
		if (setup>=2 && setup!=5) initial.data={{address,u16(count)}};
		int const line=interrupt==2 ? INPUT_LINE_NMI : 2;
		if (interrupt) {
			u32 const vector=base+(interrupt==2 ? 4 : 0x48);
			initial.program.insert(initial.program.end(),{{vector,0xf495},{vector+1,0xf4eb}});
		}
		unsigned const checkpoint=block ? (cases.size()&1 ? first+count/2 : 3) : std::array<unsigned,3>{3,first+count/2,last}[cases.size()%3];
		clocked_register_case c{{{0,std::move(initial)},{1,observation("accumulator-repeat-running",{})},{finish,std::move(final)}},checkpoint,finish};
		c.retired_instruction=retired_instruction_check{body,word,total,block ? 0U : first}; c.retired_accumulator_states=std::move(expected);
		c.initial_input_lines={{2,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
		if (interrupt) c.actions={{2,{},false,{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}}};
		c.actions.push_back({block ? 5U : extended ? 4U : 3U,{}}); c.actions.back().program={{body,block ? word : u16(0xe8ff)}};
		c.data_events=std::vector<clocked_data_event>{};
		if (setup>=2) {
			unsigned const read=setup>=4 ? 6 : 5;
			c.data_events->push_back({false,address,u16(count),read});
			c.actions.push_back({read,{}}); c.actions.back().data={{address,0xdead}};
		}
		if (interrupt) {
			c.irqs={{line,0,~0U,unsigned(setup>=2)}};
			for (bool write : {true,false}) {
				clocked_data_event frame{write,0x2fff,0,0,false};
				frame.value_range=std::array<u16,2>{u16(body+1),0x60f}; frame.value_register=r::STATE_RTN;
				c.data_events->push_back(frame);
			}
		}
		if (block) {
			c.program_events={{false,body,0xe8ff,5}};
			for (unsigned i=0;i<=outer;++i) c.program_events.push_back({false,body,word,0,false});
		} else c.single_fetch=body;
		cases.push_back(std::move(c));
	};
	if (family==repeated_accumulator::load_counts) {
		for (unsigned setup=0;setup<6;++setup) for (unsigned count : {0,1,7,255}) for (unsigned selector=0;selector<4;++selector)
		for (unsigned flags=0;flags<4;++flags) for (unsigned pattern : {2,4}) for (unsigned interrupt=0;interrupt<3;++interrupt)
		for (unsigned asm_form=0;asm_form<2;++asm_form)
			append(selector,std::array<int,3>{-16,-1,15}[(setup+count+selector+flags+pattern+interrupt+asm_form)%3],flags,pattern,count,setup,interrupt,asm_form);
	} else if (block) {
		for (unsigned count : {0,1,7,255}) for (unsigned outer : {0,2}) for (unsigned selector=0;selector<4;++selector)
		for (int shift : {-1,15}) for (unsigned flags=0;flags<4;++flags) for (unsigned pattern : {2,4})
			append(selector,shift,flags,pattern,count,0,0,false,outer);
	} else {
		for (unsigned selector=0;selector<4;++selector) for (int shift=-16;shift<16;++shift) for (unsigned flags=0;flags<4;++flags)
		for (unsigned pattern=0;pattern<8;++pattern) for (unsigned asm_form=0;asm_form<(arithmetic ? 1U : 2U);++asm_form)
			append(selector,shift,flags,pattern,std::array<unsigned,4>{0,1,7,255}[(selector+unsigned(shift+16)+flags+pattern+asm_form)%4],0,0,asm_form);
	}
	return cases;
}
}
#endif
