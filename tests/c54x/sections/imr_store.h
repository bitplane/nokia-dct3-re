// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned writer = 0; writer != 3; ++writer)
	for (unsigned source = 0; source != 16; ++source)
	for (unsigned pattern = 0; pattern != 4; ++pattern)
	for (unsigned enabled = 0; enabled != 2; ++enabled)
	for (unsigned masked = 0; masked != 2; ++masked)
	for (unsigned kind = 0; kind != 3; ++kind) {
		u16 const bit = 1U << source, old_mask = enabled ? 0xffff : 0;
		u16 const mask = std::array<u16,4>{0,0xffff,bit,u16(~bit)}[pattern];
		unsigned const write = writer ? 6 : 7;
		bool const accepted = kind == 2 || (kind == 1 && !masked && (enabled || (mask & bit)));
		unsigned const ack = !accepted ? 0 : kind == 2 || enabled ? write : write + 3;
		unsigned const seed = writer + source + pattern + enabled + masked + kind;
		u32 const base = seed % 2 ? 0x20000 : 0, vector = base + (kind == 2 ? 4 : (16 + source) * 4);
		auto initial = initial_case("IMR-publication-retains-pending-or-selected-entry", {u16(writer == 0 ? 0x7700 : writer == 1 ? 0x8800 : 0x8900)});
		for (auto &word : initial.program) word.address += base;
		if (!writer) initial.program[1].value = mask;
		initial.program.insert(initial.program.end(),{{vector,0xe955},{vector + 1,0xf4e1}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_IMR,old_mask},
			{r::STATE_ST0,0x1804},{r::STATE_ST1,masked ? 0x800U : 0U},
			{r::STATE_A,writer == 2 ? 0x1234U : writer == 1 ? unsigned(mask ^ 0xffff) : 0U},
			{r::STATE_B,writer == 2 ? unsigned(mask ^ 0xffff) : 0U}});
		auto final = observation("mask-clearing-cannot-cancel-selected-entry", {{r::STATE_IMR,mask},{r::STATE_ST0,0x1804},
			{r::STATE_IFR,kind == 1 && !accepted ? bit : 0U},
			{r::STATE_B,accepted ? 0x55U : writer == 2 ? mask : 0U}});
		if (writer) final.expected.push_back({r::STATE_A,writer == 2 ? 0x1234U : mask});
		clocked_register_case c{{{0,std::move(initial)},{write - 1,observation("old-mask-before-Execute", {{r::STATE_IMR,old_mask}})},
			{write,observation("new-mask-at-Execute", {{r::STATE_IMR,mask}})},{20,std::move(final)}},
			(writer ? std::array<unsigned,3>{4,5,6} : std::array<unsigned,3>{4,6,7})[seed % 3],20};
		c.require_idle = false;
		if (kind) {
			int const line = kind == 2 ? INPUT_LINE_NMI : int(source);
			c.actions.push_back({2,{},false,{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}});
		}
		if (!writer) {
			c.actions.push_back({4,{},false,{{base + 0x601,0xbeef}}});
			c.program_events = {{false,base + 0x601,mask,0,false}};
		} else c.actions.push_back({5,{{int(writer == 2 ? r::STATE_B : r::STATE_A),mask}}});
		// Like the old fixture, this observes IMR ownership, not independent interrupt-frame RAM traffic.
		c.data_ranges = {{0,0}};
		c.data_events = std::vector<clocked_data_event>{{true,0,mask,write}};
		if (accepted) c.irqs = {{kind == 2 ? INPUT_LINE_NMI : int(source),ack,ack,unsigned(ack > write),{}}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
