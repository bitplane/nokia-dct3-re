// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_MEMORY_EXAMPLES_H
#define MAME_NOKIA_TMS320C54X_TEST_MEMORY_EXAMPLES_H

#include "tms320c54x_test_port_case.h"
#include <utility>

namespace c54x_test {

// Ordered literal reference examples, not a generated operand census.
// Partial setup and carry-only masks are intentional: later MPY/SUB cases
// observe state left by their checked predecessors. No reset between rows.
inline auto const &memory_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, u16 opcode, std::vector<register_setting> initial,
			std::vector<register_check> expected, std::vector<memory_word> data = {},
			unsigned cycles = 3, std::vector<memory_word> expected_data = {},
			std::initializer_list<u16> extension = {}) {
			std::vector<u16> words{opcode};
			words.insert(words.end(), extension.begin(), extension.end());
			auto test = port_case(id, std::move(words), std::move(initial), std::move(expected), std::move(data), cycles, std::move(expected_data));
			test.reset_before = result.empty();
			result.push_back(std::move(test));
		};
		add("subc-b-preserves-a", 0x1f83,
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x10000}, {r::STATE_AR3, 0x0f90}, {r::STATE_ST0, 0}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x10001}, {r::STATE_ST0, 0x0800, 0x0800}, {r::STATE_AR3, 0x0f90}}, {{0x0f90, 1}});
		add("dadst-offset-preincrement", 0x5aea,
			{{r::STATE_A, 0}, {r::STATE_T, 0x2345}, {r::STATE_AR2, 0x0f00}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0x3879579b}, {r::STATE_T, 0x2345}, {r::STATE_AR2, 0x0f06}},
			{{0x0f06, 0x1534}, {0x0f07, 0x3456}}, 4, {}, {6});
		add("subb-a-borrow-in", 0x0e83, {{r::STATE_A, 6}, {r::STATE_AR3, 0x0f90}, {r::STATE_ST0, 0}},
			{{r::STATE_A, 0xffffffffffULL}, {r::STATE_ST0, 0, 0x0800}, {r::STATE_AR3, 0x0f90}}, {{0x0f90, 6}});
		add("subb-b-no-borrow", 0x0f83,
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0xff80000006ULL}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0xff80000000ULL}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("add-b-carry-bit32", 0x0183,
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0xffffffffULL}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x100000000ULL}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f90, 1}});
		add("subs-b-unsigned", 0x0b83, {{r::STATE_B, 0x10000}, {r::STATE_ST0, 0}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 1}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f90, 0xffff}});
		add("and-b-preserves-carry", 0x1983, {{r::STATE_B, 0x123456}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_B, 0x56}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f90, 0x00ff}});
		add("mpy-b-preserves-carry", 0x2183,
			{{r::STATE_B, 0}, {r::STATE_T, 0xfffe}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0xfffffffffaULL}, {r::STATE_T, 0xfffe}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f90, 3}});
		struct multiply_case { const char *id; u16 opcode, t, memory, t_after; u64 a_before, b_before, a_after, b_after; u16 ar2_after = 0x0f90, st1 = 0x0100; };
		static constexpr multiply_case cases[] = {
			{ "mpyr-b-round", 0x2383, 0x4000, 3,      0x4000, 0x1234, 0,       0x1234, 0x10000 },
			{ "mpyu-b-unsigned-source", 0x2583, 0xfffe, 3,      0xfffe, 0x1234, 0,       0x1234, 0x2fffa },
			{ "squr-b-load-t", 0x2783, 0x4000, 0xfffe, 0xfffe, 0x1234, 0,       0x1234, 4 },
			{ "mac-b-signed-product", 0x2983, 0xfffe, 3,      0xfffe, 0x1234, 10,      0x1234, 4 },
			{ "macr-b-round", 0x2b83, 0x4000, 3,      0x4000, 0x1234, 0x10000, 0x1234, 0x20000 },
			{ "mas-a-retains-b", 0x2c83, 2,      3,      2,      10,     0x5678,  4,      0x5678 },
			{ "mas-b-retains-a", 0x2d83, 2,      3,      2,      0x1234, 10,      0x1234, 4 },
			{ "masr-a-round", 0x2e83, 0x4000, 3,      0x4000, 0x30000, 0x5678, 0x20000, 0x5678 },
			{ "masr-b-round", 0x2f83, 0x4000, 3,      0x4000, 0x1234, 0x30000, 0x1234, 0x20000 },
			{ "mpy-ar2-to-a", 0x2082, 0xfffe, 3,      0xfffe, 0,      0x5678, 0xfffffffffaULL, 0x5678 },
			{ "mpy-ar2-postdecrement-to-b", 0x218a, 0xfffe, 3,      0xfffe, 0x1234, 0,      0x1234, 0xfffffffffaULL, 0x0f8f },
			{ "mac-ar2-to-a", 0x2882, 0xfffe, 3,      0xfffe, 5,      0x5678, 0xffffffffffULL, 0x5678 },
			{ "mac-ar2-fractional", 0x2882, 0xfffe, 3,      0xfffe, 5,      0x5678, 0xfffffffff9ULL, 0x5678, 0x0f90, 0x0140 },
			{ "mas-ar2-postincrement-to-b", 0x2d92, 2,      3,      2,      0x1234, 10,     0x1234, 4, 0x0f91 }
		};
		for (unsigned i = 0; i < std::size(cases); ++i)
		{
			auto const &v = cases[i];
			std::vector<register_setting> initial{{r::STATE_A, v.a_before}, {r::STATE_B, v.b_before}, {r::STATE_T, v.t},
				{r::STATE_ST0, 0x0800}, {r::STATE_ST1, v.st1}, {r::STATE_AR3, 0x0f90}};
			if (i) initial.push_back({r::STATE_AR2, 0x0f90});
			std::vector<register_check> expected{{r::STATE_A, v.a_after}, {r::STATE_B, v.b_after}, {r::STATE_T, v.t_after},
				{r::STATE_ST0, 0x0800, 0x0800}, {r::STATE_AR3, 0x0f90}, {r::STATE_ST1, v.st1}};
			if (i >= 9) expected.push_back({r::STATE_AR2, v.ar2_after});
			add(v.id, v.opcode, std::move(initial), std::move(expected), {{0x0f90, v.memory}});
		}
		struct acca_case { const char *id; u16 opcode, memory, t_before, t_after, ar3_after; u64 a_before, b_before, a_after, b_after; };
		static constexpr acca_case acca[] = {
			{ "masa-acca-source", 0x3383, 3,      0,      3,      0x0f90, 0x20000,    10,      0x20000, 4 },
			{ "maca-acca-source", 0x3583, 3,      0,      3,      0x0f90, 0x20000,    10,      0x20000, 16 },
			{ "macar-acca-round", 0x3783, 3,      0,      3,      0x0f90, 0x40000000, 0x10000, 0x40000000, 0x20000 },
			{ "poly-postincrement-retains-t", 0x3693, 0x2000, 0x5678, 0x5678, 0x0f91, 0x12340000, 0x10000, 0x06270000, 0x20000000 }
		};
		for (unsigned i = 0; i < std::size(acca); ++i)
		{
			auto const &v = acca[i];
			std::vector<register_setting> initial{{r::STATE_A, v.a_before}, {r::STATE_B, v.b_before}, {r::STATE_T, v.t_before}};
			if (i) initial.insert(initial.end(), {{r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0x0100}, {r::STATE_AR3, 0x0f90}});
			add(v.id, v.opcode, std::move(initial),
				{{r::STATE_A, v.a_after}, {r::STATE_B, v.b_after}, {r::STATE_T, v.t_after}, {r::STATE_AR3, v.ar3_after},
				 {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f90, v.memory}});
		}
		struct shifted_case { const char *id; u16 opcode, memory, t_before, t_after; u64 a_before, b_before, a_after, b_after; };
		static constexpr shifted_case shifted[] = {
			{ "load-t-stationary", 0x3083, 0xabcd, 0, 0xabcd, 0x1234, 0x5678, 0x1234, 0x5678 },
			{ "square-add-b", 0x3983, 0xfffe, 0, 0xfffe, 0x1234, 10, 0x1234, 14 },
			{ "square-sub-a", 0x3a83, 3, 0, 3, 10, 0x5678, 1, 0x5678 },
			{ "add-shift16-to-a", 0x3c83, 0xfffe, 0, 0, 0x30000, 0x5678, 0x10000, 0x5678 },
			{ "add-shift16-a-to-b", 0x3d83, 2, 0, 0, 1, 0x5678, 1, 0x20001 },
			{ "add-shift16-b-to-a", 0x3e83, 0xfffe, 0, 0, 0x1234, 0x30000, 0x10000, 0x30000 },
			{ "add-shift16-to-b", 0x3f83, 2, 0, 0, 0x1234, 1, 0x1234, 0x20001 },
			{ "add-shift16-ar2-postincrement-a", 0x3c92, 0xfffe, 0, 0, 0x30000, 0x5678, 0x10000, 0x5678 },
			{ "add-shift16-ar2-postincrement-b", 0x3f92, 2, 0, 0, 0x1234, 1, 0x1234, 0x20001 },
			{ "add-shift16-ar2-stationary-b", 0x3f82, 2, 0, 0, 0x1234, 1, 0x1234, 0x20001 },
			{ "add-shift16-ar2-not-decoy", 0x3c82, 0xfffe, 0, 0, 0x30000, 0x5678, 0x10000, 0x5678 },
			{ "load-t-ar2-postdecrement", 0x308a, 0xabcd, 0x1234, 0xabcd, 0x1234, 0x5678, 0x1234, 0x5678 }
		};
		for (unsigned i = 0; i < std::size(shifted); ++i)
		{
			auto const &v = shifted[i];
			std::vector<register_setting> initial{{r::STATE_A, v.a_before}, {r::STATE_B, v.b_before}, {r::STATE_T, v.t_before},
				{r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0x0100}, {r::STATE_AR3, 0x0f90}};
			if (i) initial.push_back({r::STATE_AR2, 0x0f92});
			std::vector<memory_word> data{{0x0f90, u16(i >= 10 ? 2 : v.memory)}};
			if (i) data.emplace_back(0x0f92, v.memory);
			std::vector<register_check> expected{{r::STATE_A, v.a_after}, {r::STATE_B, v.b_after}, {r::STATE_T, v.t_after},
				{r::STATE_AR3, 0x0f90}, {r::STATE_ST0, 0x0800, 0x0800}};
			if (i >= 7) expected.push_back({r::STATE_AR2, i == 11 ? 0x0f91U : i < 9 ? 0x0f93U : 0x0f92U});
			std::vector<memory_word> expected_data;
			if (i >= 10) expected_data = {{0x0f92, v.memory}, {0x0f90, 2}};
			add(v.id, v.opcode, std::move(initial), std::move(expected), std::move(data), 3, std::move(expected_data));
		}
		struct shifted_sub_case { const char *id; u16 opcode, st0_before, st0_after; u64 a_before, b_before, a_after, b_after; };
		static constexpr shifted_sub_case subtract[] = {
			{ "sub-shift16-retains-clear-carry", 0x4083, 0,      0,      0x30000, 0x5678, 0x10000,       0x5678 },
			{ "sub-shift16-a-to-b", 0x4183, 0x0800, 0x0800, 0x30000, 0x5678, 0x30000,       0x10000 },
			{ "sub-shift16-borrow-to-a", 0x4283, 0x0800, 0,      0x1234,  0x10000, 0xffffff0000, 0x10000 },
			{ "sub-shift16-b-retains-clear-carry", 0x4383, 0,      0,      0x1234,  0x30000, 0x1234,        0x10000 },
			{ "sub-shift16-ar2-not-decoy", 0x4082, 0x0800, 0x0800, 0x40000, 0x5678, 0x10000,       0x5678 }
		};
		for (unsigned i = 0; i < std::size(subtract); ++i)
		{
			auto const &v = subtract[i];
			std::vector<register_setting> initial{{r::STATE_A, v.a_before}, {r::STATE_B, v.b_before}, {r::STATE_T, 0x9abc},
				{r::STATE_ST0, v.st0_before}, {r::STATE_ST1, 0x0100}, {r::STATE_AR3, 0x0f90}};
			if (i) initial.push_back({r::STATE_AR2, 0x0f92});
			std::vector<register_check> expected{{r::STATE_A, v.a_after}, {r::STATE_B, v.b_after}, {r::STATE_T, 0x9abc},
				{r::STATE_AR3, 0x0f90}, {r::STATE_ST0, v.st0_after, 0x0800}};
			std::vector<memory_word> data{{0x0f90, 2}}, expected_data;
			if (i) data.emplace_back(0x0f92, 3);
			if (i == 4) { expected.push_back({r::STATE_AR2, 0x0f92}); expected_data = {{0x0f92, 3}}; }
			add(v.id, v.opcode, std::move(initial), std::move(expected), std::move(data), 3, std::move(expected_data));
		}
		add("sub-shift16-ar2-postincrement", 0x4392,
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x30000}, {r::STATE_ST0, 0}, {r::STATE_AR2, 0x0f92}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x10000}, {r::STATE_T, 0x9abc}, {r::STATE_AR2, 0x0f93},
			 {r::STATE_AR3, 0x0f90}, {r::STATE_ST0, 0, 0x0a00}}, {{0x0f92, 2}}, 3, {{0x0f92, 2}});
		add("sub-shift16-ar2-postdecrement", 0x438a, {{r::STATE_B, 0x30000}, {r::STATE_AR2, 0x0f92}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x10000}, {r::STATE_T, 0x9abc}, {r::STATE_AR2, 0x0f91},
			 {r::STATE_ST0, 0, 0x0a00}}, {}, 3, {{0x0f92, 2}});
		// Standalone entry reproduces the checked long-word predecessor's
		// C16 mode and circular markers; legacy preparation does not apply it.
		result.front().reset_initial = {{r::STATE_AR6, 0x0a01}, {r::STATE_BK, 4}, {r::STATE_ST1, 0x0180}};
		return result;
	}();
	return cases;
}

// Literal transfer/stack sequence. Partial seeds intentionally retain the
// prior row's RAM, status, pointers and accumulator slices.



inline auto const &memory_address_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, bool absolute, std::vector<u16> words, unsigned interval,
			std::vector<register_setting> initial, std::vector<register_check> expected,
			std::vector<memory_word> data = {}, std::vector<memory_word> expected_data = {}) {
			auto test = port_case(id, std::move(words), std::move(initial), std::move(expected),
				std::move(data), interval, std::move(expected_data), 2,
				absolute ? std::optional<u16>(0x0d00) : std::nullopt);
			test.reset_before = false;
			result.push_back(std::move(test));
		};
		add("dual-move-bk5-source-wrap", false, {0xe5c9}, 3,
			{{r::STATE_AR0, 1}, {r::STATE_AR2, 0x0e04}, {r::STATE_AR3, 0x0e10}, {r::STATE_AR6, 0x0a03}, {r::STATE_BK, 5}},
			{{r::STATE_AR2, 0x0e00}, {r::STATE_AR3, 0x0e11}}, {{0x0e04, 0x2468}, {0x0e10, 0}}, {{0x0e10, 0x2468}});
		add("dual-move-ar4-bk5-preserves-state", false, {0xe5e9}, 3,
			{{r::STATE_AR4, 0x0e04}, {r::STATE_AR3, 0x0e12}, {r::STATE_A, 0x12345678}, {r::STATE_B, 0x87654321}, {r::STATE_ST0, 0x1800}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_AR4, 0x0e00}, {r::STATE_AR3, 0x0e13}, {r::STATE_AR2, 0x0e00}, {r::STATE_AR0, 1}, {r::STATE_A, 0x12345678}, {r::STATE_B, 0x87654321}, {r::STATE_ST0, 0x1800}},
			{{0x0e04, 0x1357}, {0x0e12, 0}}, {{0x0e12, 0x1357}, {0x0e04, 0x1357}});
		add("unsigned-ar3-load-clears-guard", false, {0x1293}, 3,
			{{r::STATE_A, 0xffabcdef00ULL}, {r::STATE_AR3, 0x0ef0}, {r::STATE_AR6, 0x0a03}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xff80}, {r::STATE_AR3, 0x0ef1}}, {{0x0ef0, 0xff80}});
		add("byte-immediate-zero-clears-accumulator", false, {0xe800}, 3,
			{{r::STATE_A, 0xff12345678ULL}, {r::STATE_AR6, 0x0a03}}, {{r::STATE_A, 0}});
		add("absolute-double-store-high-low", false, {0x4ef8, 0x0f00}, 5,
			{{r::STATE_A, 0xffabcd1234ULL}, {r::STATE_AR6, 0x0a03}}, {}, {{0x0f00, 0}, {0x0f01, 0}}, {{0x0f00, 0xabcd}, {0x0f01, 0x1234}});
		add("odd-double-store-decrements-two", false, {0x4e8b}, 4,
			{{r::STATE_A, 0x00abcd1234ULL}, {r::STATE_AR3, 0x0f11}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_AR3, 0x0f0f}}, {{0x0f10, 0}, {0x0f11, 0}}, {{0x0f11, 0xabcd}, {0x0f10, 0x1234}});
		add("odd-double-load-sign-extends-high", false, {0x5783}, 3,
			{{r::STATE_B, 0}, {r::STATE_AR3, 0x0f21}, {r::STATE_AR6, 0x0a03}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_B, 0xff80015678ULL}, {r::STATE_AR3, 0x0f21}}, {{0x0f20, 0x5678}, {0x0f21, 0x8001}});
		add("signed-ar2-load-postincrement", false, {0x1092}, 3,
			{{r::STATE_A, 0}, {r::STATE_AR2, 0x0f20}, {r::STATE_AR6, 0x0a03}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xffffff8001ULL}, {r::STATE_AR2, 0x0f21}}, {{0x0f20, 0x8001}});
		add("store-low-ar1-postincrement", false, {0x8091}, 3,
			{{r::STATE_A, 0x12345678}, {r::STATE_AR1, 0x0f20}, {r::STATE_AR6, 0x0a03}},
			{{r::STATE_AR1, 0x0f21}}, {{0x0f20, 0}}, {{0x0f20, 0x5678}});
		add("signed-ar6-load-with-absolute-markers", true, {0x1086}, 4,
			{{r::STATE_A, 0}, {r::STATE_ST1, 0x0100}, {r::STATE_AR6, 0x0f20}},
			{{r::STATE_A, 0xffffffff80ULL}, {r::STATE_AR6, 0x0f20}}, {{0x0d00, 0x1234}, {0x0f20, 0xff80}});
		add("extended-signed-load-ar3-postdecrement", true, {0x6f8b, 0x0c40}, 5,
			{{r::STATE_A, 0}, {r::STATE_ST1, 0x0100}, {r::STATE_AR3, 0x0f21}},
			{{r::STATE_A, 0xffffffff80ULL}, {r::STATE_AR3, 0x0f20}}, {{0x0f21, 0xff80}});
		add("subtract-signed-immediate-from-b", true, {0xf310, 0xff80}, 5,
			{{r::STATE_A, 0x1234}, {r::STATE_B, 5}, {r::STATE_ST1, 0x0100}}, {{r::STATE_B, 133}, {r::STATE_A, 0x1234}});
		add("mar-ar2-decrements-once", true, {0x6d8a}, 4,
			{{r::STATE_AR2, 0x0f21}, {r::STATE_ST1, 0}}, {{r::STATE_AR2, 0x0f20}});
		add("absolute-signed-load-b-preserves-a", true, {0x11f8, 0x0f21}, 5,
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0}, {r::STATE_ST1, 0x0100}}, {{r::STATE_B, 0xffffffff80ULL}, {r::STATE_A, 0x1234}}, {{0x0f21, 0xff80}});
		add("subtract-signed-immediate-a-to-b", true, {0xf110, 0xff80}, 5,
			{{r::STATE_A, 5}, {r::STATE_B, 0}, {r::STATE_ST1, 0x0100}}, {{r::STATE_B, 133}, {r::STATE_A, 5}});
		add("absolute-double-load-a-high-low", true, {0x56f8, 0x0f20}, 5,
			{{r::STATE_A, 0}, {r::STATE_ST1, 0x0100}}, {{r::STATE_A, 0xffff801234ULL}}, {{0x0f20, 0xff80}, {0x0f21, 0x1234}});
		add("repeat-fourteen-adds-fifteen-times", false, {0xec0e, 0x0082}, 18,
			{{r::STATE_A, 0}, {r::STATE_AR2, 0x0f21}, {r::STATE_AR6, 0x0a03}}, {{r::STATE_A, 15}, {r::STATE_AR2, 0x0f21}}, {{0x0f21, 1}});
		add("mvmm-ar5-to-ar3", false, {0xe753}, 3,
			{{r::STATE_AR5, 0x1234}, {r::STATE_AR3, 0}, {r::STATE_AR6, 0x0a03}}, {{r::STATE_AR3, 0x1234}, {r::STATE_AR5, 0x1234}});
		add("mvmm-ar2-to-ar3-retains-ar5", false, {0xe723}, 3,
			{{r::STATE_AR2, 0xabcd}, {r::STATE_AR3, 0}}, {{r::STATE_AR2, 0xabcd}, {r::STATE_AR3, 0xabcd}, {r::STATE_AR5, 0x1234}});
		add("and-high-literal-preserves-b-carry", true, {0xf063, 0x0ff0}, 5,
			{{r::STATE_A, 0xff12345678ULL}, {r::STATE_B, 0x1234}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0x02300000}, {r::STATE_B, 0x1234}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("store-low-b-ar4-stationary", true, {0x8184}, 4,
			{{r::STATE_B, 0x12345678}, {r::STATE_AR4, 0x0f21}, {r::STATE_ST1, 0}}, {{r::STATE_AR4, 0x0f21}}, {{0x0f21, 0}}, {{0x0f21, 0x5678}});
		add("store-low-b-ar0-preserves-neighbor", true, {0x8180}, 4,
			{{r::STATE_A, 0xffabcd1234ULL}, {r::STATE_AR0, 0x0f23}, {r::STATE_ST0, 0x1800}},
			{{r::STATE_AR0, 0x0f23}, {r::STATE_AR4, 0x0f21}, {r::STATE_A, 0xffabcd1234ULL}, {r::STATE_B, 0x12345678}, {r::STATE_ST0, 0x1800}},
			{{0x0f23, 0}, {0x0f24, 0xcafe}}, {{0x0f23, 0x5678}, {0x0f24, 0xcafe}});
		add("immediate-al-store-preserves-high-guard", true, {0x7708, 0xbeef}, 5, {},
			{{r::STATE_A, 0xffabcdbeefULL}, {r::STATE_B, 0x12345678}, {r::STATE_ST0, 0x1800}, {r::STATE_PC, 0x05e9}});
		add("load-high-literal-b-signed", true, {0xf162, 0xff80}, 5,
			{{r::STATE_B, 0}, {r::STATE_ST1, 0x0100}}, {{r::STATE_B, 0xffff800000ULL}});
		add("mar-ar6-postincrement-absolute-markers", true, {0x6d96}, 4,
			{{r::STATE_AR6, 0x0f42}, {r::STATE_ST1, 0}}, {{r::STATE_AR6, 0x0f43}});
		add("mar-ar5-retains-ar6-and-memory", true, {0x6d95}, 4,
			{{r::STATE_AR5, 0x0f50}}, {{r::STATE_AR5, 0x0f51}, {r::STATE_AR6, 0x0f43}}, {{0x0f50, 0xbeef}}, {{0x0f50, 0xbeef}});
		add("mvmm-ar1-to-ar2-preserves-source", true, {0xe712}, 4,
			{{r::STATE_AR1, 0x1a23}, {r::STATE_AR2, 0}}, {{r::STATE_AR2, 0x1a23}, {r::STATE_AR1, 0x1a23}});
		add("mvmm-ar1-to-ar3-preserves-source", true, {0xe713}, 4,
			{{r::STATE_AR3, 0}}, {{r::STATE_AR3, 0x1a23}, {r::STATE_AR1, 0x1a23}});
		// BK=5 survives these segments in the legacy sequence. Reset context
		// is standalone-only; the final segment inherits CMPT from BANZD.
		for (unsigned index : {0, 2, 3, 4, 7, 8, 9, 11, 13, 15, 16, 19})
		{
			result[index].reset_before = true;
			result[index].reset_initial = {{r::STATE_BK, 5}, {r::STATE_ST1, index == 0 ? 0 : index == 19 ? 0x0020 : 0x0100}};
		}
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_MEMORY_EXAMPLES_H
