// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u64 accumulators[]{0,0xffffffff,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x56783933,0x80aa001234ULL};
	constexpr u16 operands[]{1,1,1,0xffff,0x8000,0xffff,0x1234,0xffff};
	std::vector<clocked_register_case> cases;
	for (unsigned shape = 0; shape < 9; ++shape)
	for (unsigned pattern = shape == 8 ? 0 : 2; pattern < (shape == 8 ? 8U : 4U); ++pattern)
	for (unsigned selector = 0; selector < 4; ++selector)
	for (unsigned flags = 0; flags < 4; ++flags)
	for (unsigned absolute = 0; absolute < 2; ++absolute) {
		bool const bk = shape == 8, b = selector & 1, subtract = selector & 2;
		u64 const source = accumulators[pattern];
		u16 const memory = bk && pattern == 0 ? 0 : operands[pattern], address = bk ? 0x19 : 0x10 + shape;
		s64 const left = source & 0x8000000000ULL ? s64(source) - (s64(1) << 40) : s64(source);
		s64 const right = flags & 1 ? s16(memory) : s64(memory), sum = subtract ? left - right : left + right;
		bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
		bool const carry = subtract ? u32(source) >= u32(right) : u64(u32(source)) + u32(right) > 0xffffffffULL;
		u64 const result = u64(overflow && (flags & 2) ? std::clamp(sum,s64(-2147483648LL),s64(2147483647LL)) : sum) & 0xffffffffffULL;
		u16 const st0 = 0x1800 | (pattern & 1 ? 0x600 : 0), final_st0 = (st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? b ? 0x200 : 0x400 : 0);
		u16 const st1 = 0x800 | ((flags & 1) << 8) | ((flags & 2) << 8);
		u32 const base = (shape + pattern + flags) & 1 ? 0x20000 : 0;
		int const source_register = bk ? r::STATE_BK : int(r::STATE_AR0 + shape);
		u16 const opcode = (subtract ? 0x800 : 0) | (b ? 0x100 : 0) | (absolute ? 0xf8 : address);
		auto initial = initial_case("mapped-word-arithmetic", {opcode,u16(absolute ? address : 0xf4e1),0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{source_register,memory},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_A,b ? 0x123456789aULL : source},{r::STATE_B,b ? source : 0x123456789aULL}});
		unsigned const read = absolute ? 6 : 5;
		clocked_register_case c{{{0,std::move(initial)}},read - ((shape + pattern + selector + flags) & 1),16};
		c.states.push_back({16,observation("mapped-source-retained", {{r::STATE_A,b ? 0x123456789aULL : result},
			{r::STATE_B,b ? result : 0x123456789aULL},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{source_register,0x2222},
			{STATE_GENPC,base + (absolute ? 0x603 : 0x602)}})});
		c.actions = {{read,{{source_register,0x2222}},false}};
		c.data_events = std::vector<clocked_data_event>{{false,address,memory,read}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
