// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 24; ++index) {
		unsigned const producer = index / 8;
		bool const gap = index & 1, dp = index & 2, extended = index & 4;
		u16 const final_sp = producer == 0 ? 0x1001 : 0xfff, address = dp ? 0x1806 : gap || extended ? final_sp + 6 : 0x1006;
		auto initial = initial_case("stack-address-dependency", {u16(producer == 0 ? 0x8a0b : producer == 1 ? 0x4a0b : 0xeeff)});
		unsigned cursor = 1 + unsigned(gap);
		initial.program[cursor++].value = extended ? 0x6f06 : 0x8006;
		if (extended) initial.program[cursor++].value = 0xc80;
		initial.program[cursor].value = 0xf4e1;
		for (unsigned at = 0xfff; at <= 0x1007; ++at) initial.data.push_back({at,0xcccc});
		initial.data.insert(initial.data.end(),{{0x1000,0x5566},{0x1806,0xcccc}});
		initial.initial.insert(initial.initial.end(), {{r::STATE_SP,0x1000},{r::STATE_ST0,0x30},
			{r::STATE_ST1,dp ? 0x800U : 0x4800U},{r::STATE_A,0x4044},{r::STATE_B,0x1234}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,32};
		c.pipeline_frontend = false;
		c.states.push_back({32,observation("category-I-latency-category-II-forwarding", {{r::STATE_SP,final_sp},
			{r::STATE_A,0x4044},{r::STATE_B,producer == 0 ? 0x5566U : 0x1234U},{STATE_GENPC,0x601 + cursor}},{{address,0x4044}})});
		c.data_events = std::vector<clocked_data_event>{};
		if (producer == 0) { c.data_events->push_back({false,0x1000,0x5566,0,false}); c.data_events->push_back({true,0xb,0x5566,0,false}); }
		if (producer == 1) { c.data_events->push_back({false,0xb,0x1234,0,false}); c.data_events->push_back({true,0xfff,0x1234,0,false}); }
		c.data_events->push_back({true,address,0x4044,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
