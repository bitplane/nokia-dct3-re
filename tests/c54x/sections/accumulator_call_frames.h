// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_HIGH_PROGRAM
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr unsigned callers[]{0,2,0x7f}, hints[]{0,3,0x7f};
	std::vector<clocked_register_case> cases;
	for (unsigned near = 0; near != 2; ++near) for (unsigned producer = 0; producer != 2; ++producer)
		for (unsigned b = 0; b != 2; ++b) for (unsigned caller : callers) for (unsigned hint : hints) for (unsigned mode = 0; mode != 3; ++mode) {
			unsigned const frame = near ? 1 : 2, prefix = producer ? 1+mode : 0, interrupt = producer ? 0 : mode;
			u16 const continuation = 0x601+prefix;
			u32 const base = caller << 16, target = (near ? caller : hint) << 16, destination = target | 0x700;
			auto initial = initial_case("CALA-FCALA-captured-target-frame-and-producer-or-IRQ",{0xf495});
			for (auto &word : initial.program) word.address += base;
			initial.program[prefix].value = (near ? 0xf4e3 : 0xf4e7) | (b << 8);
			initial.program[prefix+1].value = 0xf4e1;
			if (producer) initial.program[0].value = b ? 0x5700 : 0x5600;
			initial.program.insert(initial.program.end(),{{destination,0x880e},{destination+1,u16(near ? 0xfc00 : 0xf4e4)},
				{target+4,0xf4eb},{target+0x40,0xf4eb}});
			u64 const accumulator = producer ? 0xdeadbeef : 0x5500000700ULL | (u64(hint) << 16);
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,b ? 0x1234 : accumulator},
				{r::STATE_B,b ? accumulator : 0x2345},{r::STATE_T,0xface},{r::STATE_SP,0x1000},
				{r::STATE_ST0,producer ? 0xa628U : 0xa608U},{r::STATE_ST1,interrupt == 1 ? 0U : 0x800U},{r::STATE_IMR,interrupt == 1 ? 1U : 0U}});
			initial.data = {{0xfff,0xaaaa},{0xffe,0xbbbb},{0xffd,0xcccc},{0x1400,u16(0x8000 | hint)},{0x1401,0x700}};
			auto final = observation("callee-once-owned-frame-roundtrip-and-caller-page",{{STATE_GENPC,base+0x602+prefix},
				{r::STATE_A,b ? 0x1234U : 0xdeadbeefU},{r::STATE_B,b ? 0xdeadbeefU : 0x2345U},
				{r::STATE_T,b ? 0x1234U : 0xbeefU},{r::STATE_SP,0x1000},{r::STATE_ST0,producer ? 0xa628U : 0xa608U},
				{r::STATE_ST1,interrupt ? 0U : 0x800U}});
			final.expected_data = {{0xfff,continuation}};
			if (!near) final.expected_data.push_back({0xffe,u16(caller)});
			if (interrupt) final.expected_data.push_back({0xfff-frame,0x700});
			if (producer) final.expected_data.insert(final.expected_data.end(),{{0x1400,0xdead},{0x1401,0xbeef}});
			unsigned const choice = (b+caller+hint+mode+near+producer)%4;
			unsigned const checkpoint = producer ? std::array<unsigned,4>{4,5+prefix,6+prefix,8+prefix}[choice] :
				interrupt ? std::array<unsigned,4>{4,6,8,12}[choice] : std::array<unsigned,4>{4,5,6,8}[choice];
			clocked_register_case c{{{0,std::move(initial)},{1,observation("accumulator-call-running",{})},{48,std::move(final)}},checkpoint,48};
			c.data_events = std::vector<clocked_data_event>{};
			if (producer) { c.data_events->push_back({false,0x1400,u16(0x8000 | hint),5}); c.data_events->push_back({false,0x1401,0x700,5}); }
			for (unsigned part = 0; part != frame; ++part) c.data_events->push_back({true,0xfff-part,u16(part ? caller : continuation),5+prefix+part});
			if (interrupt) {
				c.data_events->push_back({true,0xfff-frame,0x700,0,false});
				c.data_events->push_back({false,0xfff-frame,0x700,0,false});
				c.irqs = {{interrupt == 2 ? INPUT_LINE_NMI : 0,0,~0U,frame,0x700}};
			}
			// RET's Read precedes the older STLM Execute in their shared clock; FRET consumes its pair later.
			if (near) c.data_events->push_back({false,0xfff,continuation,0,false});
			c.data_events->push_back({true,0xe,u16(b ? 0x1234 : 0xbeef),12+prefix,!interrupt});
			if (!near) for (unsigned part = 0; part != frame; ++part) c.data_events->push_back({false,0x1000-frame+part,u16(part ? continuation : caller),0,false});
			c.program_events = {{false,destination,0x880e,8+prefix,!interrupt}};
			if (interrupt) c.program_events.push_back({false,destination,0x880e,0,false});
			if (producer) c.actions.push_back({5,{},false,{}, {},{{0x1400,0xdead},{0x1401,0xbeef}}});
			c.actions.push_back({5+prefix,{{b ? r::STATE_B : r::STATE_A,0xdeadbeef}}});
			if (interrupt) c.actions.back().input_lines = {{interrupt == 2 ? INPUT_LINE_NMI : 0,ASSERT_LINE},{interrupt == 2 ? INPUT_LINE_NMI : 0,CLEAR_LINE}};
			cases.push_back(std::move(c));
		}
	return cases;
}
}
