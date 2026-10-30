// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include "tms320c54x_test_smem_address.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0,0xffffffff,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x56783933,0x80aa001234ULL};
	for (unsigned cohort = 0; cohort < 3; ++cohort)
	for (unsigned sample = 0; sample < (cohort == 0 ? 1088U : cohort == 1 ? 6144U : 3072U); ++sample) {
		bool const producer = cohort == 2;
		unsigned kind = 0, gap = 0, selector, code, memory_pattern, pattern, flags, form, reg, cmpt = 0;
		if (producer) {
			kind = sample % 3; gap = (sample / 3) % 2;
			code = std::array<unsigned,8>{0,1,15,16,31,48,49,63}[(sample / 6) % 8];
			memory_pattern = (sample / 48) % 4; pattern = (sample / 48) % 8;
			flags = (sample / 192) % 4; selector = sample / 768; form = 2; reg = 1;
		} else if (cohort == 1) {
			unsigned const index = sample % 48;
			code = index < 32 ? index : index + 16;
			memory_pattern = (sample / 48) % 4; pattern = (sample / 192) % 8;
			selector = sample / 1536; flags = (code + memory_pattern + pattern + selector) % 4; form = 0; reg = 2;
		} else {
			unsigned const slot = sample % 17; form = slot < 5 ? slot : slot + 1;
			reg = (sample / 17) % 8; cmpt = (sample / 136) % 2; selector = sample / 272;
			unsigned const index = sample % 48; code = index < 32 ? index : index + 16;
			memory_pattern = (sample / 8) % 4; pattern = (sample / 96) % 8; flags = (sample / 64) % 4;
		}
		unsigned const prefix = producer ? (kind == 0 ? 2 : 1) + gap : 0;
		auto const f = smem_address(form,reg,cmpt,pattern,flags & 1);
		unsigned const read = f.read + prefix;
		u16 const memory = std::array<u16,4>{0,1,0x8000,0xffff}[memory_pattern], t = 0xa500 | code;
		// STLM with no intervening word retains the earlier count: a projection, not silicon evidence.
		unsigned const sampled_code = producer && kind == 2 && !gap ? 15 : code;
		int const shift = sampled_code >= 32 ? int(sampled_code) - 64 : int(sampled_code);
		s64 const number = flags & 1 ? s64(s16(memory)) : s64(memory);
		u64 shifted = u64(number) & 0xffffffffffULL;
		if (shift >= 0) shifted = (shifted << shift) & 0xffffffffffULL;
		else { s64 const divisor = s64(1) << -shift;
			shifted = flags & 1 ? u64(number / divisor - (number < 0 && number % divisor != 0)) & 0xffffffffffULL : shifted >> -shift; }
		u64 const a = producer && kind == 2 ? t : selector & 1 ? 0x123456789aULL : values[pattern];
		u64 const b = selector & 1 ? values[pattern] : 0xfeabcd9876ULL;
		u64 const left_bits = selector & 1 ? b : a;
		s64 const left = left_bits & 0x8000000000ULL ? s64(left_bits) - (s64(1) << 40) : s64(left_bits);
		s64 const right = shifted & 0x8000000000ULL ? s64(shifted) - (s64(1) << 40) : s64(shifted);
		s64 const sum = selector & 2 ? left - right : left + right;
		bool const carry = selector & 2 ? u32(left_bits) >= u32(shifted) : u64(u32(left_bits)) + u32(shifted) > 0xffffffffULL;
		bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
		u64 const result = u64(overflow && (flags & 2) ? std::clamp(sum,s64(-2147483648LL),s64(2147483647LL)) : sum) & 0xffffffffffULL;
		u16 const st0 = 0x1804 | (cmpt ? 0x6000 : 0) | (pattern & 1 ? 0x600 : 0);
		u16 const addressed_st0 = cmpt && reg && form >= 2 && form != 17 ? (st0 & 0x1fff) | (reg << 13) : st0;
		u16 const final_st0 = (addressed_st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? selector & 1 ? 0x200 : 0x400 : 0);
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (cmpt ? 0x20 : 0) | (form == 1 ? 0x4000 : 0);
		u32 const base = sample & 1 ? 0x20000 : 0;
		auto initial = initial_case("T-controlled-arithmetic", {0xf495});
		if (producer) { initial.program[0].value = kind == 0 ? 0x770e : kind == 1 ? 0x3082 : 0x880e;
			if (kind == 0) initial.program[1].value = t; }
		initial.program[prefix].value = 0x0400 | (selector & 2 ? 0x0800 : 0) | ((selector & 1) << 8) | f.mode;
		if (form >= 14) initial.program[prefix + 1].value = f.literal;
		initial.program[prefix + 1 + unsigned(form >= 14)].value = 0xf4e1;
		for (auto &word : initial.program) word.address += base;
		initial.data = {{f.address,memory}};
		if (producer && kind == 1) initial.data.push_back({0x5100,t});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_T,producer ? 15U : unsigned(t)},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_BK,f.bk},{r::STATE_SP,0x1000}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,producer && kind == 1 && ar == 2 ? 0x5100U : 0x5007U});
		clocked_register_case c{{{0,std::move(initial)}},producer ? std::array<unsigned,3>{read - 1,read,read + 1}[sample % 3] :
			std::array<unsigned,3>{3,read,read + 1}[sample % 3],24};
		for (unsigned tick = 1; tick <= 24; ++tick) {
			unsigned const publish_t = !producer ? 0 : kind == 2 ? 6 : 5;
			auto state = observation("retained-shifted-operand-and-ALU-status", {{r::STATE_A,!(selector & 1) && tick > read ? result : a},
				{r::STATE_B,(selector & 1) && tick > read ? result : b},{r::STATE_ST0,tick > read ? final_st0 : tick >= read - 1 ? addressed_st0 : st0},
				{r::STATE_ST1,st1},{r::STATE_T,tick > read ? unsigned(t ^ 63) : tick >= publish_t ? unsigned(t) : 15U}});
			for (unsigned ar = 0; ar < 8; ++ar) state.expected.push_back({int(r::STATE_AR0 + ar),ar == f.selected && tick >= read - 1 ?
				tick == read - 1 ? unsigned(f.next) : 0x2222U : producer && kind == 1 && ar == 2 ? 0x5100U : 0x5007U});
			c.states.push_back({tick,std::move(state)});
		}
		if (producer ? kind == 0 : form >= 14) c.actions.push_back({4,{},false,{{base + 0x601,0xdead}}});
		c.actions.push_back({read - 1,{{int(r::STATE_AR0 + f.selected),0x2222}}});
		c.actions.push_back({read,{{r::STATE_T,unsigned(t ^ 63)}},false,{},{},{{f.address,0xdead}}});
		c.data_events = std::vector<clocked_data_event>{};
		// With no STLM gap, TS Read precedes the older mapped Write at clock6.
		bool const overlapping_stlm = producer && kind == 2 && !gap;
		if (overlapping_stlm) c.data_events->push_back({false,f.address,memory,read});
		if (producer) c.data_events->push_back({kind != 1,kind == 1 ? 0x5100U : 0xeU,t,kind == 2 ? 6U : 5U});
		if (!overlapping_stlm) c.data_events->push_back({false,f.address,memory,read});
		if (producer ? kind == 0 : form >= 14) c.program_events = {{false,base + 0x601,producer ? t : f.literal,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
