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

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_ABSOLUTE_EXAMPLES_H
