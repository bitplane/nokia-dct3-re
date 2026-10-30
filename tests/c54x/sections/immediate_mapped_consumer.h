// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned operation = 0; operation != 5; ++operation) for (unsigned b = 0; b != 2; ++b)
		for (unsigned part = 0; part != 3; ++part) for (unsigned pattern = 0; pattern != 4; ++pattern)
			for (unsigned gap = 0; gap != 2; ++gap) for (unsigned sxm = 0; sxm != 2; ++sxm) {
				u64 const value = std::array<u64,4>{0,0x123456789aULL,0xff87654321ULL,0x007fffffff}[pattern];
				u16 const literal = std::array<u16,4>{1,0x1357,0xffff,0x8000}[pattern];
				s64 const operand = sxm && literal >= 0x8000 ? s64(literal)-0x10000 : literal;
				u64 const result = (operation == 0 ? value+operand : operation == 1 ? value-operand :
					operation == 2 ? value & literal : operation == 3 ? value | literal : value ^ literal) & 0xffffffffffULL;
				u16 const field = part == 2 ? (result >> 32) & 0xff : result >> (part*16), address = (b ? 0xb : 8)+part;
				u16 const st1 = 0x800 | (sxm ? 0x100 : 0);
				u32 const base = (operation+b+part+pattern+gap+sxm)%2 ? 0x20000 : 0;
				u16 const opcode = 0xf000 | (std::array<unsigned,5>{0,1,3,4,5}[operation] << 4) | (b ? 0x300 : 0);
				auto initial = initial_case("immediate-ALU-to-opposite-accumulator-LDM",{opcode,literal});
				initial.program[2+gap].value = 0x4800 | (b ? 0 : 0x100) | address;
				initial.program[3+gap].value = 0xf4e1;
				for (auto &word : initial.program) word.address += base;
				initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,b ? 0x1234 : value},
					{r::STATE_B,b ? value : 0x5678},{r::STATE_ST0,0},{r::STATE_ST1,st1}});
				clocked_register_case c{{{0,std::move(initial)},{1,observation("before-ALU-mapped-consumer",{})},
					{16,observation("opposite-LDM-captures-result-field",{{r::STATE_A,b ? field : result},
						{r::STATE_B,b ? result : field},{r::STATE_ST1,st1}})}},6,20};
				c.data_events = std::vector<clocked_data_event>{{false,address,field,0,false}};
				c.retained_program_reads = {{base+0x601,literal}};
				cases.push_back(std::move(c));
			}
	return cases;
}
}
