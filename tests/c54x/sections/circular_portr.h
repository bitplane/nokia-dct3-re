// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned operation=0;operation<4;++operation) for (unsigned reg=0;reg<8;++reg)
	for (unsigned compatibility=0;compatibility<2;++compatibility) for (unsigned choice=0;choice<4;++choice)
	for (unsigned index=0;index<4;++index) {
		unsigned const selected = compatibility && !reg ? 4 : reg;
		// AR0 is both index and pointer here: overwritten index seeds are identical cases.
		if (!selected && !((!choice && !index) || (choice==1 && (!index || index==3)))) continue;
		u16 bk = std::array<u16,4>{0,1,7,16}[choice];
		if (!selected && bk) bk=0x2000;
		std::array<u16,8> pointers;
		for (unsigned i=0;i<8;++i) pointers[i]=0x1000+i*0x100;
		pointers[0]=std::array<u16,4>{0,1,0xffff,bk ? u16(-bk) : u16(0x8000)}[index];
		unsigned const position = !bk || !index ? 0 : index==1 ? bk-1 : index==2 ? bk/2 : bk>1 ? 1 : 0;
		u16 const address = index==3 && choice==1 ? 0x20 : 0x1000+selected*0x100+(selected ? position : 0);
		pointers[selected]=address;
		bool const excluded = address<0x60;
		s32 const step = operation&1 ? s32(s16(pointers[0])) : 1;
		s32 const delta = operation<2 ? -step : step;
		u16 updated = address+delta;
		if (bk) {
			unsigned mask=1;
			while (mask<bk) mask=(mask<<1)|1;
			s32 const remainder=(s32(address&mask)+delta)%bk;
			updated=(address&~mask)|u16(remainder<0 ? remainder+bk : remainder);
		}
		for (unsigned sample=0;sample<(excluded ? 1U : 4U);++sample)
		for (unsigned port_choice=0;port_choice<2;++port_choice) for (unsigned flags=0;flags<2;++flags) {
			u32 const base = cases.size()&1 ? 0x20000 : 0;
			u16 const port=port_choice ? 0xfffe : 0x125, word=std::array<u16,4>{0,1,0x8000,0xffff}[sample];
			u16 const st1=0x800|(flags ? 0x300 : 0)|(compatibility ? 0x20 : 0);
			u16 const st0=!excluded && compatibility && reg ? 0x608|(reg<<13) : 0x8608;
			auto initial=initial_case("circular-PORTR-retained-volatile-sample",{u16(0x74c0|(operation<<3)|reg),port,0xf4e1});
			for (auto &entry : initial.program) entry.address+=base;
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x8608},{r::STATE_ST1,st1},
				{r::STATE_PMST,flags},{r::STATE_BK,bk},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x9876543210ULL}});
			for (unsigned i=0;i<8;++i) initial.initial.push_back({r::STATE_AR0+int(i),pointers[i]});
			auto access=observation("address-update-before-port-read",{{r::STATE_ST0,st0}});
			if (excluded) access.expected.back().value=1;
			for (unsigned i=0;i<8;++i) access.expected.push_back({int(r::STATE_AR0+i),!excluded && i==selected ? updated : pointers[i]});
			auto final=observation("retained-address-and-word-not-resampled",{{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,flags},
				{r::STATE_BK,bk},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x9876543210ULL},{r::STATE_IDLE,unsigned(!excluded)}});
			if (excluded) final.expected.back()={r::STATE_ILLEGAL,1};
			for (unsigned i=0;i<8;++i) final.expected.push_back({int(r::STATE_AR0+i),excluded ? pointers[i] : u16(0x2200+i)});
			if (!excluded) final.expected_data={{address,word}};
			auto running=observation("port-sample-after-address-capture",{});
			if (excluded) running.expected.back().value=1;
			clocked_register_case c{{{0,std::move(initial)},{5,std::move(access)},{6,std::move(running)},{16,std::move(final)}},4+unsigned(cases.size()%3),16};
			c.single_fetch=base+0x600;
			c.require_idle=!excluded;
			c.retained_program_reads={{base+0x601,port}};
			c.data_events=std::vector<clocked_data_event>{};
			c.io_events=std::vector<clocked_data_event>{};
			c.actions={{4,{}}}; c.actions.back().program={{base+0x601,0xdead}};
			if (!excluded) {
				c.io_events->push_back({false,port,word,6});
				c.data_events->push_back({true,address,word,7});
				c.volatile_data_space=AS_IO; c.volatile_data_reads={{port,word}};
				c.actions.push_back({5,{}});
				for (unsigned i=0;i<8;++i) c.actions.back().values.push_back({r::STATE_AR0+int(i),0x2200+i});
			}
			cases.push_back(std::move(c));
		}
	}
	return cases;
}
}
