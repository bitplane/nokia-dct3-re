// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#define C54X_SECTION_IRQ
#define C54X_SECTION_OBSERVE_STORE
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// Retain the IRQ-drain interaction, not only the final load/store result.
	for (unsigned variant = 0; variant < 18; ++variant)
	{
		bool const load = variant >= 12;
		unsigned const n = load ? variant - 12 : variant;
		bool const b = n >= 3;
		unsigned const mode = n % 3;
		u16 const opcode = load ? (b ? 0x1100 : 0x1000) | std::array<u16,3>{0x81,0x89,0x91}[mode] :
			0x8020 + (n % 4) * 0x100;
		auto initial = initial_case("basic-memory", {opcode});
		initial.program.push_back({0x40,0xe955});
		for (unsigned at = 0x41; at < 0x50; ++at) initial.program.push_back({at,0xf495});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,load ? 0 : 8},
			{r::STATE_ST1,!load && n >= 8 ? 0x100 : 0}, {r::STATE_PMST,!load && n >= 4 ? 1 : 0},
			{r::STATE_IMR,1}, {r::STATE_AR1,0x420},
			{r::STATE_A,load ? 0 : n >= 8 ? 0xff00000000ULL : 0x0123456789ULL},
			{r::STATE_B,load ? 0 : n >= 8 ? 0xff00000001ULL : 0x0156789abcULL}});
		initial.data = {{0x420,load ? 0x1234 : 0xbeef}, {0x4a0,load ? 0x5678 : 0xbeef}};
		clocked_register_case c{{{0,std::move(initial)}},load ? 4U : 5U,10};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions.push_back({2,{},false,{},{{0,ASSERT_LINE}}});
		if (load)
		{
			u16 const updated = std::array<u16,3>{0x420,0x41f,0x421}[mode];
			c.states.push_back({4,observation("access", {{r::STATE_AR1,updated},{r::STATE_A,0},{r::STATE_B,0}})});
			c.actions.push_back({4,{{r::STATE_AR1,0x4a0}}});
			c.states.push_back({5,observation("retained-address", {{r::STATE_AR1,0x4a0},{r::STATE_A,0},{r::STATE_B,0}})});
			c.states.push_back({6,observation("execute", {{r::STATE_AR1,0x4a0},{r::STATE_A,b ? 0U : 0x1234U},{r::STATE_B,b ? 0x1234U : 0U}})});
			c.states.push_back({10,observation("isr", {{r::STATE_A,b ? 0U : 0x1234U},{r::STATE_B,0x55},{r::STATE_AR1,0x4a0}})});
			c.data_events = std::vector<clocked_data_event>{{false,0x420,0x1234,5}};
		}
		else
		{
			u16 const value = std::array<u16,12>{0x6789,0x9abc,0x2345,0x5678,0xffff,0xffff,0xffff,0xffff,0,0,0x8000,0x8000}[n];
			c.actions.push_back({5,{{r::STATE_ST0,9}}});
			c.states.push_back({6,observation("store", {}, {{0x420,value},{0x4a0,0xbeef}})});
			c.data_events = std::vector<clocked_data_event>{{true,0x420,value,6}};
		}
		c.data_events->push_back({true,0xfff,0x601,7});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
