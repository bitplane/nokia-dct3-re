// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned count : {3U,0xffffU}) {
		bool const compact = count == 3;
		u16 const continuation = compact ? 0x602 : 0x603;
		auto initial = initial_case("atomic-repeat-pending-IRQ", {
			u16(compact ? 0xec03 : 0xf070),u16(compact ? 0x0082 : count),u16(compact ? 0xf4e1 : 0x0082),0xf4e1});
		initial.program.push_back({0x48,0xf49b});
		initial.data = {{0x1002,1}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_AR2,0x1002},{r::STATE_ST1,0},{r::STATE_IMR,4}});
		clocked_register_case c{{{0,std::move(initial)}},count == 3 ? 4U : 32U,count + 40};
		c.pipeline_frontend = false;
		if (count == 0xffff) {
			c.states.push_back({32,observation("active-repeat-with-pending-IRQ", {
				{r::STATE_RC,0xff00,0xff00},{r::STATE_IFR,4}})});
			c.states.push_back({33,observation("continue-active-repeat", {})});
		}
		c.states.push_back({count + 40,observation("repeat-retires-before-IRQ", {{r::STATE_A,count + 1},
			{STATE_GENPC,unsigned(continuation + 1)},{r::STATE_SP,0x1000},{r::STATE_RC,0},{r::STATE_ST1,0,0x800}})});
		c.actions = {{3,{},false,{},{{2,ASSERT_LINE},{2,CLEAR_LINE}}}};
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned n = 0; n <= count; ++n) c.data_events->push_back({false,0x1002,1,0,false});
		c.irqs = {{2,0,~0U,count + 1,continuation}};
		c.data_events->push_back({true,0xfff,continuation,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
