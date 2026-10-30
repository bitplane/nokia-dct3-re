// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_SECTION_COMMON_H
#define C54X_SECTION_COMMON_H
#include "tms320c54x_test_case.h"
namespace c54x_test {
using r = tms320c54x_device;
inline instruction_case initial_case(char const *name, std::initializer_list<u16> words)
{
	instruction_case c{name, *words.begin(), {}, {}, {
		{r::STATE_IDLE,0}, {r::STATE_ILLEGAL,0}, {r::STATE_TCR,0x10},
		{r::STATE_PMST,0}, {r::STATE_ST0,0xe}, {r::STATE_ST1,0x800},
		{r::STATE_IFR,0}, {r::STATE_IMR,0}, {r::STATE_SP,0x1000},
		{r::STATE_A,0}, {r::STATE_B,0}, {STATE_GENPC,0x600}
	}, {{r::STATE_ILLEGAL,0}}, 0,0};
	unsigned at = 0x600;
	for (u16 word : words) c.program.push_back({at++,word});
	for (; at < 0x620; ++at) c.program.push_back({at,0xf495});
	return c;
}
inline instruction_case observation(char const *name, std::initializer_list<register_check> expected,
	std::initializer_list<memory_word> memory = {})
{
	instruction_case c{name,0,{}, {}, {}, expected,0,0};
	c.expected.push_back({r::STATE_ILLEGAL,0}); c.expected_data = memory;
	return c;
}
}
#endif
