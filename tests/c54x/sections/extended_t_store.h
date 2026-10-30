// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned excluded=0;excluded<2;++excluded) for (unsigned form=0;form<4;++form)
	for (unsigned reg=0;reg<8;++reg) for (unsigned compatibility=0;compatibility<2;++compatibility)
	for (unsigned trn=0;trn<2;++trn) for (unsigned pattern=0;pattern<(excluded ? 1U : 4U);++pattern)
	for (unsigned peripheral=0;peripheral<(excluded ? 1U : 2U);++peripheral) {
		unsigned const selected=compatibility && !reg ? 4 : reg, write=7+peripheral;
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		u16 const value=std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		u16 const destination=excluded ? 0xe : peripheral ? 0x48+selected : 0x1201+selected*0x20;
		u16 const mode=form ? (0xe0+(form-1)*8)|reg : 0xf8, literal=form==0 ? destination : form==1 ? 0xfff0 : 1;
		u16 const initial_st0=compatibility ? 0x8608 : 0x608;
		u16 const final_st0=!excluded && compatibility && reg && form ? 0x608|(reg<<13) : initial_st0;
		std::array<u16,8> ar{}; for (unsigned i=0;i<8;++i) ar[i]=0x2000+i*0x20;
		ar[selected]=form==1 ? destination+16 : destination-1;
		auto final_ar=ar; if (!excluded && form>=2) final_ar[selected]=destination;
		u16 const opcode=(trn ? 0x8d00 : 0x8c00)|mode;
		auto initial=initial_case("extended-T-TRN-store-address-and-live-source-ownership",{opcode,literal,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,initial_st0},
			{r::STATE_ST1,0x900|(compatibility ? 0x20U : 0)},{r::STATE_BK,16},{r::STATE_T,0x5aa5},{r::STATE_TRN,0x5aa5},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x9876543210ULL}});
		for (unsigned i=0;i<8;++i) initial.initial.push_back({r::STATE_AR0+i,ar[i]});
		auto read=observation("destination-captured-before-source-Execute",{});
		if (excluded) { read.expected.clear(); read.expected.push_back({r::STATE_ILLEGAL,1}); }
		else for (unsigned i=0;i<8;++i) read.expected.push_back({int(r::STATE_AR0+i),final_ar[i]});
		auto final=observation("one-publication-or-guard-without-pointer-side-effects",{{r::STATE_ILLEGAL,excluded},{r::STATE_IDLE,excluded ? 0U : 1U},
			{r::STATE_ST0,final_st0},{r::STATE_ST1,0x900|(compatibility ? 0x20U : 0)},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x9876543210ULL}});
		final.expected.pop_back(); // Explicit rejection cases must not inherit ILLEGAL=0.
		for (unsigned i=0;i<8;++i) final.expected.push_back({int(r::STATE_AR0+i),excluded ? u64(ar[i]) : 0x2200+i});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("long-store-running",{})},{6,std::move(read)},
			{7,excluded ? observation("guard-remains-latched",{{r::STATE_ILLEGAL,1}}) : observation("captured-pointer-no-longer-live",{})},
			{16,std::move(final)}},std::array<unsigned,3>{5,6,write}[cases.size()%3],16};
		if (excluded) c.states[3].observation.expected.pop_back();
		c.require_idle=!excluded;
		c.actions={{4,{},false,{{base+0x601,0xdead}}}};
		if (!excluded) {
			clocked_register_action change{6,{}};
			if (!peripheral) change.values.push_back({trn ? r::STATE_TRN : r::STATE_T,value});
			for (unsigned i=0;i<8;++i) change.values.push_back({r::STATE_AR0+i,0x2200+i});
			c.actions.push_back(std::move(change));
			if (peripheral) c.actions.push_back({7,{{trn ? r::STATE_TRN : r::STATE_T,value}}});
			c.retained_program_reads={{base+0x601,literal}};
		}
		c.data_events=std::vector<clocked_data_event>{};
		if (!excluded) c.data_events->push_back({true,destination,value,write});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
