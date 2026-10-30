// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add = [&](u16 opcode, u64 a, u64 b, u16 st0, u16 st1, u64 out_a, u64 out_b, u16 flags, u16 mask) {
		auto initial = initial_case("ASM-accumulator-arithmetic-contract",{0xf495,opcode,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		auto final = observation("shift-fill-carry-overflow-and-sibling-results",{{STATE_GENPC,0x603},
			{r::STATE_A,out_a},{r::STATE_B,out_b},{r::STATE_ST0,flags,mask},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-ASM-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("ASM-running",{})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	};
	struct row { u16 opcode,st0,st1; u64 a,b,out_a,out_b; u16 flags,mask; };
	constexpr row boundaries[]{
		{0xf480,0,0x11f,0xffffffff00ULL,0x123456789aULL,0xfffffffe80ULL,0x123456789aULL,0x800,0x800},
		{0xf681,0x800,0x101,0x10,0x40,0xffffffff90ULL,0x40,0,0x800},
		{0xf480,0,0x103,1,0x123456789aULL,9,0x123456789aULL,0,0x800},
		{0xf480,0,0x100,0x7fffffff,0x123456789aULL,0xfffffffeULL,0x123456789aULL,0x400,0xe00},
		{0xf480,0,0x300,0x7fffffff,0x123456789aULL,0x7fffffff,0x123456789aULL,0x400,0xe00},
		{0xf480,0,0x100,0xff80000000ULL,0x123456789aULL,0xff00000000ULL,0x123456789aULL,0xc00,0xe00},
		{0xf480,0,0x300,0xff80000000ULL,0x123456789aULL,0xff80000000ULL,0x123456789aULL,0xc00,0xe00},
		{0xf581,0,0x100,0xffffffffffULL,0x7fffffff,0xffffffffffULL,0x80000000,0x200,0xe00},
		{0xf581,0,0x300,0xffffffffffULL,0x7fffffff,0xffffffffffULL,0x7fffffff,0x200,0xe00},
		{0xf581,0,0x100,1,0xff80000000ULL,1,0xff7fffffffULL,0xa00,0xe00},
		{0xf581,0,0x300,1,0xff80000000ULL,1,0xff80000000ULL,0xa00,0xe00}
	};
	for (auto const &v : boundaries) add(v.opcode,v.a,v.b,v.st0,v.st1,v.out_a,v.out_b,v.flags,v.mask);
	for (int shift = -16; shift != 16; ++shift)
	for (bool subtract : {false,true})
	for (bool sign_fill : {false,true})
	for (bool saturate : {false,true})
	for (bool sticky : {false,true}) {
		// Division supplies explicit floor rounding, independently of host
		// signed-right-shift behavior and the CPU implementation's shifter.
		s64 const operand = shift >= 0 ? -65537LL * (s64(1) << shift) : sign_fill ?
			-((65537LL + (s64(1) << -shift) - 1) / (s64(1) << -shift)) : s64(0xfffffeffffULL / (u64(1) << -shift));
		s64 const result = subtract ? 17 - operand : 17 + operand;
		bool const overflow = result < -2147483648LL || result > 2147483647LL;
		u64 const stored = u64(saturate ? std::clamp<s64>(result,-2147483648LL,2147483647LL) : result) & 0xffffffffffULL;
		bool const carry = subtract ? 17 >= u32(operand) : u64(17) + u32(operand) > 0xffffffffULL;
		u16 const flags = (carry ? 0x800 : 0) | (overflow ? 0x200 : 0) | (sticky ? 0x600 : 0);
		u16 const st1 = (shift & 31) | (sign_fill ? 0x100 : 0) | (saturate ? 0x200 : 0);
		add(subtract ? 0xf581 : 0xf580,0xfffffeffffULL,17,sticky ? 0x600 : 0,st1,0xfffffeffffULL,stored,flags,0xe00);
	}
	return cases;
}
}
