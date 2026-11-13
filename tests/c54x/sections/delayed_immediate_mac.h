// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// Two-word MAC Smem,#lk,src,dst filling a BD delay (ROM4 speech encoder
// b07f: BD b09d; MAC *AR4,#7ef8,A). Smem goes to T; the accumulation uses
// the live source accumulator and the retained literal.
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form=0;form<4;++form) for (unsigned pair=0;pair<16;++pair) for (unsigned frct=0;frct<2;++frct) {
		u32 const caller=(form+pair)&1 ? 0x20000 : 0;
		u16 const memory=std::array<u16,4>{1,0x7fff,0x8000,0xfff0}[pair%4],lk=std::array<u16,4>{0x7ef8,0xffff,2,0x8000}[pair/4];
		u64 const a=0x0000123456ULL,b=0xffffedcba9ULL;
		bool const source_b=form&2,destination_b=form&1;
		s64 const source=s64((source_b ? b : a)<<24)>>24;
		s64 const product=s64(s16(memory))*s16(lk)*(frct ? 2 : 1);
		u64 const expected=u64(source+product)&0xffffffffffULL;
		u16 const opcode=u16(0x6492|(source_b ? 0x200 : 0)|(destination_b ? 0x100 : 0));
		auto initial=initial_case("BD-delay-immediate-MAC",{0xf273,0x0700,opcode,lk,0xf4e1});
		initial.program.push_back({0x700,0xf4e1});
		for (auto &word:initial.program) word.address+=caller;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,caller+0x600},{r::STATE_ST1,frct ? 0x940U : 0x900U},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0x2222},{r::STATE_AR2,0x5000}});
		initial.data={{0x5000,memory}};
		auto final=observation("MAC-in-delay-then-target",{{STATE_GENPC,caller+0x701},{r::STATE_T,memory},{r::STATE_AR2,0x5001},
			{r::STATE_A,destination_b ? a : expected},{r::STATE_B,destination_b ? expected : b}});
		cases.push_back({{{0,std::move(initial)},{60,std::move(final)}},std::array<unsigned,3>{4,6,8}[(form+pair)%3],60});
	}
	return cases;
}
}
