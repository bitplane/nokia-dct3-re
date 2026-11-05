// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
namespace literal_tables {
inline std::vector<clocked_register_case> logic_pointer_literals_cases()
{
	struct row {
		std::vector<u16> words;
		std::vector<register_setting> initial;
		std::vector<register_check> expected;
		std::vector<memory_word> data, final_data;
		std::vector<clocked_data_event> events;
	};
	std::vector<row> const rows{
		{{0xf032,0xf0f},{{r::STATE_A,0xff12345678ULL}},{{r::STATE_A,0x1438},{r::STATE_ST0,0x800}}, {},{},{}},
		{{0xf035,0xff},{{r::STATE_A,0xff12345678ULL}},{{r::STATE_A,0x1660},{r::STATE_ST0,0x800}}, {},{},{}},
		{{0xf0fb},{{r::STATE_A,0xff80000013ULL},{r::STATE_ST0,0}},{{r::STATE_A,0x4000000},{r::STATE_ST0,0x800}}, {},{},{}},
		{{0xf0fe},{{r::STATE_A,0xff80000003ULL},{r::STATE_ST0,0}},{{r::STATE_A,0x20000000},{r::STATE_ST0,0x800}}, {},{},{}},
		{{0xe741},{{r::STATE_AR1,0x1234},{r::STATE_AR4,0xcdef}},{{r::STATE_AR1,0xcdef},{r::STATE_AR4,0xcdef}}, {},{},{{false,0x14,0xcdef,0,false},{true,0x11,0xcdef,0,false}}},
		{{0x1181},{{r::STATE_B,0},{r::STATE_AR1,0xf20}},{{r::STATE_B,0xffffff8001ULL},{r::STATE_AR1,0xf20}},{{0xf20,0x8001}},{{0xf20,0x8001}},{{false,0xf20,0x8001,0,false}}},
		{{0x1a8a},{{r::STATE_A,0x120000},{r::STATE_AR2,0xf20}},{{r::STATE_A,0x1200f0},{r::STATE_AR2,0xf1f}},{{0xf20,0xf0}},{{0xf20,0xf0}},{{false,0xf20,0xf0,0,false}}},
		{{0x12d2},{{r::STATE_A,0},{r::STATE_AR2,0xf23}},{{r::STATE_A,0x8001},{r::STATE_AR2,0xf20}},{{0xf23,0x8001}},{{0xf23,0x8001}},{{false,0xf23,0x8001,0,false}}},
		{{0x1cf8,0xf20},{{r::STATE_A,0x12000f}},{{r::STATE_A,0x1200ff},{r::STATE_ST0,0x800}},{{0xf20,0xf0}},{{0xf20,0xf0}},{{false,0xf20,0xf0,0,false}}},
		{{0x6982,0xa0},{{r::STATE_AR2,0xf20}},{{r::STATE_AR2,0xf20},{r::STATE_ST0,0x800}},{{0xf20,0x1001}},{{0xf20,0x10a1}},{{false,0xf20,0x1001,0,false},{true,0xf20,0x10a1,0,false}}},
		{{0x6984,5},{{r::STATE_AR4,0xf24}},{{r::STATE_AR4,0xf24}},{{0xf24,0x8000},{0xf20,0x10a1}},{{0xf24,0x8005},{0xf20,0x10a1}},{{false,0xf24,0x8000,0,false},{true,0xf24,0x8005,0,false}}},
		{{0x1b8d},{{r::STATE_B,0x34000f},{r::STATE_AR5,0xf25}},{{r::STATE_B,0x3400ff},{r::STATE_AR5,0xf24},{r::STATE_ST0,0x800}},{{0xf25,0xf0}},{{0xf25,0xf0}},{{false,0xf25,0xf0,0,false}}},
		{{0x4595},{{r::STATE_B,0},{r::STATE_AR5,0xf30}},{{r::STATE_B,0xffff800000ULL},{r::STATE_AR5,0xf31}},{{0xf30,0xff80}},{{0xf30,0xff80}},{{false,0xf30,0xff80,0,false}}},
		{{0x4917},{{r::STATE_B,0},{r::STATE_AR7,0x8001}},{{r::STATE_B,0x8001},{r::STATE_AR7,0x8001}}, {},{},{{false,0x17,0x8001,0,false}}},
		{{0x6883,0xff0},{{r::STATE_AR3,0xf33}},{{r::STATE_AR3,0xf33},{r::STATE_ST0,0x800}},{{0xf33,0xff0f}},{{0xf33,0xf00}},{{false,0xf33,0xff0f,0,false},{true,0xf33,0xf00,0,false}}},
		{{0x6983,0xf0},{{r::STATE_AR3,0xf33}},{{r::STATE_AR3,0xf33},{r::STATE_ST0,0x800}},{{0xf33,0xf00}},{{0xf33,0xff0}},{{false,0xf33,0xf00,0,false},{true,0xf33,0xff0,0,false}}}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		auto initial = initial_case("logic-and-circular-pointer-literals", {0xf495}); initial.program.clear();
		initial.program.push_back({0x600,0xf495});
		for (unsigned i = 0; i != v.words.size(); ++i) initial.program.push_back({0x601 + i,v.words[i]});
		initial.program.push_back({0x601 + u32(v.words.size()),0xf4e1}); initial.data = v.data;
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,0x800},{r::STATE_ST1,0x100},{r::STATE_BK,4}});
		initial.initial.insert(initial.initial.end(),v.initial.begin(),v.initial.end());
		auto final = observation("fixed-literal-results-with-explicit-source-state", {{STATE_GENPC,0x602 + v.words.size()}});
		final.expected.insert(final.expected.end(),v.expected.begin(),v.expected.end()); final.expected_data = v.final_data;
		auto c = literal_sequence(std::move(initial), observation("pre-literal-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 24, 1);
		c.data_events = v.events; cases.push_back(std::move(c));
	}
	for (bool tc : {false,true}) {
		auto initial = initial_case("XC-two-MAR-slots-NTC", {0xf495,0xff20,0x6d91,0x6d92,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR1,0x120},{r::STATE_AR2,0x130},{r::STATE_ST0,tc ? 0x1000U : 0U}});
		auto final = observation("both-slots-execute-or-both-annul", {{STATE_GENPC,0x605},
			{r::STATE_AR1,tc ? 0x120U : 0x121U},{r::STATE_AR2,tc ? 0x130U : 0x131U},{r::STATE_ST0,tc ? 0x1000U : 0U}});
		auto c = literal_sequence(std::move(initial), observation("pre-XC-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 24, 1);
		c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	return cases;
}

inline std::vector<clocked_register_case> logic_program_literals_cases()
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
		auto c = literal_sequence(std::move(initial), observation("pre-literal-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 32, 1);
		c.data_events = std::vector<clocked_data_event>{};
		if (variant <= 2) c.data_events->push_back({false,address,before,0,false});
		if (variant != 2) c.data_events->push_back({true,address,after,0,false});
		if (variant >= 3) c.program_events = {{false,0xd40,0xabcd,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}

inline std::vector<clocked_register_case> accumulator_add_literals_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 7; ++variant) {
		bool const cross = variant >= 1 && variant <= 4, b = cross && variant % 2;
		bool const ovm = cross && variant >= 3;
		u16 const opcode = variant == 0 ? 0xf508 : cross ? b ? 0xf500 : 0xf600 : variant == 5 ? 0xf400 : 0xf028;
		u64 const a = variant == 0 ? 0x1234 : cross ? b ? 1 : 0x7fffffff : variant == 5 ? 0x7fffffff : 0;
		u64 const b_value = variant == 0 ? 0x10000 : cross ? b ? 0x7fffffff : 1 : 0x11223344;
		u64 const sum = cross ? ovm ? 0x7fffffff : 0x80000000 : variant == 0 ? 0x133400 : variant == 5 ? 0xfffffffeULL : 0xaa00;
		u16 const st0 = cross ? 0x1800 : 0, st1 = cross ? ovm ? 0x200 : 0 : 0x100;
		u16 const result_st0 = cross ? b ? 0x1200 : 0x1400 : variant == 5 ? 0x400 : 0;
		auto initial = initial_case("accumulator-ADD-overflow-and-shift-literals", {0xf495,opcode});
		if (variant == 6) initial.program[2].value = 0xaa;
		initial.program[variant == 6 ? 3 : 2].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b_value},{r::STATE_T,0x8888},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		auto final = observation("destination-overflow-source-T-and-TC", {{STATE_GENPC,variant == 6 ? 0x604U : 0x603U},
			{r::STATE_A,variant == 0 || b ? a : sum},{r::STATE_B,variant == 0 || b ? sum : b_value},
			{r::STATE_T,0x8888},{r::STATE_ST0,result_st0},{r::STATE_ST1,st1}});
		auto c = literal_sequence(std::move(initial), observation("pre-add-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 24, 1);
		c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	return cases;
}

inline std::vector<clocked_register_case> shift_peripheral_literals_cases()
{
	struct row { u16 opcode,extension,st0,st1,operand; u64 a,b,out_a,out_b; u16 flags,mask; unsigned interval; };
	constexpr row rows[]{
		{0xf517,0,0,0,0,0xff80000000ULL,3,0xff80000000ULL,0x7fc00003,0,0,4},
		{0xf517,0,0,0x100,0,0xff80000000ULL,3,0xff80000000ULL,0xffffc00003ULL,0,0,4},
		{0xf474,0,0,0,0,0xff80000800ULL,0,0xff80000,0,0x800,0x800,4},
		{0xf474,0,0,0x100,0,0xff80000800ULL,0,0xfffff80000ULL,0,0x800,0x800,4},
		{0x7722,0xa55a,0,0x100,0,0,0,0,0,0,0,6},
		{0xf537,0,0,0,0,0xff80000000ULL,0,0xff80000000ULL,0xff80400000ULL,0,0,4},
		{0xf537,0,0,0x100,0,0xff80000000ULL,0,0xff80000000ULL,0x400000,0,0,4},
		{0x6184,0xf0,0x800,0x100,0xa0,0,0,0,0,0x1800,0x1800,5},
		{0x6184,0xf0,0x800,0x100,0xa00,0,0,0,0,0x800,0x1800,5}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		std::vector<u16> words{0x75f8,0x100,0x124,v.opcode};
		if (v.extension) words.push_back(v.extension);
		words.insert(words.end(),{0x75f8,0x100,0x124,0xf4e1});
		auto initial = initial_case("shift-carry-bit-test-and-peripheral-literals",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_AR4,0x120},
			{r::STATE_ST0,v.st0},{r::STATE_ST1,v.st1}});
		initial.data = {{0x100,0x1111},{0x120,v.operand},{0x22,0}};
		auto final = observation("fixed-shift-bit-and-peripheral-results",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_AR4,0x120},{r::STATE_ST0,v.flags,v.mask},{r::STATE_ST1,v.st1}},
			{{0x100,0x1111},{0x120,v.operand},{0x22,u16(v.opcode == 0x7722 ? 0xa55a : 0)}});
		auto c = literal_sequence(std::move(initial), observation("before-shift-markers",{{STATE_GENPC,0x600}}), std::move(final), 48, 3);
		c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},v.interval}};
		cases.push_back(std::move(c));
	}
	auto initial = initial_case("BACC-captured-target-without-call-frame",{0xf495,0x75f8,0x100,0x124,0xf4e2,0xffff});
	initial.program.insert(initial.program.end(),{{0x700,0x75f8},{0x701,0x101},{0x702,0x124},{0x703,0xf4e1}});
	initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x700},{r::STATE_B,0x605},{r::STATE_SP,0x300},{r::STATE_ST1,0}});
	initial.data = {{0x100,0x1111},{0x101,0x3333},{0x2ff,0xbeef}};
	auto final = observation("BACC-target-unchanged-stack-and-port-time",{{STATE_GENPC,0x704},{r::STATE_SP,0x300},
		{r::STATE_A,0x700},{r::STATE_B,0x605}});
	final.expected_data = initial.data;
	auto c = literal_sequence(std::move(initial), observation("before-BACC-marker",{{STATE_GENPC,0x600}}), std::move(final), 48, 3);
	c.observed_data_space = AS_IO;
	c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x3333,0,false,{},9}};
	cases.push_back(std::move(c));
	return cases;
}
}
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (auto build : {literal_tables::logic_pointer_literals_cases,literal_tables::logic_program_literals_cases,literal_tables::accumulator_add_literals_cases,literal_tables::shift_peripheral_literals_cases}) {
		auto rows = build();
		cases.insert(cases.end(),std::make_move_iterator(rows.begin()),std::make_move_iterator(rows.end()));
	}
	return cases;
}
}
