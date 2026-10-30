// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant < 8; ++variant) {
		bool const carry = variant & 1, polarity = variant & 2, delayed = variant & 4;
		bool const taken = carry == polarity;
		auto initial = initial_case("carry-branch", {u16(0xf808 | (polarity ? 4 : 0) | (delayed ? 0x200 : 0)),0x700});
		unsigned pc = delayed ? 4 : 2;
		initial.program[pc++].value = 0xf020; initial.program[pc++].value = 0x1111; initial.program[pc].value = 0xf4e1;
		for (auto &word : initial.program) word.address += 0x10000;
		initial.program.insert(initial.program.end(), {{0x10700,0xf020},{0x10701,0x2222},{0x10702,0xf4e1}});
		u16 const status = 0x02a5 | (carry ? 0x800 : 0);
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x10600},{r::STATE_ST0,status}});
		clocked_register_case c{{{0,std::move(initial)}},variant & 1 ? 3U : 1U,32};
		c.pipeline_frontend = false;
		c.states.push_back({32,observation("carry-condition", {{r::STATE_A,taken ? 0x2222U : 0x1111U},{r::STATE_ST0,status}})});
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
