// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0,1,0xffffffffffULL,0x7fffffff,0x80000000,0xff80000000ULL,0x7fffffffffULL,0x8000000000ULL};
	for (unsigned condition = 0; condition < 14; ++condition)
	for (unsigned pattern = 0; pattern < 8; ++pattern)
	for (unsigned layout = 0; layout < 2; ++layout)
	{
		u64 const raw = values[pattern], a = condition < 6 ? raw : 0x12345678, b = condition >= 6 && condition < 12 ? raw : 0x23456789;
		s64 const value = raw & 0x8000000000ULL ? s64(raw) - (s64(1) << 40) : s64(raw);
		bool const taken = condition >= 12 ? bool(pattern & 1) == (condition == 13) :
			std::array<bool,6>{value >= 0,value < 0,value != 0,value == 0,value > 0,value <= 0}[condition % 6];
		u64 const out_a = layout ? a : 0, out_b = layout || condition >= 12 ? b : 0;
		u16 const st0 = 0xa608 | (pattern & 1 ? 0x1000 : 0), final_st0 = !layout && condition >= 12 ? st0 ^ 0x1000 : st0;
		u16 const sp = 0x1000 + ((condition + pattern) & 1);
		u32 const base = (condition + pattern + layout) & 1 ? 0x20000 : 0;
		u16 const first = layout ? 0x7712 : condition >= 12 ? st0 & 0x1000 ? 0xf4bc : 0xf5bc : 0xe800;
		u16 const second = layout ? 0xf4e1 : condition >= 12 ? 0xe800 : 0xe900;
		auto initial = initial_case("conditional-delayed-return", {
			u16(condition >= 12 ? condition == 13 ? 0xfe30 : 0xfe20 : 0xfe42 + condition % 6 + (condition >= 6 ? 8 : 0)),first,second});
		for (auto &word : initial.program) word.address += base;
		auto put = [&](u32 address, u16 word) {
			for (auto &entry : initial.program) if (entry.address == address) { entry.value = word; return; }
			initial.program.push_back({address,word});
		};
		for (u32 start : {base + 0x603,base + 0x700}) {
			put(start,0x80f8); put(start + 1,0x1004); put(start + 2,0x81f8); put(start + 3,0x1005);
			put(start + 4,start == base + 0x700 ? 0xe955 : 0xe933); put(start + 5,0xf4e1);
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},{r::STATE_ST1,0x900},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0x5aa5},{r::STATE_SP,sp},{r::STATE_AR2,0x1234}});
		initial.data = {{sp,0x700},{0x1004,0xdead},{0x1005,0xdead}};
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,5>{3,4,5,6,8}[(condition + pattern + layout) % 5],48};
		c.states.push_back({48,observation("retained-return-and-delay", {{STATE_GENPC,base + (taken ? 0x706 : 0x609)},
			{r::STATE_SP,sp + unsigned(taken)},{r::STATE_A,out_a},{r::STATE_B,taken ? 0x55U : 0x33U},
			{r::STATE_ST0,final_st0},{r::STATE_ST1,0x900},{r::STATE_T,0x5aa5},{r::STATE_AR2,layout ? 0xf4e1U : 0x1234U}},
			{{0x1004,u16(out_a)},{0x1005,u16(out_b)}})});
		c.actions = {{5,{},false,{}, {},{{sp,0x710}}}};
		c.program_events = {{false,base + 0x601,first,0,false},{false,base + 0x602,second,0,false},
			{false,base + (taken ? 0x700 : 0x603),0x80f8,0,false}};
		c.data_ranges = {{{sp,sp}},{{0x1004,0x1005}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (taken) c.data_events->push_back({false,sp,0x700,5});
		c.data_events->push_back({true,0x1004,u16(out_a),0,false});
		c.data_events->push_back({true,0x1005,u16(out_b),0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
