// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> minmax_square_cases(bool square)
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<u64,16> values{0,1,0xffffffffffULL,0x7fffffff,0x80000000,0xff80000000ULL,0xff7fffffffULL,
		0x7fffffffffULL,0x8000000000ULL,0x0180000000ULL,0xfe7fffffffULL,0x123456789aULL,0xedcba98765ULL,0x7fffff,0xffffff,0xffff000000ULL};
	constexpr std::array<u32,16> high_words{0,1,0xffff,0x10000,0x1ffff,0x7fff,0x8000,0x17fff,0x18000,0xfffe,0x10001,0x8001,0x17ffe,0x5555,0x15555,0xaaaa};
	for (unsigned pa=0;pa<16;++pa) for (unsigned pb=0;pb<(square ? 2U : 16U);++pb)
	for (unsigned selector=0;selector<(square ? 2U : 4U);++selector) for (unsigned flags=0;flags<8;++flags) {
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		u64 const a=square ? (u64(high_words[pa])<<16)|0x5aa5 : values[pa], b=values[square ? 11+pb : pb];
		u16 const st0=2|(flags&1 ? 0x800 : 0)|(pa&1 ? 0x600 : 0);
		u16 const st1=0x800|(flags&2 ? 0x200 : 0)|(flags&4 ? 0x100 : 0)|(square && (flags&1) ? 0x40 : 0);
		u64 result; u16 status;
		if (square) {
			s64 const operand=high_words[pa]>=0x10000 ? s64(high_words[pa])-0x20000 : high_words[pa];
			u64 const product=u64(operand*operand)*(flags&1 ? 2 : 1);
			bool const overflow=product>0x7fffffff;
			result=overflow && (flags&2) ? 0x7fffffff : product;
			status=st0|(overflow ? selector&1 ? 0x200 : 0x400 : 0);
			// Independent signed-17 literal examples formerly checked only on the host.
			if (!pb && !selector && !flags && pa>=2 && pa<=4) result=std::array<u64,3>{0xfffe0001ULL,0x100000000ULL,1}[pa-2];
		} else {
			// Sign-bit bias orders 40-bit values without sharing core signed conversion.
			u64 const left=a^0x8000000000ULL, right=b^0x8000000000ULL;
			bool const choose_a=selector&2 ? left<right : left>right;
			result=choose_a ? a : b; status=(st0&~0x800)|(choose_a ? 0 : 0x800);
		}
		u16 const opcode=square ? 0xf48d|((selector&1)<<8) : 0xf486|((selector&1)<<8)|(selector>>1);
		auto initial=initial_case(square ? "SQUR-uses-Execute-live-signed-17-bit-A-high" : "MAX-MIN-signed-40-bit-equality-selects-B",{opcode,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a^0xffffffffffULL},{r::STATE_B,b^0xffffffffffULL},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_T,0x5aa5}});
		auto final=observation("live-input-result-with-unselected-accumulator-and-T-preserved",{{r::STATE_A,selector&1 ? a : result},
			{r::STATE_B,selector&1 ? result : b},{r::STATE_ST0,status},{r::STATE_ST1,st1},{r::STATE_T,0x5aa5}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-live-input-write",{{r::STATE_A,a^0xffffffffffULL},
			{r::STATE_B,b^0xffffffffffULL},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_T,0x5aa5}})},
			{6,std::move(final)}},std::array<unsigned,4>{3,5,6,8}[cases.size()%4],12};
		c.actions={{5,{{r::STATE_A,a},{r::STATE_B,b}}}};
		c.single_fetch=base+0x600;
		c.forbid_data_access=true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
