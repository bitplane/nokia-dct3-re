// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	// SPRU131G 7.3.2: an adjacent same-location read consumes READA's pending
	// RAM write, not the stale word. ROM4's synthesizer queue builder reads its
	// event mask this way (a1e7 READA *AR2, a1e8 LD *AR2,B).
	std::vector<clocked_register_case> cases;
	for (unsigned gap=0;gap<3;++gap) for (unsigned absolute=0;absolute<2;++absolute)
	for (u16 word:{0x000fU,0x1234U,0x7ffeU,0x0001U}) {
		std::vector<u16> code={0x7712,0x0060,0x7682,0x5aa5,0xf020,0x1200};
		if (absolute) code.insert(code.end(),{0x7ef8,0x0060}); else code.push_back(0x7e82);
		code.insert(code.end(),gap,0xf495);
		if (absolute) code.insert(code.end(),{0x11f8,0x0060}); else code.push_back(0x1182);
		code.push_back(0xf4e1);
		auto initial=marked_program("READA-then-same-address-load",code);
		initial.program.push_back({0x1200,word});
		auto final=observation("load-sees-program-word-not-stale-ram",{
			{r::STATE_A,0x1200},{r::STATE_B,word},{r::STATE_AR2,0x60},{STATE_GENPC,u64(0x601+code.size())}},{{0x60,word}});
		cases.push_back({{{0,std::move(initial)},{200,std::move(final)}},20,200});
	}
	return cases;
}
}
