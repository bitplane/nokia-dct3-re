// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 1536; ++index) {
		unsigned const operation = index % 4, destination = (index / 4) % 2, branch_b = (index / 8) % 2;
		bool const taken = (index / 16) & 1;
		unsigned const form = (index / 32) % 3, operand = (index / 96) % 4, flags = index / 384;
		unsigned const pattern = (operand + flags + form) % 4;
		u16 const memory = std::array<u16,4>{0,1,0x8000,0xffff}[operand], address = form ? 0x1201 : 0x6b3;
		u64 const initial_value = std::array<u64,4>{0,1,0x7fffffff,0xff80000000ULL}[pattern];
		u64 a = destination ? 0x123456789aULL : initial_value, b = destination ? initial_value : 0x81abcdef01ULL;
		(branch_b ? b : a) = taken ? 0 : 1;
		u64 const source = destination ? b : a;
		s64 const left = source & 0x8000000000ULL ? s64(source) - (s64(1) << 40) : s64(source);
		s64 const right = !(operation & 2) && (flags & 1) ? s16(memory) : s64(memory);
		s64 const sum = operation & 1 ? left - right : left + right;
		bool const overflow = sum > 2147483647LL || sum < -2147483648LL;
		bool const carry = operation & 1 ? u32(source) >= u32(right) : u64(u32(source)) + u32(right) > 0xffffffffULL;
		u64 const result = u64(overflow && (flags & 2) ? std::clamp(sum,s64(-2147483648LL),s64(2147483647LL)) : sum) & 0xffffffffffULL;
		u16 const st0 = 0xa024 | (pattern & 1 ? 0x800 : 0) | (operand & 1 ? 0x600 : 0);
		u16 const final_st0 = (st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? destination ? 0x200 : 0x400 : 0);
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0);
		u32 const base = (operand + form + flags) & 1 ? 0x20000 : 0;
		u16 const opcode = std::array<u16,4>{0,0x800,0x200,0xa00}[operation] | (destination << 8) |
			std::array<u16,3>{0xf8,0xe3,0xeb}[form];
		u16 const extension = form ? 1 : address;
		auto initial = initial_case("branch-arithmetic", {u16(branch_b ? 0xfa4d : 0xfa45),0x700,opcode,extension,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.push_back({base + 0x700,0xf4e1});
		initial.data = {{address,memory}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_AR3,0x1200},{r::STATE_SP,0x2000},{r::STATE_T,0x5aa5}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{5,8,9}[(operand + flags + form) % 3],32};
		c.states.push_back({32,observation("retained-branch-and-arithmetic-operand", {{STATE_GENPC,base + (taken ? 0x701 : 0x605)},
			{r::STATE_A,destination ? a : result},{r::STATE_B,destination ? result : b},{r::STATE_ST0,final_st0},
			{r::STATE_ST1,st1},{r::STATE_AR3,form == 2 ? 0x1201U : 0x1200U},{r::STATE_SP,0x2000},{r::STATE_T,0x5aa5}})});
		c.actions = {{9,{},false,{}, {},{{address,0x5555}}}};
		c.data_events = std::vector<clocked_data_event>{{false,address,memory,9}};
		c.program_events = {{false,base + 0x603,extension,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
