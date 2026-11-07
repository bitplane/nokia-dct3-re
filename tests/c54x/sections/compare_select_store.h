// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned b=0;b<2;++b) for (u32 pair : {0U,0x00010000U,0xffff0000U,0x7fff8000U,0x80007fffU,0xffffffffU})
	for (unsigned mode=0;mode<5;++mode) for (u16 trn : {0x1234,0xffff}) {
		u8 const operand=mode<2 ? 0x20 : mode==2 ? 0x83 : mode==3 ? 0x93 : 0x8b;
		u16 const address=mode==0 ? 0x420 : mode==1 ? 0x1020 : 0x4000;
		u16 const next=mode==3 ? 0x4001 : mode==4 ? 0x3fff : 0x4000;
		u16 const st1=mode==1 ? 0x4800 : 0x800;
		s32 const high=(pair>>16)&0x8000 ? s32(pair>>16)-0x10000 : s32(pair>>16);
		s32 const low=pair&0x8000 ? s32(pair&0xffff)-0x10000 : s32(pair&0xffff);
		bool const choose_low=high<=low;
		u16 const value=choose_low ? pair : pair>>16;
		auto initial=initial_case("CMPS-signed-halves-captured-in-Read",{u16(0x8e00|(b<<8)|operand),0xf495,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,b ? 0x123456789aULL : 0xaa00000000ULL|pair},
			{r::STATE_B,b ? 0xbb00000000ULL|pair : 0x23456789abULL},{r::STATE_TRN,trn},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_AR3,0x4000},{r::STATE_SP,0x1000}});
		initial.data={{address,0xdead}};
		auto final=observation("captured-maximum-live-TRN-and-unchanged-accumulators",{{STATE_GENPC,0x603},
			{r::STATE_A,0x0102030405ULL},{r::STATE_B,0x8070605040ULL},{r::STATE_TRN,u16((0x5678<<1)|choose_low)},
			{r::STATE_ST0,u16(0xa608|(choose_low ? 0x1000 : 0))},{r::STATE_ST1,st1},
			{r::STATE_AR3,0x5000},{r::STATE_SP,0x1000}},{{address,value}});
		auto accessed=observation("Access-updates-pointer-not-result",{{r::STATE_AR3,next},{r::STATE_TRN,trn}},{{address,0xdead}});
		auto read=observation("comparison-retained-without-early-TC-publication",{{r::STATE_AR3,next},{r::STATE_TRN,trn},{r::STATE_ST0,0xa608}});
		auto executed=observation("Execute-publishes-TC-TRN-and-memory",{{r::STATE_AR3,0x5000},
			{r::STATE_TRN,u16((0x5678<<1)|choose_low)},{r::STATE_ST0,u16(0xa608|(choose_low ? 0x1000 : 0))}},{{address,value}});
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(accessed)},{5,std::move(read)},{6,std::move(executed)},{16,std::move(final)}},
			std::array<unsigned,4>{3,4,5,6}[cases.size()%4],16};
		c.actions={{5,{{r::STATE_A,0x0102030405ULL},{r::STATE_B,0x8070605040ULL},{r::STATE_TRN,0x5678},{r::STATE_AR3,0x5000}}}};
		c.data_events=std::vector<clocked_data_event>{{true,address,value,6}};
		cases.push_back(std::move(c));
	}
	for (unsigned b=0;b<2;++b) for (unsigned mode : {0xc3U,0xcbU,0xd3U,0xdbU})
	for (int index : {-2,1,2}) for (unsigned position : {0U,3U,7U}) {
		int const step=mode==0xc3 ? -1 : mode==0xcb ? -index : mode==0xd3 ? 1 : index;
		u16 const address=0x4000+position, next=0x4000+(int(position)+step+8)%8;
		auto initial=initial_case("CMPS-circular-address-is-Access-owned",{u16(0x8e00|(b<<8)|mode),0xf495,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x1234fffe},{r::STATE_B,0xfffe1234},
			{r::STATE_AR0,u16(index)},{r::STATE_AR3,address},{r::STATE_BK,8},{r::STATE_TRN,0x5555},
			{r::STATE_ST0,0x604},{r::STATE_ST1,0x800}});
		initial.data={{address,0xdead}};
		auto accessed=observation("wrapped-pointer-before-store",{{r::STATE_AR3,next},{r::STATE_TRN,0x5555}},{{address,0xdead}});
		auto final=observation("circular-store-retains-original-address",{{r::STATE_A,0x1234fffe},{r::STATE_B,0xfffe1234},{r::STATE_AR3,next},{r::STATE_BK,8},
			{r::STATE_TRN,0xaaaa|b},{r::STATE_ST0,u16(0x604|(b ? 0x1000 : 0))},{r::STATE_ST1,0x800}},{{address,0x1234}});
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(accessed)},{6,std::move(final)}},
			std::array<unsigned,4>{3,4,5,6}[cases.size()%4],16};
		c.data_events=std::vector<clocked_data_event>{{true,address,0x1234,6}};
		cases.push_back(std::move(c));
	}
	for (unsigned kind=0;kind<3;++kind) {
		auto initial=initial_case("unqualified-CMPS-composition",{0xf495});
		unsigned const at=kind==2 ? 1 : 0;
		if (kind==2) initial.program[0].value=0xec01;
		initial.program[at].value=kind==1 ? 0x8ef8 : 0x8e93;
		if (kind==1) initial.program[at+1].value=0x4000;
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR3,kind==0 ? 8U : 0x4000U},
			{r::STATE_A,0x123456789aULL},{r::STATE_TRN,0x1234},{r::STATE_ST0,0x604}});
		initial.expected.clear();
		auto final=observation("no-mapped-write-or-partial-flag-effect",{{r::STATE_ILLEGAL,1},
			{r::STATE_AR3,kind==0 ? 8U : 0x4000U},{r::STATE_A,0x123456789aULL},
			{r::STATE_TRN,0x1234},{r::STATE_ST0,0x604}});
		final.expected.pop_back();
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},3,16};
		c.require_idle=false;
		c.data_events=std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
