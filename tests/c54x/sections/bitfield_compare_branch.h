// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form < 5; ++form)
	for (unsigned index = 0; index < 32; ++index) {
		bool const compare = form >= 3, short_compare = form == 3, peripheral = form == 1 || form == 2, offset = form == 2;
		u16 const value = std::array<u16,4>{0,1,u16(compare ? 0x7f : 2),0x8000}[index % 4];
		u16 const mask = index & 16 ? 0x7f : peripheral ? 2 : 1;
		bool const tc = index & 4, old_tc = index & 8, result = compare ? value == mask : bool(value & mask), taken = result == tc;
		u16 const address = compare ? 0x3762 : peripheral ? 0x49 : 0x17;
		u32 const base = (index + index / 4 + form) & 1 ? 0x20000 : 0;
		u16 const opcode = short_compare ? 0x6081 : compare ? 0x60f8 : offset ? 0x61e1 : 0x61f8;
		u16 const extension = compare ? 0x3762 : peripheral ? offset ? 1 : 0x49 : 0x17;
		auto initial = initial_case("bitfield-compare-branch", {0xf495});
		for (auto &word : initial.program) word.address += base;
		unsigned cursor = 0;
		initial.program[cursor++].value = opcode;
		if (!short_compare) initial.program[cursor++].value = extension;
		initial.program[cursor++].value = mask; initial.program[cursor++].value = 0xf820 | (tc ? 0x10 : 0);
		initial.program[cursor++].value = 0x700; initial.program[cursor++].value = 0xe811; initial.program[cursor++].value = 0xf4e1;
		initial.program.insert(initial.program.end(),{{base + 0x700,0xe855},{base + 0x701,0xf4e1}});
		if (compare || peripheral) initial.data = {{address,value}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_AR7,value},
			{r::STATE_AR1,short_compare ? 0x3762U : 0x48U},{r::STATE_B,0x5678},{r::STATE_ST0,u64(0xa608 | (old_tc ? 0x1000 : 0))}});
		unsigned const read = short_compare ? 6 : peripheral ? 8 : 7;
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{short_compare ? 3U : 5U,read,read + 1}[(index + form) % 3],30};
		for (unsigned tick = 1; tick <= read + 1; ++tick) c.states.push_back({tick,observation("TC-owned-by-retained-source", {
			{r::STATE_ST0,u64(0xa608 | ((tick <= read ? old_tc : result) ? 0x1000 : 0))}})});
		auto final = observation("adjacent-branch-consumes-TC", {{r::STATE_A,taken ? 0x55U : 0x11U},{r::STATE_B,0x5678},
			{r::STATE_AR7,0x7777},{r::STATE_ST0,u64(0xa608 | (result ? 0x1000 : 0))},{STATE_GENPC,base + (taken ? 0x702 : 0x600 + cursor)}});
		if (short_compare) final.expected.push_back({r::STATE_AR1,0x7777});
		c.states.push_back({30,std::move(final)});
		for (unsigned tick = 1; tick <= read; ++tick) {
			clocked_register_action action{tick,{},false};
			if (short_compare && tick == 5) action.values.push_back({r::STATE_AR1,0x7777});
			if (compare && tick == 6) {
				action.program.push_back({base + 0x601,0xdead});
				if (!short_compare) action.program.push_back({base + 0x602,0xbeef});
			}
			if (tick == read) { action.values.push_back({r::STATE_AR7,0x7777}); if (compare || peripheral) action.data.push_back({address,0x7777}); }
			if (!action.values.empty() || !action.program.empty()) c.actions.push_back(std::move(action));
		}
		c.data_events = std::vector<clocked_data_event>{{false,address,value,read}};
		if (compare) {
			c.program_events.push_back({false,base + 0x601,short_compare ? mask : extension,0,false});
			if (!short_compare) c.program_events.push_back({false,base + 0x602,mask,0,false});
		}
		if (taken) c.program_events.push_back({false,base + 0x700,0xe855,read + 3});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
