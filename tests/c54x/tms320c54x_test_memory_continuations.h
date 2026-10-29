// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_MEMORY_CONTINUATIONS_H
#define MAME_NOKIA_TMS320C54X_TEST_MEMORY_CONTINUATIONS_H

#include "tms320c54x_test_port_case.h"

namespace c54x_test {

// Ordered literal continuations. Omitted RAM/register setup and partial
// assertion masks are deliberate; do not reset between these rows.
inline auto const &memory_continuations()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, std::vector<u16> words,
			std::vector<register_setting> initial, std::vector<register_check> expected,
			std::vector<memory_word> data = {}, unsigned cycles = 3,
			std::vector<memory_word> expected_data = {}) {
			auto test = port_case(id, std::move(words), std::move(initial), std::move(expected), std::move(data), cycles, std::move(expected_data));
			test.reset_before = result.empty();
			result.push_back(std::move(test));
		};
		add("abdst-old-a-high", {0xe33a},
			{{r::STATE_A, 0xffabcd0000ULL}, {r::STATE_B, 0}, {r::STATE_ST0, 0}, {r::STATE_AR4, 0x0f91}, {r::STATE_AR5, 0x0f90}},
			{{r::STATE_A, 0xffffab0000ULL}, {r::STATE_B, 0x5433}, {r::STATE_T, 0x4444}, {r::STATE_AR4, 0x0f92}, {r::STATE_AR5, 0x0f90}});
		add("abdst-signed-fractional", {0xe33a},
			{{r::STATE_A, 0xffabcd0000ULL}, {r::STATE_B, 0}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0140}, {r::STATE_AR4, 0x0f91}, {r::STATE_AR5, 0x0f90}},
			{{r::STATE_A, 0xfffffb0000ULL}, {r::STATE_B, 0xa866}, {r::STATE_T, 0x4444}}, {{0x0f90, 0xfffe}, {0x0f91, 3}});
		add("ltd-stationary", {0x4c83}, {{r::STATE_T, 0}, {r::STATE_AR3, 0x0f90}},
			{{r::STATE_T, 0x6cac}, {r::STATE_AR3, 0x0f90}}, {{0x0f90, 0x6cac}, {0x0f91, 0}}, 3, {{0x0f90, 0x6cac}, {0x0f91, 0x6cac}});
		add("ltd-successor-before-postincrement", {0x4c93}, {{r::STATE_AR3, 0x0f90}},
			{{r::STATE_T, 0x1234}, {r::STATE_AR3, 0x0f91}}, {{0x0f90, 0x1234}, {0x0f91, 0}}, 3, {{0x0f91, 0x1234}});
		add("sub-half-signed", {0xf43f}, {{r::STATE_A, 0xfffffffffcULL}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xfffffffffeULL}});
		add("sub-half-logical", {0xf43f}, {{r::STATE_A, 0xfffffffffcULL}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0x7ffffffffeULL}});
		add("xor-long-shift16-other", {0xf165, 0x00ff}, {{r::STATE_A, 0xff12340000ULL}, {r::STATE_B, 0x1234}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0xff12340000ULL}, {r::STATE_B, 0xff12cb0000ULL}, {r::STATE_ST0, 0x0800, 0x0800}}, {}, 4);
		add("macr-negative-half", {0xb43a},
			{{r::STATE_A, 0xffffff8000ULL}, {r::STATE_B, 0x1234}, {r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0x0100}, {r::STATE_AR4, 0x0f91}, {r::STATE_AR5, 0x0f90}},
			{{r::STATE_A, 0}, {r::STATE_B, 0x1234}, {r::STATE_T, 0}, {r::STATE_AR4, 0x0f92}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f90, 0}, {0x0f91, 1}});
		add("macr-below-negative-half", {0xb43a}, {{r::STATE_A, 0xffffff7fffULL}, {r::STATE_AR4, 0x0f91}},
			{{r::STATE_A, 0xffffff0000ULL}, {r::STATE_B, 0x1234}, {r::STATE_T, 0}, {r::STATE_AR4, 0x0f92}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("macp-b-coefficient", {0x798b, 0x0700},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x770000}, {r::STATE_T, 8}, {r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0x0100}, {r::STATE_AR3, 0x0f90}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x7d0b44}, {r::STATE_T, 0x0055}, {r::STATE_AR3, 0x0f8f}},
			{{0x0f90, 0x0055}, {0x0f91, 0x0066}}, 5, {{0x0f91, 0x0066}});
		result.back().program.emplace_back(0x0700, 0x1234);
		add("macd-b-successor", {0x7b8b, 0x0700}, {{r::STATE_B, 0x770000}, {r::STATE_T, 8}, {r::STATE_AR3, 0x0f90}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x7d0b44}, {r::STATE_T, 0x0055}, {r::STATE_AR3, 0x0f8f}}, {}, 5, {{0x0f91, 0x0055}});
		add("macp-absolute-b", {0x79f8, 0x0f90, 0x0700}, {{r::STATE_B, 0x770000}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x7d0b44}, {r::STATE_T, 0x0055}}, {{0x0f90, 0x0055}, {0x0f91, 0x0066}}, 6, {{0x0f91, 0x0066}});
		add("macd-absolute-b", {0x7bf8, 0x0f90, 0x0700}, {{r::STATE_B, 0x770000}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x7d0b44}, {r::STATE_T, 0x0055}}, {}, 6, {{0x0f91, 0x0055}});
		add("macp-offset-coefficient-first", {0x79ea, 0x0700, 5}, {{r::STATE_B, 0x770000}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x7d0b44}, {r::STATE_T, 0x0055}, {r::STATE_AR2, 0x0f05}},
			{{0x0f05, 0x0055}, {0x0f06, 0x0066}}, 6, {{0x0f06, 0x0066}});
		add("macd-offset-successor", {0x7bea, 0x0700, 5}, {{r::STATE_B, 0x770000}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x7d0b44}, {r::STATE_T, 0x0055}, {r::STATE_AR2, 0x0f05}}, {}, 6, {{0x0f06, 0x0055}});
		add("ltd-absolute", {0x4cf8, 0x0f90}, {{r::STATE_T, 0}},
			{{r::STATE_T, 0x6cac}}, {{0x0f90, 0x6cac}, {0x0f91, 0}}, 4, {{0x0f90, 0x6cac}, {0x0f91, 0x6cac}});
		add("ltd-offset", {0x4cea, 5}, {{r::STATE_T, 0}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_T, 0x1234}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x1234}, {0x0f06, 0}}, 4, {{0x0f05, 0x1234}, {0x0f06, 0x1234}});
		add("direct-load-store-dp", {0x1045, 0x8046},
			{{r::STATE_A, 0}, {r::STATE_ST0, 0x0012}, {r::STATE_ST1, 0x0100}, {r::STATE_SP, 0x0f00}, {r::STATE_AR5, 0x0f90}},
			{{r::STATE_A, 0x1234}, {r::STATE_AR5, 0x0f90}, {r::STATE_SP, 0x0f00}},
			{{0x0945, 0x1234}, {0x0946, 0}, {0x0f90, 0xabcd}}, 4, {{0x0946, 0x1234}, {0x0f90, 0xabcd}});
		add("direct-load-store-sp", {0x1045, 0x8046}, {{r::STATE_A, 0}, {r::STATE_ST1, 0x4100}},
			{{r::STATE_A, 0x5678}, {r::STATE_AR5, 0x0f90}, {r::STATE_SP, 0x0f00}},
			{{0x0f45, 0x5678}, {0x0f46, 0}}, 4, {{0x0f46, 0x5678}, {0x0946, 0x1234}});
		add("addm-direct-dp", {0x6b45, 1}, {{r::STATE_ST1, 0x0100}},
			{{r::STATE_AR5, 0x0f90}}, {{0x0945, 0x1234}}, 4, {{0x0945, 0x1235}});
		add("ltd-direct-sp", {0x4c45}, {{r::STATE_ST1, 0x4100}, {r::STATE_T, 0}},
			{{r::STATE_T, 0x5678}, {r::STATE_AR5, 0x0f90}, {r::STATE_SP, 0x0f00}}, {{0x0f45, 0x5678}, {0x0f46, 0}}, 3, {{0x0f46, 0x5678}});
		add("mpy-direct-dp", {0x2045}, {{r::STATE_ST1, 0x0100}, {r::STATE_T, 3}, {r::STATE_A, 0}},
			{{r::STATE_A, 12}, {r::STATE_AR5, 0x0f90}}, {{0x0945, 4}});
		add("mac-direct-sp", {0x2945}, {{r::STATE_ST1, 0x4100}, {r::STATE_B, 16}},
			{{r::STATE_B, 4}, {r::STATE_A, 12}, {r::STATE_AR5, 0x0f90}, {r::STATE_SP, 0x0f00}}, {{0x0f45, 0xfffc}});
		add("squr-offset-preincrement", {0x26ea, 5}, {{r::STATE_ST1, 0x0100}, {r::STATE_AR2, 0x0f00}, {r::STATE_T, 0}, {r::STATE_A, 0}},
			{{r::STATE_A, 9}, {r::STATE_T, 0xfffd}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0xfffd}}, 4);
		add("squr-direct-b", {0x271e}, {{r::STATE_ST0, 6}, {r::STATE_ST1, 0x0100}, {r::STATE_T, 3}, {r::STATE_B, 0x01f4}},
			{{r::STATE_B, 0x00e1}, {r::STATE_T, 0x000f}}, {{0x031e, 0x000f}});
		add("squra-direct-b", {0x391e}, {{r::STATE_B, 0x03200000}, {r::STATE_T, 3}},
			{{r::STATE_B, 0x032000e1}, {r::STATE_T, 0x000f}});
		add("squrs-direct-a", {0x3a09}, {{r::STATE_A, 0x014b5db0}, {r::STATE_T, 0x8765}},
			{{r::STATE_A, 0x0320}, {r::STATE_T, 0x1234}}, {{0x0309, 0x1234}});
		add("addc-direct-unsigned-sxm", {0x0605}, {{r::STATE_ST0, 0x0808}, {r::STATE_ST1, 0x0100}, {r::STATE_A, 0}},
			{{r::STATE_A, 0x10000}, {r::STATE_ST0, 0, 0x0800}}, {{0x0405, 0xffff}});
		add("subb-direct-inverted-carry", {0x0e05}, {{r::STATE_ST0, 8}, {r::STATE_A, 6}},
			{{r::STATE_A, 0xffffffffffULL}, {r::STATE_ST0, 0, 0x0800}}, {{0x0405, 6}});
		add("addm-negative-saturation", {0x6b94, 0xfff8}, {{r::STATE_AR4, 0x0100}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0300}},
			{{r::STATE_AR4, 0x0101}, {r::STATE_ST0, 0x0400, 0x0400}}, {{0x0100, 0x8007}}, 4, {{0x0100, 0x8000}});
		// Only standalone reset initializes the predecessor-owned source
		// words and T/SXM context. Legacy preparation must not overwrite them.
		result.front().reset_initial = {{r::STATE_AR6, 0x0a01}, {r::STATE_BK, 4}, {r::STATE_T, 0x4444}, {r::STATE_ST1, 0x0100}};
		result.front().reset_data = {{0x0f90, 0x0055}, {0x0f91, 0x00aa}};
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_MEMORY_CONTINUATIONS_H
