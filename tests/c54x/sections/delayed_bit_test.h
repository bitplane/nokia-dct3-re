// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 2048; ++index) {
		unsigned const control = index % 8, form = (index / 8) % 4, slot = (index / 32) % 2, selector = (index / 64) % 16;
		bool const invert = index / 1024, upper = (selector + form + slot) & 1, tc = (selector + form + control) & 1;
		bool const taken = control == 2 ? false : control == 6 ? !tc : control == 7 ? tc : true;
		u32 const base = (selector + form + slot) & 1 ? 0x20000 : 0, target_base = control == 4 ? base ^ 0x20000 : base;
		u32 const selected = taken ? target_base + 0x700 : base + 0x604;
		u16 const sp = 0x3000 + (control == 3 ? -1 : control == 4 ? -2 : control == 5 ? 1 : 0);
		u16 const source = form == 0 ? 0x1060 : form == 3 ? sp + 0x60 : 0x5000;
		u16 const t = selector | (upper ? 0xa5f0 : 0), final_t = upper ? t ^ 15 : t;
		u16 const mask = 1U << (15 - selector), value = invert ? u16(~mask) : mask;
		u16 const opcode = 0x3400 | (form == 0 || form == 3 ? 0x60 : form == 1 ? 0x84 : 0x94);
		unsigned const kind = control >= 6 ? 1 : control;
		auto read_clock = [&](unsigned position) { return 7 + unsigned(position != 0) + unsigned(kind == 5 || ((kind == 1 || kind == 2) && position)); };
		unsigned const read = read_clock(slot), producer_read = read_clock(0), delay = control == 5 ? 1 : 2;
		u16 const st1 = 0xb00 | (form == 3 ? 0x4000 : 0);
		u64 const a = control == 1 ? 0 : control == 2 ? 1 : 0xff12345678ULL;
		auto initial = initial_case("delayed-bitt", {0xf495});
		for (auto &word : initial.program) word.address += base;
		auto put = [&](u32 at,u16 value) {
			for (auto &word : initial.program) if (word.address == at) { word.value = value; return; }
			initial.program.push_back({at,value});
		};
		put(base + 0x600,control == 4 ? 0xfb80 | (target_base >> 16) : std::array<u16,8>{0xf273,0xfa45,0xfa45,0xf274,0,0xfe00,0xfa20,0xfa30}[control]);
		if (control != 5) put(base + 0x601,0x700);
		if (slot) put(base + 0x600 + delay,0x3083);
		put(base + 0x600 + delay + slot,opcode);
		for (u32 at : {base + 0x604,target_base + 0x700}) { put(at,0x7760); put(at + 1,u16(at)); put(at + 2,0xf4e1); }
		initial.data = {{0x60,0xdead},{0x5100,t},{source,value},{0x3000,0x700}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,0x81abcdef01ULL},
			{r::STATE_SP,0x3000},{r::STATE_T,slot ? u64(t ^ 15) : t},{r::STATE_ST0,u64(0xa820 | (tc ? 0x1000 : 0))},
			{r::STATE_ST1,st1},{r::STATE_BRC,13}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({int(r::STATE_AR0 + ar),ar == 3 ? 0x5100U : ar == 4 ? 0x5000U : 0x4000 + ar});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{3,read - 1,read}[(selector + form + control) % 3],32};
		for (unsigned tick = 1; tick <= read; ++tick) {
			auto state = observation("TC-publication-and-address-capture", {{r::STATE_ST0,u64(0xa820 | (tc ? 0x1000 : 0))}});
			if (tick == read - 1) state.expected.push_back({r::STATE_AR4,form == 2 ? 0x5001U : 0x5000U});
			if (tick == read) state.expected.push_back({r::STATE_T,t});
			c.states.push_back({tick,std::move(state)});
		}
		c.states.push_back({read + 1,observation("TC-execute", {{r::STATE_ST0,u64(0xa820 | (invert ? 0 : 0x1000))}})});
		auto final = observation("selected-continuation", {{STATE_GENPC,selected + 3},{r::STATE_SP,sp},{r::STATE_ST1,st1},
			{r::STATE_ST0,u64(0xa820 | (invert ? 0 : 0x1000))},{r::STATE_T,final_t},{r::STATE_A,a},{r::STATE_B,0x81abcdef01ULL},{r::STATE_BRC,13}},{{0x60,u16(selected)}});
		for (unsigned ar = 0; ar < 8; ++ar) final.expected.push_back({int(r::STATE_AR0 + ar),ar == 3 ? 0x5100U : ar == 4 ? 0x7000U : 0x4000 + ar});
		c.states.push_back({32,std::move(final)});
		c.actions.push_back({read - 1,{{r::STATE_AR4,0x7000}},false,
			{{base + 0x600 + delay + slot,0xe8ff}}, {},control == 5 ?
			std::vector<memory_word>{{0x3000,0x620}} : std::vector<memory_word>{}});
		c.actions.push_back({read,upper ? std::vector<register_setting>{{r::STATE_T,final_t}} : std::vector<register_setting>{},false,{}, {},upper ? std::vector<memory_word>{} : std::vector<memory_word>{{source,u16(~value)}}});
		c.data_events = std::vector<clocked_data_event>{};
		if (control == 5) c.data_events->push_back({false,0x3000,0x700,0,false});
		if (control == 3 || control == 4) c.data_events->push_back({true,0x2fff,0x604,6,false});
		if (control == 4) c.data_events->push_back({true,0x2ffe,u16(base >> 16),7,false});
		if (slot) c.data_events->push_back({false,0x5100,t,producer_read});
		c.data_events->push_back({false,source,value,read});
		c.data_events->push_back({true,0x60,u16(selected),32,false});
		// Read-stage taps precede Write-stage taps on a shared clock.
		std::stable_sort(c.data_events->begin(),c.data_events->end(),[](auto const &a,auto const &b) {
			return a.cycle < b.cycle || (a.cycle == b.cycle && !a.write && b.write);
		});
		c.clear_pipeline_before_restore = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
