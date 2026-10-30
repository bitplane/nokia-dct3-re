// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0,1,0xffffffffffULL,0x7fffffff,0xff80000000ULL,0x0180000000ULL,0x8000000000ULL,0x7fffffffffULL};
	for (unsigned index = 0; index < 5120; ++index) {
		unsigned const selector = index % 4, raw = (index / 4) % 32, flags = (index / 128) % 4, form = (index / 512) % 5;
		bool const prefix = index / 2560, b = selector & 1;
		u64 const value = values[(raw + form + selector) % 8];
		int const shift = int(raw) - (raw >= 16 ? 32 : 0);
		u64 shifted = value;
		if (shift >= 0) shifted = (shifted << shift) & 0xffffffffffULL;
		else if ((flags & 1) && (value & 0x8000000000ULL)) {
			s64 const signed_value = s64(value) - (s64(1) << 40), divisor = s64(1) << -shift;
			shifted = u64(signed_value / divisor - (signed_value % divisor != 0)) & 0xffffffffffULL;
		} else shifted >>= -shift;
		u32 stored = u32(shifted);
		if (flags & 2) {
			if (flags & 1) { s64 const signed_value = shifted & 0x8000000000ULL ? s64(shifted) - (s64(1) << 40) : s64(shifted); stored = u32(std::clamp(signed_value,s64(-2147483648LL),s64(2147483647LL))); }
			else stored = u32(std::min(shifted,u64(0xffffffff)));
		}
		u16 const word = u16(stored >> (selector & 2 ? 16 : 0)), st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (form == 1 ? 0x4000 : 0);
		u32 const base = (selector + raw + form) & 1 ? 0x20000 : 0;
		u16 const opcode = 0x8400 | (selector << 8) | (form == 2 ? 0x93 : form == 3 ? 0xad : form == 4 ? 0xb5 : 2);
		auto initial = initial_case("ASM-store-capture", {u16(prefix ? 0xed00 | raw : opcode),u16(prefix ? opcode : 0xf4e1),0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{0x1002,0xbeef}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa020},{r::STATE_PMST,flags & 2 ? 1U : 0U},
			{r::STATE_ST1,u64(st1 | (prefix ? raw ^ 31 : raw))},{r::STATE_SP,0x1000},{r::STATE_T,0x5aa5},
			{r::STATE_AR0,3},{r::STATE_AR3,0x1002},{r::STATE_AR5,0x1002},
			{r::STATE_A,b ? 0x33445566 : value ^ 0xffffffffffULL},{r::STATE_B,b ? value ^ 0xffffffffffULL : 0x33445566}});
		unsigned const read = prefix ? 6 : 5;
		clocked_register_case c{{{0,std::move(initial)}},read - 1 + (selector + raw + form + flags) % 3,16};
		for (unsigned tick = 1; tick <= 16; ++tick) c.states.push_back({tick,observation("ASM-and-address-owned-by-Read", {
			{r::STATE_ST1,u64(st1 | (tick > read || (prefix && tick < read) ? raw ^ 31 : raw))},
			{r::STATE_ST0,0xa020},{r::STATE_PMST,flags & 2 ? 1U : 0U},{r::STATE_SP,0x1000},{r::STATE_T,0x5aa5},
			{r::STATE_AR3,tick < read ? 0x1002U : tick == read ? 0x1002 + unsigned(form == 2) : 0x2200U},
			{r::STATE_AR5,tick < read ? 0x1002U : tick == read ? form == 3 ? 0xfffU : form == 4 ? 0x1005U : 0x1002U : 0x2300U}})});
		c.states.back().observation.expected.insert(c.states.back().observation.expected.end(), {
			{r::STATE_A,b ? 0x33445566 : value},{r::STATE_B,b ? value : 0x33445566}});
		c.actions = {{read,{{r::STATE_ST1,u64(st1 | (raw ^ 31))},{r::STATE_AR3,0x2200},{r::STATE_AR5,0x2300},
			{r::STATE_AR0,0xdead},{int(b ? r::STATE_B : r::STATE_A),value}},false}};
		c.data_events = std::vector<clocked_data_event>{{true,0x1002,word,read + 1}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
