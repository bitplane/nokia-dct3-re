// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add = [&](unsigned form, unsigned offset_index, unsigned selector, unsigned flags, unsigned pattern,
		bool compatibility, bool shifted, int shift) {
		unsigned const kind = form == 24 ? 3 : form/8, reg = form == 24 ? 0 : form%8;
		unsigned const selected = compatibility && !reg ? 3 : reg;
		s16 const offset = std::array<s16,4>{0,1,-1,s16(0xf495)}[offset_index];
		u16 const pointer = kind == 2 ? 0x4007 : 0x1007, bk = kind == 2 ? 8192 : 16;
		u16 const address = kind == 3 ? u16(0x1500+offset) : kind == 2 ? 0x4000 | ((7+offset) & 0x1fff) : u16(pointer+offset);
		u16 const literal = kind == 3 ? address : u16(offset), next = kind == 1 || kind == 2 ? address : pointer;
		u64 const source = std::array<u64,4>{0x12345678,0x0180000000ULL,0xff7fffffffULL,0xffffffffffULL}[pattern];
		u64 value = source;
		if (shifted) {
			if (shift >= 0) value = (value << shift) & 0xffffffffffULL;
			else if ((flags & 2) && (value & 0x8000000000ULL)) {
				s64 const number = s64(value)-0x10000000000LL, divisor = s64(1) << -shift;
				value = u64(number/divisor-(number%divisor != 0)) & 0xffffffffffULL;
			} else value >>= -shift;
		}
		u32 stored = u32(value);
		if (flags & 1) {
			if (flags & 2) stored = u32(std::clamp<s64>(value & 0x8000000000ULL ? s64(value)-0x10000000000LL : s64(value),-0x80000000LL,0x7fffffffLL));
			else stored = u32(std::min<u64>(value,0xffffffffULL));
		}
		u16 const word = stored >> (selector & 2 ? 16 : 0), mode = kind == 3 ? 0xf8 : 0xe0 | (kind << 3) | reg;
		u16 const opcode = shifted ? 0x6f00 | mode : 0x8000 | (selector << 8) | mode;
		u16 const operation = (selector & 2 ? 0x0c60 : 0x0c80) | ((selector & 1) << 8) | (shift & 31);
		u16 const st0 = compatibility ? 0x6608 : 0x608, st1 = 0x800 | (flags & 2 ? 0x100 : 0) | (compatibility ? 0x20 : 0);
		u16 const final_st0 = compatibility && reg && kind != 3 ? (st0 & 0x1fff) | (reg << 13) : st0;
		u32 const base = cases.size()%2 ? 0x20000 : 0;
		auto initial = initial_case("long-offset-STL-STH-retained-address-live-source",{opcode,literal});
		if (shifted) initial.program[2].value = operation;
		initial.program[2+shifted].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0x11112222},{r::STATE_B,0x33334444},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,flags & 1},{r::STATE_BK,bk}});
		for (unsigned ar = 0; ar != 8; ++ar) initial.initial.push_back({r::STATE_AR0+ar,ar == selected ? pointer : 0x1007U});
		initial.data = {{address,0xaaaa}};
		unsigned const read = shifted ? 7 : 6;
		auto at_read = observation("only-selected-AR-updates-at-address-presentation",{});
		auto final = observation("retained-address-and-Execute-live-saturated-source",{{r::STATE_A,selector & 1 ? 0x11112222 : source},
			{r::STATE_B,selector & 1 ? source : 0x33334444},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_PMST,flags & 1},{r::STATE_BK,bk}},{{address,word}});
		for (unsigned ar = 0; ar != 8; ++ar) {
			at_read.expected.push_back({int(r::STATE_AR0+ar),ar == selected ? next : 0x1007U});
			final.expected.push_back({int(r::STATE_AR0+ar),ar == selected ? 0x2222U : 0x1007U});
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("long-store-running",{})},{read,std::move(at_read)},
			{read+1,std::move(final)}},std::array<unsigned,3>{shifted ? 4U : 3U,read,read+1}[cases.size()%3],16};
		c.actions = {{4,{},false,{{base+0x601,0xdead}}},{read,{{r::STATE_AR0+selected,0x2222},{selector & 1 ? r::STATE_B : r::STATE_A,source}}}};
		if (shifted) c.actions.front().program.push_back({base+0x602,0xdead});
		c.retained_program_reads = {{base+0x601,literal}};
		if (shifted) c.retained_program_reads.push_back({base+0x602,operation});
		c.data_events = std::vector<clocked_data_event>{{true,address,word,read+1}};
		cases.push_back(std::move(c));
	};
	// Full unshifted routing grid; numeric values/SATD/SXM are distributed.
	for (unsigned form = 0; form != 25; ++form) for (unsigned offset = 0; offset != 4; ++offset)
		for (unsigned selector = 0; selector != 4; ++selector) for (unsigned cmpt = 0; cmpt != 2; ++cmpt)
			add(form,offset,selector,(form+offset+cmpt)%4,(form+offset+selector+cmpt)%4,cmpt,false,0);
	// Full numeric grid at one absolute destination, including every shift.
	for (unsigned pattern = 0; pattern != 4; ++pattern) for (unsigned flags = 0; flags != 4; ++flags)
		for (unsigned selector = 0; selector != 4; ++selector) for (int shift = -16; shift != 16; ++shift)
			add(24,1,selector,flags,pattern,false,true,shift);
	// Seven original shifted address forms at representative shift boundaries.
	for (unsigned form : {1,6,9,14,17,22,24}) for (unsigned offset = 0; offset != 4; ++offset)
		for (unsigned selector = 0; selector != 4; ++selector) for (int shift : {-16,-1,0,1,15})
			add(form,offset,selector,(form+offset+selector)%4,(form+offset)%4,false,true,shift);
	return cases;
}
}
