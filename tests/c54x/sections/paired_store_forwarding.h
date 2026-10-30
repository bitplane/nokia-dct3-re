// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned kind=0;kind<3;++kind) for (unsigned source=0;source<2;++source)
	for (u64 value : {0x0012345678ULL,0xff87654321ULL,0x0080000000ULL,0xff00000000ULL})
	for (unsigned mode=0;mode<3;++mode) for (unsigned gap=0;gap<2;++gap)
	for (unsigned same=0;same<2;++same) for (unsigned sxm=0;sxm<2;++sxm) for (unsigned sst=0;sst<2;++sst) {
		s64 const number=BIT(value,39) ? s64(value)-(s64(1)<<40) : s64(value);
		u32 const stored=!sst ? u32(value) : sxm ? u32(std::clamp(number,s64(-2147483648LL),s64(2147483647LL))) :
			u32(std::min(value,u64(0xffffffffULL)));
		u32 const memory=same ? stored : 0x13579bdf;
		u16 const word=kind==1 ? u16(memory) : u16(memory>>16);
		u64 const loaded=kind==2 ? u64(memory)|((sxm && BIT(memory,31)) ? 0xff00000000ULL : 0) :
			u64(word)|((sxm && BIT(word,15)) ? 0xffffff0000ULL : 0);
		u32 const base=cases.size()&1 ? 0x20000 : 0;
		u16 const address=(same ? 0x1800 : 0x1900)+(kind==1 ? 1 : 0), st1=0x4800|(sxm ? 0x100 : 0);
		auto initial=initial_case("DST-pending-pair-to-high-low-or-DLD",{u16(0x4e00|(source<<8)|(mode==0 ? 4 : mode==1 ? 0x81 : 0xf8))});
		unsigned cursor=1;
		if (mode==2) initial.program[cursor++].value=0x1800;
		cursor+=gap;
		initial.program[cursor++].value=(kind==2 ? 0x5600 : 0x1000)|((source^1)<<8)|0x82;
		initial.program[cursor].value=0xf4e1;
		for (auto &entry : initial.program) entry.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_PMST,sst},{r::STATE_ST0,0},{r::STATE_ST1,st1},
			{r::STATE_SP,0x17fc},{r::STATE_AR1,0x1800},{r::STATE_AR2,address},{r::STATE_A,source ? 0x5555 : value},
			{r::STATE_B,source ? value : 0x5555}});
		initial.data={{0x1800,0x2468},{0x1801,0xace0},{0x1900,0x1357},{0x1901,0x9bdf}};
		auto final=observation("forwarding-never-removes-real-pair-writes",{{r::STATE_A,source ? loaded : value},{r::STATE_B,source ? value : loaded},
			{r::STATE_ST0,0},{r::STATE_ST1,st1},{r::STATE_PMST,sst},{r::STATE_SP,0x17fc},{r::STATE_AR1,0x1800},{r::STATE_AR2,address}},
			{{0x1800,u16(stored>>16)},{0x1801,u16(stored)},{0x1900,0x1357},{0x1901,0x9bdf}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pair-dependency-running",{})},{48,std::move(final)}},
			cases.size()&1 ? 10U : 6U,48};
		c.single_fetch=base+0x600;
		if (mode==2) c.retained_program_reads={{base+0x601,0x1800}};
		c.data_events=std::vector<clocked_data_event>{};
		auto reads=[&]() {
			if (same && !gap) return; // Internal bypass, not a physical RAM transaction.
			c.data_events->push_back({false,address,word,0,false});
			if (kind==2) c.data_events->push_back({false,u32(address^1),u16(memory),0,false});
		};
		if (!gap) reads();
		c.data_events->push_back({true,0x1800,u16(stored>>16),0,false});
		c.data_events->push_back({true,0x1801,u16(stored),0,false});
		if (gap) reads();
		cases.push_back(std::move(c));
	}
	return cases;
}
}
