// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_DUAL_REFERENCE_H
#define MAME_NOKIA_TMS320C54X_TEST_DUAL_REFERENCE_H

#include "tms320c54x_test_port_case.h"

namespace c54x_test {

// Literal signed/circular controls without marker instructions. Partial setup
// remains ordered; only the standalone runner applies the entry reset metadata.
inline auto const &dual_address_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, u16 opcode, std::vector<register_setting> initial,
			std::vector<register_check> expected, std::vector<memory_word> data,
			std::vector<memory_word> expected_data = {}) {
			instruction_case test{id, opcode, {{0x05e0, opcode}, {0x05e1, 0xf5e1}},
				std::move(data), std::move(initial), std::move(expected), 0, 0, std::move(expected_data)};
			test.initial.insert(test.initial.end(), {{r::STATE_PC, 0x05e0}, {r::STATE_IDLE, 0}});
			test.reset_before = result.empty();
			result.push_back(std::move(test));
		};
		add("dual-mac-negative-x", 0xb03a,
			{{r::STATE_A, 10}, {r::STATE_B, 20}, {r::STATE_AR5, 0x0b00}, {r::STATE_AR4, 0x0c00}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 4}, {r::STATE_B, 20}, {r::STATE_T, 0xfffe}, {r::STATE_AR4, 0x0c01}, {r::STATE_AR5, 0x0b00}},
			{{0x0b00, 0xfffe}, {0x0c00, 3}});
		add("dual-mac-b-circular-y", 0xb3be,
			{{r::STATE_AR0, 1}, {r::STATE_AR4, 0x0c03}, {r::STATE_AR5, 0x0b00}, {r::STATE_B, 20}},
			{{r::STATE_B, 8}, {r::STATE_T, 0xfffd}, {r::STATE_AR4, 0x0c00}, {r::STATE_AR5, 0x0b01}},
			{{0x0b00, 0xfffd}, {0x0c03, 4}});
		add("dual-mac-a-circular-y", 0xb0be,
			{{r::STATE_AR0, 1}, {r::STATE_AR4, 0x0c03}, {r::STATE_AR5, 0x0b00}, {r::STATE_BK, 4},
			 {r::STATE_A, 20}, {r::STATE_B, 9}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 8}, {r::STATE_B, 9}, {r::STATE_T, 0xfffd}, {r::STATE_AR4, 0x0c00}, {r::STATE_AR5, 0x0b01}},
			{{0x0b00, 0xfffd}, {0x0c03, 4}});
		add("sqdst-circular-y", 0xe2e4,
			{{r::STATE_A, 0x30000}, {r::STATE_B, 10}, {r::STATE_AR0, 1}, {r::STATE_AR4, 0x0c03},
			 {r::STATE_AR2, 0x0d00}, {r::STATE_BK, 4}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xfffffe0000ULL}, {r::STATE_B, 19}, {r::STATE_AR4, 0x0c00}, {r::STATE_AR2, 0x0cff}},
			{{0x0c03, 5}, {0x0d00, 7}});
		add("sqdst-negative-x-two-decrements", 0xe245,
			{{r::STATE_A, 0x30000}, {r::STATE_B, 10}, {r::STATE_T, 0x2222}, {r::STATE_AR2, 0x0c10}, {r::STATE_AR3, 0x0d10}},
			{{r::STATE_A, 0xfffff40000ULL}, {r::STATE_B, 19}, {r::STATE_T, 0x2222}, {r::STATE_AR2, 0x0c0f}, {r::STATE_AR3, 0x0d0f}},
			{{0x0c10, 0xfffb}, {0x0d10, 7}}, {{0x0c10, 0xfffb}, {0x0d10, 7}});
		add("sqdst-negative-x-fract", 0xe249,
			{{r::STATE_A, 0x30000}, {r::STATE_B, 10}, {r::STATE_AR2, 0x0c10}, {r::STATE_AR3, 0x0d10}, {r::STATE_ST1, 0x0140}},
			{{r::STATE_A, 0xfffff40000ULL}, {r::STATE_B, 28}, {r::STATE_T, 0x2222}, {r::STATE_AR2, 0x0c0f}, {r::STATE_AR3, 0x0d11}},
			{}, {{0x0c10, 0xfffb}, {0x0d10, 7}});
		add("dual-mpy-negative-x-circular-y", 0xa5be,
			{{r::STATE_AR0, 1}, {r::STATE_AR4, 0x0c03}, {r::STATE_AR5, 0x0b00}, {r::STATE_BK, 4}, {r::STATE_ST1, 0}},
			{{r::STATE_B, 0xfffffffffaULL}, {r::STATE_T, 0xfffe}, {r::STATE_AR4, 0x0c00}, {r::STATE_AR5, 0x0b01}},
			{{0x0b00, 0xfffe}, {0x0c03, 3}});
		add("dual-macr-b-decrement-y", 0xb736,
			{{r::STATE_AR5, 0x0b00}, {r::STATE_AR4, 0x0c03}, {r::STATE_B, 0x8000}},
			{{r::STATE_B, 0x10000}, {r::STATE_T, 3}, {r::STATE_AR4, 0x0c02}, {r::STATE_AR5, 0x0b00}},
			{{0x0b00, 3}, {0x0c03, 4}});
		add("dual-macr-negative-x-circular-y", 0xb43e,
			{{r::STATE_A, 0x8008}, {r::STATE_B, 0x12345678}, {r::STATE_AR0, 1}, {r::STATE_AR5, 0x0b00},
			 {r::STATE_AR4, 0x0c03}, {r::STATE_BK, 4}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0x10000}, {r::STATE_B, 0x12345678}, {r::STATE_T, 0xfffe}, {r::STATE_AR5, 0x0b00}, {r::STATE_AR4, 0x0c00}},
			{{0x0b00, 0xfffe}, {0x0c03, 4}}, {{0x0b00, 0xfffe}, {0x0c03, 4}});
		add("st-b-macr-negative-y-circular", 0xd6e1,
			{{r::STATE_A, 0x8008}, {r::STATE_B, 0x12345678}, {r::STATE_T, 4}, {r::STATE_AR0, 1},
			 {r::STATE_AR4, 0x0c03}, {r::STATE_AR3, 0x0d00}, {r::STATE_BK, 4}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0x10000}, {r::STATE_B, 0x12345678}, {r::STATE_T, 4}, {r::STATE_AR4, 0x0c00}, {r::STATE_AR3, 0x0d00}},
			{{0x0c03, 0xfffe}, {0x0d00, 0}}, {{0x0d00, 0x1234}});
		add("st-b-macr-negative-y-two-pointers", 0xd6e9,
			{{r::STATE_A, 0x8008}, {r::STATE_AR4, 0x0c03}},
			{{r::STATE_A, 0x10000}, {r::STATE_B, 0x12345678}, {r::STATE_T, 4}, {r::STATE_AR4, 0x0c00}, {r::STATE_AR3, 0x0d01}},
			{{0x0c03, 0xfffe}, {0x0d00, 0}}, {{0x0d00, 0x1234}, {0x0c03, 0xfffe}});
		result.front().reset_initial = {{r::STATE_BK, 4}};
		return result;
	}();
	return cases;
}


} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_DUAL_REFERENCE_H
