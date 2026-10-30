// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](char const *name,std::initializer_list<u16> words,std::initializer_list<register_setting> setup,
		std::initializer_list<register_check> checks,u16 address,u16 operand,std::optional<u16> written = {},std::optional<u16> destination = {}) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0xaa5},
			{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},{r::STATE_AR2,0xd00},{r::STATE_AR3,0xd10}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end()); initial.data = {{address,operand}};
		auto final = observation(name,checks); final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		final.expected.insert(final.expected.end(),{{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},
			{r::STATE_AR2,0xd00},{r::STATE_AR3,0xd10}});
		clocked_register_case c{{},1,32}; c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,address,operand,0,false}};
		if (written) {
			u16 const target = destination.value_or(address);
			if (target != address) initial.data.push_back({target,0});
			c.data_events->push_back({true,target,*written,0,false});
			final.expected_data.push_back({target,*written});
		}
		if (!written || destination) final.expected_data.push_back({address,operand});
		c.states = {{0,std::move(initial)},{1,observation("pre-memory-test-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("memory-test-running", {})},{32,std::move(final)}};
		cases.push_back(std::move(c));
	};
	for (unsigned t : {0U,15U}) append("BITT-absolute-bit15-minus-T",{0x34f8,0xd00},{{r::STATE_T,t}},
		{{r::STATE_T,t},{r::STATE_ST0,t ? 0xaa5U : 0x1aa5U}},0xd00,0x8000);
	append("MVKD-absolute-extension-order",{0x70f8,0xd00,0xe00},{},{{r::STATE_ST0,0xaa5}},0xe00,0x4567,0x4567,0xd00);
	append("BITF-absolute",{0x61f8,0xd00,0x40},{},{{r::STATE_ST0,0x1aa5}},0xd00,0x140);
	append("BITF-AR2-unmatched",{0x6182,0x40},{},{{r::STATE_ST0,0xaa5}},0xd00,0x100);
	for (u16 operand : {0x140,0x100}) append("BITF-AR3-only-TC-changes",{0x6183,0x40},{},
		{{r::STATE_ST0,operand == 0x140 ? 0x1aa5U : 0xaa5U}},0xd10,operand);
	append("ANDM-absolute",{0x68f8,0xd00,0xf0},{},{{r::STATE_ST0,0xaa5}},0xd00,0xff0,0xf0);
	append("ORM-absolute",{0x69f8,0xd00,0xf00},{},{{r::STATE_ST0,0xaa5}},0xd00,0xf0,0xff0);
	append("ADDM-absolute",{0x6bf8,0xd00,3},{},{{r::STATE_ST0,0x2a5}},0xd00,0xf0,0xf3);
	return cases;
}
}
