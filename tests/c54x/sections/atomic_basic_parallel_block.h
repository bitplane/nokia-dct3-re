// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_XF
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::array<std::vector<u16>,9> const programs{{{0xf0b0},{0xed18},{0x771a,1,0xf272,0x609,0xe801,0xe902,0x6d90,0xf495,0xf495},
		{0xa43a},{0xb336},{0xd631},{0xe210},{0xfa44,0x700,0xe802,0xe903},{0xf484}}};
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != programs.size(); ++variant) {
		auto const &words = programs[variant];
		u64 const a = variant == 0 ? 0xff80000000ULL : variant == 4 ? 0x1234 : variant == 5 ? 0x10001 : variant == 6 ? 0xfffffe0000ULL : variant == 7 ? 1 : variant == 8 ? 5 : 0;
		u64 const b = variant == 3 ? 0x5678 : variant == 4 ? 20 : variant == 5 ? 0x12345678 : variant == 6 ? 10 : 0;
		u64 const out_a = variant == 0 ? 0xff80ff8000ULL : variant == 2 ? 1 : variant == 3 ? 0xfffffffffaULL :
			variant == 5 ? 0x10000 : variant == 6 ? 0xfffffd0000ULL : variant == 7 ? 2 : variant == 8 ? 0xfffffffffbULL : a;
		u64 const out_b = variant == 2 ? 2 : variant == 4 ? 8 : variant == 6 ? 14 : variant == 7 ? 3 : b;
		auto initial = initial_case("basic-shift-ASM-block-and-parallel-arithmetic",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,2},
			{r::STATE_ST0,variant == 8 ? 0x800U : 0U},{r::STATE_ST1,variant == 1 ? 0xa5a5U : 0U},
			{r::STATE_BRC,0},{r::STATE_AR0,0},{r::STATE_AR2,0x970},{r::STATE_AR3,0x960},
			{r::STATE_AR4,variant == 4 ? 0x941U : 0x940U},{r::STATE_AR5,0x950}});
		auto final = observation("fixed-delay-block-and-parallel-results",{{STATE_GENPC,variant == 7 ? 0x701U : 0x602 + words.size()},
			{r::STATE_A,out_a},{r::STATE_B,out_b},{r::STATE_T,variant == 3 ? 0xfffeU : variant == 4 ? 0xfffdU : 2U},
			{r::STATE_ST1,variant == 1 ? 0xa1b8U : 0U},{r::STATE_BRC,0},{r::STATE_AR0,variant == 2 ? 2U : 0U},
			{r::STATE_AR2,0x970},{r::STATE_AR3,0x960},{r::STATE_AR4,variant == 3 ? 0x941U : 0x940U},{r::STATE_AR5,0x950}});
		if (variant == 8) final.expected.push_back({r::STATE_ST0,0,0x800});
		std::vector<clocked_data_event> data, program;
		if (variant == 2) data.push_back({true,0x1a,1,0,false});
		if (variant == 3 || variant == 4) {
			u16 const y = variant == 3 ? 0x940 : 0x941, x_value = variant == 3 ? 0xfffe : 0xfffd, y_value = variant == 3 ? 3 : 4;
			initial.data = {{0x950,x_value},{y,y_value}}; final.expected_data = initial.data;
			data = {{false,0x950,x_value,0,false},{false,y,y_value,0,false}};
		} else if (variant == 5) {
			initial.data = {{0x950,3},{0x960,0}}; final.expected_data = {{0x950,3},{0x960,0x1234}};
			data = {{false,0x950,3,0,false},{true,0x960,0x1234,0,false}};
		} else if (variant == 6) {
			initial.data = {{0x960,5},{0x970,8}}; final.expected_data = initial.data;
			data = {{false,0x960,5,0,false},{false,0x970,8,0,false}};
		} else if (variant == 7) {
			initial.program.push_back({0x700,0xf4e1}); program.push_back({false,0x700,0xf4e1,0,false});
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-basic-operation-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("basic-operation-running",{})},{96,std::move(final)}},1,96};
		c.pipeline_frontend = false; c.data_events = std::move(data); c.program_events = std::move(program); cases.push_back(std::move(c));
	}
	return cases;
}
}
