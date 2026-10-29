// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_PORT_CASE_H
#define MAME_NOKIA_TMS320C54X_TEST_PORT_CASE_H

#include "tms320c54x_test_case.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <stdexcept>
#include <utility>

namespace c54x_test {

// Synthetic PORTW markers bracket literal instructions. This constructs only
// stimulus/entry data; arithmetic expectations and reset groups stay explicit.
inline instruction_case port_case(const char *id, std::vector<u16> words,
	std::vector<register_setting> initial, std::vector<register_check> expected,
	std::vector<memory_word> data = {}, unsigned interval = 3,
	std::vector<memory_word> expected_data = {}, unsigned writes = 2,
	std::optional<u16> marker_source = {})
{
	if (words.empty()) throw std::invalid_argument("C54x marked case has no instruction words");
	instruction_case test{id, words.front(), {{0x05e0, 0x75d6}, {0x05e1, 0x0124}},
		std::move(data), std::move(initial), std::move(expected), writes, interval, std::move(expected_data)};
	if (marker_source) test.program = {{0x05e0, 0x75f8}, {0x05e1, *marker_source}, {0x05e2, 0x0124}};
	unsigned pc = marker_source ? 0x05e3 : 0x05e2;
	for (auto word : words) test.program.emplace_back(pc++, word);
	test.program.emplace_back(pc++, marker_source ? 0x75f8 : 0x75d6);
	if (marker_source) test.program.emplace_back(pc++, *marker_source);
	for (u16 word : {0x0124, 0xf5e1}) test.program.emplace_back(pc++, word);
	test.initial.insert(test.initial.end(), {{tms320c54x_device::STATE_PC, 0x05e0}, {tms320c54x_device::STATE_IDLE, 0}});
	return test;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_PORT_CASE_H
