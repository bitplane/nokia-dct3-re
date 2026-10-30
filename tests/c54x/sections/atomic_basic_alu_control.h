// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::array<std::vector<u16>,10> const programs{{{0x07f8,0x920},{0x6e8f,0x700,0xe801,0xe902,0xe803},
		{0x24f8,0x922},{0x31f8,0x924},{0xf944,0x700},{0x7ef8,0x926,0x7ff8,0x927},{0xf485},{0x1ef8,0x928},{0x1092},{0x1092}}};
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != programs.size(); ++variant) {
		auto const &words = programs[variant];
		u64 const a = variant == 3 ? 0x30000 : variant == 4 ? 1 : variant == 5 ? 0x925 : variant == 6 ? 0xfffffffff9ULL : variant == 7 ? 0x10000 : 0;
		u64 const b = variant == 0 ? 0x1234 : 0;
		u16 const st0 = variant == 0 ? 0x800 : variant >= 8 ? 0xa000 : 0, st1 = variant == 9 ? 0x20 : 0;
		auto initial = initial_case("basic-carry-delay-multiply-call-and-address-flags",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0xffff},
			{r::STATE_AR2,0x930},{r::STATE_AR5,0x930},{r::STATE_AR7,1},{r::STATE_SP,0x300},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		u64 const result_a = variant == 1 ? 1 : variant == 2 ? 0x1fffe : variant == 4 ? 0x5a : variant == 6 ? 7 : variant == 7 ? 0x10001 : variant >= 8 ? 0x1234 : a;
		auto final = observation("explicit-basic-operation-results",{{STATE_GENPC,variant == 1 ? 0x701U : 0x602 + words.size()},
			{r::STATE_A,result_a},{r::STATE_B,variant == 0 ? 0xbe02 : variant == 1 ? 2 : variant == 3 ? 0xfffffffffaULL : b},
			{r::STATE_T,variant == 3 ? 0xfffeU : 0xffffU},{r::STATE_AR7,variant == 1 ? 0U : 1U},{r::STATE_SP,0x300}});
		std::vector<clocked_data_event> data, program;
		if (variant == 0 || variant == 2 || variant == 3 || variant == 7 || variant >= 8) {
			u16 const address = variant == 0 ? 0x920 : variant == 2 ? 0x922 : variant == 3 ? 0x924 : variant == 7 ? 0x928 : 0x930;
			u16 const value = variant == 0 ? 0xabcd : variant == 2 ? 2 : variant == 3 ? 0xfffe : variant == 7 ? 1 : 0x1234;
			initial.data = {{address,value}}; final.expected_data = initial.data; data.push_back({false,address,value,0,false});
		}
		if (variant == 0 || variant == 7) final.expected.push_back({r::STATE_ST0,variant == 7 ? 0x800U : 0U,0x800});
		if (variant >= 8) final.expected.push_back({r::STATE_ST0,variant == 8 ? 0xa000U : 0x4000U,0xe000});
		if (variant == 1) { initial.program.push_back({0x700,0xf4e1}); program.push_back({false,0x700,0xf4e1,0,false}); }
		if (variant == 4) {
			initial.program.insert(initial.program.end(),{{0x700,0xe85a},{0x701,0xfc00}}); initial.data = {{0x2ff,0xbeef}}; final.expected_data = {{0x2ff,0x603}};
			data = {{true,0x2ff,0x603,0,false},{false,0x2ff,0x603,0,false}}; program = {{false,0x700,0xe85a,0,false}};
		}
		if (variant == 5) {
			initial.program.push_back({0x925,0xcafe}); final.expected_program = {{0x925,0xbeef}};
			initial.data = {{0x925,0xface},{0x926,0},{0x927,0xbeef}}; final.expected_data = {{0x925,0xface},{0x926,0xcafe},{0x927,0xbeef}};
			data = {{true,0x926,0xcafe,0,false},{false,0x927,0xbeef,0,false}};
			program = {{false,0x925,0xcafe,0,false},{true,0x925,0xbeef,0,false}};
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-basic-operation-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("basic-operation-running",{})},{64,std::move(final)}},1,64};
		c.pipeline_frontend = false; c.data_events = std::move(data); c.program_events = std::move(program); cases.push_back(std::move(c));
	}
	return cases;
}
}
