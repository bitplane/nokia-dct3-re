// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned kind=0;kind<3;++kind) for (unsigned bitfield=0;bitfield<2;++bitfield)
	for (unsigned form=0;form<3;++form) for (unsigned pair=0;pair<16;++pair)
	for (unsigned tc=0;tc<2;++tc) for (unsigned prefix : {0U,2U}) for (unsigned irq=0;irq<3;++irq) {
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		u16 const memory=std::array<u16,4>{0,1,0x8000,0xffff}[pair%4];
		u16 const literal=std::array<u16,4>{0,1,0x8000,0xf495}[pair/4];
		bool const branch_tc=prefix ? bool(memory&1) : bool(tc), taken=!kind || (kind==1 ? branch_tc : !branch_tc);
		bool const final_tc=bitfield ? bool(memory&literal) : memory==literal;
		unsigned const read=(kind ? 9 : 8)+prefix, literal_clock=(kind ? 6 : 5)+prefix;
		auto initial=initial_case("delayed-compare-retains-branch-decision",{0xf495});
		initial.program.clear();
		for (unsigned i=0;i<64;++i) for (u32 region : {0U,0x600U,0x700U}) initial.program.push_back({base+region+i,0xf495});
		if (prefix) initial.program.insert(initial.program.end(),{{base+0x600,0x6102},{base+0x601,1}});
		initial.program.insert(initial.program.end(),{{base+0x600+prefix,u16(kind==0 ? 0xf273 : kind==1 ? 0xfa30 : 0xfa20)},
			{base+0x601+prefix,0x700},{base+0x602+prefix,u16((bitfield ? 0x6100 : 0x6000)|(form==2 ? 0x92 : 2))},
			{base+0x603+prefix,literal},{base+0x604+prefix,0xe922},{base+0x605+prefix,0xf4e1},
			{base+0x700,0xe911},{base+0x701,0xf4e1},{base+4,0xf4eb},{base+0x40,0xf4eb}});
		initial.data={{0x1002,memory}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xae20|(tc ? 0x1000U : 0)},
			{r::STATE_ST1,0x100|(form==1 ? 0x4000U : 0)},{r::STATE_IMR,1},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0x55},{r::STATE_T,0x5aa5},{r::STATE_AR2,0x1002}});
		auto final=observation("delay-TC-does-not-reselect-branch-or-repeat-pointer",{{r::STATE_B,taken ? 0x11U : 0x22U},
			{STATE_GENPC,base+(taken ? 0x700 : 0x600),~u64(0x3f)},
			{r::STATE_ST0,0xae20|(final_tc ? 0x1000U : 0)},{r::STATE_ST1,0x100|(form==1 ? 0x4000U : 0)},
			{r::STATE_A,0x123456789aULL},{r::STATE_T,0x5aa5},{r::STATE_SP,0x1000},{r::STATE_AR2,form==2 ? 0x2200U : 0x1002U}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("delay-compare-running",{})},{64,std::move(final)}},
			std::array<unsigned,4>{read-1,read,read+1,read+3}[cases.size()%4],64};
		c.require_idle=false; // A pending source may wake a prefetched IDLE.
		if (form==2) {
			c.states.insert(c.states.end()-1,{read-1,observation("Access-increments-before-pointer-poison",{{r::STATE_AR2,0x1003}})});
			c.states.insert(c.states.end()-1,{read,observation("captured-pointer-no-longer-live",{})});
		}
		c.data_ranges={{0x1002,0x1002}};
		c.data_events=std::vector<clocked_data_event>{};
		if (prefix) c.data_events->push_back({false,0x1002,memory,6});
		c.data_events->push_back({false,0x1002,memory,read});
		if (prefix) c.program_events.push_back({false,base+0x601,1,0,false});
		c.program_events.push_back({false,base+0x601+prefix,0x700,0,false});
		c.program_events.push_back({false,base+0x603+prefix,literal,literal_clock});
		for (unsigned tick=1;tick<=read;++tick) {
			clocked_register_action action{tick};
			if (irq && tick==5+prefix) action.input_lines.push_back({unsigned(irq==2 ? INPUT_LINE_NMI : 0),ASSERT_LINE});
			if (tick==literal_clock) action.program={{base+0x603+prefix,0xdead},{base+0x601+prefix,0xbeef}};
			if (form==2 && tick==read-1) action.values={{r::STATE_AR2,0x2200}};
			if (tick==read) action.data={{0x1002,0xdead}};
			if (!action.values.empty() || !action.program.empty() || !action.input_lines.empty() || !action.data.empty()) c.actions.push_back(std::move(action));
		}
		if (irq) c.irqs.push_back({irq==2 ? INPUT_LINE_NMI : 0,read+1,63,unsigned(prefix ? 2 : 1),{},
			{{r::STATE_ST0,final_tc ? 0x1000U : 0U,0x1000}}});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
