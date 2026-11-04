// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_BSP_INTERRUPT
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index=0;index<9;++index) {
		unsigned const source=index<4 ? 4 : 5;
		bool const both=index==8, enabled=both || (index&1), serviced=enabled && (both || !(index&2));
		u16 const pending=both ? 0x30 : 1U<<source;
		u16 const imr=enabled ? pending : 0, st1=!both && (index&2) ? 0x800 : 0;
		auto initial=initial_case("BSP-completed-frame-interrupt-routing",{0xf4e1,0x76f8,0x501,1,0xe800,0x8800,0xf4e1});
		for (auto &word : initial.program) word.address+=0x10400;
		initial.program.insert(initial.program.end(),{{0x10050,0xe800},{0x10051,0x8800},{0x10052,0xf4e1},
			{0x10054,0xe800},{0x10055,0x8800},{0x10056,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x10a00},{r::STATE_IMR,imr},{r::STATE_ST1,st1}});
		initial.data={{0x501,0},{0xfff,0x5a5a}};
		auto asleep=observation("await-frame-before-execution-hold",{{r::STATE_IDLE,1},{STATE_GENPC,0x10a01}},{{0x501,0}});
		auto held=observation("completed-frame-pending-before-dispatch",{{STATE_GENPC,0x10a01},
			{r::STATE_IFR,pending},{r::STATE_IMR,imr},{r::STATE_SP,0x1000},{r::STATE_ST1,st1}},{{0xfff,0x5a5a},{0x501,0}});
		auto final=observation("receive-priority-mask-and-INTM-control-dispatch",{{r::STATE_IDLE,1},
			{STATE_GENPC,serviced ? both || source==4 ? 0x10053U : 0x10057U : enabled ? 0x10a07U : 0x10a01U},
			{r::STATE_IFR,both ? 0x20U : serviced ? 0U : pending},{r::STATE_IMR,0},
			{r::STATE_SP,serviced ? 0xfffU : 0x1000U},{r::STATE_ST1,serviced ? 0x800U : st1}},
			{{0x501,u16(enabled && !serviced)},{0xfff,u16(serviced ? 0xa01 : 0x5a5a)}});
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(asleep)},
			{17,observation("frame-edge-delivered",{})},{20,std::move(held)},
			{25,observation("execution-hold-released",{})},{80,std::move(final)}},20,80};
		c.pipeline_frontend = true;
		c.initial_input_lines={{4,CLEAR_LINE},{5,CLEAR_LINE},{INPUT_LINE_HALT,CLEAR_LINE}};
		c.actions={{16,{},false,{},{{INPUT_LINE_HALT,ASSERT_LINE}}},{24,{},false,{},{{INPUT_LINE_HALT,CLEAR_LINE}}}};
		c.data_events=std::vector<clocked_data_event>{};
		if (serviced) {
			c.data_events->push_back({true,0xfff,0xa01,0,false});
			c.irqs={{int(both ? 4 : source),0,~0U,0,0xa01}};
		} else if (enabled) c.data_events->push_back({true,0x501,1,0,false});
		if (enabled) {
			c.data_events->front().registers={{r::STATE_IMR,imr}};
			c.data_events->push_back({true,0,0,0,false});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
