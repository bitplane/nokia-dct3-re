// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
namespace marker_tables {
inline std::vector<clocked_register_case> port_marked_operations_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 13; ++variant) {
		bool const branch = variant < 6 || variant >= 11;
		bool const taken = branch && (variant == 11 || (variant < 6 && !(variant & 1)));
		bool const arithmetic = variant >= 6 && variant < 10;
		bool const signed_operand = arithmetic && (variant & 1);
		u64 const a = variant == 1 || variant == 11 ? 0x100000000ULL : arithmetic ? 0x10000 : variant == 10 ? 0x12 : 0;
		u64 const b = variant == 4 || variant == 12 ? 0x100000000ULL : variant == 10 ? 0x34 : 0;
		u16 const st0 = variant == 3 ? 0x1000 : variant == 10 ? 0x800 : 0;
		u16 const st1 = signed_operand ? 0x100 : 0;
		u16 const opcode = variant == 11 ? 0xf846 : variant == 12 ? 0xf84d : variant < 2 ? 0xf845 : variant < 4 ? 0xf820 : variant < 6 ? 0xf84c :
			variant < 8 ? 0xf010 : variant < 10 ? 0xf000 : 0xf495;
		std::vector<u16> words{0x75d6,0x124,opcode};
		if (branch || arithmetic) words.push_back(branch ? 0x700 : 0xff80);
		words.insert(words.end(),{0x75d6,0x124,0xf4e1});
		auto initial = marked_program("port-marked-condition-immediate-or-NOP",words);
		initial.program.insert(initial.program.end(),{{0x700,0x75d6},{0x701,0x124},{0x702,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_AR6,0x103},
			{r::STATE_BK,4},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		initial.data = {{0x103,0x1234},{0x100,0x5678}};
		// FF80 is -128 with SXM and +65408 without it; use literal independent results.
		constexpr u64 arithmetic_results[]{0x80,0x10080,0x1ff80,0xff80};
		auto final = observation("selected-continuation-result-and-port-pointer",{{STATE_GENPC,taken ? 0x703U : 0x601+words.size()},
			{r::STATE_A,arithmetic ? arithmetic_results[variant-6] : a},{r::STATE_B,b},{r::STATE_AR6,0x101}});
		if (variant == 10) final.expected.push_back({r::STATE_ST0,0x800,0x800});
		final.expected_data = {{0x103,0x1234},{0x100,0x5678}};
		auto c = marked_sequence(std::move(initial), std::move(final), 48, 3);
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1234,0,false},
			{true,0x124,0x5678,0,false,{},branch ? taken ? 7U : 5U : arithmetic ? 4U : 3U}};
		cases.push_back(std::move(c));
	}
	for (bool call : {true,false}) {
		auto initial = initial_case("port-marked-near-call-or-return",{0xf495,0x75d6,0x124,
			u16(call ? 0xf074 : 0xfc00),u16(call ? 0x700 : 0xffff),0xf4e1});
		initial.program.insert(initial.program.end(),{{0x700,0x75d6},{0x701,0x124},{0x702,u16(call ? 0xfc00 : 0xf4e1)}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_SP,0x300},{r::STATE_AR6,0x103},{r::STATE_BK,4},
			{r::STATE_ST0,0},{r::STATE_ST1,0}});
		initial.data = {{0x103,0x1234},{0x100,0x5678},{0x2ff,0xbeef},{0x300,0x700}};
		auto final = observation("near-continuation-stack-and-port-clock-interval",{{STATE_GENPC,call ? 0x606U : 0x703U},
			{r::STATE_SP,call ? 0x300U : 0x301U},{r::STATE_AR6,0x101}},
			{{0x103,0x1234},{0x100,0x5678},{0x2ff,u16(call ? 0x605 : 0xbeef)},{0x300,0x700}});
		auto c = marked_sequence(std::move(initial), std::move(final), 48, 3);
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1234,0,false},
			{true,0x124,0x5678,0,false,{},call ? 6U : 7U}};
		cases.push_back(std::move(c));
	}
	for (unsigned variant = 0; variant != 7; ++variant) {
		bool const delayed = variant == 2;
		bool const taken = variant == 1 || variant == 2 || variant == 4 || variant == 6;
		u64 const a = variant == 5 ? 0x100000000ULL : 0;
		std::vector<u16> words{0x75d6,0x124,u16(delayed ? 0xfe00 : variant >= 5 ? 0xfc45 : variant >= 3 ? 0xfc20 : 0xfc30)};
		if (delayed) words.insert(words.end(),{0xe801,0xe802});
		words.insert(words.end(),{0x75d6,0x124,0xf4e1});
		auto initial = marked_program("port-marked-conditional-or-delayed-return",words);
		initial.program.insert(initial.program.end(),{{0x700,0x75d6},{0x701,0x124},{0x702,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_SP,0x300},{r::STATE_AR6,0x103},{r::STATE_BK,4},
			{r::STATE_ST0,variant == 1 || variant == 3 ? 0x1000U : 0U},{r::STATE_ST1,0},{r::STATE_A,a}});
		initial.data = {{0x300,0x700},{0x103,0x1234},{0x100,0x5678}};
		auto final = observation("return-selection-delay-effects-stack-and-timing",{{STATE_GENPC,taken ? 0x703U : 0x607U},
			{r::STATE_SP,taken ? 0x301U : 0x300U},{r::STATE_AR6,0x101},{r::STATE_A,delayed ? 2U : a}});
		final.expected_data = initial.data;
		auto c = marked_sequence(std::move(initial), std::move(final), 48, 3);
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1234,0,false},{true,0x124,0x5678,0,false,{},taken ? 7U : 5U}};
		cases.push_back(std::move(c));
	}
	return cases;
}

inline std::vector<clocked_register_case> port_marked_counter_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 8; ++variant) {
		bool const delayed = variant == 2;
		bool const compat = variant < 5;
		bool const taken = variant == 0 || variant == 2 || variant == 3 || variant == 5;
		bool const store = variant == 7;
		u16 const opcode = delayed ? 0x6e88 : variant < 2 ? 0x6c88 : variant < 5 ? 0x6c8b : variant < 7 ? 0xf847 : 0x880e;
		u64 const a = variant == 5 ? 0xff00000000ULL : variant == 6 ? 1 : store ? 0xff12345678ULL : 0;
		std::vector<u16> words{0x75f8,0x100,0x124,opcode};
		if (!store) words.push_back(0x700);
		if (delayed) words.insert(words.end(),{0xf495,0xf495});
		words.insert(words.end(),{0x75f8,0x101,0x124,0xf4e1});
		auto initial = marked_program("port-marked-counter-branch-sign-or-STLM",words);
		initial.program.insert(initial.program.end(),{{0x700,0x75f8},{0x701,0x102},{0x702,0x124},{0x703,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_T,0},{r::STATE_ST0,compat ? 0x8000U : 0U},
			{r::STATE_ST1,compat ? 0x20U : 0U},{r::STATE_AR0,variant == 1 ? 1U : 0U},
			{r::STATE_AR4,variant == 1 ? 0U : 1U},{r::STATE_AR3,variant == 4 ? 0U : 1U}});
		initial.data = {{0x100,0x1111},{0x101,0x2222},{0x102,0x3333}};
		auto final = observation("predecrement-predicate-selected-marker-and-timing",{{STATE_GENPC,taken ? 0x704U : 0x601+words.size()},
			{r::STATE_A,a},{r::STATE_T,store ? 0x5678U : 0U},{r::STATE_AR0,variant == 1 ? 1U : 0U},
			{r::STATE_AR4,variant == 1 ? 0xffffU : variant < 3 ? 0U : 1U},
			{r::STATE_AR3,variant == 3 ? 0U : variant == 4 ? 0xffffU : 1U}});
		final.expected_data = initial.data;
		auto c = marked_sequence(std::move(initial), std::move(final), 48, 3);
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},
			{true,0x124,u16(taken ? 0x3333 : 0x2222),0,false,{},store ? 4U : variant >= 5 ? taken ? 8U : 6U : taken ? 7U : 5U}};
		cases.push_back(std::move(c));
	}
	return cases;
}

inline std::vector<clocked_register_case> port_control_examples_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 6; ++variant) {
		bool const returning = variant < 2, branch = variant == 3 || variant == 4;
		bool const taken = variant == 0 || variant == 3;
		std::vector<u16> words{0x75f8,0x100,0x124};
		if (returning) words.push_back(0xfc4d);
		else if (variant == 2) words.insert(words.end(),{0x47e2,5,0x6d90});
		else if (branch) words.insert(words.end(),{0xf84e,0x700});
		else words.insert(words.end(),{0xec01,0x6d90});
		words.insert(words.end(),{0x75f8,0x101,0x124,0xf4e1});
		auto initial = marked_program("absolute-marked-return-branch-and-repeat",words);
		initial.program.insert(initial.program.end(),{{0x700,0x75f8},{0x701,0x102},{0x702,0x124},{0x703,0xf4e1}});
		u64 const b = variant == 1 || variant == 3 ? 1 : variant == 4 ? 0xff00000000ULL : 0;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_B,b},{r::STATE_ST0,0},{r::STATE_ST1,0},
			{r::STATE_SP,returning ? 0x2ffU : 0x300U},{r::STATE_AR0,variant == 5 ? 0x120U : 0U},{r::STATE_AR2,0x120},{r::STATE_RC,0}});
		initial.data = {{0x100,0x1111},{0x101,0x2222},{0x102,0x3333},{0x2ff,0x700},{0x125,2}};
		auto final = observation("selected-path-stack-repeat-and-port-time",{{STATE_GENPC,taken ? 0x704U : 0x601+words.size()},
			{r::STATE_A,0},{r::STATE_B,b},{r::STATE_SP,returning && !taken ? 0x2ffU : 0x300U},
			{r::STATE_AR0,variant == 2 ? 3U : variant == 5 ? 0x122U : 0U},{r::STATE_AR2,0x120},{r::STATE_RC,0}});
		final.expected_data = initial.data;
		auto c = marked_sequence(std::move(initial), std::move(final), 64, 3);
		// RPT count-Read holds and repeat-tail Prefetch refill belong to the staged schedule.
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,9},
			{true,0x124,u16(taken ? 0x3333 : 0x2222),0,false,{},variant == 2 ? 15U : variant == 5 ? 10U : taken ? 8U : 6U}};
		cases.push_back(std::move(c));
	}
	return cases;
}

inline std::vector<clocked_register_case> port_delayed_branch_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned condition = 0; condition != 3; ++condition)
		for (bool taken : {false,true}) {
			u16 const opcode = condition == 0 ? 0xfa20 : condition == 1 ? 0xfa4d : 0xfa30;
			u16 const st0 = (condition == 0 ? !taken : condition == 2 && taken) ? 0x1000 : 0;
			auto initial = initial_case("delayed-condition-before-slot-effects",{0xf495,0x75f8,0x100,0x124,
				opcode,0x700,0xe801,0xe902,0xe803,0x75f8,0x101,0x124,0xf4e1});
			initial.program.insert(initial.program.end(),{{0x700,0x75f8},{0x701,0x102},{0x702,0x124},{0x703,0xf4e1}});
			initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_B,condition == 1 && !taken ? 1U : 0U},
				{r::STATE_ST0,st0},{r::STATE_ST1,0}});
			initial.data = {{0x100,0x1111},{0x101,0x2222},{0x102,0x3333}};
			auto final = observation("both-slots-execute-before-selected-continuation",{{STATE_GENPC,taken ? 0x704U : 0x60dU},
				{r::STATE_A,taken ? 1U : 3U},{r::STATE_B,2},{r::STATE_ST0,st0},{r::STATE_ST1,0}});
			final.expected_data = initial.data;
			auto c = marked_sequence(std::move(initial), std::move(final), 48, 3);
			c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},
				{true,0x124,u16(taken ? 0x3333 : 0x2222),0,false,{},9}};
			cases.push_back(std::move(c));
		}
	return cases;
}
}
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (auto build : {marker_tables::port_marked_operations_cases,marker_tables::port_marked_counter_cases,marker_tables::port_control_examples_cases,marker_tables::port_delayed_branch_cases}) {
		auto rows = build();
		cases.insert(cases.end(),std::make_move_iterator(rows.begin()),std::make_move_iterator(rows.end()));
	}
	return cases;
}
}
