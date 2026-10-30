// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include "tms320c54x_test_word_examples.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (auto const &vector : instruction_word_examples()) {
		auto initial = initial_case(vector.id,{vector.opcode});
		initial.program = vector.program; initial.data = vector.data;
		initial.initial.insert(initial.initial.end(),vector.initial.begin(),vector.initial.end());
		auto final = observation(vector.id,{});
		final.expected = vector.expected; final.expected_data = vector.expected_data;
		clocked_register_case c{{{0,std::move(initial)},{40,std::move(final)}},cases.size() & 1 ? 5U : 1U,40};
		c.pipeline_frontend = false;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
