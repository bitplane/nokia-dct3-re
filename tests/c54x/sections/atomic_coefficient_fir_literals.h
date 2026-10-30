// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 5; ++variant) {
		bool const repeated = variant == 2 || variant == 4, fir = variant >= 3;
		u16 const opcode = variant == 0 ? 0x788b : variant == 1 ? 0x7a8b : variant == 2 ? 0x7893 : 0xe03a;
		auto initial = initial_case("program-coefficient-MACP-MACD-FIRS-literals",{0xf495});
		unsigned at = 1;
		if (repeated) initial.program[at++].value = 0xec02;
		initial.program[at++].value = opcode; initial.program[at++].value = 0x700; initial.program[at].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,variant == 2 ? 0U : 0x770000U},
			{r::STATE_B,fir ? 0U : 0x11223344U},{r::STATE_T,fir ? 0x4444U : 8U},
			{r::STATE_ST0,0},{r::STATE_ST1,0x100},{r::STATE_AR3,0xf90},{r::STATE_AR4,0xf91},{r::STATE_AR5,0xf90}});
		initial.data = fir ? repeated ? std::vector<memory_word>{{0xf90,1},{0xf91,2},{0xf92,2},{0xf93,2}} :
			std::vector<memory_word>{{0xf90,0x55},{0xf91,0xaa}} : repeated ?
			std::vector<memory_word>{{0xf90,1},{0xf91,2},{0xf92,3}} : std::vector<memory_word>{{0xf90,0x55},{0xf91,0x66}};
		u64 const result_a = variant <= 1 ? 0x7d0b44 : variant == 2 ? 32 : variant == 3 ? 0xff0000 : 0x30000;
		auto final = observation("fixed-coefficient-result-T-and-pointer-state",{{STATE_GENPC,0x601 + at},
			{r::STATE_A,result_a},{r::STATE_B,fir ? variant == 3 ? 0x8762cU : 0x86U : 0x11223344U},
			{r::STATE_T,fir ? 0x4444U : repeated ? 3U : 0x55U},{r::STATE_ST1,0x100},
			{r::STATE_AR3,fir ? 0xf90U : repeated ? 0xf93U : 0xf8fU},
			{r::STATE_AR4,fir ? repeated ? 0xf94U : 0xf92U : 0xf91U},{r::STATE_AR5,0xf90}});
		final.expected_data = initial.data;
		if (variant == 1) final.expected_data[1].value = 0x55;
		if (repeated) final.expected.push_back({r::STATE_RC,0});
		std::vector<clocked_data_event> reads, coefficients;
		for (unsigned i = 0; i != (repeated ? 3U : 1U); ++i) {
			u16 const coefficient = repeated ? fir ? i + 1 : i + 4 : 0x1234;
			initial.program.push_back({0x700 + i,coefficient}); final.expected_program.push_back({0x700 + i,coefficient});
			coefficients.push_back({false,0x700 + i,coefficient,0,false});
			reads.push_back({false,fir ? 0xf90U : 0xf90U + i,u16(repeated ? fir ? 1 : i + 1 : 0x55),0,false});
			if (fir) reads.push_back({false,0xf91 + i,u16(repeated ? 2 : 0xaa),0,false});
			if (variant == 1) reads.push_back({true,0xf91,0x55,0,false});
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-coefficient-operation-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("coefficient-operation-running",{})},{64,std::move(final)}},1,64};
		c.pipeline_frontend = false; c.data_events = std::move(reads); c.program_events = std::move(coefficients); cases.push_back(std::move(c));
	}
	return cases;
}
}
