// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
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
		auto initial = initial_case("atomic-port-marked-condition-immediate-or-NOP",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
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
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-first-port",{{STATE_GENPC,0x601}})},
			{2,observation("marked-operation-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false;
		c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1234,0,false},
			{true,0x124,0x5678,0,false,{},branch ? taken ? 7U : 5U : arithmetic ? 4U : 3U}};
		cases.push_back(std::move(c));
	}
	for (bool call : {true,false}) {
		auto initial = initial_case("atomic-port-marked-near-call-or-return",{0xf495,0x75d6,0x124,
			u16(call ? 0xf074 : 0xfc00),u16(call ? 0x700 : 0xffff),0xf4e1});
		initial.program.insert(initial.program.end(),{{0x700,0x75d6},{0x701,0x124},{0x702,u16(call ? 0xfc00 : 0xf4e1)}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_SP,0x300},{r::STATE_AR6,0x103},{r::STATE_BK,4},
			{r::STATE_ST0,0},{r::STATE_ST1,0}});
		initial.data = {{0x103,0x1234},{0x100,0x5678},{0x2ff,0xbeef},{0x300,0x700}};
		auto final = observation("near-continuation-stack-and-port-clock-interval",{{STATE_GENPC,call ? 0x606U : 0x703U},
			{r::STATE_SP,call ? 0x300U : 0x301U},{r::STATE_AR6,0x101}},
			{{0x103,0x1234},{0x100,0x5678},{0x2ff,u16(call ? 0x605 : 0xbeef)},{0x300,0x700}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-call-markers",{{STATE_GENPC,0x601}})},
			{2,observation("call-markers-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
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
		auto initial = initial_case("atomic-port-marked-conditional-or-delayed-return",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.program.insert(initial.program.end(),{{0x700,0x75d6},{0x701,0x124},{0x702,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_SP,0x300},{r::STATE_AR6,0x103},{r::STATE_BK,4},
			{r::STATE_ST0,variant == 1 || variant == 3 ? 0x1000U : 0U},{r::STATE_ST1,0},{r::STATE_A,a}});
		initial.data = {{0x300,0x700},{0x103,0x1234},{0x100,0x5678}};
		auto final = observation("return-selection-delay-effects-stack-and-timing",{{STATE_GENPC,taken ? 0x703U : 0x607U},
			{r::STATE_SP,taken ? 0x301U : 0x300U},{r::STATE_AR6,0x101},{r::STATE_A,delayed ? 2U : a}});
		final.expected_data = initial.data;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-return-markers",{{STATE_GENPC,0x601}})},
			{2,observation("return-markers-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1234,0,false},{true,0x124,0x5678,0,false,{},taken ? 7U : 5U}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
