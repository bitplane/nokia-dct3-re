// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 8; ++variant) {
		bool const parallel = variant == 1 || variant == 2;
		u16 const opcode = std::array<u16,8>{0x4f82,0xd6e1,0xd6e1,0x1093,0x1084,0x1094,0x4912,0x4911}[variant];
		u64 const a = parallel || (variant >= 3 && variant <= 5) ? 0 : 0x1234;
		u64 const b = variant == 0 ? 0x8ffff0000ULL : variant == 1 ? 0x7ffff0000ULL : variant == 2 ? 0x3ffff0000ULL : variant == 7 ? 0xffffffffffULL : 0x1234;
		u16 const st1 = variant == 0 || variant == 4 ? 0 : variant == 2 ? 0x101 : 0x100;
		u16 const address = variant == 0 ? 0xf90 : variant == 3 ? 0xfa0 : 0xf90;
		u16 const operand = variant == 3 ? 0x8001 : variant == 4 ? 0x8002 : 0xfffd;
		auto initial = initial_case("DST-parallel-ST-LD-LDM-literals", {0xf495,opcode,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0},
			{r::STATE_ST0,0x800},{r::STATE_ST1,st1},{r::STATE_PMST,variant <= 2 ? 1U : 0U},
			{r::STATE_AR0,1},{r::STATE_AR1,0xabcd},{r::STATE_AR2,variant >= 6 ? 0x9abcU : 0xf90U},
			{r::STATE_AR3,0xfa0},{r::STATE_AR4,0xf90},{r::STATE_BK,4}});
		u64 const result_a = variant == 3 ? 0xffffff8001ULL : variant == 4 ? 0x8002 : variant == 5 ? 0xfffffffffdULL : a;
		u64 const result_b = variant == 6 ? 0x9abc : variant == 7 ? 0xabcd : b;
		auto final = observation("literal-memory-results-and-independent-siblings", {{STATE_GENPC,0x603},
			{r::STATE_A,result_a},{r::STATE_B,result_b},{r::STATE_T,0},{r::STATE_ST1,st1},
			{r::STATE_PMST,variant <= 2 ? 1U : 0U},{r::STATE_AR0,1},{r::STATE_AR1,0xabcd},
			{r::STATE_AR2,variant >= 6 ? 0x9abcU : 0xf90U},{r::STATE_AR3,variant == 3 ? 0xfa1U : 0xfa0U},
			{r::STATE_AR4,parallel || variant == 5 ? 0xf91U : 0xf90U},{r::STATE_BK,4}});
		if (!parallel) final.expected.push_back({r::STATE_ST0,0x800});
		std::vector<clocked_data_event> events;
		if (variant == 0) {
			initial.data = {{0xf90,0},{0xf91,0}}; final.expected_data = {{0xf90,0xffff},{0xf91,0xffff}};
			events = {{true,0xf90,0xffff,0,false},{true,0xf91,0xffff,0,false}};
		} else if (parallel) {
			initial.data = {{0xf90,0},{0xfa0,0}}; final.expected_data = {{0xf90,0},{0xfa0,0x7fff}};
			events = {{false,0xf90,0,0,false},{true,0xfa0,0x7fff,0,false}};
		} else if (variant <= 5) {
			initial.data = {{address,operand}}; final.expected_data = {{address,operand}};
			events = {{false,address,operand,0,false}};
		} else events = {{false,variant == 6 ? 0x12U : 0x11U,u16(variant == 6 ? 0x9abc : 0xabcd),0,false}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-literal-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("literal-running", {})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	}
	return cases;
}
}
