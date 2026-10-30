// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned source = 0; source < 2; ++source) for (unsigned destination = 0; destination < 2; ++destination)
	for (unsigned flags = 0; flags < 4; ++flags) for (unsigned shift : {0U,1U,7U,15U})
	for (unsigned pattern = 0; pattern < 2; ++pattern)
	{
		bool const sxm = flags & 1, ovm = flags & 2;
		u16 const literal = pattern ? 0xf495 : 1;
		s64 const input = pattern ? -2147483648LL : 2147483647LL;
		s64 const operand = (sxm ? s64(s16(literal)) : s64(literal)) * (s64(1) << shift);
		s64 const sum = input + operand;
		bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
		bool const carry = u64(u32(input)) + u32(operand) > 0xffffffffULL;
		u64 const result = u64(ovm && overflow ? std::clamp(sum,s64(-2147483648LL),s64(2147483647LL)) : sum) & 0xffffffffffULL;
		u64 const a = source ? 0x1111222233ULL : u64(input) & 0xffffffffffULL;
		u64 const b = source ? u64(input) & 0xffffffffffULL : 0x4444555566ULL;
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0);
		auto initial = initial_case("immediate-add", {u16(0xf000 | (source << 9) | (destination << 8) | shift),literal});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x1b},{r::STATE_ST1,st1},{r::STATE_A,a},{r::STATE_B,b}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,0x1b},{r::STATE_ST1,st1}});
		auto final = observation("published", {{r::STATE_A,destination ? a : result},{r::STATE_B,destination ? result : b},
			{r::STATE_ST0,unsigned(0x1b | (carry ? 0x800 : 0) | (overflow ? destination ? 0x200 : 0x400 : 0))},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)},{7,std::move(final)}},
			3 + (source + destination + flags + shift + pattern) % 2,9};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions = {{3,{},false,{{0x601,0}}}};
		c.program_events = {{false,0x601,literal,3}};
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
