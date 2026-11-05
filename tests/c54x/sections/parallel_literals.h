// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
namespace literal_tables {
inline void append_parallel_literals(std::vector<clocked_register_case> &cases)
{
	auto append = [&](char const *name,u16 opcode,u16 source,u16 word,u16 destination,u16 stored,
			std::initializer_list<register_setting> setup,std::initializer_list<register_check> checks) {
		auto initial = initial_case(name,{opcode,0xf4e1});
		initial.program.push_back({0x5ff,0xf495}); initial.data = {{source,word}};
		if (source != destination) initial.data.push_back({destination,0xdead});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0},{r::STATE_ST1,0}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		auto final = observation(name,checks,{{destination,stored}});
		final.expected.push_back({STATE_GENPC,0x602});
		if (source != destination) final.expected_data.push_back({source,word});
		auto c = literal_sequence(std::move(initial), observation("pre-parallel-snapshot", {{STATE_GENPC,0x5ff}}), std::move(final), 32, 1);
		c.data_events = std::vector<clocked_data_event>{{false,source,word,0,false},{true,destination,stored,0,false}};
		cases.push_back(std::move(c));
	};
	append("ST-MPY-fractional",0xcdb9,0x300,0x4000,0x200,0x8421,
		{{r::STATE_A,0xff84211234ULL},{r::STATE_B,0},{r::STATE_T,0x4000},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300},{r::STATE_ST1,0x140}},
		{{r::STATE_A,0xff84211234ULL},{r::STATE_B,0x20000000},{r::STATE_T,0x4000},{r::STATE_AR3,0x201},{r::STATE_AR5,0x301}});
	append("ST-MPY-old-destination",0xcfb9,0x300,3,0x200,0x1234,
		{{r::STATE_A,0xabcdef},{r::STATE_B,0x12345678},{r::STATE_T,2},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0xabcdef},{r::STATE_B,6},{r::STATE_T,2},{r::STATE_AR3,0x201},{r::STATE_AR5,0x301}});
	append("ST-ADD-opposite",0xc131,0x300,2,0x200,0x1234,
		{{r::STATE_A,0x12345678},{r::STATE_B,0xdead},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0x12345678},{r::STATE_B,0x12365678},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300}});
	append("ST-SUB-circular-ASM",0xc5f5,0x300,0x8001,0x1ff,0x842,
		{{r::STATE_A,0xff84210000ULL},{r::STATE_B,0x10000001},{r::STATE_AR0,2},{r::STATE_BK,4},{r::STATE_AR3,0x1ff},{r::STATE_AR5,0x300},{r::STATE_ST1,0x101}},
		{{r::STATE_A,0xff84210000ULL},{r::STATE_B,0xfffbe00000ULL},{r::STATE_AR3,0x1fe},{r::STATE_AR5,0x302}});
	append("ST-LD-signed",0xca31,0x300,0x8001,0x200,0x1234,
		{{r::STATE_A,0},{r::STATE_B,0x12345678},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0xff80010000ULL},{r::STATE_B,0x12345678},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300}});
	append("ST-LD-T-same-cell",0xe411,0x200,0x80ff,0x200,0x1234,
		{{r::STATE_A,0x12345678},{r::STATE_T,0},{r::STATE_AR3,0x200},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0x12345678},{r::STATE_T,0x80ff},{r::STATE_AR3,0x200}});
	for (unsigned b = 0; b < 2; ++b) append("ST-LD-T-circular",b ? 0xe6e9 : 0xe4e9,0x203,0x80ff,0x300,b ? 0x8765 : 0x1234,
		{{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},{r::STATE_T,0},{r::STATE_AR0,1},{r::STATE_BK,4},
		 {r::STATE_AR3,0x300},{r::STATE_AR4,0x203},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},{r::STATE_T,0x80ff},{r::STATE_AR3,0x301},{r::STATE_AR4,0x200},{r::STATE_ST0,0xaa5}});
	append("ST-ADD-saturates",0xc131,0x300,1,0x200,0x7fff,
		{{r::STATE_A,0x7fffffff},{r::STATE_B,0},{r::STATE_ST0,0x800},{r::STATE_ST1,0x300},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300}},
		{{r::STATE_B,0x7fffffff},{r::STATE_ST0,0x200,0xe00}});
	append("ST-SUB-saturates",0xc531,0x300,0x8000,0x200,1,
		{{r::STATE_A,0x10000},{r::STATE_B,0},{r::STATE_ST0,0},{r::STATE_ST1,0x300},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300}},
		{{r::STATE_B,0xff80000000ULL},{r::STATE_ST0,0xa00,0xe00}});
}

inline void append_parallel_load_literals(std::vector<clocked_register_case> &cases)
{
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
		auto c = literal_sequence(std::move(initial), observation("pre-literal-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 24, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
	}
}

inline void append_alu_store_literals(std::vector<clocked_register_case> &cases)
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
		// Cold staged entry has not advanced Prefetch's PC after its first clock.
		auto c = literal_sequence(std::move(initial), observation("pre-literal-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 24, 1);
		c.data_events = std::vector<clocked_data_event>{};
		if (v.address) c.data_events->push_back({v.write,v.address,v.memory,0,false});
		cases.push_back(std::move(c));
	}
}

inline void append_high_store_literals(std::vector<clocked_register_case> &cases)
{
	for (unsigned variant = 0; variant != 5; ++variant) {
		bool const b = variant % 2 && variant != 4, ar2 = variant >= 2, circular = variant != 4;
		u16 const opcode = (b ? 0x8300 : 0x8200) | (circular ? 0xd0 : 0x90) | (ar2 ? 2 : 3);
		u16 const address = circular ? 0xf93 : 0xf95, word = b ? 0x8765 : 0x1234;
		auto initial = initial_case("STH-high-half-pointer-and-neighbor-literals", {0xf495,opcode,0xf4e1});
		initial.data = {{address,0},{address - 1,0xbeef},{address + 1,0xcafe}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0x12345678},{r::STATE_B,0xff87654321ULL},
			{r::STATE_AR2,ar2 ? address : 0x3002U},{r::STATE_AR3,ar2 ? 0xf90U : address},
			{r::STATE_BK,4},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100}});
		auto final = observation("only-selected-word-and-pointer-change", {{STATE_GENPC,0x603},
			{r::STATE_A,0x12345678},{r::STATE_B,0xff87654321ULL},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100},
			{r::STATE_BK,4},{r::STATE_AR2,ar2 ? circular ? 0xf90U : 0xf96U : 0x3002U},{r::STATE_AR3,0xf90}},
			{{address,word},{address - 1,0xbeef},{address + 1,0xcafe}});
		// Cold staged entry has not advanced Prefetch's PC after its first clock.
		auto c = literal_sequence(std::move(initial), observation("pre-store-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 24, 1);
		c.data_events = std::vector<clocked_data_event>{{true,address,word,0,false}};
		cases.push_back(std::move(c));
	}
}
}
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (auto append : {
		literal_tables::append_parallel_literals,
		literal_tables::append_parallel_load_literals,
		literal_tables::append_alu_store_literals,
		literal_tables::append_high_store_literals}) append(cases);
	return cases;
}
}
