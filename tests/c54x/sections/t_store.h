// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include "tms320c54x_test_shifted_word.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0,1,0x000f000f,0x001f001f,0x00300030,0x003f003f,0x0180008000ULL,0xff7fff8000ULL};
	for (unsigned cohort = 0; cohort < 3; ++cohort)
	for (unsigned sample = 0; sample < (cohort == 0 ? 256U : cohort == 1 ? 128U : 2560U); ++sample) {
		bool const consumer = cohort == 2;
		unsigned const form = cohort == 0 ? sample % 4 : 0, reg = cohort == 0 ? (sample / 4) % 8 : 2;
		unsigned const kind = cohort == 0 ? (sample / 32) % 4 : cohort == 1 ? sample % 4 : (sample / 5) % 4;
		unsigned const cmpt = cohort == 0 ? sample / 128 : 0, selected = cmpt && !reg ? 4 : reg;
		unsigned const pattern = cohort == 0 ? (sample / 8 + kind) % 8 : cohort == 1 ? (sample / 4) % 8 : (sample / 20) % 8;
		unsigned const flags = cohort == 0 ? (sample / 16 + form) % 4 : cohort == 1 ? sample / 32 : (sample / 160) % 4;
		unsigned const layout = consumer ? sample % 5 : 0, gap = layout == 1 || layout == 3;
		bool const long_consumer = consumer && (layout == 2 || layout == 3), merge = consumer && layout == 4;
		unsigned const read = 7 + gap + unsigned(long_consumer);
		u16 const memory = consumer ? std::array<u16,4>{0,1,0x8000,0xffff}[sample / 640] : 0;
		u64 const value = values[pattern];
		u32 stored = u32(value);
		if (flags & 1) {
			if (flags & 2) { s64 const number = value & 0x8000000000ULL ? s64(value) - (s64(1) << 40) : s64(value);
				stored = u32(std::clamp(number,s64(-2147483648LL),s64(2147483647LL))); }
			else stored = u32(std::min(value,u64(0xffffffff)));
		}
		u16 const word = u16(stored >> (kind & 2 ? 16 : 0));
		u64 const result = shifted_word40(memory,layout == 0 ? 3 : word,flags & 2);
		u16 const mode = form == 0 ? 0xf8 : (0xe0 + (form - 1) * 8) | reg;
		u16 const literal = form == 0 ? 0xe : form == 1 ? 0xffce : 1;
		u16 const st1 = 0x800 | (flags & 2 ? 0x100 : 0) | (cmpt ? 0x20 : 0);
		u16 const final_st0 = cmpt && reg && form ? (0x8e20 & 0x1fff) | (reg << 13) : 0x8e20;
		std::array<u16,8> pointers;
		for (unsigned ar = 0; ar < 8; ++ar) pointers[ar] = 0x2000 + ar;
		pointers[selected] = !form ? 0x4000 + selected : form == 1 ? 0x40 : 0xd;
		u32 const base = sample & 1 ? 0x20000 : 0;
		auto initial = initial_case("mapped-T-store-consumer", {u16(0x8000 | (kind << 8) | mode),literal});
		if (consumer) {
			initial.program[2 + gap].value = merge ? 0x3002 : 0x1400 | (!(kind & 1) << 8) | (long_consumer ? 0xf8 : 2);
			if (long_consumer) initial.program[3 + gap].value = 0x1002;
		}
		initial.program[2 + (consumer ? gap + 1 + unsigned(long_consumer) : 0)].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{0x1002,memory}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,kind & 1 ? 0x2233445566ULL : value ^ 0xffffffffffULL},
			{r::STATE_B,kind & 1 ? value ^ 0xffffffffffULL : 0x2233445566ULL},{r::STATE_T,3},{r::STATE_BK,16},
			{r::STATE_ST0,0x8e20},{r::STATE_ST1,st1},{r::STATE_PMST,flags & 1},{r::STATE_SP,0x1000}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,0x3300 + ar});
		clocked_register_case c{{{0,std::move(initial)}},consumer ? std::array<unsigned,3>{6,read,read + 1}[sample % 3] :
			std::array<unsigned,3>{3,6,7}[sample % 3],16};
		for (unsigned tick = 1; tick <= 16; ++tick) {
			u64 const source = tick <= 6 ? value ^ 0xffffffffffULL : value;
			u64 const other = consumer && !merge && tick > read ? result : 0x2233445566ULL;
			u16 const expected_t = tick < 7 ? 3 : !consumer ? tick == 7 ? word : 0xbeef :
				tick > read ? merge ? 0xbeef : word ^ 63 : merge && tick == read ? memory : word;
			auto state = observation("Read-address-Execute-data-and-younger-T", {{r::STATE_A,kind & 1 ? other : source},
				{r::STATE_B,kind & 1 ? source : other},{r::STATE_T,expected_t},{r::STATE_ST0,tick >= 6 ? final_st0 : 0x8e20U},
				{r::STATE_ST1,st1},{r::STATE_PMST,flags & 1}});
			for (unsigned ar = 0; ar < 8; ++ar) state.expected.push_back({int(r::STATE_AR0 + ar),tick <= 5 ? 0x3300U + ar :
				tick > 6 ? 0x2200U + ar : form >= 2 && ar == selected ? 0xeU : unsigned(pointers[ar])});
			c.states.push_back({tick,std::move(state)});
		}
		c.actions = {{4,{},false,{{base + 0x601,0xdead}}},{5,{}},{6,{{int(kind & 1 ? r::STATE_B : r::STATE_A),value}}}};
		for (unsigned ar = 0; ar < 8; ++ar) { c.actions[1].values.push_back({r::STATE_AR0 + ar,pointers[ar]});
			c.actions[2].values.push_back({r::STATE_AR0 + ar,0x2200 + ar}); }
		c.actions.push_back({consumer ? read : 7,{{r::STATE_T,!consumer || merge ? 0xbeefU : unsigned(word ^ 63)}}});
		if (consumer) c.actions.back().data.push_back({0x1002,u16(memory ^ 0xffff)});
		c.data_events = std::vector<clocked_data_event>{};
		if (consumer && read == 7) c.data_events->push_back({false,0x1002,memory,read});
		c.data_events->push_back({true,0xe,word,7});
		if (consumer && read != 7) c.data_events->push_back({false,0x1002,memory,read});
		c.program_events = {{false,base + 0x601,literal,3}};
		if (long_consumer) c.program_events.push_back({false,base + 0x603 + gap,0x1002,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
