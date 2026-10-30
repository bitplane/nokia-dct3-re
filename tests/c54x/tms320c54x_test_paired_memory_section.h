// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_PAIRED_MEMORY_SECTION_H
#define C54X_PAIRED_MEMORY_SECTION_H
namespace c54x_test {
inline bool paired_modulo_domain(u16 pointer, int step, u16 bk)
{
	unsigned mask=1; while (mask<bk) mask=(mask<<1)|1;
	return bk && (pointer&mask)<bk && step>=-int(bk) && step<=int(bk);
}
inline u16 paired_modulo(u16 pointer, int step, u16 bk)
{
	if (!paired_modulo_domain(pointer,step,bk)) throw emu_fatalerror("Paired-memory fixture outside documented circular domain");
	unsigned mask=1; while (mask<bk) mask=(mask<<1)|1;
	int const position=(int(pointer&mask)+step)%bk;
	return (pointer&~mask)|u16(position<0 ? position+bk : position);
}
inline std::vector<clocked_register_case> paired_memory_cases(bool load)
{
	// Guard the oracle itself: |step|<=BK and an initial index inside the block (SPRU131G 5.5.3.4).
	struct guard { u16 pointer,bk; int step; bool valid; u16 next; };
	for (auto const &g : std::initializer_list<guard>{{0x1007,16,-2923,false,0},{0x4007,8192,-2923,true,0x549c},
		{0x1007,16,4103,false,0},{0x1007,8192,4103,true,0xe},{0x1007,8192,-4103,true,0},
		{0x5007,16,32767,false,0},{0x5007,32768,32767,true,0x5006},{0x5007,32768,-32768,true,0x5007},
		{0x10,16,1,false,0},{0x1f,31,-1,false,0},{0xf,16,1,true,0},{0,16,-1,true,15},
		{0x1e,31,1,true,0},{0x1007,0,0,false,0}})
		if (paired_modulo_domain(g.pointer,g.step,g.bk)!=g.valid || (g.valid && paired_modulo(g.pointer,g.step,g.bk)!=g.next))
			throw emu_fatalerror("Paired-memory modulo oracle guard failed");
	std::vector<clocked_register_case> cases;
	for (unsigned form=0;form<16;++form) {
		if (load && form==3) continue; // Short preincrement is write-only, including paired accesses.
		for (unsigned reg=0;reg<8;++reg) for (unsigned accumulator=0;accumulator<2;++accumulator)
		for (unsigned pattern=0;pattern<4;++pattern) for (unsigned flags=0;flags<(load ? 8U : 4U);++flags) {
			bool const short_form=form<12, compatibility=load ? flags&2 : flags&1;
			unsigned const selected=compatibility && !reg ? 3 : reg;
			u16 pointer, next, address, literal=0, bk;
			if (short_form) {
				pointer=0x1007+(pattern&1); bk=!selected && (form==9 || form==11) ? 8192 : 16;
				int const index=selected ? 3 : pointer;
				int const delta=form==1 || form==8 ? -2 : form==2 || form==3 || form==10 ? 2 : form==5 || form==9 ? -index : index;
				auto reverse=[](u16 value) { u16 result=0; for (unsigned i=0;i<16;++i) result|=((value>>i)&1)<<(15-i); return result; };
				next=pointer;
				if (form==4 || form==7) next=reverse(u16(reverse(pointer)+(form==4 ? -int(reverse(index)) : int(reverse(index)))));
				else if (form>=8) next=paired_modulo(pointer,delta,bk);
				else if (form) next=pointer+delta;
				address=form==3 ? next : pointer;
			} else {
				int const offset=std::array<s16,4>{0,1,-1,s16(0xf495)}[pattern];
				pointer=form==14 ? 0x4007 : 0x1007; bk=form==14 ? 8192 : 16;
				address=form==14 ? paired_modulo(pointer,offset,bk) : u16(pointer+offset);
				literal=form==15 ? address : u16(offset); next=form==13 || form==14 ? address : pointer;
			}
			u64 value=load ? std::array<u32,4>{0x12345678,0x87654321,0x80000000,0xffff0001}[pattern] :
				std::array<u64,4>{0x0012345678ULL,0xff87654321ULL,0x017fffffffULL,0xfe80000000ULL}[pattern];
			u32 stored=u32(value);
			if (load) { if ((flags&1) && BIT(value,31)) value|=0xff00000000ULL; }
			else if (flags&2) {
				s64 const number=BIT(value,39) ? s64(value)-(s64(1)<<40) : s64(value);
				stored=u32(std::clamp(number,s64(-2147483648LL),s64(2147483647LL)));
			}
			u16 const mode=short_form ? 0x80+form*8+reg : form==15 ? 0xf8 : 0xe0+(form-12)*8+reg;
			u16 const st0=compatibility && reg && form!=15 ? 0x608|(reg<<13) : 0x6608;
			u16 const st1=load ? 0x800|((flags&1)<<8)|((flags&2)<<4)|((flags&4)<<5) : 0x4900|(compatibility ? 0x20 : 0);
			u32 const base=(cases.size()^form)&1 ? 0x20000 : 0;
			unsigned const read=short_form ? 5 : 6, access=read-1, write=read+1, finish=load ? 18 : 24;
			auto initial=initial_case(load ? "paired-indirect-DLD" : "paired-indirect-DST",{u16((load ? 0x5600 : 0x4e00)|(accumulator<<8)|mode)});
			if (!short_form) initial.program[1].value=literal;
			for (auto &word : initial.program) word.address+=base;
			u64 const a=load ? 0x1111222233ULL : accumulator ? 0xdead : value;
			u64 const b=load ? 0x4444555566ULL : accumulator ? value : 0xdead;
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x6608},{r::STATE_ST1,st1},
				{r::STATE_PMST,load ? 0U : flags>>1},{r::STATE_BK,bk},{r::STATE_A,a},{r::STATE_B,b}});
			auto captured=observation("paired-Access-updates-pointer-once",{{int(r::STATE_AR0+selected),next},{r::STATE_ST0,st0},
				{r::STATE_A,a},{r::STATE_B,b}});
			auto final=observation("retained-pair-after-source-and-pointer-poison",{{r::STATE_ST0,st0},{r::STATE_ST1,st1},
				{r::STATE_PMST,load ? 0U : flags>>1},{r::STATE_BK,32},
				{r::STATE_A,load ? (accumulator ? a : value) : (accumulator ? a : 0)},
				{r::STATE_B,load ? (accumulator ? value : b) : (accumulator ? 0 : b)}});
			for (unsigned i=0;i<8;++i) {
				u16 const seed=i==selected ? pointer : !i ? 3 : 0x3000+i;
				initial.initial.push_back({r::STATE_AR0+i,seed});
				final.expected.push_back({int(r::STATE_AR0+i),i==selected ? 0x2222U : seed});
			}
			initial.data={{address,load ? u16(stored>>16) : u16(0xaaaa)},{u32(address^1),load ? u16(stored) : u16(0xaaaa)}};
			if (!load) final.expected_data={{address,u16(stored>>16)},{u32(address^1),u16(stored)}};
			clocked_register_case c{{{0,std::move(initial)},{1,observation("paired-running",{})},{access,std::move(captured)},
				{read,observation("pair-sampled-before-Execute",{})},{finish,std::move(final)}},
				std::array<unsigned,3>{short_form ? 3U : 4U,load ? access : read,load ? read : write}[cases.size()%3],finish};
			if (load) {
				c.states[3].observation=observation("paired-Read-preserves-accumulators",{{r::STATE_A,a},{r::STATE_B,b}});
				c.states.insert(c.states.end()-1,{read+1,observation("paired-Execute-publishes-retained-words",
					{{r::STATE_A,accumulator ? a : value},{r::STATE_B,accumulator ? value : b}})});
			}
			c.require_idle=false; c.single_fetch=base+0x600;
			if (!short_form) c.retained_program_reads={{base+0x601,literal}};
			c.data_events=std::vector<clocked_data_event>{{!load,address,u16(stored>>16),load ? read : write},
				{!load,u32(address^1),u16(stored),load ? read : write+1}};
			if (!short_form) { c.actions={{4,{}}}; c.actions.back().program={{base+0x601,0xdead}}; }
			c.actions.push_back({access,{{r::STATE_AR0+selected,0x2222},{r::STATE_BK,32}}});
			if (load) {
				c.actions.push_back({read,{}}); c.actions.back().data={{address,0xdead},{u32(address^1),0xbeef}};
			} else c.actions.push_back({read,{{accumulator ? r::STATE_B : r::STATE_A,0}}});
			cases.push_back(std::move(c));
		}
	}
	return cases;
}
}
#endif
