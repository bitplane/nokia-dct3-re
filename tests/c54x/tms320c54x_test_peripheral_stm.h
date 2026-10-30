// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> peripheral_stm_cases(unsigned profile)
{
	std::vector<clocked_register_case> cases;
	std::vector<u16> const addresses=profile==1 ? std::vector<u16>{0x28,0x29,0x2b} :
		profile==2 ? std::vector<u16>{0x28} : std::vector<u16>{0x24,0x25,0x26,0x58,0x22};
	for (u16 address : addresses) for (unsigned pattern=0;pattern<4;++pattern) {
		bool const timer=address>=0x24 && address<=0x26;
		u16 const value=(address==0x22 ? std::array<u16,4>{0xc008,0xc0c8,0,0xffff} :
			address==0x58 ? std::array<u16,4>{0,0x3146,0x8006,0xffff} :
			profile==2 ? std::array<u16,4>{0x2000,0x7fff,0x8006,0xffff} :
			address==0x26 ? std::array<u16,4>{0x10,0x13,0x30,0xffff} :
			std::array<u16,4>{0,0x7fff,0x8006,0xffff})[pattern];
		auto initial=initial_case("immediate-peripheral-write-retains-literal",{u16(0x7700|address),value,0xf4e1});
		if (timer) {
			// TRB seeds PSC=3 while stopped; debugger setup is not observed traffic.
			initial.data={{0x25,0x4567},{0x26,0x33},{0x24,0x2345}};
			initial.initial.push_back({r::STATE_TCR,0xd3});
		} else initial.data={{0x28,0},{0x29,0},{0x2b,0}};
		if (address==0x58) initial.data.insert(initial.data.end(),{{0x58,0},{0x58,0}});
		auto final=observation("one-write-with-device-mask-and-replay",{});
		if (timer) {
			u16 const tcr=address==0x26 ? (value&0x0c1f)|((value&0x20) ? (value&0xf)<<6 : 0xc0) : 0xd3;
			final.expected_data={{0x24,u16(address==0x24 ? value : address==0x26 && (value&0x20) ? 0x4567 : 0x2345)},
				{0x25,u16(address==0x25 ? value : 0x4567)},{0x26,tcr}};
			final.expected.push_back({r::STATE_IFR,0});
		} else final.expected_data={{address,u16(address==0x58 ? (value&~u16(1))|BIT(value,1) :
			value&(profile==2 ? 0x7fff : address==0x29 ? 0xf006 : address==0x2b ? 1 : 0xffff))}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("STM-running",{})},{12,std::move(final)}},
			std::array<unsigned,3>{6,7,9}[cases.size()%3],12};
		c.actions={{5,{},false,{{0x601,u16(value^0xffff)}}}};
		c.retained_program_reads={{0x601,value}};
		c.data_events=std::vector<clocked_data_event>{{true,address,value,8}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
