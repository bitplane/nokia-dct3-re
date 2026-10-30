// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form < 6; ++form) for (unsigned address = 0x60; address < 0x80; ++address)
	for (u16 value : {0U,1U,0x7fU,0x80U,0xffU,0x7fffU,0x8000U,0xffffU}) for (unsigned stack = 0; stack < 4; ++stack) {
		unsigned const seed = form + address + value + stack, gap = form == 3 ? 1 : form == 4 ? 3 : 0;
		u16 const sp = stack < 2 ? 0x3000 + stack : address + (form == 5 ? 2 : 1) + stack - 2;
		u16 const next = 0x60 | ((address + 1) & 31), st1 = 0x900 | (seed & 1 ? 0x4000 : 0);
		u16 const second = form == 5 && sp - 1 != next ? value ^ 0x5aa5 : value;
		u32 const base = seed & 2 ? 0x20000 : 0;
		auto initial = initial_case("page-zero-stack-address-and-alias", {u16((form == 1 ? 0x8a00 : 0x4a00) | address)});
		if (form >= 2) initial.program[1 + gap].value = form == 5 ? 0x4a00 | next : 0x8a00 | address;
		unsigned const idle = form >= 2 ? 2 + gap : 1;
		initial.program[idle].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{address,value},{next,u16(value ^ 0x5aa5)}};
		if (form == 1) initial.data.push_back({sp,value});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0xbeef},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_SP,sp}});
		auto final = observation("owned-stack-transactions", {{STATE_GENPC,base + 0x601 + idle},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0xbeef},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},
			{r::STATE_SP,u16(form == 0 ? sp - 1 : form == 1 ? sp + 1 : form == 5 ? sp - 2 : sp)}});
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},std::array<unsigned,3>{4,6,9}[seed % 3],16};
		u16 const first_read = form == 1 ? sp : address, first_write = form == 1 ? address : sp - 1;
		c.data_events = std::vector<clocked_data_event>{{false,first_read,value,5},{true,first_write,value,6}};
		c.actions = {{5,{},false,{},{},{{first_read,u16(value ^ 0xffff)}}}};
		if (form >= 2) {
			u16 const second_read = form == 5 ? next : sp - 1, second_write = form == 5 ? sp - 2 : address;
			// A nonalias PSHM source Read precedes the older Write. An alias,
			// like adjacent POPM, observes the pending stack publication first.
			if (form == 5 && second_read != first_write) c.data_events->insert(c.data_events->begin() + 1,{false,second_read,second,6});
			else c.data_events->push_back({false,second_read,second,6 + gap});
			c.data_events->push_back({true,second_write,second,7 + gap});
			c.actions.push_back({6 + gap,{},false,{},{},{{second_read,u16(second ^ 0xffff)}}});
		}
		cases.push_back(std::move(c));
	}
	for (unsigned pop = 0; pop < 2; ++pop) for (unsigned encoding = 0; encoding < 192; ++encoding) {
		u16 const address = encoding < 64 ? 0x20 + encoding : 0x40 + encoding;
		u32 const base = encoding & 1 ? 0x20000 : 0;
		auto initial = initial_case("page-zero-stack-implementation-exclusion", {u16((pop ? 0x8a00 : 0x4a00) | address),0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0xbeef},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x4900},{r::STATE_SP,0x3000}});
		auto final = observation("reject-before-source-or-stack-effect", {{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0xbeef},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x4900},{r::STATE_SP,0x3000}});
		final.expected.pop_back(); final.expected.push_back({r::STATE_ILLEGAL,1});
		clocked_register_case c{{{0,std::move(initial)},{3,std::move(final)}},encoding & 1 ? 7U : 4U,16};
		c.require_idle = false; c.forbid_data_access = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
