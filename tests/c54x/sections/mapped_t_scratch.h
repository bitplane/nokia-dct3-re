// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// ROM4 abc8..abd2 uses T and TRN as indirect scratch: STH B,*AR4 (AR4=T),
// RPT #15; SUBC *AR4,A, then ST A,*AR5 || LD *AR2+,B (AR5=TRN). The older
// store precedes the younger RPT; each SUBC issue reads T; TRN takes A's high word.
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	static constexpr std::array<u64,5> dividends{0x800,0x12345,0x7fff,0,0x00fffe0001ULL};
	static constexpr std::array<u16,5> divisors{0x10,1,0x7fff,3,0x8000};
	auto signed40=[](u64 value) { return BIT(value,39) ? s64(value)-(s64(1)<<40) : s64(value); };
	for (unsigned dividend=0; dividend<5; ++dividend) for (unsigned divisor=0; divisor<5; ++divisor)
	for (unsigned sxm=0; sxm<2; ++sxm) for (unsigned page=0; page<2; ++page) {
		u32 const base=page ? 0x20000 : 0;
		u16 const st1=0x800|(sxm ? 0x100 : 0), loaded=u16(0x8000|(dividend*5+divisor));
		u64 const b=u64(divisors[divisor])<<16;
		auto initial=initial_case("mapped-T-TRN-scratch-division",{0x8384,0xec0f,0x1e84,0xc983,0xf4e1});
		for (auto &word:initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST1,st1},{r::STATE_PMST,0},
			{r::STATE_A,dividends[dividend]},{r::STATE_B,b},{r::STATE_T,0xdead},{r::STATE_TRN,0xbeef},
			{r::STATE_AR2,0x1200},{r::STATE_AR4,0x0e},{r::STATE_AR5,0x0f}});
		initial.data.push_back({0x1200,loaded});
		s64 const subtrahend=(sxm ? s64(s16(divisors[divisor])) : s64(divisors[divisor]))*32768;
		u64 a=dividends[dividend];
		for (unsigned i=0; i<16; ++i) {
			s64 const output=signed40(a)-subtrahend;
			a=(output>=0 ? u64(output)*2+1 : a*2)&0xffffffffffULL;
		}
		u64 const final_b=u64((sxm ? s64(s16(loaded)) : s64(loaded))*65536)&0xffffffffffULL;
		auto final=observation("T-divisor-and-TRN-scratch",{{r::STATE_IDLE,1},{r::STATE_A,a},{r::STATE_B,final_b},
			{r::STATE_T,divisors[divisor]},{r::STATE_TRN,u16(a>>16)},{r::STATE_AR2,0x1201},{r::STATE_AR4,0x0e},{r::STATE_AR5,0x0f},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)},{60,std::move(final)}},std::array<unsigned,3>{4,10,20}[cases.size()%3],60};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
