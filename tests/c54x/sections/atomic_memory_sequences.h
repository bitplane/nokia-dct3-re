// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	{
		auto initial = initial_case("SUB-XOR-and-STM-source-change", {0x0883,0x1d83,0x1c83,0x7713,0xf01,0x1c93,0xf4e1});
		initial.program.push_back({0x5ff,0xf495}); initial.data = {{0xf00,0xffff},{0xf01,0xf0}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,5},{r::STATE_B,0x123400},
			{r::STATE_ST1,0x100},{r::STATE_AR3,0xf00}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-sequence-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("sequence-running", {})},{48,observation("signed-sub-and-unextended-XOR-words", {{STATE_GENPC,0x607},
				{r::STATE_A,0xff09},{r::STATE_B,0x12cbff},{r::STATE_AR3,0xf02},{r::STATE_ST1,0x100}},{{0xf00,0xffff},{0xf01,0xf0}})}},1,48};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,0xf00,0xffff,0,false},{false,0xf00,0xffff,0,false},
			{false,0xf00,0xffff,0,false},{true,0x13,0xf01,0,false},{false,0xf01,0xf0,0,false}};
		cases.push_back(std::move(c));
	}
	{
		auto initial = initial_case("IDLE-stops-before-illegal-next-word", {0xf4e1,0xffff});
		initial.program.push_back({0x5ff,0xf495}); initial.initial.push_back({STATE_GENPC,0x5ff});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-IDLE-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("IDLE-entered", {})},{32,observation("next-word-not-executed", {{STATE_GENPC,0x601}})}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	{
		auto initial = initial_case("reset-ASM-before-register-load-and-store", {0xed00,0xf482,0x8083,0xf4e1});
		initial.program.push_back({0x5ff,0xf495}); initial.data = {{0xf10,0}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,0x1234},{r::STATE_ST1,0x11f},{r::STATE_AR3,0xf10}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-ASM-reset-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("ASM-reset-running", {})},{32,observation("zero-ASM-retains-A-and-store-address", {{STATE_GENPC,0x604},
				{r::STATE_A,0x1234},{r::STATE_ST1,0x100},{r::STATE_AR3,0xf10}},{{0xf10,0x1234}})}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{{true,0xf10,0x1234,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
