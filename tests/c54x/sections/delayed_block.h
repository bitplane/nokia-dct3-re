// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 first[]{0x6d95,0x770e,0x10e1,0x6deb};
	constexpr u16 second[]{0x6d96,0x1234,3,0x0a40};
	for (unsigned count : {0U,1U,2U,7U}) for (unsigned layout = 0; layout < 4; ++layout)
	for (unsigned scenario = 0; scenario < 8; ++scenario)
	{
		bool const interrupt = scenario >= 4, late = interrupt && (scenario & 2);
		unsigned const pattern = interrupt ? 0 : scenario;
		int const line = scenario & 1 ? INPUT_LINE_NMI : 2;
		u16 const source = std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		u32 const base = (count + layout + scenario) % 2 ? 0x20000 : 0;
		auto initial = initial_case("delayed-block", {0xf272,0x606,first[layout],second[layout],0xe589,0x6d94,0xf495,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		if (interrupt) {
			initial.program[7].value = 0xf495; initial.program[15].value = 0xf4e1;
			u32 const vector = base + (line == INPUT_LINE_NMI ? 4 : 0x48);
			initial.program.insert(initial.program.end(), {{vector,0xf495},{vector + 1,0xf4eb}});
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},
			{r::STATE_ST1,interrupt ? 0x100 : 0x900},{r::STATE_IMR,interrupt && line == 2 ? 4 : 0},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},
			{r::STATE_T,0x5aa5},{r::STATE_SP,0x3000},{r::STATE_BRC,count},
			{r::STATE_AR1,0x4000},{r::STATE_AR2,0x5000},{r::STATE_AR3,layout == 3 ? 0x55c0 : 0x6000},
			{r::STATE_AR4,0x7000},{r::STATE_AR5,0x7100},{r::STATE_AR6,0x7200}});
		initial.data = {{0x4003,source}};
		u64 const loaded = source & 0x8000 ? 0xffffff0000ULL | source : source;
		auto final = observation("completed", {{STATE_GENPC,base + (interrupt ? 0x610 : 0x608)},{r::STATE_BRC,0},
			{r::STATE_RSA,0x604},{r::STATE_REA,0x606},{r::STATE_ST0,0xa608},{r::STATE_ST1,interrupt ? 0x100U : 0x900U},
			{r::STATE_SP,0x3000},{r::STATE_AR1,0x4000},{r::STATE_AR2,0x5001 + count},
			{r::STATE_AR3,0x6001 + count},{r::STATE_AR4,0x7001 + count},
			{r::STATE_AR5,0x7100 + unsigned(layout == 0)},{r::STATE_AR6,0x7200 + unsigned(layout == 0)},
			{r::STATE_A,layout == 2 ? loaded : 0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},
			{r::STATE_T,layout == 1 ? 0x1234U : 0x5aa5U}});
		clocked_register_case c{{{0,std::move(initial)}},
			std::array<unsigned,4>{3,6,8,13}[(count + layout + pattern) % 4],96};
		c.actions = {{3,{},false,{{base + 0x601,0xdead}}},{5,{},false,{{base + 0x603,0xdead}}}};
		if (layout == 2) { c.actions.push_back({8,{}}); c.actions.back().data = {{0x4003,0xdead}}; }
		c.program_events = {{false,base + 0x601,0x606,0,false},
			{false,base + 0x602,first[layout],0,false},{false,base + 0x603,second[layout],0,false}};
		c.data_events = std::vector<clocked_data_event>{};
		if (layout == 1) c.data_events->push_back({true,0x0e,0x1234,0,false});
		if (layout == 2) c.data_events->push_back({false,0x4003,source,0,false});
		for (unsigned i = 0; i <= count; ++i)
		{
			u16 const value = source ^ u16(i * 0x1357);
			c.states.front().observation.data.insert(c.states.front().observation.data.end(), {{0x5000 + i,value},{0x6000 + i,0xdead}});
			final.expected_data.push_back({0x6000 + i,value});
			c.data_events->push_back({false,0x5000 + i,value,0,false});
			c.data_events->push_back({true,0x6000 + i,value,i ? 0U : 10U,i == 0});
		}
		c.states.push_back({96,std::move(final)});
		if (interrupt) {
			unsigned const at = late ? 10 : 5;
			if (late) c.actions.push_back({at,{},false,{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}});
			else c.actions[1].input_lines = {{line,ASSERT_LINE},{line,CLEAR_LINE}};
			c.checkpoint = std::array<unsigned,6>{3,4,6,8,11,17}[(count + layout + scenario) % 6];
			unsigned const before = unsigned(layout == 1 || layout == 2) + (late && count ? 4U : 2U);
			// Model regression: early service resumes at the next loop iteration
			// or continuation; late service has decoded two further body words.
			u16 const continuation = count ? late ? 0x606 : 0x604 : late ? 0x609 : 0x607;
			c.data_events->insert(c.data_events->begin() + before,
				{{true,0x2fff,continuation,0,false},{false,0x2fff,continuation,0,false}});
			c.irq = clocked_irq_expectation{line,9,96,before};
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
