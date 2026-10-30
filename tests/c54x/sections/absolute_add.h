// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 operands[]{0,1,0x8000,0xffff}, addresses[]{0x11,0x19,0x18,0x500};
	constexpr u64 accumulators[]{0,0x7fffffff,0xff80000000ULL,0xffffffffffULL};
	for (unsigned source = 0; source < 4; ++source) for (unsigned operand_index = 0; operand_index < 4; ++operand_index)
	for (unsigned left_index = 0; left_index < 4; ++left_index) for (unsigned flags = 0; flags < 4; ++flags)
	for (unsigned b = 0; b < 2; ++b)
	{
		bool const sxm = flags & 1, ovm = flags & 2;
		u16 const operand = operands[operand_index], address = addresses[source];
		u64 const left_bits = accumulators[left_index];
		// Independent integer ADD reference: signed40 input, signed/unsigned16 operand,
		// carry from bit32 and optional signed32 saturation. No CPU arithmetic helpers.
		s64 const left = left_bits & 0x8000000000ULL ? s64(left_bits) - 0x10000000000LL : s64(left_bits);
		s64 const right = sxm && (operand & 0x8000) ? s64(operand) - 0x10000 : operand;
		s64 const sum = left + right;
		bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
		bool const carry = u64(u32(left_bits)) + u32(right) > 0xffffffffULL;
		u64 const value = overflow && ovm ? sum < 0 ? 0xff80000000ULL : 0x7fffffffULL : u64(sum) & 0xffffffffffULL;
		u16 const status = 8 | (carry ? 0x800 : 0) | (overflow ? b ? 0x200 : 0x400 : 0);
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) |
			(source == 2 && (operand_index + left_index + flags + b) % 2 ? 0x4000 : 0);
		auto initial = initial_case("absolute-add", {u16(b ? 0x01f8 : 0x00f8),address});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,8},{r::STATE_ST1,st1},
			{r::STATE_SP,source == 2 ? operand : 0x1000},{r::STATE_AR1,operand},{r::STATE_BK,operand},
			{r::STATE_A,b ? 0x2233445566ULL : left_bits},{r::STATE_B,b ? left_bits : 0x2233445566ULL}});
		initial.data = {{0x500,operand}};
		auto final = observation("sum", {{r::STATE_A,b ? 0x2233445566ULL : value},
			{r::STATE_B,b ? value : 0x2233445566ULL},{r::STATE_ST0,status},{r::STATE_ST1,st1},
			{r::STATE_SP,source == 2 ? 0x1234U : 0x1000U}});
		clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},
			std::array<unsigned,4>{4,6,7,8}[(source + operand_index + left_index + flags + b) % 4],20};
		c.require_idle = false; c.retained_program_reads = {{0x601,address}};
		c.actions.push_back({4,{},false,{{0x601,0x510}}});
		clocked_register_action poison{6,{}};
		if (source == 3) poison.data = {{0x500,0x1234}};
		else poison.values = {{std::array<int,3>{r::STATE_AR1,r::STATE_BK,r::STATE_SP}[source],0x1234}};
		c.actions.push_back(std::move(poison));
		c.data_events = std::vector<clocked_data_event>{{false,address,operand,6}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
