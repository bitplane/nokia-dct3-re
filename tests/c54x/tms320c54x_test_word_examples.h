// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_WORD_EXAMPLES_H
#define MAME_NOKIA_TMS320C54X_TEST_WORD_EXAMPLES_H

#include "tms320c54x_test_case.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <array>
#include <utility>

namespace c54x_test {

inline auto const &instruction_word_examples()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		static constexpr const char *ids[] = {
			"word-6f95-v0", "word-6f95-v1", "word-1087-v0", "word-1087-v1", "word-1182-v0", "word-1182-v1",
			"word-6c8d-v0", "word-6c8d-v1", "word-6c8e-v0", "word-6c8e-v1", "word-6db7-v0", "word-6db7-v1",
			"word-80b4-v0", "word-80b4-v1", "word-838a-v0", "word-838a-v1", "word-8383-v0", "word-8383-v1",
			"word-f67b-v0", "word-f67b-v1", "word-3185-v0", "word-3185-v1", "word-f301-v0", "word-f301-v1",
			"word-f648-v0", "word-f648-v1", "word-6b82-v0", "word-6b82-v1", "word-4583-v0", "word-4583-v1",
			"word-21f8-v0", "word-21f8-v1", "word-3ed3-v0", "word-3ed3-v1", "word-5793-v0", "word-5793-v1",
			"word-818b-v0", "word-818b-v1", "word-9a5f-v0", "word-9a5f-v1", "word-f20f-v0", "word-f20f-v1",
			"word-f47d-v0", "word-f47d-v1", "word-f50f-v0", "word-f50f-v1", "word-f736-v0", "word-f736-v1",
			"word-f761-v0", "word-f761-v1"
		};
		std::vector<instruction_case> result;
		auto add = [&](u16 opcode, u16 second, std::vector<register_setting> initial,
			std::vector<register_check> expected, std::vector<memory_word> data,
			std::vector<memory_word> expected_data, bool branch = false, bool trailing_idle = true) {
			std::vector<memory_word> program{{0x0800, opcode}, {0x0801, second}};
			if (trailing_idle) program.emplace_back(0x0802, 0xf4e1);
			if (branch)
				program = {{0x0800, opcode}, {0x0801, 0x0900}, {0x0802, 0xe801}, {0x0803, 0xf4e1},
					{0x0900, 0xe802}, {0x0901, 0xf4e1}};
			initial.emplace_back(r::STATE_PC, 0x0800);
			expected.insert(expected.begin(), {{r::STATE_IDLE, 1}, {r::STATE_ILLEGAL, 0}});
			result.push_back({ids[result.size()], opcode, std::move(program), std::move(data),
				std::move(initial), std::move(expected), 0, 0, std::move(expected_data)});
		};

		for (unsigned family = 0; family < 3; ++family)
			for (bool sxm : {false, true})
			{
				u64 const loaded = sxm ? 0xffffff8001ULL : 0x8001;
				add(family == 0 ? 0x6f95 : family == 1 ? 0x1087 : 0x1182, family == 0 ? 0x0e20 : 0xf4e1,
					{{r::STATE_A, 3}, {r::STATE_B, 5}, {r::STATE_AR2, 0x0600}, {r::STATE_AR5, 0x0600},
					 {r::STATE_AR7, 0x0600}, {r::STATE_ST0, 0}, {r::STATE_ST1, sxm ? 0x0100 : 0}},
					{{r::STATE_A, family == 0 ? (sxm ? 0x8004 : 0xffffff8004ULL) : family == 1 ? loaded : 3},
					 {r::STATE_B, family == 2 ? loaded : 5}, {r::STATE_AR5, u64(family == 0 ? 0x0601 : 0x0600)},
					 {r::STATE_AR2, 0x0600}, {r::STATE_AR7, 0x0600}}, {{0x0600, 0x8001}}, {{0x0600, 0x8001}});
			}

		constexpr u16 address_ops[]{0x6c8d, 0x6c8e, 0x6db7, 0x80b4, 0x838a, 0x8383};
		for (unsigned family = 0; family < std::size(address_ops); ++family)
			for (bool variant : {false, true})
			{
				u64 const a = variant ? 0xabcdef01 : 0x12345678, b = variant ? 0x89abcdef : 0x23456789;
				std::vector<register_setting> initial{{r::STATE_A, a}, {r::STATE_B, b}, {r::STATE_AR0, variant ? 0xffff : 3}};
				std::vector<register_check> expected{{r::STATE_A, family < 2 ? (variant ? 2 : 1) : a}, {r::STATE_B, b}};
				for (unsigned ar = 2; ar < 8; ++ar)
				{
					initial.emplace_back(r::STATE_AR0 + ar, 0x0600);
					u16 value = 0x0600;
					if (family < 2 && ar == 5 + family) value = variant ? 1 : 0xffff;
					if ((family == 2 && ar == 7) || (family == 3 && ar == 4)) value = variant ? 0x05ff : 0x0603;
					if (family == 4 && ar == 2) value = 0x05ff;
					expected.push_back({int(r::STATE_AR0 + ar), value});
				}
				// Preserve the checked seed followed by its branch-specific override.
				if (family < 2) initial.emplace_back(r::STATE_AR5 + family, variant ? 2 : 0);
				initial.insert(initial.end(), {{r::STATE_ST0, 0}, {r::STATE_ST1, 0}});
				add(address_ops[family], family < 2 ? 0x0900 : 0xf4e1, std::move(initial), std::move(expected),
					{{0x0600, 0xdead}}, {{0x0600, family < 3 ? 0xdead : family == 3 ? u16(a) : u16(b >> 16)}}, family < 2, false);
			}

		constexpr u16 math_ops[]{0xf67b, 0x3185, 0xf301, 0xf648};
		for (unsigned family = 0; family < std::size(math_ops); ++family)
			for (bool variant : {false, true})
			{
				u64 const b = family == 0 ? 0xffffffffdfULL : family == 1 ? 0x1234 :
					family == 2 ? (variant ? 0xfffffffff0ULL : 5) : (variant ? 0xfffffffffeULL : 0x123456);
				u64 const a_after = family == 0 ? (variant ? 0xfffffffffeULL : 0x07fffffffeULL) :
					family == 1 ? 0x20000 : family == 2 ? 0x1234 : (variant ? 0xfffffffe00ULL : 0x12345600);
				u64 const b_after = family == 1 ? (variant ? 0xfffffffff4ULL : 0xfffffffffaULL) :
					family == 2 ? (variant ? 0x7ff0 : 0x8005) : b;
				add(math_ops[family], family == 2 ? 0x4000 : 0xf4e1,
					{{r::STATE_AR5, 0x0600}, {r::STATE_A, family == 1 ? 0x20000 : 0x1234}, {r::STATE_B, b},
					 {r::STATE_T, 0xbeef}, {r::STATE_ST0, family == 1 ? 0x0800 : 0},
					 {r::STATE_ST1, family == 0 ? (variant ? 0x0100 : 0) : family == 1 ? (variant ? 0x0040 : 0) : 0}},
					{{r::STATE_A, a_after}, {r::STATE_B, b_after}, {r::STATE_T, u64(family == 1 ? 0xfffd : 0xbeef)},
					 {r::STATE_ST0, u64(family < 2 || (family == 2 && variant) ? 0x0800 : 0)}, {r::STATE_AR5, 0x0600}},
					{{0x0600, 0xfffd}}, {{0x0600, 0xfffd}});
			}

		constexpr u16 memory_ops[]{0x6b82, 0x4583, 0x21f8, 0x3ed3, 0x5793, 0x818b};
		for (unsigned family = 0; family < std::size(memory_ops); ++family)
			for (bool variant : {false, true})
			{
				u64 const b = family == 5 ? (variant ? 0x89abcdef : 0x23456789) : 5;
				u64 const b_after = family == 1 ? (variant ? 0xff80010000ULL : 0x0080010000ULL) :
					family == 2 ? (variant ? 0xfffffffff4ULL : 0xfffffffffaULL) :
					family == 4 ? (variant ? 0xff80012345ULL : 0x0080012345ULL) : b;
				u16 const word = family == 0 ? (variant ? 0x7fff : 0xffff) : family == 2 ? 0xfffd : 0x8001;
				add(memory_ops[family], family == 0 ? 1 : family == 2 ? 0x0600 : 0xf4e1,
					{{r::STATE_A, 3}, {r::STATE_B, b}, {r::STATE_T, 2}, {r::STATE_AR2, 0x0600},
					 {r::STATE_AR3, family == 3 ? 0x0607 : 0x0600}, {r::STATE_BK, family == 3 && variant ? 8 : 0},
					 {r::STATE_ST0, family == 3 ? 0x0800 : 0}, {r::STATE_ST1, family == 0 ? (variant ? 0 : 0x0100) :
						family == 1 || family == 4 ? (variant ? 0x0100 : 0) : family == 2 && variant ? 0x0040 : 0}},
					{{r::STATE_A, u64(family == 3 ? 0x20005 : 3)}, {r::STATE_B, b_after}, {r::STATE_T, 2},
					 {r::STATE_ST0, u64(family == 0 ? (variant ? 0x0400 : 0x0800) : family == 3 ? 0x0800 : 0)},
					 {r::STATE_AR2, 0x0600}, {r::STATE_AR3, u64(family == 3 ? (variant ? 0x0600 : 0x0608) :
						family == 4 ? 0x0602 : family == 5 ? 0x05ff : 0x0600)}},
					{{0x0600, word}, {0x0601, 0x2345}, {0x0607, 2}},
					{{0x0600, family == 0 ? (variant ? 0x8000 : 0) : family == 2 ? 0xfffd : family == 5 ? u16(b) : 0x8001},
					 {0x0601, 0x2345}});
			}

		constexpr u16 final_ops[]{0x9a5f, 0xf20f, 0xf47d, 0xf50f, 0xf736, 0xf761};
		for (unsigned family = 0; family < std::size(final_ops); ++family)
			for (bool variant : {false, true})
			{
				u64 const a = family == 0 || family == 3 ? (variant ? 0xfffffffffeULL : 2) : family == 2 ? 0xfffffffff7ULL : 3;
				u64 const b = family == 1 ? (variant ? 0xfffffffff0ULL : 5) : family == 4 ? (variant ? 0xfffffffc00ULL : 0x400) :
					family == 5 ? 0x40000000 : 5;
				u64 const a_after = family == 1 ? (variant ? 0x7ff0 : 0x8005) :
					family == 2 ? (variant ? 0x1ffffffffeULL : 0xfffffffffeULL) : family == 0 || family == 3 ? a : 3;
				u64 const b_after = family == 3 ? (variant ? 0xffffff0005ULL : 0x10005) :
					family == 4 ? (variant ? 0xfffffffc01ULL : 0x3ff) : family == 5 ? (variant ? 0x7fffffff : 0x80000000) : b;
				add(final_ops[family], family == 1 ? 1 : 0xf4e1,
					{{r::STATE_A, a}, {r::STATE_B, b}, {r::STATE_AR3, 0x0600}, {r::STATE_ST0, 0},
					 {r::STATE_ST1, family == 2 ? (variant ? 0 : 0x0100) : family == 4 ? 0x0100 : family == 5 && variant ? 0x0200 : 0}},
					{{r::STATE_A, a_after}, {r::STATE_B, b_after},
					 {r::STATE_ST0, u64(family == 2 || (family == 1 && variant) || (family == 4 && !variant) ? 0x0800 : family == 5 ? 0x0200 : 0)},
					 {r::STATE_AR3, u64(family == 0 ? 0x05ff : 0x0600)}},
					{{0x0600, 0xdead}}, {{0x0600, family == 0 ? (variant ? 0xffff : 1) : 0xdead}});
			}
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_WORD_EXAMPLES_H
