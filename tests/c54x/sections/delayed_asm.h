// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned control = 0; control < 4; ++control)
	for (unsigned form = 0; form < 5; ++form)
	for (unsigned raw = 0; raw < 32; ++raw)
	for (unsigned negative = 0; negative < 2; ++negative)
	for (unsigned flags = 0; flags < 4; ++flags)
	{
		unsigned const prefix = control == 3, extended = form >= 3;
		unsigned const read = 7 + extended + unsigned(extended && (control == 1 || control == 2)) + prefix;
		unsigned const literal = 5 + unsigned(control == 1 || control == 2) + prefix;
		unsigned const target = control == 2 ? 0x604 : 0x700, store = target + extended;
		u32 const base = (raw + form + flags + negative + control) & 1 ? 0x20000 : 0;
		u16 const source = 0xa5a0 | raw;
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (form == 1 ? 0x4000 : 0);
		u64 const b = negative ? 0xfffffffff9ULL : 7;
		// ASM is signed five-bit; SXM affects right shifts, OVM does not saturate LD.
		int const shift = raw < 16 ? int(raw) : int(raw) - 32;
		u64 const result = shift >= 0 ? (b << shift) & 0xffffffffffULL :
			negative && (flags & 1) ? u64(-((7 + (s64(1) << -shift) - 1) >> -shift)) & 0xffffffffffULL : b >> -shift;
		auto initial = initial_case("delayed-asm", {0xf495});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa620},
			{r::STATE_ST1,unsigned(st1 | (raw ^ 31))},{r::STATE_A,prefix ? 0x1234000000ULL | source : control == 1 ? 0 : control == 2 ? 1 : 0x123456789aULL},
			{r::STATE_B,b},{r::STATE_T,0x5aa5},{r::STATE_SP,0x1002}});
		for (unsigned i = 0; i < 8; ++i)
			initial.initial.push_back({r::STATE_AR0 + i,i == 3 ? form == 3 ? 0xfffU : 0x1002U : 0x5100 + i});
		unsigned const start = 0x600 + prefix;
		auto put = [&](unsigned address, u16 value) {
			for (auto &word : initial.program) if (word.address == base + address) { word.value = value; return; }
			initial.program.push_back({base + address,value});
		};
		if (prefix) put(0x600,form == 1 ? 0x8000 : 0x8002);
		put(start,control == 1 || control == 2 ? 0xfa45 : 0xf273);
		put(start + 1,0x700);
		put(start + 2,std::array<u16,5>{0x3202,0x3200,0x3293,0x32e3,0x32f8}[form]);
		put(start + 3,extended ? form == 3 ? 3 : 0x1002 : 0xf682);
		if (extended) put(target,0xf682);
		put(store,0x880f); put(store + 1,0xf4e1);
		initial.data.push_back({0x1002,prefix ? u16(0xdead) : source});
		clocked_register_case c{{{0,std::move(initial)}},
			std::array<unsigned,3>{3 + prefix,read,read + 1}[(raw + form + flags + negative + control) % 3],24};
		for (unsigned tick = 1; tick <= 24; ++tick)
		{
			auto state = observation("asm-publication", {{r::STATE_ST1,st1 | (tick > read ? raw : raw ^ 31)}});
			if (tick == read - 1) state.expected.push_back({r::STATE_AR3,form == 2 ? 0x1003U : form == 3 ? 0xfffU : 0x1002U});
			if (tick == 24) state.expected.insert(state.expected.end(), {{STATE_GENPC,base + store + 2},
				{r::STATE_A,result},{r::STATE_B,b},{r::STATE_TRN,u16(result)},{r::STATE_SP,0x1002},
				{r::STATE_ST0,0xa620},{r::STATE_T,0x5aa5},{r::STATE_AR3,0x4444}});
			c.states.push_back({tick,std::move(state)});
			clocked_register_action action{tick,{}};
			if (extended && tick == literal) action.program.push_back({base + start + 3,0xdead});
			if (tick == read - 1) action.values.push_back({r::STATE_AR3,0x4444});
			if (tick == read) action.data.push_back({0x1002,0xdead});
			if (!action.program.empty() || !action.values.empty() || !action.data.empty()) c.actions.push_back(std::move(action));
		}
		c.data_events = std::vector<clocked_data_event>{};
		if (prefix) c.data_events->push_back({true,0x1002,source,6});
		c.data_events->push_back({false,0x1002,source,read});
		c.data_events->push_back({true,0xf,u16(result),0,false});
		if (extended) c.program_events.push_back({false,base + start + 3,u16(form == 3 ? 3 : 0x1002),literal});
		c.clear_pipeline_before_restore = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
