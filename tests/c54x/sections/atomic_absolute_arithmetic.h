// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,literal,operand; bool extension; u64 a,b,out_a,out_b; u16 carry; unsigned interval; };
	constexpr row rows[]{
		{0x0af8,0x120,0xffff,true,0x10000,0,1,0,0x800,5},
		{0x0af8,0x120,0xffff,true,0,0,0xffffff0001ULL,0,0,5},
		{0x1e82,0,1,false,0x8000,0x12345678,1,0x12345678,0x800,4},
		{0x1e82,0,1,false,0x7fff,0x12345678,0xfffe,0x12345678,0,4},
		{0x1e84,0,1,false,0x8000,0x12345678,1,0x12345678,0x800,4},
		{0x1e84,0,1,false,0x7fff,0x12345678,0xfffe,0x12345678,0,4},
		{0x6dea,0xfff9,0,true,0,0,0,0,0,5},
		{0xf300,0xffff,0,true,0,0,0,0xffffffffffULL,0,5},
		{0xf300,1,0,true,0,0xffffffffULL,0,0x100000000ULL,0x800,5},
		{0xf210,2,0,true,0x1234,10,8,10,0x800,5},
		{0xf210,1,0,true,8,0,0xffffffffffULL,0,0,5},
		{0xf150,0xff00,0,true,0xff12345678ULL,0,0xff12345678ULL,0xff1234a978ULL,0x800,5},
		{0x0282,0,0xffff,false,1,0x76543210,0x10000,0x76543210,0,4},
		{0x0282,0,1,false,0xffffffffULL,0x76543210,0x100000000ULL,0x76543210,0x800,4},
		{0x0387,0,0xffff,false,0x12345678,1,0x12345678,0x10000,0,4},
		{0x0387,0,1,false,0x12345678,0xffffffffULL,0x12345678,0x100000000ULL,0x800,4}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		std::vector<u16> words{0x75f8,0x100,0x124,v.opcode};
		if (v.extension) words.push_back(v.literal);
		words.insert(words.end(),{0x75f8,0x100,0x124,0xf4e1});
		auto initial = initial_case("atomic-absolute-marker-arithmetic-fixed-vectors",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},
			{r::STATE_AR2,v.opcode == 0x6dea ? 0x1000U : 0x120U},{r::STATE_AR4,0x120},{r::STATE_AR7,0x120},
			{r::STATE_ST0,v.opcode == 0xf150 ? 0x800U : 0U},{r::STATE_ST1,0x100}});
		initial.data = {{0x100,0x1111},{0x120,v.operand}};
		auto final = observation("literal-result-carry-and-source-preservation",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_ST0,v.carry,0x800},{r::STATE_ST1,0x100},
			{r::STATE_AR2,v.opcode == 0x6dea ? 0xff9U : 0x120U},{r::STATE_AR4,0x120},{r::STATE_AR7,0x120}});
		final.expected_data = initial.data;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-arithmetic-markers",{{STATE_GENPC,0x601}})},
			{2,observation("arithmetic-markers-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},v.interval}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
