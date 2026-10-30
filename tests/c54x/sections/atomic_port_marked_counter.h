// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 8; ++variant) {
		bool const delayed = variant == 2;
		bool const compat = variant < 5;
		bool const taken = variant == 0 || variant == 2 || variant == 3 || variant == 5;
		bool const store = variant == 7;
		u16 const opcode = delayed ? 0x6e88 : variant < 2 ? 0x6c88 : variant < 5 ? 0x6c8b : variant < 7 ? 0xf847 : 0x880e;
		u64 const a = variant == 5 ? 0xff00000000ULL : variant == 6 ? 1 : store ? 0xff12345678ULL : 0;
		std::vector<u16> words{0x75f8,0x100,0x124,opcode};
		if (!store) words.push_back(0x700);
		if (delayed) words.insert(words.end(),{0xf495,0xf495});
		words.insert(words.end(),{0x75f8,0x101,0x124,0xf4e1});
		auto initial = initial_case("atomic-port-marked-counter-branch-sign-or-STLM",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.program.insert(initial.program.end(),{{0x700,0x75f8},{0x701,0x102},{0x702,0x124},{0x703,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_T,0},{r::STATE_ST0,compat ? 0x8000U : 0U},
			{r::STATE_ST1,compat ? 0x20U : 0U},{r::STATE_AR0,variant == 1 ? 1U : 0U},
			{r::STATE_AR4,variant == 1 ? 0U : 1U},{r::STATE_AR3,variant == 4 ? 0U : 1U}});
		initial.data = {{0x100,0x1111},{0x101,0x2222},{0x102,0x3333}};
		auto final = observation("predecrement-predicate-selected-marker-and-timing",{{STATE_GENPC,taken ? 0x704U : 0x601+words.size()},
			{r::STATE_A,a},{r::STATE_T,store ? 0x5678U : 0U},{r::STATE_AR0,variant == 1 ? 1U : 0U},
			{r::STATE_AR4,variant == 1 ? 0xffffU : variant < 3 ? 0U : 1U},
			{r::STATE_AR3,variant == 3 ? 0U : variant == 4 ? 0xffffU : 1U}});
		final.expected_data = initial.data;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-counter-markers",{{STATE_GENPC,0x601}})},
			{2,observation("counter-markers-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},
			{true,0x124,u16(taken ? 0x3333 : 0x2222),0,false,{},store ? 4U : variant >= 5 ? taken ? 8U : 6U : taken ? 7U : 5U}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
