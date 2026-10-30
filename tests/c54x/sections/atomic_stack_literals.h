// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](char const *name, std::initializer_list<u16> words,
		std::initializer_list<register_setting> setup, std::initializer_list<memory_word> data,
		std::initializer_list<register_check> checks, std::initializer_list<memory_word> memory,
		std::vector<clocked_data_event> events) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_SP,0x300}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end()); initial.data = data;
		auto final = observation(name,checks,memory); final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-stack-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("stack-operation-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	append("SSBX-INTM-and-PSHM-AL",{0xf7bb,0x4a08},{{r::STATE_A,0x12345678},{r::STATE_ST1,0}},{{0x2ff,0}},
		{{r::STATE_A,0x12345678},{r::STATE_SP,0x2ff},{r::STATE_ST1,0x800}},{{0x2ff,0x5678}},
		{{false,8,0x5678,0,false},{true,0x2ff,0x5678,0,false}});
	append("POPM-AL-preserves-upper-fields",{0x8a08},{{r::STATE_A,0x12340000},{r::STATE_SP,0x2ff}},{{0x2ff,0x5678}},
		{{r::STATE_A,0x12345678},{r::STATE_SP,0x300}},{{0x2ff,0x5678}},
		{{false,0x2ff,0x5678,0,false},{true,8,0x5678,0,false}});
	append("PSHM-page-zero-pair",{0x4a60,0x4a61},{{r::STATE_A,0x12345678}},{{0x60,0x1234},{0x61,0xabcd},{0x2ff,0},{0x2fe,0}},
		{{r::STATE_A,0x12345678},{r::STATE_SP,0x2fe}},{{0x60,0x1234},{0x61,0xabcd},{0x2ff,0x1234},{0x2fe,0xabcd}},
		{{false,0x60,0x1234,0,false},{true,0x2ff,0x1234,0,false},{false,0x61,0xabcd,0,false},{true,0x2fe,0xabcd,0,false}});
	append("POPM-page-zero-reverse-pair",{0x8a61,0x8a60},{{r::STATE_A,0x12345678},{r::STATE_SP,0x2fe}},
		{{0x60,0},{0x61,0},{0x2fe,0xabcd},{0x2ff,0x1234}},{{r::STATE_A,0x12345678},{r::STATE_SP,0x300}},
		{{0x60,0x1234},{0x61,0xabcd}},{{false,0x2fe,0xabcd,0,false},{true,0x61,0xabcd,0,false},
		{false,0x2ff,0x1234,0,false},{true,0x60,0x1234,0,false}});
	append("RSBX-INTM",{0xf6bb},{{r::STATE_ST1,0x800}},{},{{r::STATE_ST1,0},{r::STATE_SP,0x300}},{},{});
	append("PSHM-AH-then-eight-bit-AG",{0x4a09,0x4a0a},{{r::STATE_A,0x123456789a}},{{0x2ff,0},{0x2fe,0}},
		{{r::STATE_A,0x123456789a},{r::STATE_SP,0x2fe}},{{0x2ff,0x3456},{0x2fe,0x12}},
		{{false,9,0x3456,0,false},{true,0x2ff,0x3456,0,false},{false,10,0x12,0,false},{true,0x2fe,0x12,0,false}});
	append("POPM-AG-AH-retains-AL",{0x8a0a,0x8a09},{{r::STATE_A,0xabcd987654},{r::STATE_SP,0x2fe}},
		{{0x2fe,0x12},{0x2ff,0x3456}},{{r::STATE_A,0x1234567654},{r::STATE_SP,0x300}},{},
		{{false,0x2fe,0x12,0,false},{true,10,0x12,0,false},{false,0x2ff,0x3456,0,false},{true,9,0x3456,0,false}});
	append("PSHM-BL-BH-BG",{0x4a0b,0x4a0c,0x4a0d},{{r::STATE_B,0x7f1234abcd}},{{0x2ff,0},{0x2fe,0},{0x2fd,0}},
		{{r::STATE_B,0x7f1234abcd},{r::STATE_SP,0x2fd}},{{0x2ff,0xabcd},{0x2fe,0x1234},{0x2fd,0x7f}},
		{{false,11,0xabcd,0,false},{true,0x2ff,0xabcd,0,false},{false,12,0x1234,0,false},
		{true,0x2fe,0x1234,0,false},{false,13,0x7f,0,false},{true,0x2fd,0x7f,0,false}});
	append("POPM-BG-BH-BL-preserves-A",{0x8a0d,0x8a0c,0x8a0b},
		{{r::STATE_B,0x5511223344},{r::STATE_A,0x1234567654},{r::STATE_SP,0x2fd}},{{0x2fd,0x7f},{0x2fe,0x1234},{0x2ff,0xabcd}},
		{{r::STATE_B,0x7f1234abcd},{r::STATE_A,0x1234567654},{r::STATE_SP,0x300}},{},
		{{false,0x2fd,0x7f,0,false},{true,13,0x7f,0,false},{false,0x2fe,0x1234,0,false},
		{true,12,0x1234,0,false},{false,0x2ff,0xabcd,0,false},{true,11,0xabcd,0,false}});
	return cases;
}
}
