// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned field=8; field<=0x0d; ++field)
	for (u16 value : {u16(0x5a7c),u16(0xfedc)})
	for (unsigned consumer=0; consumer<2; ++consumer)
	for (unsigned gap=0; gap<3; ++gap)
	{
		auto initial=initial_case("mapped-program-read",{0x7ef8,u16(field)});
		for (unsigned i=0; i<gap; ++i) initial.program[2+i].value=0xf495;
		initial.program[2+gap].value=0x4800 | (consumer<<8) | field;
		initial.program.push_back({0x700,value});
		initial.initial.push_back({r::STATE_A,0x700});
		initial.initial.push_back({r::STATE_B,0x123456789ULL});
		u64 a=0x700,b=0x123456789ULL;
		unsigned const shift=((field-8)%3)*16;
		u64 const mask=shift==32 ? 0xff : 0xffff;
		u64 &destination=field<0x0b ? a : b;
		destination=(destination & ~(mask<<shift)) | ((value & mask)<<shift);
		(consumer ? b : a)=value & mask;
		auto final=observation("mapped-read-consumer",{{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_ST1,0x800},{r::STATE_SP,0x1000}});
		clocked_register_case c{{{0,std::move(initial)},{32,std::move(final)}},8+gap,32};
		c.require_idle=false;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
