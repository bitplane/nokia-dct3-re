// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include "tms320c54x_test_smem_address.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned cohort = 0; cohort < 3; ++cohort)
	for (unsigned sample = 0; sample < (cohort == 0 ? 544U : cohort == 1 ? 1024U : 768U); ++sample) {
		bool const producer = cohort == 2;
		unsigned const destination = sample & 1;
		unsigned const pattern = producer ? (sample / 2) % 4 : cohort == 1 ? (sample / 2) % 4 : sample % 4;
		unsigned const code = producer ? std::array<unsigned,8>{0,1,15,16,31,32,48,63}[(sample / 8) % 8] :
			cohort == 1 ? (sample / 8) % 64 : (sample * 13 + sample / 17) % 64;
		unsigned const flags = producer ? (sample / 64) % 4 : cohort == 1 ? ((sample / 512) & 1) | ((sample / 16) & 2) : (sample / 4) % 4;
		unsigned const kind = producer ? sample / 256 : 0, prefix = producer ? kind == 1 ? 1 : 2 : 0;
		unsigned const slot = (sample / 2) % 17, form = cohort == 0 ? slot < 5 ? slot : slot + 1 : 2;
		unsigned const reg = cohort == 0 ? (sample / 34) % 8 : 1, cmpt = cohort == 0 ? sample / 272 : 0;
		auto const f = smem_address(form,reg,cmpt,pattern,flags & 1);
		unsigned const read = f.read + prefix;
		u16 const memory = std::array<u16,4>{0,1,0x8000,0xffff}[pattern], t = 0xa500 | code;
		int const shift = code >= 32 ? int(code) - 64 : int(code);
		s64 const number = flags & 1 ? s64(s16(memory)) : s64(memory);
		u64 result = u64(number) & 0xffffffffffULL;
		if (shift >= 0) result = (result << shift) & 0xffffffffffULL;
		else {
			s64 const divisor = s64(1) << -shift;
			result = flags & 1 ? u64(number / divisor - (number < 0 && number % divisor != 0)) & 0xffffffffffULL : result >> -shift;
		}
		u16 const st0 = 0x1804 | (cmpt ? 0x6000 : 0) | (pattern & 1 ? 0x600 : 0);
		u16 const addressed_st0 = cmpt && reg && form >= 2 && form != 17 ? (st0 & 0x1fff) | (reg << 13) : st0;
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (cmpt ? 0x20 : 0) | (form == 1 ? 0x4000 : 0);
		u32 const base = sample & 2 ? 0x20000 : 0;
		auto initial = initial_case("T-controlled-load", {0xf495});
		for (auto &word : initial.program) word.address += base;
		if (producer) {
			initial.program[0].value = kind == 0 ? 0x770e : kind == 1 ? 0x3082 : 0x880e;
			if (kind == 0) initial.program[1].value = t;
		}
		initial.program[prefix].value = 0x1400 | (destination << 8) | f.mode;
		if (!producer && form >= 14) initial.program[1].value = f.literal;
		initial.program[prefix + (form >= 14 ? 2 : 1)].value = 0xf4e1;
		initial.data = {{f.address,memory}};
		if (producer && kind == 1) initial.data.push_back({0x5100,t});
		u64 const a = producer && kind == 2 ? t : 0x123456789aULL, b = 0x2233445566ULL;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_T,producer ? 0xfU : unsigned(t)},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_BK,f.bk},{r::STATE_SP,0x1000}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,producer && ar == 2 ? 0x5100U : 0x5007U});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{3,read - 1,read}[sample % 3],24};
		for (unsigned tick = 1; tick <= 24; ++tick) {
			unsigned const publish_t = !producer ? 0 : kind == 2 ? 6 : 5;
			auto state = observation("retained-T-memory-and-pointer", {{r::STATE_A,!destination && tick > read ? result : a},
				{r::STATE_B,destination && tick > read ? result : b},{r::STATE_ST0,tick >= read - 1 ? addressed_st0 : st0},
				{r::STATE_ST1,st1},{r::STATE_T,tick > read ? unsigned(t ^ 0x3f) : tick >= publish_t ? unsigned(t) : 0xfU}});
			for (unsigned ar = 0; ar < 8; ++ar) state.expected.push_back({int(r::STATE_AR0 + ar),ar == f.selected && tick >= read - 1 ?
				tick == read - 1 ? unsigned(f.next) : 0x2222U : producer && ar == 2 ? 0x5100U : 0x5007U});
			c.states.push_back({tick,std::move(state)});
		}
		if (producer ? kind == 0 : form >= 14) c.actions.push_back({4,{},false,{{base + 0x601,0xdead}}});
		c.actions.push_back({read - 1,{{int(r::STATE_AR0 + f.selected),0x2222}}});
		c.actions.push_back({read,{{r::STATE_T,unsigned(t ^ 0x3f)}},false,{},{},{{f.address,0xdead}}});
		c.data_events = std::vector<clocked_data_event>{};
		if (producer) c.data_events->push_back({kind != 1,kind == 1 ? 0x5100U : 0xeU,t,kind == 2 ? 6U : 5U});
		c.data_events->push_back({false,f.address,memory,read});
		if (producer ? kind == 0 : form >= 14) c.program_events = {{false,base + 0x601,producer ? t : f.literal,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
