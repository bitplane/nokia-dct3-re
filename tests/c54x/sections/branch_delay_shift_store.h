// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	// A two-word shifted store fills both BD delay words. ROM4's speech path
	// has BD ab1d with STL A,-8,*AR5 (6f85 0c98) at ab16.
	std::vector<clocked_register_case> cases;
	for (unsigned form=0;form<4;++form) for (unsigned kind=0;kind<4;++kind)
	for (int shift : {-16,-8,-1,0,15}) for (bool sxm : {false,true}) for (unsigned page=0;page<2;++page) {
		u32 const base=page ? 0x20000 : 0;
		u16 const mode=std::array<u16,4>{0x85,0x95,0x8d,0x03}[form];
		u16 const address=form==3 ? 0x0e*128+3 : 0x5000;
		u64 const source=kind&1 ? 0xff87654321ULL : 0x0012345678ULL;
		s64 const input=sxm && (source&0x8000000000ULL) ? s64(source)-0x10000000000LL : s64(source);
		s64 const divisor=s64(1)<<unsigned(shift<0 ? -shift : 0);
		u64 const bits=u64(shift>=0 ? input*(s64(1)<<shift) : input/divisor-(input<0 && input%divisor!=0))&0xffffffffffULL;
		u16 const value=u16(u32(bits)>>(kind&2 ? 16 : 0));
		u16 const extension=(kind&2 ? 0x0c60 : 0x0c80)|((kind&1)<<8)|(shift&31);
		auto initial=initial_case("BD-delay-shift-store",{0xf273,0x0610,u16(0x6f00|mode),extension,0xf4e1});
		initial.program.push_back({0x610,0xf4e1});
		for (auto &word:initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST1,sxm ? 0x900U : 0x800U},
			{r::STATE_A,0x0012345678ULL},{r::STATE_B,0xff87654321ULL},{r::STATE_AR5,0x5000}});
		initial.data={{address,0xdead}};
		u16 const ar5=form==1 ? 0x5001 : form==2 ? 0x4fff : 0x5000;
		auto final=observation("store-once-then-branch-target",{{STATE_GENPC,base+0x611},{r::STATE_AR5,ar5},
			{r::STATE_A,0x0012345678ULL},{r::STATE_B,0xff87654321ULL},{r::STATE_ST1,sxm ? 0x900U : 0x800U}},{{address,value}});
		cases.push_back({{{0,std::move(initial)},{60,std::move(final)}},std::array<unsigned,3>{4,6,8}[(form+kind+page)%3],60});
	}
	return cases;
}
}
