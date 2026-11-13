// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	// ROM4's speech-interrupt context saves (b5ed, b682, b70c) push a
	// memory-mapped register range with RPT PSHD *AR0+, which reads AR0 and
	// SP themselves, and restore it with RPT POPD *AR0-, which pops into them.
	// The pair must return every register in the range unchanged.
	std::vector<clocked_register_case> cases;
	struct range { u16 first, last; };
	for (range const span : {range{0x0e,0x1c},range{0x12,0x1c},range{0x10,0x19}})
	for (unsigned pattern=0;pattern<4;++pattern) for (unsigned gap=0;gap<2;++gap) for (unsigned page=0;page<2;++page) {
		u32 const base=page ? 0x30000 : 0;
		u16 const count=span.last-span.first;
		std::vector<u16> code={0x4a10,0x7710,span.first,0xf070,count,0x4b90};
		code.insert(code.end(),gap,0xf495);
		// Clobber everything saved except AR0 (the pointer) and SP.
		for (u16 address=span.first;address<=span.last;++address)
			if (address!=0x10 && address!=0x18 && address!=0x1a && address!=0x1b && address!=0x1c)
				code.insert(code.end(),{u16(0x7700|address),u16(0xdead+address)});
		code.insert(code.end(),{0x7710,span.last,0xf070,count,0x8b88,0x8a10,0xf4e1});
		auto initial=initial_case("RPT-PSHD-POPD-context-round-trip",{0xf495});
		for (unsigned i=0;i<code.size();++i)
			if (i<initial.program.size()) initial.program[i].value=code[i]; else initial.program.push_back({0x600+i,code[i]});
		for (auto &word:initial.program) word.address+=base;
		u16 const seed=u16(0x1357*(pattern+1));
		std::vector<std::pair<int,u64>> values={{r::STATE_T,u16(seed^0x0101)},{r::STATE_TRN,u16(seed^0x0202)},
			{r::STATE_BK,u16(seed&0x7ff)},{r::STATE_SP,0x3000}};
		for (unsigned i=0;i<8;++i) values.push_back({int(r::STATE_AR0+i),u16(0x4000+seed+i*0x111)});
		for (auto const &[index,value]:values) initial.initial.push_back({index,value});
		initial.initial.push_back({STATE_GENPC,base+0x600});
		auto final=observation("every-saved-register-restored",{{STATE_GENPC,base+0x600+code.size()}});
		for (auto const &[index,value]:values) final.expected.push_back({index,value});
		cases.push_back({{{0,std::move(initial)},{400,std::move(final)}},std::array<unsigned,3>{20,40,60}[(pattern+gap+page)%3],400});
	}
	return cases;
}
}
