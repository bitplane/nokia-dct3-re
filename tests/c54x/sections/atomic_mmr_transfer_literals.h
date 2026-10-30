// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 4; ++variant) {
		std::vector<u16> const words = variant == 0 ? std::vector<u16>{0x45f8,0xf24} :
			variant == 1 ? std::vector<u16>{0x4816,0x4913} : variant == 2 ?
			std::vector<u16>{0x7215,0xf60,0x7314,0xf62,0x7315,0xf63} :
			std::vector<u16>{0x8816,0x8817,0x8821,0x8910,0x8911,0x4920};
		auto initial = initial_case("high-load-and-mixed-MMR-transfer-literals", {0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,variant == 3 ? 0x1234U : 0U},{r::STATE_B,variant == 3 ? 0x5678U : 0U},
			{r::STATE_ST0,0x800},{r::STATE_ST1,0x100},{r::STATE_AR0,0},{r::STATE_AR1,0},
			{r::STATE_AR3,0x8003},{r::STATE_AR4,0xabcd},{r::STATE_AR5,0},{r::STATE_AR6,0x8006},{r::STATE_AR7,0}});
		auto final = observation("fixed-results-and-unmodified-source-registers", {{STATE_GENPC,0x602 + words.size()},
			{r::STATE_A,variant == 1 ? 0x8006U : variant == 3 ? 0x1234U : 0U},
			{r::STATE_B,variant == 0 ? 0xff80010000ULL : variant == 1 ? 0x8003 : variant == 3 ? 0x9abc : 0},
			{r::STATE_ST0,0x800},{r::STATE_ST1,0x100},{r::STATE_AR3,0x8003},{r::STATE_AR4,0xabcd},
			{r::STATE_AR5,variant == 2 ? 0x4321U : 0U},{r::STATE_AR6,variant == 3 ? 0x1234U : 0x8006U},
			{r::STATE_AR7,variant == 3 ? 0x1234U : 0U},{r::STATE_AR0,variant == 3 ? 0x5678U : 0U},{r::STATE_AR1,variant == 3 ? 0x5678U : 0U}});
		std::vector<clocked_data_event> events;
		if (variant == 0) {
			initial.data = {{0xf24,0x8001}}; final.expected_data = initial.data;
			events = {{false,0xf24,0x8001,0,false}};
		} else if (variant == 1) events = {{false,0x16,0x8006,0,false},{false,0x13,0x8003,0,false}};
		else if (variant == 2) {
			initial.data = {{0xf60,0x4321},{0xf62,0},{0xf63,0}}; final.expected_data = {{0xf60,0x4321},{0xf62,0xabcd},{0xf63,0x4321}};
			events = {{false,0xf60,0x4321,0,false},{true,0x15,0x4321,0,false},{false,0x14,0xabcd,0,false},
				{true,0xf62,0xabcd,0,false},{false,0x15,0x4321,0,false},{true,0xf63,0x4321,0,false}};
		} else {
			initial.data = {{0x20,0x9abc},{0x21,0}}; final.expected_data = {{0x20,0x9abc},{0x21,0x1234}};
			events = {{true,0x16,0x1234,0,false},{true,0x17,0x1234,0,false},{true,0x21,0x1234,0,false},
				{true,0x10,0x5678,0,false},{true,0x11,0x5678,0,false},{false,0x20,0x9abc,0,false}};
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-transfer-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("transfer-running", {})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	}
	return cases;
}
}
