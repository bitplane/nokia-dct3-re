// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 sources[]{0x0012345678ULL,0xff87654321ULL,0x0180001200ULL,0xff7fff1200ULL};
	// Literal signed-32 saturation boundaries, independent of CPU helpers.
	constexpr u32 normal[]{0x12345678,0x87654321,0x80001200,0x7fff1200};
	constexpr u32 saturated[]{0x12345678,0x87654321,0x7fffffff,0x80000000};
	for (unsigned n = 0; n < 64; ++n)
	{
		bool const b = n & 1, sst = n & 2, compiler = n & 32;
		unsigned const pattern = (n >> 2) & 3;
		u16 const offset = 0x7c + ((n >> 4) & 1);
		u16 const address = (compiler ? 0x500 : 0x400) + offset;
		u32 const value = sst ? saturated[pattern] : normal[pattern];
		auto initial = initial_case("paired-store", {u16(0x4e00 | (b << 8) | offset)});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x608},{r::STATE_ST1,compiler ? 0x4900 : 0x900},
			{r::STATE_PMST,unsigned(sst)},{r::STATE_SP,0x500},
			{r::STATE_A,b ? 0x1111222233ULL : sources[pattern]},
			{r::STATE_B,b ? sources[pattern] : 0x4444555566ULL}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_ST0,0x608},{r::STATE_ST1,compiler ? 0x4900U : 0x900U},
			{r::STATE_PMST,unsigned(sst)},{r::STATE_SP,0x500}});
		initial.data = initial.expected_data = {{address,0xaaaa},{address ^ 1,0xaaaa}};
		auto high = observation("high-word", {}); high.expected = initial.expected;
		high.expected_data = {{address,u16(value >> 16)},{address ^ 1,0xaaaa}};
		auto low = high; low.expected_data[1].value = u16(value);
		clocked_register_case c{{{0,std::move(initial)},{6,std::move(high)},{7,std::move(low)}},4 + n % 3,10};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions.push_back({5,{{b ? r::STATE_B : r::STATE_A,0}}});
		c.data_events = std::vector<clocked_data_event>{{true,address,u16(value >> 16),6},{true,u32(address ^ 1),u16(value),7}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
