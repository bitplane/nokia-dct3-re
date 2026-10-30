// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant=0;variant<3;++variant) for (unsigned divider=0;divider<16;++divider) {
		u16 const status=0x10|divider|(divider<<6), request=0x30|divider;
		auto initial=initial_case("stopped-timer-TRB-reloads-TIM-and-readonly-PSC",{0x7725,0x1234,0x7726,request,
			u16(variant ? 0x6926 : 0xf495),u16(variant ? variant==1 ? 0 : 0x3c0 : 0xf495),0x4826,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,0},{r::STATE_A,0},{r::STATE_TCR,0x10}});
		auto final=observation("reload-stays-stopped-PSC-writes-ignored-TRB-reads-zero",{{r::STATE_A,status},
			{r::STATE_TIM,0x1234},{r::STATE_PRD,0x1234},{r::STATE_TCR,status},{r::STATE_IFR,0}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("timer-program-running",{})},{20,std::move(final)}},
			std::array<unsigned,3>{1,3,6}[cases.size()%3],20};
		c.pipeline_frontend=false;
		c.data_events=std::vector<clocked_data_event>{{true,0x25,0x1234,0,false},{true,0x26,request,0,false}};
		if (variant) c.data_events->insert(c.data_events->end(),{{false,0x26,status,0,false},
			{true,0x26,u16(status|(variant==2 ? 0x3c0 : 0)),0,false}});
		c.data_events->push_back({false,0x26,status,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
