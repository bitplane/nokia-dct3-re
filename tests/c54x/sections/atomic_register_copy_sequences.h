// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 4; ++variant) {
		std::vector<u16> const words = variant == 0 ? std::vector<u16>{0x7718,0x320,0x7728,0x5aa5,0x7758,2,0x4858} :
			variant == 1 ? std::vector<u16>{0x8812,0x881a,0x731a,0xf50,0x8915,0x891a} :
			variant == 2 ? std::vector<u16>{0xe589,0xe734} : std::vector<u16>{0xec06,0x6d91};
		auto initial = initial_case("STM-STLM-MVDD-MVMM-RPT-register-sequences", {0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,variant == 0 ? 0U : 0x1234U},{r::STATE_B,0x5678},
			{r::STATE_ST0,0x800},{r::STATE_ST1,0x100},{r::STATE_SP,0x300},{r::STATE_BRC,0},
			{r::STATE_AR1,0x120},{r::STATE_AR2,variant == 2 ? 0xf60U : 0U},
			{r::STATE_AR3,0xf70},{r::STATE_AR4,0},{r::STATE_AR5,variant == 0 ? 0xf13U : 0U}});
		auto final = observation("retained-copy-value-and-selected-register-updates", {{STATE_GENPC,0x602 + words.size()},
			{r::STATE_A,variant == 0 ? 3U : 0x1234U},{r::STATE_B,0x5678},{r::STATE_ST0,0x800},{r::STATE_ST1,0x100},
			{r::STATE_SP,variant == 0 ? 0x320U : 0x300U},{r::STATE_BRC,variant == 1 ? 0x5678U : 0U},
			{r::STATE_AR1,variant == 3 ? 0x127U : 0x120U},{r::STATE_AR2,variant == 1 ? 0x1234U : variant == 2 ? 0xf61U : 0U},
			{r::STATE_AR3,variant == 2 ? 0xf71U : 0xf70U},{r::STATE_AR4,variant == 2 ? 0xf71U : 0U},
			{r::STATE_AR5,variant == 0 ? 0xf13U : variant == 1 ? 0x5678U : 0U}});
		std::vector<clocked_data_event> events;
		if (variant == 0) {
			initial.data = {{0x28,0}}; final.expected_data = {{0x28,0x5aa5}};
			events = {{true,0x18,0x320,0,false},{true,0x28,0x5aa5,0,false},{true,0x58,2,0,false},{false,0x58,3,0,false}};
		} else if (variant == 1) {
			initial.data = {{0xf50,0}}; final.expected_data = {{0xf50,0x1234}};
			events = {{true,0x12,0x1234,0,false},{true,0x1a,0x1234,0,false},{false,0x1a,0x1234,0,false},
				{true,0xf50,0x1234,0,false},{true,0x15,0x5678,0,false},{true,0x1a,0x5678,0,false}};
		} else if (variant == 2) {
			initial.data = {{0xf60,0xbeef},{0xf70,0}}; final.expected_data = {{0xf60,0xbeef},{0xf70,0xbeef}};
			events = {{false,0xf60,0xbeef,0,false},{true,0xf70,0xbeef,0,false},{false,0x13,0xf71,0,false},{true,0x14,0xf71,0,false}};
		} else final.expected.push_back({r::STATE_RC,0});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-sequence-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("sequence-running", {})},{64,std::move(final)}},1,64};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	}
	return cases;
}
}
