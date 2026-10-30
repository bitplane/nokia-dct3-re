// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	auto reverse = [](u16 value) { u16 out = 0; for (unsigned bit = 0; bit < 16; ++bit) out = (out << 1) | ((value >> bit) & 1); return out; };
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 2688; ++index) {
		unsigned const kind = index % 3, ar = 1 + (index / 3) % 7;
		bool const subtract = (index / 21) & 1;
		u16 const address = std::array<u16,4>{0x6000,0x6001,0x6fff,0x7fff}[(index / 42) % 4], offset = 1U << (index / 168);
		u16 const next = reverse(u16(subtract ? reverse(address) - reverse(offset) : reverse(address) + reverse(offset)));
		auto initial = initial_case("reverse-carry-address", {u16((kind == 0 ? 0x6d00 : kind == 1 ? 0x1000 : 0x8000) | (subtract ? 0xa0 : 0xb8) | ar),0xf4e1});
		initial.data = {{address,0x1234}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0x5678},{r::STATE_ST0,0},{r::STATE_ST1,0x900},
			{r::STATE_AR0,offset},{int(r::STATE_AR0 + ar),address}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("reverse-bit-order-carry-or-borrow", {{int(r::STATE_AR0 + ar),next},{r::STATE_AR0,offset},
			{r::STATE_A,kind == 1 ? 0x1234U : 0x5678U},{STATE_GENPC,0x602}},{{address,u16(kind == 2 ? 0x5678 : 0x1234)}})});
		c.data_events = std::vector<clocked_data_event>{};
		if (kind) c.data_events->push_back({kind == 2,address,u16(kind == 2 ? 0x5678 : 0x1234),0,false});
		cases.push_back(std::move(c));
	}
	constexpr u16 sequence[]{0x60,0x68,0x64,0x6c,0x62,0x6a,0x66,0x6e,0x61,0x69,0x65,0x6d,0x63,0x6b,0x67,0x6f};
	for (unsigned subtract = 0; subtract < 2; ++subtract) {
		auto initial = initial_case("manual-reversed-traversal", {0xf495});
		auto put = [&](u32 at,u16 value) { for (auto &word : initial.program) if (word.address == at) { word.value = value; return; } initial.program.push_back({at,value}); };
		for (unsigned i = 0; i < 16; ++i) { put(0x600 + i * 3,0x7313); put(0x601 + i * 3,0x500 + i); put(0x602 + i * 3,subtract ? 0x6da3 : 0x6dbb); }
		put(0x630,0xf4e1);
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST1,0x900},{r::STATE_AR0,8},{r::STATE_AR3,0x60}});
		clocked_register_case c{{{0,std::move(initial)}},subtract ? 25U : 7U,256};
		c.pipeline_frontend = false;
		auto final = observation("full-cycle-wrap", {{r::STATE_AR3,0x60},{STATE_GENPC,0x631}});
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned i = 0; i < 16; ++i) {
			u16 const value = sequence[subtract ? (16 - i) % 16 : i];
			final.expected_data.push_back({0x500 + i,value});
			c.data_events->push_back({false,0x13,value,0,false}); c.data_events->push_back({true,0x500 + i,value,0,false});
		}
		c.states.push_back({256,std::move(final)}); cases.push_back(std::move(c));
	}
	return cases;
}
}
