// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<u64,4> patterns{0x12abcdef01ULL,0xff8000ffffULL,0x017fff0000ULL,0x807fff8001ULL};
	for (unsigned destination=8;destination<=13;++destination) for (unsigned source=0;source<2;++source)
	for (unsigned pattern=0;pattern<4;++pattern) for (bool producer : {false,true}) {
		std::array<u64,2> const before{patterns[pattern],patterns[(pattern+1)%4]};
		auto after=before;
		if (producer) after[source]=0x12;
		u16 const value=u16(after[source]); // SST is clear; store source is the live low word.
		unsigned const target=destination>=11;
		unsigned const shift=((destination-8)%3)*16;
		u64 const field=shift==32 ? 0xff : 0xffff;
		after[target]=(after[target]&~(field<<shift))|((u64(value)&field)<<shift);
		auto initial=initial_case("direct-STLM-accumulator-register-alias",{
			u16(producer ? (source ? 0xe912 : 0xe812) : 0xf495),
			u16(0x8800|(source<<8)|destination),u16(target ? 0x4f82 : 0x4e82),0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,before[0]},{r::STATE_B,before[1]},
			{r::STATE_AR2,0x4000},{r::STATE_ST0,0x1604},{r::STATE_ST1,0x900}});
		auto final=observation("one-field-publication-and-adjacent-DST-consumer",{{STATE_GENPC,0x604},
			{r::STATE_A,after[0]},{r::STATE_B,after[1]},{r::STATE_ST0,0x1604},{r::STATE_ST1,0x900},
			{r::STATE_AR2,0x4000}},{{0x4000,u16(after[target]>>16)},{0x4001,u16(after[target])}});
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},3+unsigned(cases.size()%4)*2,24};
		c.data_events=std::vector<clocked_data_event>{{true,destination,value,0,false},
			{true,0x4000,u16(after[target]>>16),0,false},{true,0x4001,u16(after[target]),0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
