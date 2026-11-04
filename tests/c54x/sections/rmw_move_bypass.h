// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned operation=0;operation<4;++operation) for (bool absolute : {false,true})
	for (bool indirect : {false,true}) for (unsigned gap : {0U,1U}) for (unsigned pattern=0;pattern<5;++pattern) {
		bool const alias=pattern!=4, ovm=pattern==1;
		u16 const before=std::array<u16,5>{0,0x7fff,0xffff,0x7fff,0x8123}[pattern], immediate=14;
		u16 const source=alias ? 0x6000 : 0x6001;
		s32 const sum=s32(s16(before))+s16(immediate);
		bool const overflow=sum<-32768 || sum>32767;
		u16 const result=operation==0 ? before&immediate : operation==1 ? before|immediate :
			operation==2 ? before^immediate : ovm && overflow ? 0x7fff : u16(sum);
		u16 const selected=alias ? result : 0xa55a;
		u16 flags=0x1000;
		if (operation==3) {
			if (u64(u32(s32(s16(before))))+u32(s32(s16(immediate)))>0xffffffffULL) flags|=0x800;
			if (overflow) flags|=0x400;
		}
		auto initial=initial_case("RAM-RMW-to-Access-owned-MVDK",{u16(0x6800+operation*0x100+(absolute ? 0xf8 : 4))});
		initial.program.erase(initial.program.begin()+1,initial.program.end());
		if (absolute) initial.program.push_back({0x601,0x6000});
		initial.program.push_back({u32(absolute ? 0x602 : 0x601),immediate});
		unsigned pc=absolute ? 0x603 : 0x602;
		if (gap) initial.program.push_back({pc++,0xf495});
		initial.program.push_back({pc++,u16(indirect ? 0x7191 : alias ? 0x7104 : 0x7105)});
		initial.program.push_back({pc++,0x12});initial.program.push_back({pc,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_SP,0x5ffc},{r::STATE_ST1,u16(0x4900|(ovm ? 0x200 : 0))},
			{r::STATE_ST0,0x1000},{r::STATE_AR1,source},{r::STATE_AR2,0x1234}});
		initial.data={{0x6000,before},{0x6001,0xa55a}};
		auto final=observation("same-address-bypass-not-stale-RAM-or-neighbor",{{r::STATE_AR2,selected},
			{r::STATE_AR1,u16(source+unsigned(indirect))},{r::STATE_ST0,flags},{r::STATE_SP,0x5ffc}},
			{{0x6000,u16(alias ? 0xdead : result)},{0x6001,u16(alias ? 0xa55a : 0xdead)}});
		clocked_register_case c{{{0,std::move(initial)},{40,std::move(final)}},6+pattern%4,40};
		c.actions={{12,{},false,{}, {},{{source,0xdead}}}};
		c.data_ranges={{0x6000,0x6001}};
		c.data_events=std::vector<clocked_data_event>{{false,0x6000,before,0,false}};
		if (!gap && !alias) c.data_events->push_back({false,source,selected,0,false});
		c.data_events->push_back({true,0x6000,result,0,false});
		if (gap) c.data_events->push_back({false,source,selected,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
