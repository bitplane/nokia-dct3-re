// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 6; ++variant) {
		std::vector<u16> words{0x75f8,0x100,0x124};
		if (variant == 0) words.insert(words.end(),{0x75ea,0x124,5});
		else if (variant == 1) words.insert(words.end(),{0x7dea,0x9000,5});
		else if (variant == 2) words.insert(words.end(),{0x7fea,5});
		else if (variant == 3) words.insert(words.end(),{0x74ea,0x123,5});
		else words.push_back(0x1080);
		words.insert(words.end(),{0x75f8,0x100,0x124,0xf4e1});
		auto initial = initial_case("atomic-offset-transport-or-stationary-AR0-load",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.program.push_back({0x9000,0});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,variant == 2 ? 0x9000U : 0U},
			{r::STATE_B,0x87654321},{r::STATE_T,0x1234},{r::STATE_AR0,0x122},{r::STATE_AR1,0x120},{r::STATE_AR2,0x120},
			{r::STATE_ST0,0xaa5},{r::STATE_ST1,variant == 5 ? 0U : 0x100U}});
		initial.data = {{0x100,0x1111},{0x120,0xbeef},{0x122,0x8002},{0x125,u16(variant == 0 ? 0xabcd : variant == 1 ? 0xbeef : variant == 2 ? 0x1234 : 0)}};
		auto final = observation("extension-order-result-pointer-and-port-time",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,variant == 2 ? 0x9000ULL : variant == 4 ? 0xffffff8002ULL : variant == 5 ? 0x8002ULL : 0ULL},
			{r::STATE_B,0x87654321},{r::STATE_T,0x1234},{r::STATE_AR0,0x122},{r::STATE_AR1,0x120},
			{r::STATE_AR2,variant < 4 ? 0x125U : 0x120U},{r::STATE_ST0,0xaa5},{r::STATE_ST1,variant == 5 ? 0U : 0x100U}});
		final.expected_data = initial.data;
		if (variant == 3) final.expected_data.back().value = 0xabcd;
		final.expected_program = {{0x9000,u16(variant == 1 ? 0xbeef : variant == 2 ? 0x1234 : 0)}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-transport-markers",{{STATE_GENPC,0x601}})},
			{2,observation("transport-markers-running",{})},{64,std::move(final)}},3,64};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO; c.initial_io = {{0x123,0xabcd}};
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false}};
		if (variant == 0 || variant == 3) c.data_events->push_back({variant == 0,u32(variant == 0 ? 0x124 : 0x123),0xabcd,0,false,
			{},variant == 3 ? std::optional<unsigned>(3) : std::nullopt});
		c.data_events->push_back({true,0x124,0x1111,0,false,{},variant == 0 ? std::optional<unsigned>(3) :
			variant == 3 ? std::nullopt : std::optional<unsigned>(variant == 1 ? 8 : variant == 2 ? 9 : 4)});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
