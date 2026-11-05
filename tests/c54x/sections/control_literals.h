// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
namespace literal_tables {
inline void append_conditional_control_examples(std::vector<clocked_register_case> &cases)
{
	for (unsigned family = 0; family < 6; ++family) for (unsigned taken = 0; taken < 2; ++taken) {
		bool const returning = family == 0, call = family == 2 || family == 3, delayed = family == 1 || family >= 4;
		u16 const opcode = std::array<u16,6>{0xfc4b,0xfa45,0xf947,0xf930,0x6e8f,0x6e82}[family];
		u64 const negative = 0xff00000000ULL;
		u64 const a = family == 1 ? !taken : family == 2 ? (taken ? negative : 1) : family == 3 ? 1 : 0x55;
		u64 const b = returning ? (taken ? negative : 1) : family == 3 ? 2 : 0x12345678;
		u16 const st0 = family == 3 ? 0x800 | (taken << 12) : 0;
		unsigned const words = delayed ? 4 : returning ? 1 : 2;
		auto initial = initial_case("conditional-control", {opcode});
		if (!returning) initial.program[1].value = 0x700;
		if (delayed) { initial.program[2].value = 0xe801; initial.program[3].value = 0xe802; }
		initial.program[words].value = 0xf4e1;
		initial.program.push_back({0x700,0xf4e1}); initial.program.push_back({0x5ff,0xf495});
		initial.data = {{0x3000,0x700},{0x2fff,0xbeef}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_SP,0x3000},{r::STATE_ST0,st0},{r::STATE_ST1,0x100},
			{r::STATE_AR2,family == 5 ? taken : 0x1200U},{r::STATE_AR7,family == 4 ? taken : 0x1234U}});
		auto final = observation("retained-predicate-and-owned-stack-effect", {{STATE_GENPC,taken ? 0x701U : 0x601 + words},
			{r::STATE_A,delayed ? 2 : a},{r::STATE_B,b},{r::STATE_ST0,st0},{r::STATE_ST1,0x100},
			{r::STATE_SP,0x3000U + (returning ? taken : 0) - (call ? taken : 0)},
			{r::STATE_AR2,family == 5 ? taken : 0x1200U},{r::STATE_AR7,family == 4 ? u16(taken - 1) : 0x1234U}},
			{{0x2fff,call && taken ? u16(0x602) : u16(0xbeef)}});
		auto c = literal_sequence(std::move(initial), observation("pre-control-snapshot", {{STATE_GENPC,0x5ff}}), std::move(final), 32, 1);
		c.data_events = std::vector<clocked_data_event>{};
		if (taken && returning) c.data_events->push_back({false,0x3000,0x700,0,false});
		if (taken && call) c.data_events->push_back({true,0x2fff,0x602,0,false});
		cases.push_back(std::move(c));
	}
}

inline void append_conditional_slot_literals(std::vector<clocked_register_case> &cases)
{
	for (unsigned kind = 0; kind != 4; ++kind)
	for (bool taken : {false,true}) {
		u16 const opcode = std::array<u16,4>{0xf843,0xff30,0xff45,0xff4e}[kind];
		u64 const a = kind == 0 ? taken ? 0xff00000000ULL : 0x0100000000ULL : kind == 2 ? taken ? 0 : 1 : 0x1234;
		u64 const b = kind == 3 ? taken ? 0x0100000000ULL : 0xff00000000ULL : 0x7654;
		u16 const st0 = kind == 1 && taken ? 0x1000 : 0;
		auto initial = initial_case("BC-ALT-and-XC-TC-AEQ-BGT-literals", {0xf495,opcode});
		if (kind == 0) {
			initial.program[2].value = 0x700; initial.program[3].value = 0xf4e1;
			initial.program.push_back({0x700,0xf4e1});
		} else {
			initial.program[2].value = 0x6d91; initial.program[3].value = 0x6d92; initial.program[4].value = 0xf4e1;
		}
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,st0},
			{r::STATE_ST1,0},{r::STATE_AR1,0x120},{r::STATE_AR2,0x130}});
		auto final = observation("taken-target-or-both-guarded-slots", {{STATE_GENPC,kind == 0 ? taken ? 0x701U : 0x604U : 0x605U},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,st0},{r::STATE_ST1,0},
			{r::STATE_AR1,kind && taken ? 0x121U : 0x120U},{r::STATE_AR2,kind && taken ? 0x131U : 0x130U}});
		auto c = literal_sequence(std::move(initial), observation("pre-condition-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 32, 1);
		c.data_events = std::vector<clocked_data_event>{};
		if (kind == 0 && taken) c.program_events = {{false,0x700,0xf4e1,0,false}};
		cases.push_back(std::move(c));
	}
	for (bool pmst : {false,true}) {
		auto initial = initial_case("load-MAR-stores-or-STM-PMST-literal", {0xf495});
		std::vector<u16> const words = pmst ? std::vector<u16>{0x771d,0x4567} : std::vector<u16>{0x1192,0x6dd2,0x7683,0x5aa5,0x8085};
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x1234abcd},{r::STATE_B,0},{r::STATE_AR2,0xf22},
			{r::STATE_AR3,0xf30},{r::STATE_AR4,0},{r::STATE_AR5,pmst ? 0xf13U : 0xf31U},
			{r::STATE_BK,4},{r::STATE_ST0,0x800},{r::STATE_ST1,pmst ? 0U : 0x100U}});
		if (!pmst) initial.data = {{0xf22,0x8000},{0xf30,0},{0xf31,0}};
		auto final = observation("literal-copy-state-or-direct-PMST-write", {{STATE_GENPC,0x602 + words.size()},
			{r::STATE_A,0x1234abcd},{r::STATE_B,pmst ? 0 : 0xffffff8000ULL},{r::STATE_PMST,pmst ? 0x4567U : 0U},
			{r::STATE_ST0,0x800},{r::STATE_ST1,pmst ? 0U : 0x100U},{r::STATE_BK,4},
			{r::STATE_AR2,pmst ? 0xf22U : 0xf20U},{r::STATE_AR3,0xf30},{r::STATE_AR4,0},{r::STATE_AR5,pmst ? 0xf13U : 0xf31U}});
		if (!pmst) final.expected_data = {{0xf22,0x8000},{0xf30,0x5aa5},{0xf31,0xabcd}};
		auto c = literal_sequence(std::move(initial), observation("pre-operation-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 32, 1);
		c.data_events = pmst ? std::vector<clocked_data_event>{{true,0x1d,0x4567,0,false}} :
			std::vector<clocked_data_event>{{false,0xf22,0x8000,0,false},{true,0xf30,0x5aa5,0,false},{true,0xf31,0xabcd,0,false}};
		cases.push_back(std::move(c));
	}
}

inline void append_fast_return_examples(std::vector<clocked_register_case> &cases)
{
	auto initial = initial_case("CALL-publishes-restorable-RTN", {0xf495});
	initial.program = {{0x2067f,0xf495},{0x20680,0xf074},{0x20681,0x700},{0x20682,0xf4e1},
		{0x20700,0xf4e1},{0x20701,0xf49b},{0x30780,0xf074},{0x30781,0x700},{0x30700,0xf4e1}};
	initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x2067f},{r::STATE_SP,0x400}});
	initial.data = {{0x3ff,0xbeef}};
	clocked_register_case c{{{0,std::move(initial)},
		{12,observation("callee-paused-with-CALL-frame", {{STATE_GENPC,0x20701},{r::STATE_SP,0x3ff},{r::STATE_RTN,0x682}},{{0x3ff,0x682}})},
		{13,observation("fast-return-running", {})},
		{28,observation("RETF-uses-RTN-not-stack-sentinel", {{STATE_GENPC,0x20683},{r::STATE_SP,0x400},{r::STATE_RTN,0x682}},{{0x3ff,0xdead}})},
		{29,observation("second-real-CALL-running", {})},
		{48,observation("second-CALL-overwrites-saved-RTN", {{STATE_GENPC,0x30701},{r::STATE_SP,0x3ff},{r::STATE_RTN,0x782}},{{0x3ff,0x782}})}},12,48};
	// Snapshot after the first CALL, with the stack poisoned and RETF ready.
	// Execute a second real CALL before restore so replay must recover the old latch.
	// RETF is present before Prefetch; waking IDLE must not rely on code patching.
	c.actions = {{12,{{r::STATE_IDLE,0}},false,{}, {},{{0x3ff,0xdead}}},
		{28,{{STATE_GENPC,0x30780},{r::STATE_IDLE,0}}}};
	c.data_events = std::vector<clocked_data_event>{{true,0x3ff,0x682,0,false},{true,0x3ff,0x782,0,false}};
	cases.push_back(std::move(c));
}
}
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (auto append : {
		literal_tables::append_conditional_control_examples,
		literal_tables::append_conditional_slot_literals,
		literal_tables::append_fast_return_examples}) append(cases);
	return cases;
}
}
