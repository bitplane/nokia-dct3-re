// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,st0,st1,pmst,address,memory; unsigned ar; u16 next; u64 a,b,result_a,result_b; u16 result_st0; bool write; };
	constexpr row rows[]{
		{0x1183,0,0x100,0,0xfa0,0xfffe,3,0xfa0,0x11223344,0,0x11223344,0xfffffffffeULL,0,false},
		{0x0093,0,0x200,0,0xfa1,1,3,0xfa2,0x7fffffff,0x11223344,0x7fffffff,0x11223344,0x400,false},
		{0xf1a0,0xc00,0,0,0,0,2,0x1111,0xff00000000ULL,0xff,0xff00000000ULL,0xff000000ffULL,0xc00,false},
		{0xf2c0,0xc00,0,0,0,0,2,0x1111,0xff000000f0ULL,0xff0000000fULL,0xff,0xff0000000fULL,0xc00,false},
		{0xf620,0,0x200,0,0,0,2,0x1111,0x7fffffff,0xffffffffffULL,0x7fffffff,0xffffffffffULL,0x400,false},
		{0x818a,0x800,0x200,0,0xf10,0x1234,2,0xf0f,0x11223344,0x1234,0x11223344,0x1234,0x800,true},
		{0x81d3,0x800,0x200,0,0xffb,0xabcd,3,0xff8,0x11223344,0xabcd,0x11223344,0xabcd,0x800,true},
		{0x8282,0x800,0x100,1,0xf90,0x7fff,2,0xf90,0x7ffff0000ULL,0xabcd,0x7ffff0000ULL,0xabcd,0x800,true}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		auto initial = initial_case("literal-LD-ADD-OR-XOR-SUB-STL-STH", {0xf495,v.opcode,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_ST0,v.st0},
			{r::STATE_ST1,v.st1},{r::STATE_PMST,v.pmst},{r::STATE_BK,4},
			{int(r::STATE_AR0) + int(v.ar),v.address ? v.address : 0x1111U}});
		if (v.address) initial.data.push_back({v.address,u16(v.write ? 0xbeef : v.memory)});
		auto final = observation("literal-result-pointer-flags-and-sibling", {{STATE_GENPC,0x603},
			{r::STATE_A,v.result_a},{r::STATE_B,v.result_b},{r::STATE_ST0,v.result_st0},{r::STATE_ST1,v.st1},
			{r::STATE_PMST,v.pmst},{r::STATE_BK,4},{int(r::STATE_AR0) + int(v.ar),v.next}});
		if (v.address) final.expected_data.push_back({v.address,v.memory});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-literal-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("literal-running", {})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{};
		if (v.address) c.data_events->push_back({v.write,v.address,v.memory,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
