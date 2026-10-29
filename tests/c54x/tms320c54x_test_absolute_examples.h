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
inline auto const &absolute_operand_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, std::vector<u16> words, std::vector<register_setting> initial,
			std::vector<register_check> expected, std::vector<memory_word> data = {},
			unsigned interval = 4, std::vector<memory_word> expected_data = {}) {
			auto test = port_case(id, std::move(words), std::move(initial), std::move(expected),
				std::move(data), interval, std::move(expected_data), 2, 0x0d00);
			test.reset_before = result.empty();
			result.push_back(std::move(test));
		};
		add("literal-load-b-retains-a", {0xe908}, {{r::STATE_A, 0x123456}, {r::STATE_B, 0xffffffffffULL}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_B, 8}, {r::STATE_A, 0x123456}});
		add("mmr-load-ar4-b-unsigned", {0x4914}, {{r::STATE_AR4, 0x8001}, {r::STATE_B, 0}},
			{{r::STATE_B, 0x8001}, {r::STATE_AR4, 0x8001}, {r::STATE_A, 0x123456}});
		add("logical-right-b-carry", {0xf3ff}, {{r::STATE_B, 0xff80000003ULL}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_B, 0x40000001}, {r::STATE_A, 0x123456}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("compare-absolute-literal", {0x60f8, 0x0f24, 0x1234}, {{r::STATE_ST0, 0x0800}},
			{{r::STATE_ST0, 0x1800, 0x1800}}, {{0x0f24, 0x1234}}, 6);
		add("subtract-signed-postincrement", {0x0892}, {{r::STATE_A, 0}, {r::STATE_AR2, 0x0f20}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 1}, {r::STATE_AR2, 0x0f21}}, {{0x0f20, 0xffff}});
		add("store-low-retains-source-pointer", {0x8082}, {{r::STATE_A, 0xabcd1234ULL}, {r::STATE_AR2, 0x0f30}},
			{{r::STATE_A, 0xabcd1234ULL}, {r::STATE_AR2, 0x0f30}}, {{0x0f30, 0}}, 4, {{0x0f30, 0x1234}});
		add("load-signed-circular-wrap", {0x10d2}, {{r::STATE_BK, 4}, {r::STATE_A, 0}, {r::STATE_AR2, 0x0f23}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xffffff8001ULL}, {r::STATE_AR2, 0x0f20}}, {{0x0f23, 0x8001}});
		add("dual-copy-y-postincrement", {0xe50b}, {{r::STATE_AR2, 0x0f20}, {r::STATE_AR5, 0x0f33}},
			{{r::STATE_AR2, 0x0f20}, {r::STATE_AR5, 0x0f34}}, {{0x0f20, 0x5a3c}, {0x0f33, 0}}, 4, {{0x0f33, 0x5a3c}});
		add("load-signed-stationary", {0x1083}, {{r::STATE_A, 0}, {r::STATE_AR3, 0x0f35}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xffffff8002ULL}, {r::STATE_AR3, 0x0f35}}, {{0x0f35, 0x8002}});
		add("load-unsigned-stationary", {0x1284}, {{r::STATE_A, 0}, {r::STATE_AR4, 0x0f36}},
			{{r::STATE_A, 0x8002}, {r::STATE_AR4, 0x0f36}}, {{0x0f36, 0x8002}});
		add("mapped-t-store-from-b", {0x890e}, {{r::STATE_B, 0x12345678}, {r::STATE_T, 0}},
			{{r::STATE_T, 0x5678}, {r::STATE_B, 0x12345678}});
		add("logical-right-cross-carry", {0xf1f4}, {{r::STATE_A, 0x12345e78}, {r::STATE_B, 0xffffffff}, {r::STATE_ST0, 0}},
			{{r::STATE_B, 0x12345}, {r::STATE_A, 0x12345e78}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("register-and-preserves-b", {0xf280}, {{r::STATE_A, 0xf0f0}, {r::STATE_B, 0x0ff0}},
			{{r::STATE_A, 0x00f0}, {r::STATE_B, 0x0ff0}});
		add("literal-xor-preserves-a", {0xf350, 0x00ff}, {{r::STATE_A, 0xabcd}, {r::STATE_B, 0x12345678}},
			{{r::STATE_B, 0x12345687}, {r::STATE_A, 0xabcd}}, {}, 5);
		add("copy-circular-source-wrap", {0x71d2, 0x0f40}, {{r::STATE_BK, 4}, {r::STATE_AR2, 0x0f23}},
			{{r::STATE_AR2, 0x0f20}}, {{0x0f23, 0x5a3c}, {0x0f40, 0}}, 5, {{0x0f40, 0x5a3c}});
		add("signed-add-stationary", {0x0083}, {{r::STATE_A, 1}, {r::STATE_AR3, 0x0f52}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xffffffffffULL}, {r::STATE_AR3, 0x0f52}}, {{0x0f52, 0xfffe}});
		add("mapped-move-auxiliary", {0xe726}, {{r::STATE_AR2, 0x4567}, {r::STATE_AR6, 0}},
			{{r::STATE_AR2, 0x4567}, {r::STATE_AR6, 0x4567}});
		add("timer-control-store-readback", {0x7726, 0x0010, 0x4826}, {{r::STATE_A, 0}}, {{r::STATE_A, 0x0010}}, {}, 8);
		add("fractional-mac-negative", {0x2883}, {{r::STATE_A, 5}, {r::STATE_AR3, 0x0f52}, {r::STATE_T, 0xfffe}, {r::STATE_ST1, 0x0040}},
			{{r::STATE_A, 0xfffffffff9ULL}, {r::STATE_AR3, 0x0f52}, {r::STATE_T, 0xfffe}}, {{0x0f52, 3}});
		add("fractional-dual-multiply", {0xa43a}, {{r::STATE_A, 0}, {r::STATE_AR5, 0x0f60}, {r::STATE_AR4, 0x0f70}},
			{{r::STATE_A, 0xfffffffff4ULL}, {r::STATE_T, 0xfffe}, {r::STATE_AR5, 0x0f60}, {r::STATE_AR4, 0x0f71}}, {{0x0f60, 0xfffe}, {0x0f70, 3}});
		add("fractional-square-distance", {0xe201}, {{r::STATE_A, 0x00020000}, {r::STATE_B, 3}, {r::STATE_AR2, 0x0f80}, {r::STATE_AR3, 0x0f81}, {r::STATE_ST1, 0x0140}},
			{{r::STATE_A, 0xfffffe0000ULL}, {r::STATE_B, 11}, {r::STATE_AR2, 0x0f80}, {r::STATE_AR3, 0x0f81}}, {{0x0f80, 0xffff}, {0x0f81, 1}});
		add("rounded-multiply-negative-carry", {0x2282}, {{r::STATE_A, 0}, {r::STATE_T, 0xfffd}, {r::STATE_AR2, 0x0f90}, {r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0xffffff0000ULL}, {r::STATE_T, 0xfffd}, {r::STATE_AR2, 0x0f90}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f90, 0x4000}});
		add("rounded-mac-after-accumulation", {0x2a82}, {{r::STATE_A, 0x4000}, {r::STATE_T, 1}},
			{{r::STATE_A, 0x10000}, {r::STATE_T, 1}, {r::STATE_AR2, 0x0f90}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("rounded-mas-negative-carry", {0x2e82}, {{r::STATE_A, 0}, {r::STATE_T, 3}},
			{{r::STATE_A, 0xffffff0000ULL}, {r::STATE_T, 3}, {r::STATE_AR2, 0x0f90}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("fractional-unsigned-multiply", {0x2482}, {{r::STATE_A, 0}, {r::STATE_T, 0xffff}, {r::STATE_ST1, 0x0040}},
			{{r::STATE_A, 0x1fffc0002ULL}, {r::STATE_T, 0xffff}, {r::STATE_AR2, 0x0f90}}, {{0x0f90, 0xffff}});
		add("load-high-ignores-ovm", {0x4482}, {{r::STATE_A, 0}, {r::STATE_AR2, 0x0f90}, {r::STATE_ST0, 0x0400}, {r::STATE_ST1, 0x0200}},
			{{r::STATE_A, 0x80000000}, {r::STATE_AR2, 0x0f90}, {r::STATE_ST0, 0x0400, 0x0600}}, {{0x0f90, 0x8000}});
		add("mapped-ar5-load-a-unsigned", {0x4815}, {{r::STATE_A, 0xff12345678ULL}, {r::STATE_AR5, 0x8001}, {r::STATE_ST0, 0x0400}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0x8001}, {r::STATE_AR5, 0x8001}, {r::STATE_ST0, 0x0400}});
		add("mapped-ar5-load-b-unsigned", {0x4915}, {{r::STATE_B, 0xff12345678ULL}},
			{{r::STATE_B, 0x8001}, {r::STATE_A, 0x8001}, {r::STATE_AR5, 0x8001}, {r::STATE_ST0, 0x0400}});
		add("indirect-mmr-address-mask", {0x4882}, {{r::STATE_A, 0}, {r::STATE_AR2, 0xff15}, {r::STATE_AR5, 0x8001}},
			{{r::STATE_A, 0x8001}, {r::STATE_AR2, 0x0015}});
		add("indirect-mmr-store-postincrement", {0x8892}, {{r::STATE_A, 0x3456}, {r::STATE_AR2, 0xff17}, {r::STATE_AR7, 0}},
			{{r::STATE_AR7, 0x3456}, {r::STATE_AR2, 0x0018}});
		return result;
	}();
	return cases;
}

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

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_ABSOLUTE_EXAMPLES_H
