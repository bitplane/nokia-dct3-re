// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include "tms320c54x_test_smem_address.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 sources[]{0,0xffffffff,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x56783933,0x80aa001234ULL};
	constexpr u16 operands[]{1,1,1,0xffff,0x8000,0xffff,0x1234,0xffff};
	for (unsigned family = 0; family < 3; ++family) for (unsigned selector = 0; selector < (family == 2 ? 8U : 4U); ++selector)
	for (unsigned cohort = 0; cohort < 2; ++cohort) for (unsigned sample = 0; sample < (cohort ? 32U : 272U); ++sample) {
		bool const shift16 = family == 2, unsigned_word = family == 1;
		unsigned const slot = sample % 17, form = cohort ? 2 : slot < 5 ? slot : slot + 1;
		unsigned const reg = cohort ? 3 : (sample / 17) % 8, cmpt = cohort ? 0 : sample / 136;
		unsigned const pattern = cohort ? sample % 8 : (sample + selector + family) % 8;
		unsigned const flags = cohort ? sample / 8 : (sample / 8 + selector) % 4;
		unsigned const destination = selector & 1, source = shift16 ? (selector >> 1) & 1 : destination;
		bool const subtract = selector & (shift16 ? 4 : 2), sxm = flags & 1, ovm = flags & 2;
		auto const f = smem_address(form,reg,cmpt,pattern,sxm);
		u64 const value = sources[pattern];
		u16 const memory = operands[pattern];
		s64 const left = value & 0x8000000000ULL ? s64(value) - (s64(1) << 40) : s64(value);
		s64 const right = (sxm && !unsigned_word ? s64(s16(memory)) : s64(memory)) * (shift16 ? 65536 : 1);
		s64 const sum = subtract ? left - right : left + right;
		bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
		bool const carry = subtract ? u32(value) >= u32(right) : u64(u32(value)) + u32(right) > 0xffffffffULL;
		u64 const result = u64(overflow && ovm ? sum < 0 ? -2147483648LL : 2147483647LL : sum) & 0xffffffffffULL;
		u16 const st0 = (shift16 ? 0x1004 | (pattern & 1 ? 0x800 : 0) : 0x1804) | (cmpt ? 0x6000 : 0) | (pattern & 1 ? 0x600 : 0);
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) | (cmpt ? 0x20 : 0) | (form == 1 ? 0x4000 : 0);
		u16 const addressed_st0 = cmpt && reg && form >= 2 && form != 17 ? (st0 & 0x1fff) | (reg << 13) : st0;
		u16 final_st0 = addressed_st0;
		// Shift16 ADD only sets C; shift16 SUB only clears it. Ordinary forms replace C.
		if (!shift16 || carry != subtract) final_st0 = (final_st0 & ~0x800) | (carry ? 0x800 : 0);
		if (overflow) final_st0 |= destination ? 0x200 : 0x400;
		u16 const opcode = (shift16 ? (subtract ? 0x4000 : 0x3c00) | ((selector & 3) << 8) :
			(unsigned_word ? subtract ? 0x0a00 : 0x0200 : subtract ? 0x0800 : 0) | (destination << 8)) | f.mode;
		u32 const base = (sample + selector) & 1 ? 0x20000 : 0;
		auto initial = initial_case("Smem-arithmetic-capture", {opcode,form >= 14 ? f.literal : u16(0xf4e1),0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.data = {{f.address,memory}};
		std::array<u64,2> accumulators{0x123456789aULL,0x123456789aULL}; accumulators[source] = value;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_BK,f.bk},{r::STATE_SP,0x1000},{r::STATE_A,accumulators[0]},{r::STATE_B,accumulators[1]}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,0x5007});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{3,f.read - 1,f.read}[(sample + selector) % 3],18};
		for (unsigned tick = 1; tick <= 18; ++tick) {
			auto expected = accumulators;
			if (tick > f.read) expected[destination] = result;
			auto state = observation("address-read-arithmetic-publication", {{r::STATE_A,expected[0]},{r::STATE_B,expected[1]},
				{r::STATE_ST0,tick > f.read ? final_st0 : tick >= f.read - 1 ? addressed_st0 : st0},{r::STATE_ST1,st1}});
			for (unsigned ar = 0; ar < 8; ++ar) state.expected.push_back({int(r::STATE_AR0 + ar),ar != f.selected || tick < f.read - 1 ?
				0x5007U : tick == f.read - 1 ? unsigned(f.next) : 0x2222U});
			c.states.push_back({tick,std::move(state)});
		}
		// Extended literals finish Decode at4, independently of the later operand Read.
		if (form >= 14) c.actions.push_back({4,{},false,{{base + 0x601,0xdead}}});
		clocked_register_action pointer{f.read - 1,{{int(r::STATE_AR0 + f.selected),0x2222}}};
		c.actions.push_back(std::move(pointer));
		c.actions.push_back({f.read,{},false,{},{},{{f.address,0xdead}}});
		c.data_events = std::vector<clocked_data_event>{{false,f.address,memory,f.read}};
		if (form >= 14) c.program_events = {{false,base + 0x601,f.literal,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
