// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add = [&](std::vector<u16> words, std::vector<register_setting> registers,
		std::vector<register_check> expected, u16 source, u16 value, std::vector<memory_word> extra = {}, bool long_word = false) {
		auto initial = initial_case("memory-arithmetic-fixed-literal-contract",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,0x800},{r::STATE_ST1,0x100},{r::STATE_T,0},
			{r::STATE_AR2,0xf92},{r::STATE_AR3,0xf90}});
		initial.initial.insert(initial.initial.end(),registers.begin(),registers.end()); initial.data = {{source,value}};
		initial.data.insert(initial.data.end(),extra.begin(),extra.end());
		auto final = observation("fixed-A-B-T-carry-and-address-results",{{STATE_GENPC,0x602 + words.size()}});
		final.expected.insert(final.expected.end(),expected.begin(),expected.end()); final.expected_data = initial.data;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-arithmetic-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("arithmetic-running",{})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{{false,source,value,0,false}};
		if (long_word) c.data_events->push_back({false,u32(source + 1),extra.front().value,0,false});
		cases.push_back(std::move(c));
	};
	add({0x1f83},{{r::STATE_A,0x1234},{r::STATE_B,0x10000},{r::STATE_ST0,0},{r::STATE_ST1,0x180}},
		{{r::STATE_A,0x1234},{r::STATE_B,0x10001},{r::STATE_ST0,0x800,0x800},{r::STATE_AR3,0xf90}},0xf90,1);
	add({0x5aea,6},{{r::STATE_A,0},{r::STATE_B,0x10001},{r::STATE_T,0x2345},{r::STATE_AR2,0xf00}},
		{{r::STATE_A,0x3879579b},{r::STATE_T,0x2345},{r::STATE_AR2,0xf06}},0xf06,0x1534,{{0xf07,0x3456}},true);
	add({0x0e83},{{r::STATE_A,6},{r::STATE_ST0,0}},{{r::STATE_A,0xffffffffffULL},{r::STATE_ST0,0,0x800},{r::STATE_AR3,0xf90}},0xf90,6);
	add({0x0f83},{{r::STATE_A,0x1234},{r::STATE_B,0xff80000006ULL}},
		{{r::STATE_A,0x1234},{r::STATE_B,0xff80000000ULL},{r::STATE_ST0,0x800,0x800}},0xf90,6);
	add({0x0183},{{r::STATE_A,0x1234},{r::STATE_B,0xffffffffULL},{r::STATE_ST0,0}},
		{{r::STATE_A,0x1234},{r::STATE_B,0x100000000ULL},{r::STATE_ST0,0x800,0x800}},0xf90,1);
	add({0x0b83},{{r::STATE_A,0x1234},{r::STATE_B,0x10000},{r::STATE_ST0,0}},
		{{r::STATE_A,0x1234},{r::STATE_B,1},{r::STATE_ST0,0x800,0x800}},0xf90,0xffff);
	add({0x1983},{{r::STATE_B,0x123456}},{{r::STATE_B,0x56},{r::STATE_ST0,0x800,0x800}},0xf90,0xff);
	add({0x2183},{{r::STATE_A,0x1234},{r::STATE_B,0},{r::STATE_T,0xfffe}},
		{{r::STATE_A,0x1234},{r::STATE_B,0xfffffffffaULL},{r::STATE_T,0xfffe},{r::STATE_ST0,0x800,0x800}},0xf90,3);
	struct multiply { u16 opcode,t,memory,out_t; u64 a,b,out_a,out_b; u16 ar2 = 0xf90,st1 = 0x100; };
	constexpr multiply products[]{
		{0x2383,0x4000,3,0x4000,0x1234,0,0x1234,0x10000},
		{0x2583,0xfffe,3,0xfffe,0x1234,0,0x1234,0x2fffa},
		{0x2783,0x4000,0xfffe,0xfffe,0x1234,0,0x1234,4},
		{0x2983,0xfffe,3,0xfffe,0x1234,10,0x1234,4},
		{0x2b83,0x4000,3,0x4000,0x1234,0x10000,0x1234,0x20000},
		{0x2c83,2,3,2,10,0x5678,4,0x5678},{0x2d83,2,3,2,0x1234,10,0x1234,4},
		{0x2e83,0x4000,3,0x4000,0x30000,0x5678,0x20000,0x5678},
		{0x2f83,0x4000,3,0x4000,0x1234,0x30000,0x1234,0x20000},
		{0x2082,0xfffe,3,0xfffe,0,0x5678,0xfffffffffaULL,0x5678},
		{0x218a,0xfffe,3,0xfffe,0x1234,0,0x1234,0xfffffffffaULL,0xf8f},
		{0x2882,0xfffe,3,0xfffe,5,0x5678,0xffffffffffULL,0x5678},
		{0x2882,0xfffe,3,0xfffe,5,0x5678,0xfffffffff9ULL,0x5678,0xf90,0x140},
		{0x2d92,2,3,2,0x1234,10,0x1234,4,0xf91}
	};
	for (auto const &v : products) add({v.opcode},{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_T,v.t},{r::STATE_AR2,0xf90},{r::STATE_ST1,v.st1}},
		{{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_T,v.out_t},{r::STATE_AR2,v.ar2},{r::STATE_AR3,0xf90},{r::STATE_ST1,v.st1},{r::STATE_ST0,0x800,0x800}},0xf90,v.memory);
	struct acca { u16 opcode,memory,t,out_t,ar3; u64 a,b,out_a,out_b; };
	constexpr acca accumulator_sources[]{
		{0x3383,3,0,3,0xf90,0x20000,10,0x20000,4},{0x3583,3,0,3,0xf90,0x20000,10,0x20000,16},
		{0x3783,3,0,3,0xf90,0x40000000,0x10000,0x40000000,0x20000},
		{0x3693,0x2000,0x5678,0x5678,0xf91,0x12340000,0x10000,0x6270000,0x20000000}
	};
	for (auto const &v : accumulator_sources) add({v.opcode},{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_T,v.t}},
		{{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_T,v.out_t},{r::STATE_AR3,v.ar3},{r::STATE_ST0,0x800,0x800}},0xf90,v.memory);
	struct shifted { u16 opcode,memory,t,out_t; u64 a,b,out_a,out_b; };
	constexpr shifted shifts[]{
		{0x3083,0xabcd,0,0xabcd,0x1234,0x5678,0x1234,0x5678},
		{0x3983,0xfffe,0,0xfffe,0x1234,10,0x1234,14},{0x3a83,3,0,3,10,0x5678,1,0x5678},
		{0x3c83,0xfffe,0,0,0x30000,0x5678,0x10000,0x5678},{0x3d83,2,0,0,1,0x5678,1,0x20001},
		{0x3e83,0xfffe,0,0,0x1234,0x30000,0x10000,0x30000},{0x3f83,2,0,0,0x1234,1,0x1234,0x20001},
		{0x3c92,0xfffe,0,0,0x30000,0x5678,0x10000,0x5678},{0x3f92,2,0,0,0x1234,1,0x1234,0x20001},
		{0x3f82,2,0,0,0x1234,1,0x1234,0x20001},{0x3c82,0xfffe,0,0,0x30000,0x5678,0x10000,0x5678},
		{0x308a,0xabcd,0x1234,0xabcd,0x1234,0x5678,0x1234,0x5678}
	};
	for (unsigned i = 0; i != std::size(shifts); ++i) {
		auto const &v = shifts[i]; bool const ar2 = i >= 7;
		add({v.opcode},{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_T,v.t}},
			{{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_T,v.out_t},{r::STATE_AR3,0xf90},
			{r::STATE_AR2,i == 11 ? 0xf91U : i == 7 || i == 8 ? 0xf93U : 0xf92U},{r::STATE_ST0,0x800,0x800}},
			ar2 ? 0xf92 : 0xf90,v.memory,ar2 ? std::vector<memory_word>{{0xf90,u16(i >= 10 ? 2 : v.memory)}} : std::vector<memory_word>{});
	}
	struct sub { u16 opcode,before,after; u64 a,b,out_a,out_b; };
	constexpr sub subtracts[]{
		{0x4083,0,0,0x30000,0x5678,0x10000,0x5678},{0x4183,0x800,0x800,0x30000,0x5678,0x30000,0x10000},
		{0x4283,0x800,0,0x1234,0x10000,0xffffff0000ULL,0x10000},{0x4383,0,0,0x1234,0x30000,0x1234,0x10000},
		{0x4082,0x800,0x800,0x40000,0x5678,0x10000,0x5678}
	};
	for (unsigned i = 0; i != std::size(subtracts); ++i) {
		auto const &v = subtracts[i]; add({v.opcode},{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_T,0x9abc},{r::STATE_ST0,v.before}},
			{{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_T,0x9abc},{r::STATE_AR3,0xf90},{r::STATE_AR2,0xf92},{r::STATE_ST0,v.after,0x800}},
			i == 4 ? 0xf92 : 0xf90,i == 4 ? 3 : 2,i == 4 ? std::vector<memory_word>{{0xf90,2}} : std::vector<memory_word>{});
	}
	for (bool decrement : {false,true}) add({u16(decrement ? 0x438a : 0x4392)},{{r::STATE_A,0x1234},{r::STATE_B,0x30000},{r::STATE_T,0x9abc},{r::STATE_ST0,0}},
		{{r::STATE_A,0x1234},{r::STATE_B,0x10000},{r::STATE_T,0x9abc},{r::STATE_AR2,decrement ? 0xf91U : 0xf93U},{r::STATE_AR3,0xf90},{r::STATE_ST0,0,0xa00}},0xf92,2);
	return cases;
}
}
