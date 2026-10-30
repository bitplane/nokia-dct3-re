// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	{
		auto initial = initial_case("READA-WRITA-ignore-executing-XPC", {0xf495});
		initial.program = {{0x105ff,0xf495},{0x10600,0x7ff8},{0x10601,0x500},{0x10602,0x7ef8},
			{0x10603,0x501},{0x10604,0xf4e1},{0x20900,0},{0x10900,0x1111},{0x900,0x2222}};
		initial.data = {{0x500,0xbeef},{0x501,0}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x105ff},{r::STATE_A,0x20900}});
		auto final = observation("extended-A-address-used-for-both-transfers", {{STATE_GENPC,0x10605},{r::STATE_A,0x20900}},{{0x501,0xbeef}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-transfer-snapshot", {{STATE_GENPC,0x10600}})},
			{2,observation("transfer-running", {})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,0x500,0xbeef,0,false},{true,0x501,0xbeef,0,false}};
		// The later READA must observe WRITA's value at the same extended address.
		c.program_events = {{false,0x20900,0xbeef,0,false}};
		cases.push_back(std::move(c));
	}
	{
		auto initial = initial_case("near-CALL-RET-on-extended-page", {0xf495});
		initial.program = {{0x205ff,0xf495},{0x20600,0xf074},{0x20601,0x700},{0x20602,0xf4e1},{0x20700,0xe803},{0x20701,0xfc00}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x205ff},{r::STATE_SP,0x400}});
		auto final = observation("one-word-frame-and-retained-XPC", {{STATE_GENPC,0x20603},{r::STATE_A,3},
			{r::STATE_SP,0x400},{r::STATE_XPC,2}},{{0x3ff,0x602}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-call-snapshot", {{STATE_GENPC,0x20600}})},
			{2,observation("call-running", {})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{true,0x3ff,0x602,0,false},{false,0x3ff,0x602,0,false}};
		cases.push_back(std::move(c));
	}
	{
		auto initial = initial_case("ISR-explicit-XPC-save-and-FRETE", {0xf495});
		initial.program = {{0x10603,0xf495},{0x10604,0xf4e1},{0x10605,0xf4e1},
			{0x10148,0x4a1e},{0x10149,0xf883},{0x1014a,0x700},{0x30700,0xe809},{0x30701,0xf4e5}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x10603},{r::STATE_SP,0x400},{r::STATE_PMST,0x100},
			{r::STATE_IMR,4},{r::STATE_ST1,0}});
		auto final = observation("IRQ-stacks-PC-only-ISR-supplies-page", {{STATE_GENPC,0x10606},{r::STATE_A,9},
			{r::STATE_SP,0x400},{r::STATE_XPC,1},{r::STATE_ST1,0,0x800}},{{0x3fe,1},{0x3ff,0x605}});
		clocked_register_case c{{{0,std::move(initial)},{2,observation("idle-before-physical-IRQ", {{STATE_GENPC,0x10605}})},
			{5,observation("ISR-running", {})},{64,std::move(final)}},2,64};
		c.pipeline_frontend = false;
		c.actions = {{4,{},false,{},{{2,ASSERT_LINE}}}};
		c.irqs = {{2,0,~0U,0,0x605}};
		c.data_events = std::vector<clocked_data_event>{{true,0x3ff,0x605,0,false},{false,0x1e,1,0,false},
			{true,0x3fe,1,0,false},{false,0x3fe,1,0,false},{false,0x3ff,0x605,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
