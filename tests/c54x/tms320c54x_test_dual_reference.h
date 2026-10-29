// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_DUAL_REFERENCE_H
#define MAME_NOKIA_TMS320C54X_TEST_DUAL_REFERENCE_H

#include "tms320c54x_test_port_case.h"

namespace c54x_test {

// Atomic MAC/MAS routing and parallel-store references. RAM not rewritten by a
// row deliberately continues from its checked predecessor. No reset within the
// group, and no implicit strengthening of carry/overflow/preservation masks.
inline auto const &dual_reference_cases()
{
	using r = tms320c54x_device;
	static const auto cases = [] {
		std::vector<instruction_case> result;
		auto initial = [](u64 a, u64 b, u16 t, u16 st0 = 0x0800, u16 st1 = 0x0100, bool alias = false) {
			std::vector<register_setting> values{{r::STATE_A, a}, {r::STATE_B, b}, {r::STATE_T, t},
				{r::STATE_ST0, st0}, {r::STATE_ST1, st1}};
			if (!alias) values.push_back({r::STATE_AR4, 0x0f91});
			values.push_back({r::STATE_AR5, 0x0f90});
			return values;
		};
		auto expected = [](u64 a, u64 b) {
			return std::vector<register_check>{{r::STATE_A, a}, {r::STATE_B, b}, {r::STATE_T, 2},
				{r::STATE_AR4, 0x0f92}, {r::STATE_AR5, 0x0f90}, {r::STATE_ST0, 0x0800, 0x0800}};
		};
		auto add = [&](const char *id, u16 opcode, std::vector<register_setting> setup,
			std::vector<register_check> checks, std::vector<memory_word> data = {},
			std::vector<memory_word> expected_data = {}) {
			auto test = port_case(id, {opcode}, std::move(setup), std::move(checks), std::move(data), 3, std::move(expected_data));
			test.reset_before = result.empty();
			result.push_back(std::move(test));
		};
		struct dual_mac_case { const char *id; u16 opcode; u64 a_after, b_after; };
		static constexpr dual_mac_case dual[] = {
			{ "dual-mac-a-a", 0xb03a, 0x18006, 0x28000 }, { "dual-mac-a-b", 0xb13a, 0x18000, 0x18006 },
			{ "dual-mac-b-a", 0xb23a, 0x28006, 0x28000 }, { "dual-mac-b-b", 0xb33a, 0x18000, 0x28006 },
			{ "dual-macr-a-a", 0xb43a, 0x20000, 0x28000 }, { "dual-macr-a-b", 0xb53a, 0x18000, 0x20000 },
			{ "dual-macr-b-a", 0xb63a, 0x30000, 0x28000 }, { "dual-macr-b-b", 0xb73a, 0x18000, 0x30000 },
			{ "dual-mas-a-a", 0xb83a, 0x17ffa, 0x28000 }, { "dual-mas-a-b", 0xb93a, 0x18000, 0x17ffa },
			{ "dual-mas-b-a", 0xba3a, 0x27ffa, 0x28000 }, { "dual-mas-b-b", 0xbb3a, 0x18000, 0x27ffa },
			{ "dual-masr-a-a", 0xbc3a, 0x10000, 0x28000 }, { "dual-masr-a-b", 0xbd3a, 0x18000, 0x10000 },
			{ "dual-masr-b-a", 0xbe3a, 0x20000, 0x28000 }, { "dual-masr-b-b", 0xbf3a, 0x18000, 0x20000 }
		};
		for (unsigned i = 0; i < std::size(dual); ++i)
		{
			auto const &v = dual[i];
			add(v.id, v.opcode, initial(0x18000, 0x28000, 0), expected(v.a_after, v.b_after),
				i ? std::vector<memory_word>{} : std::vector<memory_word>{{0x0f90, 2}, {0x0f91, 3}});
		}
		struct parallel_mac_case { const char *id; u16 opcode, store; u64 a_after, b_after; };
		static constexpr parallel_mac_case parallel[] = {
			{ "st-a-mac-to-a", 0xd03a, 1, 0x18006, 0x28000 }, { "st-a-mac-to-b", 0xd13a, 1, 0x18000, 0x28006 },
			{ "st-b-mac-to-a", 0xd23a, 2, 0x18006, 0x28000 }, { "st-b-mac-to-b", 0xd33a, 2, 0x18000, 0x28006 },
			{ "st-a-macr-to-a", 0xd43a, 1, 0x20000, 0x28000 }, { "st-a-macr-to-b", 0xd53a, 1, 0x18000, 0x30000 },
			{ "st-b-macr-to-a", 0xd63a, 2, 0x20000, 0x28000 }, { "st-b-macr-to-b", 0xd73a, 2, 0x18000, 0x30000 },
			{ "st-a-mas-to-a", 0xd83a, 1, 0x17ffa, 0x28000 }, { "st-a-mas-to-b", 0xd93a, 1, 0x18000, 0x27ffa },
			{ "st-b-mas-to-a", 0xda3a, 2, 0x17ffa, 0x28000 }, { "st-b-mas-to-b", 0xdb3a, 2, 0x18000, 0x27ffa },
			{ "st-a-masr-to-a", 0xdc3a, 1, 0x10000, 0x28000 }, { "st-a-masr-to-b", 0xdd3a, 1, 0x18000, 0x20000 },
			{ "st-b-masr-to-a", 0xde3a, 2, 0x10000, 0x28000 }, { "st-b-masr-to-b", 0xdf3a, 2, 0x18000, 0x20000 }
		};
		for (unsigned i = 0; i < std::size(parallel); ++i)
		{
			auto const &v = parallel[i];
			std::vector<memory_word> data;
			if (!i) data.emplace_back(0x0f90, 3);
			data.emplace_back(0x0f91, 0);
			add(v.id, v.opcode, initial(0x18000, 0x28000, 2), expected(v.a_after, v.b_after),
				std::move(data), {{0x0f91, v.store}});
		}
		add("st-a-mas-asm5", 0xd93a, initial(0x111111, 0x1111, 0x0400, 0, 0x0105),
			{{r::STATE_A, 0x111111}, {r::STATE_B, 0xfffef38d11ULL}, {r::STATE_T, 0x0400},
			 {r::STATE_AR4, 0x0f92}, {r::STATE_AR5, 0x0f90}},
			{{0x0f90, 0x4321}, {0x0f91, 0x1234}}, {{0x0f91, 0x0222}});
		add("st-a-masr-asm1", 0xddba, initial(0x111111, 0x1111, 0x0400, 0, 0x0101),
			{{r::STATE_A, 0x111111}, {r::STATE_B, 0xfffef40000ULL}, {r::STATE_T, 0x0400},
			 {r::STATE_AR4, 0x0f92}, {r::STATE_AR5, 0x0f91}},
			{{0x0f91, 0x1234}}, {{0x0f91, 0x0022}});
		add("st-mas-same-ar", 0xd9b7, initial(0x18000, 0x28000, 2, 0x0800, 0x0100, true),
			{{r::STATE_A, 0x18000}, {r::STATE_B, 0x27ffa}, {r::STATE_T, 2}, {r::STATE_AR5, 0x0f91}},
			{{0x0f90, 3}}, {{0x0f90, 1}});
		struct multiply_mode_case {
			const char *id;
			u16 opcode, x, y, t_before, t_after, st1, st0_after, store;
			u64 a_before, b_before, a_after, b_after;
		};
		static constexpr multiply_mode_case modes[] = {
			{ "dual-mas-fract", 0xb83a, 2,      3, 0, 2,      0x0140, 0,      3,
			  0x18000,    0x28000, 0x17ff4,    0x28000 },
			{ "st-mas-fract", 0xd93a, 3,      0, 2, 2,      0x0140, 0,      1,
			  0x18000,    0x28000, 0x18000,    0x27ff4 },
			{ "dual-mas-saturate", 0xb83a, 0xffff, 2, 0, 0xffff, 0x0300, 0x0400, 2,
			  0x7fffffff, 0,       0x7fffffff, 0 },
			{ "st-mas-saturate", 0xd93a, 0xffff, 0, 2, 2,      0x0300, 0x0200, 1,
			  0x18000,    0x7fffffff, 0x18000, 0x7fffffff }
		};
		for (auto const &v : modes)
			add(v.id, v.opcode, initial(v.a_before, v.b_before, v.t_before, 0, v.st1),
				{{r::STATE_A, v.a_after}, {r::STATE_B, v.b_after}, {r::STATE_T, v.t_after},
				 {r::STATE_ST0, v.st0_after, 0x0600}, {r::STATE_AR4, 0x0f92}, {r::STATE_AR5, 0x0f90}},
				{{0x0f90, v.x}, {0x0f91, v.y}}, {{0x0f91, v.store}});
		result.front().reset_initial = {{r::STATE_AR6, 0x0a03}, {r::STATE_BK, 4}};
		return result;
	}();
	return cases;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_DUAL_REFERENCE_H
