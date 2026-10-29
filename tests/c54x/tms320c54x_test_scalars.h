// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SCALARS_H
#define MAME_NOKIA_TMS320C54X_TEST_SCALARS_H

#include "tms320c54x_test_case.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <initializer_list>
#include <utility>

namespace c54x_test {

struct scalar_vector
{
	instruction_case test;
	bool reset;
};

// Literal TI arithmetic boundaries. A false reset marks an intentional
// continuation: program words, flags or an accumulator come from the preceding
// checked case. The standalone runner resets at each named group's first case.
inline auto const &scalar_vectors()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<scalar_vector> result;
		auto add = [&](const char *id, u16 opcode, unsigned pc, std::initializer_list<u16> words,
			std::vector<register_setting> initial, std::vector<register_check> expected,
			bool reset = true, std::vector<memory_word> data = {}, unsigned writes = 0, unsigned interval = 0) {
			instruction_case test{id, opcode, {}, std::move(data), std::move(initial), std::move(expected), writes, interval};
			unsigned address = pc;
			for (auto word : words) test.program.emplace_back(address++, word);
			test.initial.insert(test.initial.end(), {{r::STATE_PC, pc}, {r::STATE_IDLE, 0}});
			result.push_back({std::move(test), reset});
		};
		struct negate { const char *id; u16 opcode; u64 source, a, b; u16 st0, st1; };
		for (auto const &v : {
			negate{"neg-b-to-a", 0xf684, 0xfffffffffeULL, 2, 0xfffffffffeULL, 0x1000, 0x0100},
			negate{"neg-b-to-b", 0xf784, 0xfffffffffeULL, 0x12345678, 2, 0x1000, 0x0100},
			negate{"neg-b-zero-to-a", 0xf684, 0, 0, 0, 0x1800, 0x0100},
			negate{"neg-b-zero-to-b", 0xf784, 0, 0x12345678, 0, 0x1800, 0x0100},
			negate{"neg-b-overflow-to-a", 0xf684, 0xff80000000ULL, 0x80000000, 0xff80000000ULL, 0x1400, 0x0100},
			negate{"neg-b-overflow-to-b", 0xf784, 0xff80000000ULL, 0x12345678, 0x80000000, 0x1200, 0x0100},
			negate{"neg-b-saturate-to-a", 0xf684, 0xff80000000ULL, 0x7fffffff, 0xff80000000ULL, 0x1400, 0x0300},
			negate{"neg-b-saturate-to-b", 0xf784, 0xff80000000ULL, 0x12345678, 0x7fffffff, 0x1200, 0x0300}})
			add(v.id, v.opcode, 0x05e0, {0x75f8, 0x0d00, 0x0124, v.opcode, 0x75f8, 0x0d00, 0x0124, 0xf5e1},
				{{r::STATE_A, 0x12345678}, {r::STATE_B, v.source}, {r::STATE_ST0, 0x1800}, {r::STATE_ST1, v.st1}},
				{{r::STATE_A, v.a}, {r::STATE_B, v.b}, {r::STATE_ST0, v.st0}, {r::STATE_ST1, v.st1}, {r::STATE_PC, 0x05e8}},
				true, {}, 2, 4);

		// Six loads keep A, then the loaded B, live across the group.
		add("ld-b-unsigned", 0xf120, 0x0480, {0xf120, 0xffff, 0xf5e1},
			{{r::STATE_A, 0x12345678}, {r::STATE_ST1, 0}},
			{{r::STATE_ILLEGAL, 0}, {r::STATE_IDLE, 1}, {r::STATE_PC, 0x0483}, {r::STATE_A, 0x12345678}, {r::STATE_B, 0xffff}});
		add("ld-b-signed", 0xf120, 0x0480, {0xf120, 0x8000}, {{r::STATE_ST1, 0x0100}},
			{{r::STATE_ILLEGAL, 0}, {r::STATE_IDLE, 1}, {r::STATE_PC, 0x0483}, {r::STATE_A, 0x12345678}, {r::STATE_B, 0xffffff8000ULL}}, false);
		add("ld-b-shift16-signed", 0xf162, 0x0480, {0xf162, 0x8000}, {{r::STATE_ST1, 0x0100}},
			{{r::STATE_ILLEGAL, 0}, {r::STATE_IDLE, 1}, {r::STATE_PC, 0x0483}, {r::STATE_A, 0x12345678}, {r::STATE_B, 0xff80000000ULL}}, false);
		add("ld-b-shift16-unsigned", 0xf162, 0x0480, {0xf162, 0xffff}, {{r::STATE_ST1, 0}},
			{{r::STATE_ILLEGAL, 0}, {r::STATE_IDLE, 1}, {r::STATE_PC, 0x0483}, {r::STATE_A, 0x12345678}, {r::STATE_B, 0x00ffff0000}}, false);
		add("ld-a-shift15-unsigned", 0xf02f, 0x0480, {0xf02f, 0x8000}, {{r::STATE_ST1, 0}},
			{{r::STATE_IDLE, 1}, {r::STATE_A, 0x0040000000}, {r::STATE_B, 0x00ffff0000}}, false);
		add("ld-a-shift15-signed", 0xf02f, 0x0480, {}, {{r::STATE_ST1, 0x0100}},
			{{r::STATE_IDLE, 1}, {r::STATE_A, 0xffc0000000ULL}, {r::STATE_B, 0x00ffff0000}}, false);

		add("abs-saturate", 0xf585, 0x0490, {0xf585, 0xf5e1},
			{{r::STATE_A, 0x0312345678ULL}, {r::STATE_ST1, 0x0200}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_B, 0x7fffffff}, {r::STATE_ST0, 0x0200, 0x0a00}});
		add("abs-sticky-zero", 0xf585, 0x0490, {}, {{r::STATE_A, 0}},
			{{r::STATE_B, 0}, {r::STATE_ST0, 0x0a00, 0x0a00}}, false);
		add("immediate-add-saturate", 0xf000, 0x04a0, {0xf000, 1, 0xf5e1},
			{{r::STATE_A, 0x7fffffff}, {r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0x0200}},
			{{r::STATE_A, 0x7fffffff}, {r::STATE_ST0, 0x0400, 0x0c00}});
		add("immediate-sub-saturate", 0xf010, 0x04a0, {0xf010}, {{r::STATE_A, 0xff80000000ULL}},
			{{r::STATE_A, 0xff80000000ULL}, {r::STATE_ST0, 0x0c00, 0x0c00}}, false);
		add("sfta-right-zero-fill", 0xf47f, 0x04b0, {0xf47f, 0xf5e1},
			{{r::STATE_A, 0xffffffffffULL}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0x7fffffffffULL}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("sfta-right-sign-fill", 0xf47f, 0x04b0, {}, {{r::STATE_A, 0xffffffffffULL}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xffffffffffULL}, {r::STATE_ST0, 0x0800, 0x0800}}, false);
		add("memory-add-wrap", 0x00f8, 0x04c0, {0x00f8, 0x0a80, 0xf5e1},
			{{r::STATE_A, 0xffffffffffULL}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0}, {r::STATE_ST0, 0x0800, 0x0800}}, true, {{0x0a80, 1}});
		add("memory-sub-borrow", 0x08f8, 0x04c0, {0x08f8}, {},
			{{r::STATE_A, 0xffffffffffULL}, {r::STATE_ST0, 0, 0x0800}}, false);
		add("shifted-immediate-carry", 0xf02f, 0x04d0, {0xf02f, 0xffff, 0xf5e1},
			{{r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0x0200}},
			{{r::STATE_A, 0x007fff8000}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("asm-load-saturate-carry", 0xf482, 0x04d0, {0xf482, 0xf5e1},
			{{r::STATE_A, 0x0040000000}, {r::STATE_ST1, 0x0202}},
			{{r::STATE_A, 0x7fffffff}, {r::STATE_ST0, 0x0c00, 0x0c00}}, false);

		add("neg-40bit-minimum", 0xf484, 0x0560, {0xf484, 0xf5e1},
			{{r::STATE_A, 0x8000000000ULL}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0x8000000000ULL}, {r::STATE_ST0, 0x0400, 0x0c00}});
		add("neg-40bit-minimum-saturate", 0xf484, 0x0560, {}, {{r::STATE_ST1, 0x0200}},
			{{r::STATE_A, 0x7fffffff}, {r::STATE_ST0, 0x0400, 0x0c00}}, false);
		add("sftl-right", 0xf0ff, 0x05a0, {0xf0ff, 0xf5e1}, {{r::STATE_A, 0xffffffffffULL}, {r::STATE_ST1, 0}},
			{{r::STATE_A, 0x7fffffff}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("sftl-left", 0xf0e1, 0x05a0, {0xf0e1}, {{r::STATE_A, 0xffffffffffULL}},
			{{r::STATE_A, 0xfffffffe}, {r::STATE_ST0, 0x0800, 0x0800}}, false);
		add("sftl-zero-clears-carry", 0xf0e0, 0x05a0, {0xf0e0}, {{r::STATE_A, 0xffffffffffULL}},
			{{r::STATE_A, 0xffffffff}, {r::STATE_ST0, 0, 0x0800}}, false);
		add("sfta-left-guard-overflow", 0xf765, 0x05c0, {0xf765, 0xf5e1},
			{{r::STATE_B, 0x80aa001234ULL}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0}},
			{{r::STATE_B, 0x1540024680ULL}, {r::STATE_ST0, 0x0200, 0x0a00}});
		add("sfta-left-saturate", 0xf765, 0x05c0, {}, {{r::STATE_B, 0x80aa001234ULL}, {r::STATE_ST1, 0x0200}},
			{{r::STATE_B, 0xff80000000ULL}, {r::STATE_ST0, 0x0200, 0x0a00}}, false);
		add("rol-a-carry-out", 0xf491, 0x05e0, {0xf491, 0xf5e1},
			{{r::STATE_A, 0xff80000000ULL}, {r::STATE_B, 0x12345678}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0200}},
			{{r::STATE_A, 0}, {r::STATE_B, 0x12345678}, {r::STATE_ST0, 0x0800, 0x0800}});
		add("rol-b-carry-in", 0xf591, 0x05e0, {0xf591},
			{{r::STATE_A, 0x12345678}, {r::STATE_B, 0x12345678}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0x12345678}, {r::STATE_B, 0x2468acf1}, {r::STATE_ST0, 0, 0x0800}}, false);
		add("xor-immediate-guards", 0xf050, 0x05e0, {0xf050, 0x00ff, 0xf5e1},
			{{r::STATE_A, 0xff12345678ULL}, {r::STATE_B, 0x12345678}, {r::STATE_ST0, 0x0800}, {r::STATE_ST1, 0x0200}},
			{{r::STATE_A, 0xff12345687ULL}, {r::STATE_B, 0x12345678}, {r::STATE_ST0, 0x0800, 0x0800}, {r::STATE_PC, 0x05e3}});
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SCALARS_H
