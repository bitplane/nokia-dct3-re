// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_TEST_SHIFTED_ENDPOINT_SECTION_H
#define C54X_TEST_SHIFTED_ENDPOINT_SECTION_H
#include "c54x_section_common.h"
#include <set>
namespace c54x_test {
enum class shifted_endpoint { peripheral, unowned, bsp };
inline std::vector<clocked_register_case> shifted_endpoint_cases(shifted_endpoint kind)
{
	bool const bsp=kind==shifted_endpoint::bsp,unowned=kind==shifted_endpoint::unowned;
	std::vector<clocked_register_case> cases;
	auto append=[&](unsigned address,unsigned code,unsigned form,unsigned flags,unsigned b,u16 word,bool interior) {
		unsigned const literals=form==2 || form==4 ? 2 : 1,read=(literals==2 ? 8 : 7)-unsigned(unowned);
		unsigned const point=read-2+cases.size()%(bsp ? 4 : 3);
		u32 const base=!interior && cases.size()%2 ? 0x20000 : 0;
		u16 const st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0)|(form==1 ? 0x4000 : 0);
		u16 const opcode=0x6f00|(form<2 ? address : form==2 ? 0xf8 : form==3 ? 0x91 : 0xe1);
		u16 const operation=0x0c40|(b ? 0x100 : 0)|code;
		int const shift=code>=16 ? int(code)-32 : int(code);
		s64 const number=flags&1 ? s16(word) : s32(word);
		u64 const result=shift>=0 ? u64(number*(s64(1)<<shift))&0xffffffffffULL :
			flags&1 ? u64(number>>-shift)&0xffffffffffULL : u64(number)>>-shift;
		u16 const ar1=form==4 ? u16(address-0x10) : address;
		auto initial=initial_case("shifted-endpoint-retained-literals-and-source",{opcode});
		for (auto &p : initial.program) p.address+=base;
		if (literals==2) initial.program[1].value=form==2 ? address : 0x10;
		initial.program[literals].value=operation;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xb800},
			{r::STATE_ST1,st1},{r::STATE_SP,0},{r::STATE_AR1,ar1},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL}});
		clocked_register_case c{{},point,16};
		std::set<unsigned> ticks{0,point,read,read+1,16}; if (form==3) ticks.insert(read-1);
		for (unsigned tick : ticks) {
			auto state=tick ? observation("shifted-endpoint-publication",{}) : std::move(initial);
			state.expected.insert(state.expected.end(),{{r::STATE_A,tick>read && !b ? result : 0x123456789aULL},
				{r::STATE_B,tick>read && b ? result : 0xfedcba9876ULL},{r::STATE_ST0,0xb800},{r::STATE_ST1,st1},{r::STATE_SP,0}});
			if (tick>=read-1 && form==3) state.expected.push_back({r::STATE_AR1,tick==read-1 ? address+1 : 0x4444});
			else if (tick==16) state.expected.push_back({r::STATE_AR1,ar1});
			if (tick==point || tick==16) state.expected_data={{address,tick>=read ? u16(word^0xffff) : word}};
			c.states.push_back({tick,std::move(state)});
		}
		clocked_register_action poison{read-2,{}};
		for (unsigned i=1;i<=literals;++i) {
			c.retained_program_reads.push_back({base+0x600+i,i==literals ? operation : u16(form==2 ? address : 0x10)});
			poison.program.push_back({base+0x600+i,0xffff});
		}
		c.actions.push_back(std::move(poison));
		if (form==3) c.actions.push_back({read-1,{{r::STATE_AR1,0x4444}}});
		c.data_events=std::vector<clocked_data_event>{{false,address,word,read}};
		if (!bsp) c.volatile_data_reads={{address,word}};
		c.single_fetch=base+0x600; c.require_idle=false;
		cases.push_back(std::move(c));
	};
	for (unsigned address=unowned ? 2 : 0x20;address<(unowned ? 6U : bsp ? 0x21U : 0x60U);++address) {
		bool const interior=!bsp && !unowned && address!=0x20 && address!=0x5f;
		for (unsigned code=0;code<(bsp || unowned ? 32U : 4U);++code) for (unsigned form=0;form<5;++form)
		for (unsigned flags=0;flags<(interior ? 1U : 4U);++flags) for (unsigned b=0;b<(interior ? 1U : 2U);++b)
		for (unsigned pattern=0;pattern<(interior ? 1U : 4U);++pattern)
			append(address,bsp || unowned ? code : std::array<unsigned,4>{16,31,4,15}[code],form,flags,b,
				interior ? 0x8000 : std::array<u16,4>{0,0xffff,0x8000,0x1234}[pattern],interior);
	}
	return cases;
}
}
#endif
