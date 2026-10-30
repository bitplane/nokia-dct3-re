// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// Keep all counts, mapping regions and destination modes; balance page/replay.
	for (unsigned count : {0U,1U,3U,15U}) for (unsigned mapped = 0; mapped < 2; ++mapped)
	for (unsigned advance = 0; advance < 2; ++advance) for (unsigned interrupt = 0; interrupt < 3; ++interrupt)
	{
		u32 const base = (count + mapped + advance) % 2 ? 0x30000 : 0x20000, origin = base + 0x9000;
		u32 const source = mapped ? 0x700 : base + 0x700;
		int const line = interrupt == 2 ? INPUT_LINE_NMI : 0;
		auto initial = initial_case("repeat-mvpd", {0xf070,u16(count),u16(advance ? 0x7c92 : 0x7c82),0x700,0xe955});
		for (auto &word : initial.program) word.address += origin - 0x600;
		// NOP stream permits the continuation and the returning ISR to settle.
		for (unsigned at = 0x620; at < 0x650; ++at) initial.program.push_back({base + 0x9000 + at - 0x600,0xf495});
		if (interrupt) {
			u32 const vector = (mapped ? 0 : base) + (interrupt == 2 ? 4 : 0x40);
			initial.program.insert(initial.program.end(), {{vector,0x4a1e},{vector + 1,0xf495},{vector + 2,0xf4e5}});
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,origin},{r::STATE_PMST,mapped ? 0x20 : 0},
			{r::STATE_ST0,0x608},{r::STATE_ST1,interrupt == 1 ? 0 : 0x800},
			{r::STATE_AR2,0x420},{r::STATE_IMR,interrupt == 1 ? 1 : 0}});
		initial.expected.push_back({r::STATE_B,0});
		for (unsigned i = 0; i <= count; ++i) {
			initial.program.push_back({source + i,0x1200 + i}); initial.data.push_back({0x420 + i,0xffff});
		}
		auto final = observation("continued", {{r::STATE_AR2,0x420 + (advance ? count + 1 : 0)},
			{r::STATE_B,0x66},{r::STATE_SP,0x1000}});
		for (unsigned i = 0; i <= count; ++i)
			final.expected_data.push_back({0x420 + (advance ? i : 0),u16(0x1200 + (advance ? i : count))});
		unsigned const point = interrupt ? 9 : std::array<unsigned,4>{6,9,11,20}[(count + mapped + advance) % 4];
		clocked_register_case c{{{0,std::move(initial)},
			{count + 11,observation("repeat-retired", {})},{count + 40,std::move(final)}},point,count + 40};
		c.require_idle = false;
		c.actions = {{6,{},false,{{origin + 4,0xe966}}},{9,{},false,{{source,0xaaaa}}}};
		if (interrupt) {
			c.actions.insert(c.actions.begin(), {3,{},false,{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}});
			c.irq = clocked_irq_expectation{line,count + 10,~0U,count + 1,0x9004};
		}
		c.data_range = std::array<u32,2>{0x420,0x430};
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned i = 0; i <= count; ++i) {
			c.program_events.push_back({false,source + i,u16(0x1200 + i),i ? 10 + i : 9});
			c.data_events->push_back({true,0x420 + (advance ? i : 0),u16(0x1200 + i),10 + i});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
