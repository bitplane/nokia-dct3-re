// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::array<std::vector<u16>,13> const programs{{
		{0xf273,0x700,0xf495,0xf495},{0xf4eb},{0xfa45,0x700,0xf495,0xf495},{0x6ff8,0x920,0xc48},
		{0xf5e2},{0x09f8,0x918},{0xfc4b},{0xf947,0x700},{0xf520},{0xf070,1,0x7f92},{0x3292},{0xeeff,0xee02},{0xf0ff}
	}};
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != programs.size(); ++variant) {
		auto const &words = programs[variant];
		bool const target = variant == 0 || variant == 1 || variant == 2 || variant == 4 || variant == 6 || variant == 7;
		u64 const a = variant == 8 ? 3 : variant == 9 ? 0x940 : variant == 12 ? 0x35 : 0;
		u64 const b = variant == 4 ? 0x700 : variant == 5 ? 5 : variant == 6 ? 0xffffffffffULL : variant == 8 ? 9 : 0;
		u16 const sp = variant == 1 || variant == 6 ? 0x2ff : 0x300;
		u16 const st1 = variant == 1 ? 0x800 : variant == 10 ? 2 : 0;
		auto initial = initial_case("basic-control-stack-ASM-and-program-transfer-sequences",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1; initial.program.push_back({0x700,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_SP,sp},{r::STATE_ST0,0},{r::STATE_ST1,st1},
			{r::STATE_AR2,variant == 10 ? 0x918U : 0x920U}});
		initial.data = {{0x2ff,0x700}};
		auto final = observation("explicit-control-target-and-final-machine-state",{{STATE_GENPC,target ? 0x701U : 0x602 + words.size()},
			{r::STATE_A,variant == 3 ? 0xaaaa00U : variant == 12 ? 0x1aU : a},
			{r::STATE_B,variant == 5 ? 4U : variant == 8 ? 6U : b},
			{r::STATE_SP,variant == 1 || variant == 6 ? 0x300U : variant == 7 ? 0x2ffU : variant == 11 ? 0x301U : sp},
			{r::STATE_AR2,variant == 9 ? 0x922U : variant == 10 ? 0x919U : 0x920U}},{{0x2ff,u16(variant == 7 ? 0x603 : 0x700)}});
		std::vector<clocked_data_event> data, program;
		if (target) program.push_back({false,0x700,0xf4e1,0,false});
		if (variant == 1 || variant == 6) data.push_back({false,0x2ff,0x700,0,false});
		if (variant == 7) data.push_back({true,0x2ff,0x603,0,false});
		if (variant == 1) final.expected.push_back({r::STATE_ST1,0,0x800});
		if (variant == 3 || variant == 5 || variant == 10) {
			u16 const address = variant == 3 ? 0x920 : 0x918, value = variant == 3 ? 0xaaaa : 1;
			initial.data.push_back({address,value}); final.expected_data.push_back({address,value}); data.push_back({false,address,value,0,false});
		}
		if (variant == 9) {
			initial.data.insert(initial.data.end(),{{0x920,0xaaaa},{0x921,0xbbbb}});
			final.expected_data.insert(final.expected_data.end(),{{0x920,0xaaaa},{0x921,0xbbbb}});
			initial.program.insert(initial.program.end(),{{0x940,0},{0x941,0}}); final.expected_program = {{0x940,0xaaaa},{0x941,0xbbbb}};
			data = {{false,0x920,0xaaaa,0,false},{false,0x921,0xbbbb,0,false}};
			program = {{true,0x940,0xaaaa,0,false},{true,0x941,0xbbbb,0,false}};
		}
		if (variant == 10) final.expected.push_back({r::STATE_ST1,1,0x1f});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-basic-operation-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("basic-operation-running",{})},{96,std::move(final)}},1,96};
		c.pipeline_frontend = false; c.data_events = std::move(data); c.program_events = std::move(program); cases.push_back(std::move(c));
	}
	return cases;
}
}
