// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned operation = 0; operation != 3; ++operation) for (unsigned mode = 0; mode != 3; ++mode)
		for (unsigned pattern = 0; pattern != 3; ++pattern) for (unsigned destination = 0; destination != 2; ++destination) for (unsigned sxm = 0; sxm != 2; ++sxm) {
			u16 const operand = std::array<u16,3>{0xf0,0x8000,0xffff}[pattern];
			u32 const base = (pattern ^ destination ^ mode) & 1 ? 0x20000 : 0;
			u16 const opcode = std::array<u16,3>{0x1800,0x1a00,0x1c00}[operation] | (destination << 8) | (mode == 0 ? 0x25 : mode == 1 ? 0x81 : 0x91);
			auto initial = initial_case("paid-peripheral-logic-read-and-pointer-update",{0x7711,0x25,u16(destination ? 0xe955 : 0xe855),opcode,0xf4e1});
			for (auto &word : initial.program) word.address += base;
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_PRD,operand},{r::STATE_ST0,0x600},
				{r::STATE_ST1,0x800U | (sxm << 8)},{r::STATE_AR1,0x410},{r::STATE_SP,0x1000},
				{r::STATE_A,destination ? 0x2233445566ULL : 0ULL},{r::STATE_B,destination ? 0ULL : 0x2233445566ULL}});
			u64 const expected = operation == 0 ? 0x55 & operand : operation == 1 ? 0x55 | operand : 0x55 ^ operand;
			auto final = observation("zero-extended-logical-result-from-retained-read",{{r::STATE_A,destination ? 0x2233445566ULL : expected},
				{r::STATE_B,destination ? expected : 0x2233445566ULL},{r::STATE_AR1,mode == 2 ? 0x26U : 0x25U},
				{r::STATE_ST0,0x600},{r::STATE_ST1,0x800U | (sxm << 8)},{r::STATE_PRD,0x1234},{r::STATE_SP,0x1000}});
			clocked_register_case c{{{0,std::move(initial)},{1,observation("peripheral-logic-running",{})},{20,std::move(final)}},
				7+(operation+mode+pattern+destination+sxm)%4,20};
			// The setup's separate AR1 publication is outside the operand-bus contract.
			c.data_ranges = {{0x25,0x26}};
			c.data_events = std::vector<clocked_data_event>{{false,0x25,operand,9}};
			c.actions = {{9,{{r::STATE_PRD,0x1234}}}};
			cases.push_back(std::move(c));
		}
	return cases;
}
}
