// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_ABSOLUTE_EXAMPLES_H
#define MAME_NOKIA_TMS320C54X_TEST_ABSOLUTE_EXAMPLES_H

#include "tms320c54x_test_port_case.h"

namespace c54x_test {

inline auto const &memory_route_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, std::vector<u16> words, std::vector<register_setting> initial,
			std::vector<register_check> expected, std::vector<memory_word> data = {},
			unsigned interval = 5, std::vector<memory_word> expected_data = {}) {
			auto test = port_case(id, std::move(words), std::move(initial), std::move(expected),
				std::move(data), interval, std::move(expected_data), 2, 0x0d00);
			test.reset_before = false;
			result.push_back(std::move(test));
		};
		// Each segment retains partial seeds and its checked predecessor state.
		// Reset metadata supplies only external context to the standalone runner.
		add("bitf-long-preupdate-retains-carry", {0x61ea, 5, 0x8000}, {{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_ST0, 0x1800, 0x1800}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x8001}}, 6);
		add("cmpm-immediate-before-offset", {0x60e2, 0x1235, 5}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_ST0, 0x0800, 0x1800}, {r::STATE_AR2, 0x0f00}}, {{0x0f05, 0x1234}}, 6);
		add("bitf-ar0-absent-bit", {0x6180, 1}, {{r::STATE_AR0, 0x0f20}, {r::STATE_ST0, 0x1800}},
			{{r::STATE_ST0, 0x0800, 0x1800}, {r::STATE_AR0, 0x0f20}}, {{0x0f20, 4}});
		add("mvmm-ar3-to-ar6", {0xe736}, {{r::STATE_AR3, 0x1357}, {r::STATE_AR6, 0}},
			{{r::STATE_AR6, 0x1357}, {r::STATE_AR3, 0x1357}}, {}, 4);
		add("push-long-preupdated-source", {0x4bea, 5}, {{r::STATE_SP, 0x0300}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_SP, 0x02ff}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x1234}, {0x02ff, 0}}, 5, {{0x02ff, 0x1234}});
		add("pop-long-preupdated-destination", {0x8bea, 5}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_SP, 0x0300}, {r::STATE_AR2, 0x0f05}}, {{0x02ff, 0xbeef}, {0x0f05, 0}}, 5, {{0x0f05, 0xbeef}});
		add("double-store-b-long-high-low", {0x4fea, 6}, {{r::STATE_B, 0x12345678}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f06}}, {{0x0f06, 0}, {0x0f07, 0}}, 6, {{0x0f06, 0x1234}, {0x0f07, 0x5678}});
		add("double-load-b-long-signed", {0x57e2, 6}, {{r::STATE_B, 0}, {r::STATE_AR2, 0x0f00}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_B, 0xff80012345ULL}, {r::STATE_AR2, 0x0f00}}, {{0x0f06, 0x8001}, {0x0f07, 0x2345}});
		add("sub-high-long-preupdated-source", {0x40ea, 5}, {{r::STATE_A, 0x30000}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0x20000}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 1}});
		add("bitt-long-stationary-source", {0x34e2, 5}, {{r::STATE_T, 0}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_ST0, 0x1000, 0x1000}, {r::STATE_AR2, 0x0f00}}, {{0x0f05, 0x8000}});
		add("multiply-long-preupdated-source", {0x20ea, 5}, {{r::STATE_T, 2}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 6}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 3}});
		add("load-ar2-unsigned-with-sxm", {0x1282}, {{r::STATE_A, 0}, {r::STATE_AR2, 0x0f00}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0x8001}, {r::STATE_AR2, 0x0f00}}, {{0x0f00, 0x8001}}, 4);
		add("load-ar2-signed-same-source", {0x1082}, {{r::STATE_A, 0}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0xffffff8001ULL}, {r::STATE_AR2, 0x0f00}}, {}, 4);
		add("store-bl-to-ar4", {0x8914}, {{r::STATE_B, 0x5678cafe}, {r::STATE_AR4, 0}},
			{{r::STATE_AR4, 0xcafe}}, {}, 4);
		add("mvdk-offset-before-destination", {0x71ea, 5, 0x0f20}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0xbeef}, {0x0f20, 0}}, 6, {{0x0f20, 0xbeef}});
		add("mvkd-offset-before-source", {0x70ea, 5, 0x0f20}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f20, 0x5678}, {0x0f05, 0}}, 6, {{0x0f05, 0x5678}});
		add("reada-long-preupdated-destination", {0x7eea, 5}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0}}, 9, {{0x0f05, 0xabcd}});
		result.back().program.emplace_back(0x0600, 0xabcd);
		add("mvpd-program-address-before-offset", {0x7cea, 0x0600, 5}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0}}, 7, {{0x0f05, 0xbeef}});
		result.back().program.emplace_back(0x0600, 0xbeef);
		add("load-shift-zero-offset-before-operation", {0x6fea, 5, 0x0c40}, {{r::STATE_A, 0}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0x2345}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x2345}}, 6);
		add("mvdd-ar2-to-ar5-postincrement", {0xe58b}, {{r::STATE_AR2, 0x0d20}, {r::STATE_AR5, 0x0e20}},
			{{r::STATE_AR2, 0x0d21}, {r::STATE_AR5, 0x0e21}}, {{0x0d20, 0x4321}, {0x0e20, 0}}, 4, {{0x0e20, 0x4321}});
		add("ldu-b-ar2-circular-wrap", {0x13d2}, {{r::STATE_BK, 4}, {r::STATE_B, 0}, {r::STATE_AR2, 0x0f23}},
			{{r::STATE_B, 0x8001}, {r::STATE_AR2, 0x0f20}}, {{0x0f23, 0x8001}}, 4);
		add("and-ar2-stationary-source", {0x1882}, {{r::STATE_A, 0x1234}, {r::STATE_AR2, 0x0f20}},
			{{r::STATE_A, 0x0204}, {r::STATE_AR2, 0x0f20}}, {{0x0f20, 0x0f0f}}, 4);
		add("load-ar1-signed-stationary-source", {0x1081}, {{r::STATE_A, 0}, {r::STATE_AR1, 0x0f20}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xffffff8001ULL}, {r::STATE_AR1, 0x0f20}}, {{0x0f20, 0x8001}}, 4);
		add("frame-increment-crosses-page", {0xee01}, {{r::STATE_ST1, 0x0100}, {r::STATE_SP, 0x03ff}}, {{r::STATE_SP, 0x0400}}, {}, 4);
		add("or-immediate-b-retains-upper-bits", {0xf340, 0x00f0}, {{r::STATE_B, 0xab00123400ULL}},
			{{r::STATE_B, 0xab001234f0ULL}});
		add("and-immediate-b-to-a-retains-b", {0xf230, 0x0ff0}, {{r::STATE_A, 0x123456}, {r::STATE_B, 0xffabcdf123ULL}},
			{{r::STATE_A, 0x0120}, {r::STATE_B, 0xffabcdf123ULL}});
		for (unsigned index : {0, 4, 13, 15, 16, 18, 23})
		{
			result[index].reset_before = true;
			result[index].reset_initial = {{r::STATE_ST1, index == 4 ? 0 : 0x0100}};
		}
		result[16].reset_initial.emplace_back(r::STATE_A, 0x0600);
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_ABSOLUTE_EXAMPLES_H
