// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_STAGED_IDLE
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned mode=0;mode<3;++mode) for (unsigned kind=0;kind<6;++kind) {
		if (mode && (kind==3 || kind==4)) continue;
		for (unsigned page : {0U,2U}) {
			bool const blocked=kind==1 || kind==4, timer=kind==3 || kind==4;
			unsigned const source=timer ? 3 : 0;
			u32 const base=page<<16;
			u16 const vector=kind==5 ? 4 : 0x40+source*4;
			u16 const idle=std::array<u16,3>{0xf4e1,0xf6e1,0xf5e1}[mode];
			auto initial=initial_case("staged-IDLE-discarded-younger-word-and-real-return",{idle,0xe811});
			initial.program.clear();
			for (unsigned at=0x600;at<0x700;++at) initial.program.push_back({base+at,at==0x600 ? idle : at==0x601 ? u16(0xe811) : u16(0xf495)});
			initial.program.insert(initial.program.end(),{{base+vector,0x4a1e},{base+vector+1,0xe955},{base+vector+2,0xf4e5}});
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x608},
				{r::STATE_ST1,blocked || kind==5 ? 0x800U : 0U},{r::STATE_IMR,kind==5 ? 0U : 1U<<source},
				{r::STATE_PRD,0xffff},{r::STATE_TCR,0x20},{r::STATE_TIM,timer ? 31U : 0x7fffU}});
			initial.data={{0xffe,0x5a5a},{0xfff,0xa5a5}};
			auto asleep=observation("younger-words-cannot-retire-while-asleep",{{r::STATE_IDLE,1},
				{r::STATE_A,0},{r::STATE_B,0},{r::STATE_SP,0x1000}});
			auto final=observation("fresh-continuation-after-foreground-or-ISR-wake",{{r::STATE_IDLE,0},
				{r::STATE_A,0x44},{r::STATE_B,blocked ? 0U : 0x55U},{r::STATE_SP,0x1000},
				{r::STATE_XPC,page},{r::STATE_ST1,blocked ? 0x800U : 0U},
				{r::STATE_IFR,kind==2 ? 2U : blocked ? 1U<<source : 0U}});
			unsigned const checkpoint=kind==2 ? 18 : std::array<unsigned,4>{4,10,18,80}[(cases.size()+mode+kind)%4];
			clocked_register_case c{{{0,std::move(initial)},{6,std::move(asleep)},
				{20,observation("wake-window",{})},{100,std::move(final)}},checkpoint,100};
			c.require_idle=false;
			for (auto &state : c.states) state.observation.expected.insert(state.observation.expected.end(),{{r::STATE_ST0,0,0xe000},{r::STATE_ST1,0,0x20}});
			c.initial_input_lines={{0,CLEAR_LINE},{1,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
			if (kind==2) c.actions.push_back({12,{},false,{},{{1,ASSERT_LINE},{1,CLEAR_LINE}}});
			c.actions.push_back({16,{},false,{{base+0x601,0xe844}}});
			if (!timer) c.actions.push_back({20,{},false,{},{{kind==5 ? INPUT_LINE_NMI : 0,ASSERT_LINE},{kind==5 ? INPUT_LINE_NMI : 0,CLEAR_LINE}}});
			c.program_events={{false,base+0x601,0xe811,0,false},{false,base+0x601,0xe844,0,false}};
			c.data_events=std::vector<clocked_data_event>{};
			if (!blocked) {
				*c.data_events={{true,0xfff,0x601,0,false},{false,0x1e,u16(page),0,false},{true,0xffe,u16(page),0,false},
					{false,0xffe,u16(page),0,false},{false,0xfff,0x601,0,false}};
				c.irqs={{kind==5 ? INPUT_LINE_NMI : int(source),0,~0U,0,0x601}};
			}
			cases.push_back(std::move(c));
		}
	}
	return cases;
}
}
