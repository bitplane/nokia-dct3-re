// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 5; ++variant) {
		u16 const opcode = std::array<u16,5>{0x6885,0x6985,0x6fd2,0x7e8b,0x7e92}[variant];
		auto initial = initial_case("AR5-logic-shifted-LD-and-READA-literals", {0xf495,opcode});
		bool const extended = variant <= 2;
		if (extended) initial.program[2].value = variant == 0 ? 0xff0 : variant == 1 ? 0xf0 : 0xc45;
		initial.program[2 + extended].value = 0xf4e1;
		u16 const address = variant < 2 ? 0xf40 : variant == 2 ? 0xf23 : variant == 3 ? 0xf33 : 0xf34;
		u16 const before = variant == 0 ? 0xff0f : variant == 1 ? 0xf00 : variant == 2 ? 0xff80 : 0;
		u16 const after = variant == 0 ? 0xf00 : variant == 1 ? 0xff0 : variant == 2 ? 0xff80 : 0xabcd;
		u64 const a = variant < 2 ? 0x12345678 : variant == 2 ? 0 : 0xd40;
		initial.data = {{address,before}};
		if (variant < 2) initial.data.insert(initial.data.end(),{{0xf3f,0xbeef},{0xf41,0xcafe},{0xf33,0xff0}});
		if (variant >= 3) { initial.program.push_back({0xd40,0xabcd}); initial.data.push_back({0xd40,0xface}); }
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,0x76543210},{r::STATE_ST0,0x800},
			{r::STATE_ST1,0x100},{r::STATE_BK,4},{r::STATE_AR2,variant == 2 ? 0xf23U : variant == 4 ? 0xf34U : 0x3002U},
			{r::STATE_AR3,variant == 4 ? 0xf32U : 0xf33U},{r::STATE_AR5,0xf40}});
		auto final = observation("selected-address-memory-space-and-neighbors", {{STATE_GENPC,0x603 + unsigned(extended)},
			{r::STATE_A,variant == 2 ? 0xfffffff000ULL : a},{r::STATE_B,0x76543210},{r::STATE_ST0,0x800},
			{r::STATE_ST1,0x100},{r::STATE_BK,4},{r::STATE_AR2,variant == 2 ? 0xf20U : variant == 4 ? 0xf35U : 0x3002U},
			{r::STATE_AR3,variant >= 3 ? 0xf32U : 0xf33U},{r::STATE_AR5,0xf40}},{{address,after}});
		if (variant < 2) final.expected_data.insert(final.expected_data.end(),{{0xf3f,0xbeef},{0xf41,0xcafe},{0xf33,0xff0}});
		if (variant >= 3) final.expected_data.push_back({0xd40,0xface});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-literal-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("literal-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{};
		if (variant <= 2) c.data_events->push_back({false,address,before,0,false});
		if (variant != 2) c.data_events->push_back({true,address,after,0,false});
		if (variant >= 3) c.program_events = {{false,0xd40,0xabcd,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
