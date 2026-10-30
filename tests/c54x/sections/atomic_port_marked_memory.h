// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,extension,operand,st0,st1; u64 a,b,out_a,out_b; u16 flags,mask; unsigned interval; };
	constexpr row rows[]{
		{0xf020,0xff80,0,0,0,0,5,0xff80,5,0,0,4},
		{0xf020,0xff80,0,0,0x100,0,5,0xffffffff80ULL,5,0,0,4},
		{0x8094,0,0,0,0x100,0x12345678,5,0x12345678,5,0,0,3},
		{0x00f8,0x120,1,0,0,0xffffffffffULL,5,0,5,0x800,0x800,4},
		{0x00f8,0x120,0xfffe,0,0x100,1,5,0xffffffffffULL,5,0,0,4},
		{0x01f8,0x120,0x8001,0x1800,0,0x12345678,1,0x12345678,0x8002,0x1000,0xffff,4},
		{0x01f8,0x120,0x8001,0x1800,0x100,0x12345678,1,0x12345678,0xffffff8002ULL,0x1000,0xffff,4},
		{0x09f8,0x120,1,0,0,0x12345678,0,0x12345678,0xffffffffffULL,0,0x800,4},
		{0x09f8,0x120,0xfffe,0,0x100,0x12345678,0xffffffffffULL,0x12345678,1,0x800,0x800,4},
		{0xec05,0x0082,1,0,0,0,5,6,5,0,0,9},
		{0x7593,0x124,0xabcd,0,0,0,5,0,5,0,0,4}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		bool const store = v.opcode == 0x8094;
		bool const port = v.opcode == 0x7593;
		std::vector<u16> words{0x75d6,0x124,v.opcode};
		if (!store) words.push_back(v.extension);
		words.insert(words.end(),{0x75d6,0x124,0xf4e1});
		auto initial = initial_case("atomic-port-marked-load-store-ALU-and-repeat",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_ST0,v.st0},{r::STATE_ST1,v.st1},
			{r::STATE_AR2,0x120},{r::STATE_AR3,0x120},{r::STATE_AR4,0x120},{r::STATE_AR6,0x103},{r::STATE_BK,4}});
		initial.data = {{0x103,0x1234},{0x100,0x5678},{0x120,v.operand},{0x121,0xbeef}};
		auto final = observation("fixed-results-source-preservation-and-port-interval",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_ST0,v.flags,v.mask},{r::STATE_ST1,v.st1},
			{r::STATE_AR2,0x120},{r::STATE_AR3,port ? 0x121U : 0x120U},{r::STATE_AR4,store ? 0x121U : 0x120U},
			{r::STATE_AR6,0x101}},{{0x103,0x1234},{0x100,0x5678},{0x120,u16(store ? 0x5678 : v.operand)},{0x121,0xbeef}});
		if (v.opcode == 0xec05) final.expected.push_back({r::STATE_RC,0});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-memory-markers",{{STATE_GENPC,0x601}})},
			{2,observation("memory-markers-running",{})},{64,std::move(final)}},3,64};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1234,0,false}};
		if (port) c.data_events->push_back({true,0x124,0xabcd,0,false,{},2});
		c.data_events->push_back({true,0x124,0x5678,0,false,{},port ? 2U : v.interval});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
