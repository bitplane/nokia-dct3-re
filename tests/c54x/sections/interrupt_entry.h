// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 10; ++index) {
		bool const nmi = index >= 5;
		unsigned const checkpoint = index % 5 + 1;
		u16 const vector = nmi ? 4 : 0x40;
		int const line = nmi ? INPUT_LINE_NMI : 0;
		auto initial = initial_case("atomic-entry-stage-restoration", {0xf4e1});
		for (auto &word : initial.program) word.address += 0x200;
		initial.program.insert(initial.program.end(),{{vector,0xf020},{vector + 1,0x1234},{vector + 2,0xf4e1}});
		initial.data = {{0xfff,0xbeef}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x800},{r::STATE_ST1,nmi ? 0x800U : 0U},
			{r::STATE_IMR,1},{r::STATE_IFR,3}});
		clocked_register_case c{{{0,std::move(initial)}},checkpoint,24};
		c.pipeline_frontend = false;
		if (nmi) c.initial_input_lines = {{line,ASSERT_LINE},{line,CLEAR_LINE}};
		for (unsigned tick = 1; tick <= 5; ++tick) c.states.push_back({tick,observation("entry-ownership-boundary", {
			{r::STATE_SP,tick >= 3 ? 0xfffU : 0x1000U},{r::STATE_IFR,nmi || tick < 4 ? 3U : 2U},
			{r::STATE_ST1,nmi || tick >= 4 ? 0x800U : 0U}},{{0xfff,u16(tick >= 5 ? 0x800 : 0xbeef)}})});
		c.states.push_back({24,observation("accepted-source-and-retained-vector", {{STATE_GENPC,u64(vector + 3)},
			{r::STATE_SP,0xfff},{r::STATE_IFR,nmi ? 3U : 2U},{r::STATE_A,0x1234}})});
		c.actions = {{checkpoint,{{r::STATE_IMR,2}},false,checkpoint >= 4 ? std::vector<memory_word>{{vector,0xf4e1}} : std::vector<memory_word>{}}};
		c.irqs = {{line,0,5,0,0x800}};
		c.program_events = {{false,vector,0xf020,0,false},{false,u32(vector + 1),0x1234,0,false}};
		c.data_events = std::vector<clocked_data_event>{{true,0xfff,0x800,0,false}};
		cases.push_back(std::move(c));
	}
	for (bool reset : {false,true}) {
		auto initial = initial_case("cancel-captured-vector", {0xf4e1});
		for (auto &word : initial.program) word.address += 0x200;
		initial.program.insert(initial.program.end(), {{4,0xf020},{5,0x1234},{6,0xf4e1}});
		initial.data = {{0xfff,0xbeef}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x800},{r::STATE_IMR,1},{r::STATE_IFR,3}});
		clocked_register_case c{{{0,std::move(initial)},
			{4,observation("vector-captured-before-stack-write", {{r::STATE_SP,0xfff},{r::STATE_A,0}},{{0xfff,0xbeef}})},
			{5,observation("entry-cancelled", {{r::STATE_A,0}})},
			{24,observation("foreground-idle-not-vector", {{STATE_GENPC,0x801},{r::STATE_A,0}})}},2,24};
		c.pipeline_frontend = false;
		c.initial_input_lines = {{INPUT_LINE_NMI,ASSERT_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
		c.actions = {{4,{{reset ? int(STATE_GENPC) : int(r::STATE_PC),0x800},{r::STATE_IDLE,0}},false,{},{},{},reset}};
		c.program_events = {{false,4,0xf020,0,false}};
		c.data_events = std::vector<clocked_data_event>{};
		c.irqs = {{INPUT_LINE_NMI,0,5,0,0x800}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
