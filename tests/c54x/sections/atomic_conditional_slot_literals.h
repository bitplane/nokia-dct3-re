// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned kind = 0; kind != 4; ++kind)
	for (bool taken : {false,true}) {
		u16 const opcode = std::array<u16,4>{0xf843,0xff30,0xff45,0xff4e}[kind];
		u64 const a = kind == 0 ? taken ? 0xff00000000ULL : 0x0100000000ULL : kind == 2 ? taken ? 0 : 1 : 0x1234;
		u64 const b = kind == 3 ? taken ? 0x0100000000ULL : 0xff00000000ULL : 0x7654;
		u16 const st0 = kind == 1 && taken ? 0x1000 : 0;
		auto initial = initial_case("BC-ALT-and-XC-TC-AEQ-BGT-literals", {0xf495,opcode});
		if (kind == 0) {
			initial.program[2].value = 0x700; initial.program[3].value = 0xf4e1;
			initial.program.push_back({0x700,0xf4e1});
		} else {
			initial.program[2].value = 0x6d91; initial.program[3].value = 0x6d92; initial.program[4].value = 0xf4e1;
		}
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,st0},
			{r::STATE_ST1,0},{r::STATE_AR1,0x120},{r::STATE_AR2,0x130}});
		auto final = observation("taken-target-or-both-guarded-slots", {{STATE_GENPC,kind == 0 ? taken ? 0x701U : 0x604U : 0x605U},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,st0},{r::STATE_ST1,0},
			{r::STATE_AR1,kind && taken ? 0x121U : 0x120U},{r::STATE_AR2,kind && taken ? 0x131U : 0x130U}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-condition-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("condition-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{};
		if (kind == 0 && taken) c.program_events = {{false,0x700,0xf4e1,0,false}};
		cases.push_back(std::move(c));
	}
	for (bool pmst : {false,true}) {
		auto initial = initial_case("load-MAR-stores-or-STM-PMST-literal", {0xf495});
		std::vector<u16> const words = pmst ? std::vector<u16>{0x771d,0x4567} : std::vector<u16>{0x1192,0x6dd2,0x7683,0x5aa5,0x8085};
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x1234abcd},{r::STATE_B,0},{r::STATE_AR2,0xf22},
			{r::STATE_AR3,0xf30},{r::STATE_AR4,0},{r::STATE_AR5,pmst ? 0xf13U : 0xf31U},
			{r::STATE_BK,4},{r::STATE_ST0,0x800},{r::STATE_ST1,pmst ? 0U : 0x100U}});
		if (!pmst) initial.data = {{0xf22,0x8000},{0xf30,0},{0xf31,0}};
		auto final = observation("literal-copy-state-or-direct-PMST-write", {{STATE_GENPC,0x602 + words.size()},
			{r::STATE_A,0x1234abcd},{r::STATE_B,pmst ? 0 : 0xffffff8000ULL},{r::STATE_PMST,pmst ? 0x4567U : 0U},
			{r::STATE_ST0,0x800},{r::STATE_ST1,pmst ? 0U : 0x100U},{r::STATE_BK,4},
			{r::STATE_AR2,pmst ? 0xf22U : 0xf20U},{r::STATE_AR3,0xf30},{r::STATE_AR4,0},{r::STATE_AR5,pmst ? 0xf13U : 0xf31U}});
		if (!pmst) final.expected_data = {{0xf22,0x8000},{0xf30,0x5aa5},{0xf31,0xabcd}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-operation-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("operation-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false;
		c.data_events = pmst ? std::vector<clocked_data_event>{{true,0x1d,0x4567,0,false}} :
			std::vector<clocked_data_event>{{false,0xf22,0x8000,0,false},{true,0xf30,0x5aa5,0,false},{true,0xf31,0xabcd,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
