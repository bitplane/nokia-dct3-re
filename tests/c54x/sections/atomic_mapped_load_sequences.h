// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 4; ++variant) {
		std::vector<u16> const words = variant == 0 ? std::vector<u16>{0x4812,0x8095,0x4813,0x8095,0x4814,0x8095,0x4817,0x8095,0x4910} :
			variant == 1 ? std::vector<u16>{0xf200,0xfffe} : variant == 2 ? std::vector<u16>{0xf540} :
			std::vector<u16>{0x10da,0x6d93,0x7308,0xf50,0x7313,0xf51,0x7681,0x5a5a};
		auto initial = initial_case("mapped-load-copy-and-accumulator-transfer-sequences", {0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,variant == 1 ? 0U : 0x800U},{r::STATE_ST1,0x100}});
		auto final = observation("explicit-sequence-result-and-original-flags", {{STATE_GENPC,0x602 + words.size()},
			{r::STATE_ST0,variant == 2 ? 0xa00U : 0x800U},{r::STATE_ST1,0x100}});
		std::vector<clocked_data_event> events;
		if (variant == 0) {
			initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_B,0},{r::STATE_AR0,0x8000},
				{r::STATE_AR2,0x8002},{r::STATE_AR3,0x8003},{r::STATE_AR4,0x8004},{r::STATE_AR5,0xf40},{r::STATE_AR7,0x8007}});
			final.expected.insert(final.expected.end(),{{r::STATE_A,0x8007},{r::STATE_B,0x8000},{r::STATE_AR0,0x8000},
				{r::STATE_AR2,0x8002},{r::STATE_AR3,0x8003},{r::STATE_AR4,0x8004},{r::STATE_AR5,0xf44},{r::STATE_AR7,0x8007}});
			unsigned i = 0;
			for (unsigned ar : {2U,3U,4U,7U}) {
				initial.data.push_back({0xf40 + i,0}); final.expected_data.push_back({0xf40 + i,u16(0x8000 + ar)});
				events.push_back({false,0x10 + ar,u16(0x8000 + ar),0,false});
				events.push_back({true,0xf40 + i,u16(0x8000 + ar),0,false}); ++i;
			}
			events.push_back({false,0x10,0x8000,0,false});
		} else if (variant == 1) {
			initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x1234},{r::STATE_B,0x100}});
			final.expected.insert(final.expected.end(),{{r::STATE_A,0xfe},{r::STATE_B,0x100}});
		} else if (variant == 2) {
			initial.initial.insert(initial.initial.end(),{{r::STATE_A,0xff000000f0ULL},{r::STATE_B,0x1234}});
			final.expected.insert(final.expected.end(),{{r::STATE_A,0xff000000f0ULL},{r::STATE_B,0xff000000f0ULL}});
		} else {
			initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_B,0x1234},{r::STATE_AR0,2},
				{r::STATE_AR1,0xf52},{r::STATE_AR2,0xf22},{r::STATE_AR3,0xf30},{r::STATE_BK,4}});
			initial.data = {{0xf22,0x8001},{0xf50,0},{0xf51,0},{0xf52,0}};
			final.expected.insert(final.expected.end(),{{r::STATE_A,0xffffff8001ULL},{r::STATE_B,0x1234},{r::STATE_AR0,2},
				{r::STATE_AR1,0xf52},{r::STATE_AR2,0xf20},{r::STATE_AR3,0xf31},{r::STATE_BK,4}});
			final.expected_data = {{0xf22,0x8001},{0xf50,0x8001},{0xf51,0xf31},{0xf52,0x5a5a}};
			events = {{false,0xf22,0x8001,0,false},{false,8,0x8001,0,false},{true,0xf50,0x8001,0,false},
				{false,0x13,0xf31,0,false},{true,0xf51,0xf31,0,false},{true,0xf52,0x5a5a,0,false}};
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-sequence-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("sequence-running", {})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	}
	return cases;
}
}
