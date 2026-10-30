// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 9; ++variant) {
		std::vector<u16> words{0xf495};
		switch (variant) {
		case 0: words.insert(words.end(),{0x730b,0xfa0}); break;
		case 1: words.insert(words.end(),{0x6882,0xf0f}); break;
		case 2: words.insert(words.end(),{0x6884,0xff0f}); break;
		case 3: words.push_back(0x6db1); break;
		case 4: words.push_back(0x6dc2); break;
		case 5: words.push_back(0xe900); break;
		case 6: words.push_back(0xe901); break;
		case 7: words.insert(words.end(),{0xec1f,0x7693,0x5a3c}); break;
		case 8: words.insert(words.end(),{0x7690,0xbeef}); break;
		}
		words.push_back(0xf4e1);
		auto initial = initial_case("MVMD-ANDM-MAR-LD-RPT-ST-literals", {0xf495}); initial.program.clear();
		for (unsigned i = 0; i != words.size(); ++i) initial.program.push_back({0x600 + i,words[i]});
		u64 const b = variant == 0 ? 0x1234 : variant == 5 || variant == 6 ? 0xffffffffffULL : 0x22222222;
		u16 const st1 = variant == 3 || variant == 4 ? 0 : 0x100;
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0x11111111},{r::STATE_B,b},{r::STATE_T,0x3333},
			{r::STATE_AR0,variant == 8 ? 0xf60U : 3U},{r::STATE_AR1,0xf10},
			{r::STATE_AR2,variant == 1 ? 0xf90U : 0xf08U},{r::STATE_AR3,variant == 8 ? 0xd20U : 0xd00U},
			{r::STATE_AR4,0xf92},{r::STATE_BK,4},{r::STATE_ST0,0x800},{r::STATE_ST1,st1}});
		auto final = observation("literal-memory-state-and-selected-pointer", {{STATE_GENPC,0x600 + words.size()},
			{r::STATE_A,0x11111111},{r::STATE_B,variant == 5 ? 0U : variant == 6 ? 1U : b},{r::STATE_T,0x3333},
			{r::STATE_ST0,0x800},{r::STATE_ST1,st1},{r::STATE_BK,4},
			{r::STATE_AR0,variant == 8 ? 0xf61U : 3U},{r::STATE_AR1,variant == 3 ? 0xf13U : 0xf10U},
			{r::STATE_AR2,variant == 1 ? 0xf90U : variant == 4 ? 0xf0bU : 0xf08U},
			{r::STATE_AR3,variant >= 7 ? 0xd20U : 0xd00U},{r::STATE_AR4,0xf92}});
		std::vector<clocked_data_event> events;
		if (variant == 0) {
			initial.data = {{0xfa0,0}}; final.expected_data = {{0xfa0,0x1234}};
			events = {{false,0xb,0x1234,0,false},{true,0xfa0,0x1234,0,false}};
		} else if (variant == 1 || variant == 2) {
			u16 const address = variant == 1 ? 0xf90 : 0xf92, before = variant == 1 ? 0xff00 : 0x1234, after = variant == 1 ? 0xf00 : 0x1204;
			initial.data = {{address,before}}; final.expected_data = {{address,after}};
			events = {{false,address,before,0,false},{true,address,after,0,false}};
		} else if (variant == 7) {
			for (unsigned i = 0; i != 32; ++i) {
				initial.data.push_back({0xd00 + i,0}); final.expected_data.push_back({0xd00 + i,0x5a3c});
				events.push_back({true,0xd00 + i,0x5a3c,0,false});
			}
		} else if (variant == 8) {
			initial.data = {{0xf5f,0x1234},{0xf60,0},{0xf61,0xabcd}};
			final.expected_data = {{0xf5f,0x1234},{0xf60,0xbeef},{0xf61,0xabcd}};
			events = {{true,0xf60,0xbeef,0,false}};
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-operation-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("operation-running", {})},{128,std::move(final)}},1,128};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	}
	return cases;
}
}
