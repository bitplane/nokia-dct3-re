// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned arp = 0; arp < 8; ++arp) for (unsigned encoded : {0U,2U})
	for (unsigned compatibility = 0; compatibility < 2; ++compatibility)
	for (unsigned advance = 0; advance < 2; ++advance)
	{
		unsigned const selected = compatibility && !encoded ? arp : encoded;
		u16 const address = 0x420 + selected * 16, word = 0x1100 + selected;
		auto initial = initial_case("immediate-address-pointer", {u16(0xf4a0 | arp),u16(0x1000 | (advance ? 0x90 : 0x80) | encoded)});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xa608},{r::STATE_ST1,compatibility ? 0x820 : 0x800}});
		auto final = observation("selected", {{r::STATE_A,word},
			{r::STATE_ST0,0x608 | ((compatibility && encoded ? encoded : arp) << 13)},
			{r::STATE_ST1,compatibility ? 0x820U : 0x800U}});
		for (unsigned ar = 0; ar < 8; ++ar) {
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),0x420 + ar * 16});
			initial.data.push_back({0x420 + ar * 16,0x1100 + ar});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),0x420 + ar * 16 + unsigned(advance && ar == selected)});
		}
		clocked_register_case c{{{0,std::move(initial)},{7,std::move(final)}},
			3 + (arp + encoded / 2 + compatibility + advance) % 4,10};
		c.require_idle = false;
		c.actions = {{6,{}}}; c.actions.front().data = {{address,0x9999}};
		c.data_events = std::vector<clocked_data_event>{{false,address,word,6}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
