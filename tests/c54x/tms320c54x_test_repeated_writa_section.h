// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_REPEATED_WRITA_SECTION_H
#define C54X_REPEATED_WRITA_SECTION_H
namespace c54x_test {
inline std::vector<clocked_register_case> repeated_writa_cases(bool standard_width)
{
	std::vector<clocked_register_case> cases;
	for (unsigned form=0;form<5;++form) for (unsigned setup=0;setup<5;++setup) for (unsigned reg=0;reg<8;++reg)
	for (unsigned count : {0,1,7,255}) for (unsigned target_kind=0;target_kind<3;++target_kind) for (unsigned overlay=0;overlay<2;++overlay) {
		u32 const page=!standard_width && ((form+setup+reg+count+target_kind+overlay)&1) ? 0x20000 : 0;
		u32 const target=standard_width ? std::array<u32,3>{0x3200,0x7ffe,0xfffe}[target_kind] : std::array<u32,3>{0x033200,0x037ffe,0x7ffffe}[target_kind];
		unsigned const words=setup==0 || setup==2 ? 1 : 2, read=5+words+(setup>=2 ? 2 : 0), write=read+5;
		u16 const source=form<2 ? 0x1201 : 0x3000, count_address=setup==2 ? 0x1202 : setup==3 ? 0x1400 : 0x10+(reg+1)%8;
		int const direction=form==3 ? -1 : form==4 ? 1 : 0;
		auto sample=[reg](unsigned n) { return u16(0x2468^(reg*0x1111)^(n*0x0101)); };
		auto destination=[=](unsigned n) {
			if (standard_width) return (target+n)&0xffff;
			u32 const logical=(target+n)&0x7fffff;
			return overlay && !(logical&0x8000) ? logical&0xffff : logical;
		};
		u16 const opcode=0x7f00|(form<2 ? 1 : (form==2 ? 0x80 : form==3 ? 0x88 : 0x90)|reg);
		auto initial=initial_case("repeated-WRITA-retains-logical-PAR-and-fresh-Read-stream",
			{u16(setup==0 ? 0xec00|count : setup==1 ? 0xf070 : setup==2 ? 0x4702 : 0x47f8)});
		if (words==2) initial.program[1].value=setup==1 ? count : count_address;
		initial.program[words].value=opcode; initial.program[words+1].value=0xf4e1; initial.opcode=opcode;
		for (auto &word : initial.program) word.address=page+word.address+0x8a00;
		u16 const st1=0x900|(form==1 ? 0x4000 : 0);
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x9000},{r::STATE_A,0xff00000000ULL|target},{r::STATE_B,0x81abcdef01ULL},
			{r::STATE_ST0,0xa824},{r::STATE_ST1,st1},{r::STATE_PMST,overlay ? 0x20U : 0U},{r::STATE_SP,0x1200},{r::STATE_T,0x5aa5}});
		auto final=observation("count-plus-one-program-writes-and-independent-PAR",{{r::STATE_A,0x56789a},{r::STATE_B,0x81abcdef01ULL},
			{r::STATE_ST0,0xa824},{r::STATE_ST1,st1},{r::STATE_PMST,overlay ? 0x20U : 0U},{r::STATE_SP,0x1200},{r::STATE_T,0x5aa5},{r::STATE_RC,0}});
		for (unsigned i=0;i<8;++i) {
			u16 const seed=setup==4 && i==(reg+1)%8 ? count : form>=2 && i==reg ? source : 0x4000+i;
			initial.initial.push_back({r::STATE_AR0+i,seed});
			final.expected.push_back({int(r::STATE_AR0+i),setup==4 && i==(reg+1)%8 ? u16(count^0xffff) :
				form>=2 && i==reg ? u16(source+direction*int(count+1)) : seed});
		}
		initial.data={{source,sample(0)}};
		if (setup==2 || setup==3) initial.data.push_back({count_address,u16(count)});
		unsigned const checkpoint=std::array<unsigned,4>{read,read+1,write,write+3}[cases.size()%4];
		clocked_register_case c{{{0,std::move(initial)},{1,observation("WRITA-repeat-running",{})},{write+count+24,std::move(final)}},checkpoint,write+count+24};
		c.extended_program=!standard_width; c.single_fetch=page+0x9000+words;
		c.data_events=std::vector<clocked_data_event>{};
		if (setup>=2) {
			unsigned const count_read=setup==2 ? 5 : 6;
			c.data_events->push_back({false,count_address,u16(count),count_read});
			c.actions.push_back({count_read,{}});
			if (setup==4) c.actions.back().values={{r::STATE_AR0+(reg+1)%8,u16(count^0xffff)}};
			else c.actions.back().data={{count_address,u16(count^0xffff)}};
		}
		c.actions.push_back({read,{}}); c.actions.back().data={{source,0xdead}};
		c.actions.push_back({read+1,{{r::STATE_A,0x56789a}}});
		for (unsigned n=0;n<=count;++n) {
			clocked_data_event operand{false,u16(source+direction*int(n)),sample(n),n ? write+n : read};
			operand.program_events_before=n;
			c.data_events->push_back(std::move(operand));
			c.program_events.push_back({true,destination(n),sample(n),write+n});
			c.states.front().observation.program.push_back({destination(n),u16(sample(n)^0xffff)});
			c.states.back().observation.expected_program.push_back({destination(n),sample(n)});
			if (n<count) {
				c.actions.push_back({write+n,{}});
				c.actions.back().data={{u16(source+direction*int(n+1)),sample(n+1)}};
			}
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
#endif
