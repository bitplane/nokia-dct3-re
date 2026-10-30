// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_ABSOLUTE_EXAMPLES_H
#define MAME_NOKIA_TMS320C54X_TEST_ABSOLUTE_EXAMPLES_H

#include "tms320c54x_test_port_case.h"

namespace c54x_test {

// Literal atomic references with absolute PORTW timing markers. Partial seeds
// are intentional: paired outcomes retain their checked predecessor's status.
inline auto const &absolute_arithmetic_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, u16 opcode, std::vector<register_setting> initial,
			std::vector<register_check> expected, std::vector<memory_word> data = {},
			std::initializer_list<u16> extension = {}, unsigned interval = 5) {
			std::vector<u16> words{opcode};
			words.insert(words.end(), extension.begin(), extension.end());
			auto test = port_case(id, std::move(words), std::move(initial), std::move(expected),
				std::move(data), interval, {}, 2, 0x0d00);
			test.reset_before = result.empty();
			result.push_back(std::move(test));
		};
		add("atomic-subs-absolute-carry", 0x0af8,
			{{r::STATE_A, 0x10000}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 1}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f64, 0xffff}}, {0x0f64});
		add("atomic-subs-absolute-borrow", 0x0af8, {{r::STATE_A, 0}},
			{{r::STATE_A, 0xffffff0001ULL}, {r::STATE_ST0, 0, 0x0800}}, {}, {0x0f64});
		for (unsigned reg : {2U, 4U})
			for (unsigned outcome = 0; outcome < 2; ++outcome)
			{
				bool const ar4 = reg == 4, success = !outcome;
				std::vector<register_setting> initial{{r::STATE_A, success ? 0x8000 : 0x7fff}};
				if (success) {
					if (ar4) initial.push_back({r::STATE_B, 0x12345678});
					initial.insert(initial.end(), {{r::STATE_AR0 + reg, ar4 ? 0x0f66 : 0x0f65}, {r::STATE_ST0, 0}});
				}
				std::vector<register_check> expected{{r::STATE_A, success ? 1ULL : 0xfffeULL},
					{r::STATE_ST0, success ? 0x0800ULL : 0, 0x0800}, {int(r::STATE_AR0 + reg), ar4 ? 0x0f66ULL : 0x0f65ULL}};
				if (ar4) expected.push_back({r::STATE_B, 0x12345678});
				add(ar4 ? (success ? "atomic-subc-ar4-success" : "atomic-subc-ar4-failure") :
					(success ? "atomic-subc-ar2-success" : "atomic-subc-ar2-failure"), 0x1e80 | reg,
					std::move(initial), std::move(expected), success ? std::vector<memory_word>{{ar4 ? 0x0f66 : 0x0f65, 1}} : std::vector<memory_word>{}, {}, 4);
				if (ar4) result.back().expected_data = {{0x0f66, 1}};
			}
		add("atomic-mar-preoffset-negative", 0x6dea, {{r::STATE_AR2, 0x1000}},
			{{r::STATE_AR2, 0x0ff9}}, {}, {0xfff9});
		add("atomic-add-immediate-b-signed", 0xf300,
			{{r::STATE_B, 0}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_B, 0xffffffffffULL}, {r::STATE_ST0, 0, 0x0800}}, {}, {0xffff});
		add("atomic-add-immediate-b-carry", 0xf300, {{r::STATE_B, 0xffffffffULL}},
			{{r::STATE_B, 0x100000000ULL}, {r::STATE_ST0, 0x0800, 0x0800}}, {}, {1});
		for (unsigned outcome = 0; outcome < 2; ++outcome)
		{
			std::vector<register_setting> initial = outcome ? std::vector<register_setting>{{r::STATE_B, 0}} :
				std::vector<register_setting>{{r::STATE_A, 0x1234}, {r::STATE_B, 10}, {r::STATE_ST0, 0}};
			add(outcome ? "atomic-sub-immediate-cross-borrow" : "atomic-sub-immediate-cross-carry", 0xf210,
				std::move(initial), {{r::STATE_A, outcome ? 0xffffffffffULL : 8},
				 {r::STATE_B, outcome ? 0ULL : 10}, {r::STATE_ST0, outcome ? 0ULL : 0x0800, 0x0800}}, {}, {u16(outcome ? 1 : 2)});
		}
		add("atomic-xor-immediate-cross", 0xf150,
			{{r::STATE_A, 0xff12345678ULL}, {r::STATE_B, 0}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0xff12345678ULL}, {r::STATE_B, 0xff1234a978ULL}, {r::STATE_ST0, 0x0800, 0x0800}}, {}, {0xff00});
		return result;
	}();
	return cases;
}

// Absolute timing markers do not borrow an auxiliary pointer from the case.
// The legacy adapter keeps intervening branch/call/return checks in place.

inline auto const &long_offset_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, std::vector<u16> words, std::vector<register_setting> initial,
			std::vector<register_check> expected, std::vector<memory_word> data = {},
			unsigned interval = 5, std::vector<memory_word> expected_data = {}) {
			auto test = port_case(id, std::move(words), std::move(initial), std::move(expected),
				std::move(data), interval, std::move(expected_data), 2, 0x0d00);
			test.reset_before = result.empty();
			result.push_back(std::move(test));
		};
		add("addc-long-preupdate-clear-carry", {0x06ea, 5}, {{r::STATE_A, 0x13}, {r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0x18}, {r::STATE_AR2, 0x0f05}, {r::STATE_ST0, 0, 0x0800}}, {{0x0f05, 4}});
		add("adds-long-offset-stationary", {0x02e2, 5}, {{r::STATE_A, 1}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0x10000}, {r::STATE_AR2, 0x0f00}}, {{0x0f05, 0xffff}});
		add("adds-long-circular-preupdate", {0x02f2, 2}, {{r::STATE_A, 1}, {r::STATE_AR2, 0x0f03}, {r::STATE_BK, 4}},
			{{r::STATE_A, 4}, {r::STATE_AR2, 0x0f01}}, {{0x0f01, 3}});
		add("store-low-long-negative-preupdate", {0x80ea, 0xfffe}, {{r::STATE_A, 0x12345678}, {r::STATE_AR2, 0x0f10}},
			{{r::STATE_AR2, 0x0f0e}}, {{0x0f0e, 0}}, 5, {{0x0f0e, 0x5678}});
		add("addm-long-offset-before-immediate", {0x6bea, 5, 1}, {{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 3}}, 6, {{0x0f05, 4}});
		add("andm-immediate-before-long-offset", {0x68e2, 0x0f0f, 5}, {{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f05, 0xff00}}, 6, {{0x0f05, 0x0f00}});
		add("orm-long-circular-retains-carry", {0x69f2, 0x8000, 2}, {{r::STATE_AR2, 0x0f03}, {r::STATE_BK, 4}},
			{{r::STATE_AR2, 0x0f01}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f01, 1}}, 6, {{0x0f01, 0x8001}});
		add("store-long-offset-before-literal", {0x76ea, 5, 0xcafe}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0}}, 6, {{0x0f05, 0xcafe}});
		add("store-high-long-preupdate", {0x82ea, 5}, {{r::STATE_A, 0x12345678}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0}}, 5, {{0x0f05, 0x1234}});
		add("store-high-b-long-stationary", {0x83e2, 5}, {{r::STATE_B, 0x56789abc}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f00}}, {{0x0f05, 0}}, 5, {{0x0f05, 0x5678}});
		add("store-t-long-preupdate", {0x8cea, 5}, {{r::STATE_T, 0x4321}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0}}, 5, {{0x0f05, 0x4321}});
		add("load-signed-long-preupdate", {0x10ea, 5}, {{r::STATE_ST1, 0x0100}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0xffffff8001ULL}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x8001}});
		add("load-unsigned-long-stationary", {0x12e2, 5}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0x8001}, {r::STATE_AR2, 0x0f00}});
		add("load-high-b-long-preupdate", {0x45ea, 5}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_B, 0x12340000}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x1234}});
		result.front().reset_initial = {{r::STATE_ST1, 0x0100}};
		return result;
	}();
	return cases;
}

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
