// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 words[]{0,1,0x7fff,0x8000,0xffff,0xfffb,7,0x1234};
	for (u16 x : words) for (u16 y : words) for (unsigned flags = 0; flags < 4; ++flags) {
		bool const sxm = flags & 1, ovm = flags & 2;
		// SPRU172C: extend each word before the implicit 16-bit shift.
		s64 const difference = (sxm ? s64(s16(x)) - s16(y) : s64(x) - y) * 65536;
		bool const overflow = difference < -2147483648LL || difference > 2147483647LL;
		u64 const result = u64(overflow && ovm ? std::clamp<s64>(difference,-2147483648LL,2147483647LL) : difference) & 0xffffffffffULL;
		u16 const st1 = 0x800 | (flags << 8);
		auto initial = initial_case("atomic-dual-memory-SUB", {0xa245,0xf4e1});
		initial.program.push_back({0x5ff,0xf495});
		initial.data = {{0x2000,x},{0x2100,y}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,0x123456},{r::STATE_B,0x56789a},{r::STATE_T,0x4444},
			{r::STATE_AR2,0x2000},{r::STATE_AR3,0x2100},{r::STATE_ST0,0},{r::STATE_ST1,st1}});
		auto final = observation("extended-operands-independent-decrements", {{STATE_GENPC,0x602},{r::STATE_A,result},
			{r::STATE_B,0x56789a},{r::STATE_T,0x4444},{r::STATE_AR2,0x1fff},{r::STATE_AR3,0x20ff},
			{r::STATE_ST0,(x >= y ? 0x800U : 0U) | (overflow ? 0x400U : 0U)},{r::STATE_ST1,st1}},{{0x2000,x},{0x2100,y}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("snapshot-before-SUB", {{r::STATE_A,0x123456}})},
			{2,observation("SUB-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,0x2000,x,0,false},{false,0x2100,y,0,false}};
		cases.push_back(std::move(c));
	}
	constexpr u64 fractions[]{1,0x17fff,0x18000,0x1ffff,0xffffff0001ULL,0xffffff8000ULL,0xffffff7fffULL,0xffffffffffULL};
	for (unsigned pattern = 0; pattern < 8; ++pattern) for (u16 x : {0U,1U,0x7fffU,0x8000U})
	for (unsigned source_b = 0; source_b < 2; ++source_b) for (unsigned destination_b = 0; destination_b < 2; ++destination_b) {
		u64 const a = fractions[pattern], b = fractions[(pattern + 3) % 8];
		// SPRU172C4-185: subtract the whole opposite accumulator, including fractional bits.
		u64 const result = (u64(s64(s16(x)) * 65536) - (destination_b ? a : b)) & 0xffffffffffULL;
		u16 const stored = (source_b ? b : a) >> 16;
		auto initial = initial_case("atomic-parallel-ST-SUB-fraction", {u16(0xc401 | (source_b << 9) | (destination_b << 8)),0xf4e1});
		initial.program.push_back({0x5ff,0xf495});
		initial.data = {{0x2000,x},{0x2100,0xdead}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_SP,0x1000},
			{r::STATE_AR2,0x2000},{r::STATE_AR3,0x2100},{r::STATE_ST0,0},{r::STATE_ST1,0x900}});
		auto final = observation("fractional-borrow-and-preinstruction-store", {{STATE_GENPC,0x602},
			{r::STATE_A,destination_b ? a : result},{r::STATE_B,destination_b ? result : b},{r::STATE_SP,0x1000}},
			{{0x2000,x},{0x2100,stored}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("snapshot-before-ST-SUB", {{r::STATE_A,a},{r::STATE_B,b}})},
			{2,observation("ST-SUB-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,0x2000,x,0,false},{true,0x2100,stored,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
