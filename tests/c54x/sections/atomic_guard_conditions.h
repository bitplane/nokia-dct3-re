// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode; u64 a,b; u16 st0; bool taken; };
	constexpr row rows[]{
		{0xf84a,1,0,0,true},{0xf84a,1,0xff00000000ULL,0,false},
		{0xf84b,1,0xff00000000ULL,0,true},{0xf84b,1,0,0,false},
		{0xf84f,1,0,0,true},{0xf84f,1,0xff00000000ULL,0,true},{0xf84f,1,1,0,false},
		{0xfc47,1,0,0,false},{0xfc47,0xff00000000ULL,0,0,true},
		{0xff47,0,0,0,true},{0xff47,1,0,0,false},
		{0xff4c,0,0xff00000000ULL,0,true},{0xff4c,0,0,0,false},
		{0xff4f,1,0xff00000000ULL,0x1800,true},{0xff4f,1,0,0x1800,true},{0xff4f,1,0x0100000000ULL,0x1800,false},
		{0xff44,0x0100000000ULL,0,0x1800,true},{0xff44,0,0,0x1800,false}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		bool const branch = (v.opcode & 0xff00) == 0xf800, ret = v.opcode == 0xfc47;
		auto initial = initial_case("40-bit-guard-branch-return-and-XC-literals", {0xf495,v.opcode});
		if (branch) { initial.program[2].value = 0x700; initial.program[3].value = 0xf4e1; }
		else if (ret) initial.program[2].value = 0xf4e1;
		else { initial.program[2].value = 0x6d91; initial.program[3].value = 0x6d92; initial.program[4].value = 0xf4e1; }
		initial.program.push_back({0x700,0xf4e1}); initial.data = {{0x300,0x700}};
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_ST0,v.st0},
			{r::STATE_ST1,0x100},{r::STATE_SP,0x300},{r::STATE_AR1,0x120},{r::STATE_AR2,0x130}});
		u32 const pc = branch || ret ? v.taken ? 0x701 : ret ? 0x603 : 0x604 : 0x605;
		auto final = observation("guard-sign-zero-stack-and-annulled-slot-results", {{STATE_GENPC,pc},
			{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_ST0,v.st0},{r::STATE_ST1,0x100},{r::STATE_SP,ret && v.taken ? 0x301U : 0x300U},
			{r::STATE_AR1,!branch && !ret && v.taken ? 0x121U : 0x120U},{r::STATE_AR2,!branch && !ret && v.taken ? 0x131U : 0x130U}},{{0x300,0x700}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-condition-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("condition-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{};
		if (ret && v.taken) c.data_events->push_back({false,0x300,0x700,0,false});
		if ((branch || ret) && v.taken) c.program_events = {{false,0x700,0xf4e1,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
