// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#define C54X_SECTION_HIGH_PROGRAM
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned count_index = 0; count_index < 4; ++count_index)
	for (unsigned ovly = 0; ovly < 2; ++ovly) for (unsigned boundary = 0; boundary < 2; ++boundary)
	for (unsigned ram = 0; ram < 2; ++ram) for (unsigned interrupt = 0; interrupt < 3; ++interrupt)
	{
		unsigned const count = std::array<unsigned,4>{0,1,3,15}[count_index];
		unsigned const page = std::array<unsigned,3>{0,2,0x7f}[(count_index + ovly + boundary + ram) % 3];
		u32 const source = (page << 16) | (boundary ? 0x7ffe : 0x700);
		u16 const counter = ram ? 0x500 : 0x11;
		int const line = interrupt == 2 ? INPUT_LINE_NMI : 0;
		auto physical = [ovly](u32 logical) { return ovly && !(logical & 0x8000) ? logical & 0xffff : logical; };
		auto initial = initial_case("memory-repeat", {0x47f8,counter,0x7e92,0xe955});
		for (auto &word : initial.program) word.address += 0x19000 - 0x600;
		for (unsigned i = 0x20; i < 0x50; ++i) initial.program.push_back({0x19000 + i,0xf495});
		if (interrupt) {
			u32 const vector = (ovly ? 0 : 0x10000) + (interrupt == 2 ? 4 : 0x40);
			initial.program.insert(initial.program.end(), {{vector,0x4a1e},{vector + 1,0xf495},{vector + 2,0xf4e5}});
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x19000},{r::STATE_PMST,ovly ? 0x20 : 0},
			{r::STATE_ST0,0x608},{r::STATE_ST1,interrupt == 1 ? 0 : 0x800},
			{r::STATE_AR1,count},{r::STATE_AR2,0x420},{r::STATE_A,source},{r::STATE_IMR,interrupt == 1 ? 1 : 0}});
		initial.expected.push_back({r::STATE_B,0});
		initial.data = {{0x500,count}};
		for (unsigned i = 0; i <= count; ++i) {
			initial.program.push_back({physical(source + i),0x1200 + i}); initial.data.push_back({0x420 + i,0xffff});
		}
		auto final = observation("continued", {{r::STATE_AR2,0x420 + count + 1},{r::STATE_A,source + 0x40},
			{r::STATE_B,0x66},{r::STATE_SP,0x1000}});
		for (unsigned i = 0; i <= count; ++i) final.expected_data.push_back({0x420 + i,u16(0x1200 + i)});
		unsigned const point = interrupt ? 9 : std::array<unsigned,4>{4,6,10,16}[(count_index + ovly + boundary + ram) % 4];
		clocked_register_case c{{{0,std::move(initial)},
			{count + 15,observation("repeat-retired", {})},{count + 40,std::move(final)}},point,count + 40};
		c.require_idle = false;
		c.actions = {{5,{},false,{{0x19003,0xe966}}},{6,{}},{10,{{r::STATE_A,source + 0x40}}},
			{13,{},false,{{physical(source),0xaaaa}}}};
		if (ram) c.actions[1].data = {{0x500,0xffff}};
		else c.actions[1].values = {{r::STATE_AR1,0xffff}};
		if (interrupt) {
			c.actions.insert(c.actions.begin(), {3,{},false,{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}});
			c.irq = clocked_irq_expectation{line,count + 14,~0U,count + 2,0x9003};
		}
		c.data_ranges = {{counter,counter},{0x420,0x430}};
		c.data_events = std::vector<clocked_data_event>{{false,counter,u16(count),6}};
		for (unsigned i = 0; i <= count; ++i) {
			c.program_events.push_back({false,physical(source + i),u16(0x1200 + i),i ? 14 + i : 13});
			c.data_events->push_back({true,0x420 + i,u16(0x1200 + i),14 + i});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
