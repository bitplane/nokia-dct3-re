// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned count : {0U,1U,4U,11U})
	for (unsigned kind = 0; kind != 4; ++kind)
	for (unsigned long_form = 0; long_form != 2; ++long_form)
	for (unsigned irq = 0; irq != 3; ++irq) {
		bool const up = kind == 1 || kind == 3;
		u16 const first = kind >= 2 ? 11 : up ? 0x900 : 0x900 + count;
		u32 const base = (count + kind + long_form + irq) % 2 ? 0x20000 : 0;
		u32 const body = base + (long_form ? 0x602 : 0x601);
		unsigned const write = long_form ? 8 : 7, finish = write + count + 32;
		unsigned const checkpoint = std::array<unsigned,4>{4,write - 1,write,write + count}[(count + kind + long_form + irq) % 4];
		u16 const sp = 0xfff - count;
		auto initial = initial_case("repeated-PSHD-retained-reads-and-deferred-interrupt", {0xf495});
		initial.program = {{base + 0x600,u16(long_form ? 0xf070 : 0xec00 | count)},
			{body,u16(up ? 0x4b97 : 0x4b8f)},{body + 1,0xf4e1}};
		if (long_form) initial.program.push_back({base + 0x601,u16(count)});
		if (irq) {
			u32 const vector = base + (irq == 2 ? 4 : 0x40);
			initial.program.insert(initial.program.end(),{{vector,0x4a1e},{vector + 1,0xf495},{vector + 2,0xf4e5}});
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa600},{r::STATE_ST1,0},
			{r::STATE_IMR,irq == 1 ? 1U : 0U},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},
			{r::STATE_T,0x5678},{r::STATE_TRN,0x9abc}});
		for (unsigned ar = 0; ar != 8; ++ar) initial.initial.push_back({int(r::STATE_AR0) + int(ar),first});
		for (unsigned address = 2; address <= 5; ++address) initial.data.push_back({address,u16(0x9000 + address)});
		auto value = [&](unsigned i) -> u16 {
			if (kind < 2) return 0x5100 + i;
			unsigned const source = up ? first + i : first - i;
			if (source >= 0x10) return first;
			return std::array<u16,16>{u16(irq == 1),u16(irq == 1),0x9002,0x9003,0x9004,0x9005,
				0xa600,0,0x789a,0x3456,0x12,0x9876,0xabcd,0xfe,0x5678,0x9abc}[source];
		};
		auto final = observation("all-pushes-retire-before-interrupt-frame", {{STATE_GENPC,body + 2},
			{r::STATE_SP,sp},{r::STATE_RC,0},{r::STATE_AR7,u16(up ? first + count + 1 : first - count - 1)},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL}});
		for (unsigned i = 0; i <= count; ++i) {
			u16 const source = up ? first + i : first - i;
			if (kind < 2) initial.data.push_back({source,value(i)});
			initial.data.push_back({0xfff - i,0}); final.expected_data.push_back({0xfff - i,value(i)});
		}
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},checkpoint,finish};
		if (irq) c.actions.push_back({5,{},false,{},{{irq == 2 ? INPUT_LINE_NMI : 0,ASSERT_LINE}}});
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned i = 0; i <= count; ++i) {
			u16 const source = up ? first + i : first - i;
			c.data_events->push_back({false,source,value(i),write - 1 + i});
			if (i) c.data_events->push_back({true,0x1000 - i,value(i - 1),write - 1 + i});
			if (kind < 2) c.actions.push_back({write - 1 + i,{},false,{}, {},{{source,0xdead}}});
		}
		c.data_events->push_back({true,0xfff - count,value(count),write + count});
		c.program_events = {{false,body,u16(up ? 0x4b97 : 0x4b8f),0,false}};
		if (irq) {
			u16 const continuation = u16(body + 1);
			c.irqs = {{irq == 2 ? INPUT_LINE_NMI : 0,write + count + 1,~0U,2 * (count + 1),continuation}};
			c.data_events->insert(c.data_events->end(),{{true,u32(sp - 1),continuation,0,false},
				{false,0x1e,u16(base >> 16),0,false},{true,u32(sp - 2),u16(base >> 16),0,false},
				{false,u32(sp - 2),u16(base >> 16),0,false},{false,u32(sp - 1),continuation,0,false}});
		}
		cases.push_back(std::move(c));
	}
	for (u16 source : {0x17,0x18}) {
		auto initial = initial_case("PSHD-rejects-modifying-self-AR-and-SP", {0x4b97,0xf4e1});
		initial.initial.push_back({r::STATE_AR7,source});
		initial.expected.clear();
		auto final = observation("reject-before-allocation-or-data-access", {{r::STATE_SP,0x1000},{r::STATE_AR7,source}});
		final.expected.back() = {r::STATE_ILLEGAL,1};
		clocked_register_case c{{{0,std::move(initial)},{8,std::move(final)}},4,8};
		c.require_idle = false; c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
