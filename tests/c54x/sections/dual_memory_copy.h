// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned repeated = 0; repeated < 2; ++repeated) for (unsigned encoding = 0; encoding < 256; ++encoding)
	for (unsigned layout = 0; layout < 3; ++layout) for (unsigned linear = 0; linear < 2; ++linear)
	for (unsigned option = 0; option < (repeated ? 6U : 4U); ++option) {
		unsigned const x = encoding >> 4, y = encoding & 15, seed = encoding + layout + linear + option;
		unsigned const count = repeated ? std::array<unsigned,3>{0,1,15}[option % 3] : 0;
		unsigned const prefix = repeated ? 1 + option / 3 : 0, interrupt = repeated ? seed % 3 : 0;
		unsigned const first_write = 6 + prefix, finish = first_write + count + 24;
		u32 const base = seed & 1 ? 0x20000 : 0;
		u16 const st0 = 0xa602 | (y & 1 ? 0x1800 : 0), st1 = (y & 2 ? 0x4320 : 0) | (interrupt == 1 ? 0 : 0x800);
		auto seed_word = [](unsigned address) { return u16(0xa500U ^ ((address - 0x1000) * 73U)); };
		std::array<u16,4> initial_pointers;
		for (unsigned ar = 0; ar < 4; ++ar) initial_pointers[ar] = layout == 2 ? 0x1048 + ar : 0x100f + (layout == 0 ? ar * 0x100 : 0);
		auto pointers = initial_pointers;
		std::array<u16,16> sources{}, destinations{}, words{};
		auto modify = [linear](u16 address, unsigned mode) -> u16 {
			if (mode == 1) return address - 1;
			if (mode == 2) return address + 1;
			if (mode == 3) return linear ? address + 3 : address / 16 * 16 + (address % 16 + 3) % 16;
			return address;
		};
		for (unsigned n = 0; n <= count; ++n) {
			sources[n] = pointers[x & 3]; destinations[n] = pointers[y & 3];
			pointers[x & 3] = modify(pointers[x & 3],x >> 2);
			if ((x & 3) != (y & 3)) pointers[y & 3] = modify(pointers[y & 3],y >> 2);
		}
		std::array<u16,1024> memory;
		for (unsigned n = 0; n < memory.size(); ++n) memory[n] = seed_word(0x1000 + n);
		u16 const input = std::array<u16,4>{0,1,0x8000,0xffff}[option % 4];
		if (!repeated) memory[sources[0] - 0x1000] = input;
		// Reference streaming memory: next Read precedes the previous Write.
		for (unsigned n = 0; n <= count; ++n) {
			words[n] = memory[sources[n] - 0x1000];
			if (n) memory[destinations[n - 1] - 0x1000] = words[n - 1];
		}
		u16 const opcode = 0xe500 | encoding, continuation = 0x601 + prefix;
		auto initial = initial_case("dual-memory-copy", {0xf495});
		if (prefix) initial.program[0].value = prefix == 2 ? 0xf070 : 0xec00 | count;
		if (prefix == 2) initial.program[1].value = count;
		initial.program[prefix].value = opcode; initial.program[prefix + 1].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		if (interrupt) initial.program.push_back({base + (interrupt == 2 ? 4U : 0x40U),0xf4eb});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_IMR,interrupt == 1 ? 1U : 0U},{r::STATE_SP,0x3000},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0xdead},{r::STATE_AR0,3},{r::STATE_BK,linear ? 0U : 16U}});
		for (unsigned ar = 0; ar < 4; ++ar) initial.initial.push_back({r::STATE_AR2 + ar,initial_pointers[ar]});
		for (unsigned n = 0; n <= count; ++n) {
			initial.data.push_back({sources[n],seed_word(sources[n])});
			initial.data.push_back({destinations[n],seed_word(destinations[n])});
		}
		if (!repeated) initial.data.push_back({sources[0],input});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,4>{3 + prefix,first_write - 2,first_write - 1,first_write + count}[seed % 4],finish};
		if (!repeated) {
			for (unsigned tick = 1; tick <= 5; ++tick) {
				auto state = observation("Access-owned-X-priority-pointer-update", {});
				for (unsigned ar = 0; ar < 4; ++ar) state.expected.push_back({int(r::STATE_AR2 + ar),tick < 4 ?
					unsigned(initial_pointers[ar]) : tick == 4 ? unsigned(pointers[ar]) : 0x2222U + ar + 2});
				c.states.push_back({tick,std::move(state)});
			}
			c.actions = {{4,{}},{5,{},false,{},{},{{sources[0],0xdead}}}};
			for (unsigned ar = 0; ar < 4; ++ar) c.actions[0].values.push_back({r::STATE_AR2 + ar,0x2222 + ar + 2});
		}
		auto final = observation("copy-repeat-and-interrupt-retired-on-caller-page", {{STATE_GENPC,base + 0x602 + prefix},
			{r::STATE_SP,0x3000},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0xdead},
			{r::STATE_ST0,st0},{r::STATE_ST1,unsigned(st1 & ~(interrupt ? 0x800 : 0))}},{{destinations[count],words[count]}});
		for (unsigned ar = 0; ar < 4; ++ar) final.expected.push_back({int(r::STATE_AR2 + ar),repeated ? unsigned(pointers[ar]) : 0x2222U + ar + 2});
		if (repeated) final.expected.push_back({r::STATE_RC,0});
		c.states.push_back({finish,std::move(final)});
		if (prefix == 2) c.actions.push_back({4,{},false,{{base + 0x601,0xffff}}});
		if (interrupt) c.actions.insert(c.actions.begin(),{3,{},false,{},{{interrupt == 2 ? INPUT_LINE_NMI : 0,ASSERT_LINE},
			{interrupt == 2 ? INPUT_LINE_NMI : 0,CLEAR_LINE}}});
		c.program_events = {};
		if (prefix == 2) c.program_events.push_back({false,base + 0x601,u16(count),0,false});
		c.program_events.push_back({false,base + 0x600 + prefix,opcode,0,false});
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned n = 0; n <= count; ++n) {
			c.data_events->push_back({false,sources[n],words[n],first_write - 1 + n});
			if (n) c.data_events->push_back({true,destinations[n - 1],words[n - 1],first_write - 1 + n});
		}
		c.data_events->push_back({true,destinations[count],words[count],first_write + count});
		if (interrupt) {
			c.irqs = {{interrupt == 2 ? INPUT_LINE_NMI : 0,first_write + count + 1,~0U,2 * (count + 1),continuation}};
			c.data_events->push_back({true,0x2fff,continuation,0,false});
			c.data_events->push_back({false,0x2fff,continuation,0,false});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
