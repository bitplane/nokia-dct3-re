// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_PRODUCT_EXAMPLES_H
#define MAME_NOKIA_TMS320C54X_TEST_PRODUCT_EXAMPLES_H

#include "tms320c54x_test_case.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <initializer_list>
#include <utility>

namespace c54x_test {

// One ordered literal sequence. Unspecified registers deliberately continue
// from checked predecessors; in particular MACAR FRCT retains B/T/status and
// DELAY retains T across all four addressing forms. No reset between rows.
inline auto const &product_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto add = [&](const char *id, std::initializer_list<u16> words,
			std::vector<register_setting> initial, std::vector<register_check> expected,
			std::vector<memory_word> data = {}, unsigned cycles = 3,
			std::vector<memory_word> expected_data = {}, unsigned writes = 2) {
			instruction_case test{id, *words.begin(), {{0x05e0, 0x75d6}, {0x05e1, 0x0124}},
				std::move(data), std::move(initial), std::move(expected), writes, cycles, std::move(expected_data)};
			unsigned pc = 0x05e2;
			for (auto word : words) test.program.emplace_back(pc++, word);
			for (u16 word : {0x75d6, 0x0124, 0xf5e1}) test.program.emplace_back(pc++, word);
			test.initial.insert(test.initial.end(), {{r::STATE_PC, 0x05e0}, {r::STATE_IDLE, 0}});
			result.push_back(std::move(test));
		};
		add("mpya-t-signed17", {0xf58c},
			{{r::STATE_A, 0x0080000000ULL}, {r::STATE_B, 0}, {r::STATE_T, 2}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0x0080000000ULL}, {r::STATE_B, 0x10000}, {r::STATE_T, 2}});
		add("mpya-memory-publishes-t", {0x3183}, {{r::STATE_B, 0}, {r::STATE_T, 0x1234}, {r::STATE_AR3, 0x0200}},
			{{r::STATE_B, 0x10000}, {r::STATE_T, 2}}, {{0x0200, 2}});
		add("maca-old-source", {0xf488}, {{r::STATE_A, 0x0080000000ULL}, {r::STATE_B, 0x1234}, {r::STATE_T, 1}},
			{{r::STATE_A, 0x0080008000ULL}, {r::STATE_B, 0x1234}});
		add("macar-integer-round", {0xf689},
			{{r::STATE_A, 0xfffffe0000ULL}, {r::STATE_B, 0x800c}, {r::STATE_T, 4}, {r::STATE_ST0, 0x1800}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0x10000}, {r::STATE_B, 0x800c}, {r::STATE_T, 4}, {r::STATE_ST0, 0x1800}, {r::STATE_ST1, 0}});
		add("macar-fractional-round", {0xf689}, {{r::STATE_A, 0xfffffe0000ULL}, {r::STATE_ST1, 0x0040}},
			{{r::STATE_A, 0}, {r::STATE_B, 0x800c}, {r::STATE_T, 4}, {r::STATE_ST0, 0x1800}, {r::STATE_ST1, 0x0040}});
		add("masar-negative-product", {0xf48b}, {{r::STATE_A, 0x10000}, {r::STATE_B, 0x1234}, {r::STATE_T, 0x8000}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0x20000}, {r::STATE_B, 0x1234}});
		add("sqdst-signed17", {0xe211}, {{r::STATE_A, 0x0080010000ULL}, {r::STATE_B, 0}, {r::STATE_AR3, 0x0200}},
			{{r::STATE_A, 0}, {r::STATE_B, 0x40010001}, {r::STATE_AR3, 0x0200}}, {{0x0200, 0}});
		add("squr-signed17", {0xf48d}, {{r::STATE_A, 0x0080010000ULL}}, {{r::STATE_A, 0x40010001}});
		add("mac-immediate-fractional", {0xf167, 0x0345},
			{{r::STATE_A, 0x1000}, {r::STATE_B, 0}, {r::STATE_T, 0x0400}, {r::STATE_ST1, 0x0040}},
			{{r::STATE_A, 0x1000}, {r::STATE_B, 0x001a3800}, {r::STATE_T, 0x0400}}, {}, 4);
		add("mac-memory-immediate-postincrement", {0x6495, 0x1234},
			{{r::STATE_A, 0x1000}, {r::STATE_T, 0}, {r::STATE_ST1, 0}, {r::STATE_AR5, 0x0100}},
			{{r::STATE_A, 0x06261060}, {r::STATE_T, 0x5678}, {r::STATE_AR5, 0x0101}}, {{0x0100, 0x5678}}, 4);
		add("mac-absolute-extension-order", {0x65f8, 0x0200, 5}, {{r::STATE_A, 0x100}, {r::STATE_B, 0}, {r::STATE_T, 0}},
			{{r::STATE_A, 0x100}, {r::STATE_B, 0x10f}, {r::STATE_T, 3}}, {{0x0200, 3}}, 5);
		add("add-immediate-shift16-carry", {0xf160, 0xffff},
			{{r::STATE_ILLEGAL, 0}, {r::STATE_A, 0x00010000}, {r::STATE_B, 0x1234}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0x00010000}, {r::STATE_B, 0}, {r::STATE_ST0, 0x0800, 0x0800}}, {}, 4);
		add("sub-immediate-shift16-carry", {0xf361, 1}, {{r::STATE_A, 0x1234}, {r::STATE_B, 0x00020000}, {r::STATE_ST0, 0}},
			{{r::STATE_A, 0x1234}, {r::STATE_B, 0x00010000}, {r::STATE_ST0, 0x0800, 0x0800}}, {}, 4);
		add("add-immediate-unsigned-overflow", {0xf060, 0xffff}, {{r::STATE_A, 0}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0x00ffff0000ULL}, {r::STATE_ST0, 0x0400, 0x0400}}, {}, 4);
		add("mac-offset-extension-order", {0x65e3, 4, 2},
			{{r::STATE_A, 0x10}, {r::STATE_B, 0}, {r::STATE_T, 0}, {r::STATE_AR3, 0x0200}},
			{{r::STATE_A, 0x10}, {r::STATE_B, 0x1c}, {r::STATE_T, 3}, {r::STATE_AR3, 0x0200}}, {{0x0202, 3}}, 5);
		add("mac-immediate-fractional-saturate", {0xf167, 0x8000},
			{{r::STATE_A, 0}, {r::STATE_B, 0}, {r::STATE_T, 0x8000}, {r::STATE_PMST, 0}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0240}},
			{{r::STATE_A, 0}, {r::STATE_B, 0x007fffffffULL}, {r::STATE_ST0, 0x0200, 0x0200}}, {}, 4);
		add("delay-stationary", {0x4d83}, {{r::STATE_T, 0x1234}, {r::STATE_AR3, 0x0200}},
			{{r::STATE_T, 0x1234}, {r::STATE_AR3, 0x0200}}, {{0x0200, 0x6cac}, {0x0201, 0}}, 3, {{0x0200, 0x6cac}, {0x0201, 0x6cac}});
		add("delay-preincrement", {0x4deb, 1}, {{r::STATE_AR3, 0x01ff}},
			{{r::STATE_T, 0x1234}, {r::STATE_AR3, 0x0200}}, {{0x01ff, 0x1111}, {0x0200, 0x2222}, {0x0201, 0}}, 4,
			{{0x01ff, 0x1111}, {0x0201, 0x2222}});
		add("delay-absolute", {0x4df8, 0x0200}, {}, {{r::STATE_T, 0x1234}, {r::STATE_AR3, 0x0200}},
			{{0x0200, 0x55aa}, {0x0201, 0}}, 4, {{0x0200, 0x55aa}, {0x0201, 0x55aa}});
		add("delay-offset", {0x4de3, 2}, {{r::STATE_AR3, 0x0200}}, {{r::STATE_T, 0x1234}, {r::STATE_AR3, 0x0200}},
			{{0x0202, 0xabcd}, {0x0203, 0}}, 4, {{0x0202, 0xabcd}, {0x0203, 0xabcd}});
		add("mpyu-fractional-saturate", {0x2483},
			{{r::STATE_A, 0}, {r::STATE_T, 0xffff}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0240}, {r::STATE_AR3, 0x0200}},
			{{r::STATE_A, 0x007fffffffULL}, {r::STATE_T, 0xffff}, {r::STATE_AR3, 0x0200}, {r::STATE_ST0, 0x0400, 0x0400}}, {{0x0200, 0xffff}});
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_PRODUCT_EXAMPLES_H
