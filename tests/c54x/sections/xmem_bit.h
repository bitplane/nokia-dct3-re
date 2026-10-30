// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned mode = 0; mode != 16; ++mode)
	for (unsigned bit = 0; bit != 16; ++bit)
	for (unsigned value = 0; value != 2; ++value)
	for (unsigned tc = 0; tc != 2; ++tc)
	for (unsigned gap = 0; gap != 3; ++gap) {
		u16 const mask = 1U << (15 - bit), operand = value ? mask : u16(~mask);
		auto initial = initial_case("BIT-Xmem-retains-bit-and-delays-TC-visibility", {u16(0x9600 | (mode << 4) | bit)});
		initial.program[1 + gap].value = 0xfd20;
		initial.program[2 + gap].value = 0xe855;
		initial.program[3 + gap].value = 0xf4e1;
		initial.data = {{0x1000,operand}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x608 | (tc ? 0x1000U : 0U)},
			{r::STATE_B,0x1234},{r::STATE_AR0,3},{r::STATE_BK,0}});
		for (unsigned ar = 2; ar <= 5; ++ar) initial.initial.push_back({int(r::STATE_AR0) + int(ar),0x1000});
		auto before = observation("Decode-keeps-TC-and-pointers", {{r::STATE_ST0,0x608 | (tc ? 0x1000U : 0U)}});
		auto access = observation("Access-updates-one-pointer-not-TC", {{r::STATE_ST0,0x608 | (tc ? 0x1000U : 0U)}});
		auto execute = observation("Execute-publishes-only-TC", {{r::STATE_ST0,0x608 | (value ? 0x1000U : 0U)}});
		auto final = observation("XC-observes-old-TC-until-two-NOP-gap", {{STATE_GENPC,0x604 + gap},
			{r::STATE_ST0,0x608 | (value ? 0x1000U : 0U)},{r::STATE_A,(gap < 2 ? tc : value) ? 0U : 0x55U},
			{r::STATE_B,0x1234},{r::STATE_AR0,3},{r::STATE_BK,0}},{{0x1000,0xdead}});
		for (unsigned ar = 2; ar <= 5; ++ar) {
			int const delta = ar == 2 + (mode & 3) ? std::array<int,4>{0,-1,1,3}[mode >> 2] : 0;
			before.expected.push_back({int(r::STATE_AR0) + int(ar),0x1000});
			for (auto *state : {&access,&execute,&final}) state->expected.push_back({int(r::STATE_AR0) + int(ar),u64(0x1000 + delta)});
		}
		clocked_register_case c{{{0,std::move(initial)},{3,std::move(before)},{4,std::move(access)},
			{6,std::move(execute)},{16,std::move(final)}},3 + (mode + bit + value + tc + gap) % 3,16};
		c.actions = {{5,{},false,{}, {},{{0x1000,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{{false,0x1000,operand,5}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
