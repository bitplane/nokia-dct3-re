// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<u16,8> operands{0,1,0x7fff,0x8000,0xffff,0x1234,0x5555,0xaaaa};
	constexpr std::array<u64,8> accumulators{0,1,0x7fffffff,0xff80000000ULL,0xffffffffULL,0x7fffffffffULL,0x8000000000ULL,0x123456789aULL};
	for (unsigned operation = 0; operation != 3; ++operation)
	for (unsigned mode = 0; mode != 16; ++mode)
	for (unsigned shift = 0; shift != 16; ++shift)
	for (unsigned pattern = 0; pattern != operands.size(); ++pattern)
	for (bool sxm : {false,true})
	for (unsigned ovm = 0; ovm != (operation ? 2U : 1U); ++ovm)
	for (bool b : {false,true}) {
		unsigned const parity = operation + mode + shift + pattern + sxm + ovm + b;
		u32 const base = parity % 2 ? 0x20000 : 0;
		u16 const st0 = 0xa002 | (parity % 2 ? 0x1e00 : 0);
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) | (pattern % 2 ? 0x4020 : 0);
		u16 const address = 0x100f + 0x100 * (mode & 3);
		bool const linear = pattern >= 4;
		u16 const next = address + std::array<int,4>{0,-1,1,linear ? 3 : -13}[mode >> 2];
		u64 const source = operation ? accumulators[pattern] : b ? 0xfeabcd9876ULL : 0x123456789aULL;
		s64 const operand = (sxm ? s64(s16(operands[pattern])) : s64(operands[pattern])) * (s64(1) << shift);
		u64 result = u64(operand) & 0xffffffffffULL;
		u16 result_st0 = st0;
		if (operation) {
			s64 const left = source & 0x8000000000ULL ? s64(source) - (s64(1) << 40) : s64(source);
			s64 const sum = operation == 2 ? left - operand : left + operand;
			bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
			bool const carry = operation == 2 ? u32(source) >= u32(operand) : u64(u32(source)) + u32(operand) > 0xffffffffULL;
			result_st0 = (st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? b ? 0x200 : 0x400 : 0);
			result = u64(ovm ? std::clamp<s64>(sum,-2147483648LL,2147483647LL) : sum) & 0xffffffffffULL;
		}
		u16 const opcode = (operation ? 0x9000 | (operation == 2 ? 0x200 : 0) : 0x9400) | (b ? 0x100 : 0) | (mode << 4) | shift;
		auto initial = initial_case("Xmem-load-ALU-retains-Read-until-Execute", {opcode,0xf4e1});
		for (auto &instruction : initial.program) instruction.address += base;
		initial.data = {{address,operands[pattern]}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_SP,0x4000},{r::STATE_AR0,3},{r::STATE_BK,linear ? 0U : 16U},
			{r::STATE_A,b ? 0x123456789aULL : source},{r::STATE_B,b ? source : 0xfeabcd9876ULL},{r::STATE_T,0xdead}});
		auto access = observation("Access-publishes-one-pointer-update", {});
		auto read = observation("Read-does-not-publish-accumulator", {{int(b ? r::STATE_B : r::STATE_A),source}});
		auto execute = observation("Execute-publishes-result-and-arithmetic-flags", {{int(b ? r::STATE_B : r::STATE_A),result},{r::STATE_ST0,result_st0}});
		auto final = observation("one-read-no-writes-and-retained-result", {{STATE_GENPC,base + 0x602},
			{r::STATE_ST0,result_st0},{r::STATE_ST1,st1},{r::STATE_SP,0x4000},{r::STATE_AR0,3},
			{r::STATE_BK,linear ? 0U : 16U},{r::STATE_T,0xdead},
			{r::STATE_A,b ? 0x123456789aULL : result},{r::STATE_B,b ? result : 0xfeabcd9876ULL}},{{address,0xdead}});
		for (unsigned ar = 2; ar <= 5; ++ar) {
			u16 const pointer = 0x100f + 0x100 * (ar - 2);
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),pointer});
			access.expected.push_back({int(r::STATE_AR0) + int(ar),ar == 2 + (mode & 3) ? next : pointer});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == 2 + (mode & 3) ? 0x2222U : pointer});
		}
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(access)},{5,std::move(read)},
			{6,std::move(execute)},{12,std::move(final)}},3 + parity % 3,12};
		c.actions = {{4,{{int(r::STATE_AR0) + 2 + int(mode & 3),0x2222}}},{5,{},false,{}, {},{{address,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{{false,address,operands[pattern],5}};
		c.program_events = {{false,base + 0x600,opcode,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
