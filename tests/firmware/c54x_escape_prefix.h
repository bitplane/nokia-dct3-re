// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// Acquired AA88 compiler prefix, 03:bdb6..bdc2; not a generic ISA oracle.
	constexpr std::array<u16,13> body{0x1106,0x08f8,0x000b,0xf843,0xbdd2,0xf640,0x10f8,0x0008,0x4e0a,0x560a,0x7002,0x0011,0x4e00};
	for(unsigned length_index=0;length_index<8;++length_index)for(unsigned pattern=0;pattern<4;++pattern)for(bool sxm : {false,true}){
		u16 const length=std::array<u16,8>{12,0,6,7,8,13,21,143}[length_index],stale=std::array<u16,4>{0xff84,0,7,0xffff}[pattern];bool const emits=length>=7;u32 const base=(length_index+pattern+sxm)&1 ? 0x20000 : 0;
		auto initial=initial_case("compiler-prefix-forwards-pending-double-store",{0xf495});initial.program.clear();
		for(unsigned i=0;i<body.size();++i)initial.program.push_back({base+0xbdb6+i,body[i]});
		initial.program.insert(initial.program.end(),{{base+0xbdc3,0xf4e1},{base+0xbdd2,0xf4e1}});
		initial.data={{0x1406,7},{0x140a,0x7777},{0x140b,0x8888},{0x1400,0xaaaa},{0x1401,0xbbbb},{0x1402,0xcccc}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0xbdb6},{r::STATE_ST0,0},{r::STATE_ST1,0x4800|(sxm ? 0x100U : 0U)},{r::STATE_SP,0x1400},{r::STATE_A,length},{r::STATE_B,stale},{r::STATE_AR1,3}});
		auto final=observation("threshold-controls-outgoing-arguments-without-backing-reload",{{r::STATE_B,7}},{{0x1400,emits ? u16(0) : u16(0xaaaa)},{0x1401,emits ? u16(7) : u16(0xbbbb)},{0x1402,emits ? u16(3) : u16(0xcccc)},{0x140a,emits ? u16(0) : u16(0x7777)},{0x140b,emits ? u16(7) : u16(0x8888)}});
		clocked_register_case c{{{0,std::move(initial)},{48,std::move(final)}},6,48};c.data_events=std::vector<clocked_data_event>{{false,0x1406,7,0,false},{false,0xb,7,0,false}};
		if(emits)c.data_events->insert(c.data_events->end(),{{false,8,7,0,false},{true,0x140a,0,0,false},{true,0x140b,7,0,false},{false,0x11,3,0,false},{true,0x1402,3,0,false},{true,0x1400,0,0,false},{true,0x1401,7,0,false}});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
