// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned count : {2,7,17}) for (unsigned size : {1,7,16}) for (unsigned reg=0;reg<8;++reg)
	for (unsigned compatibility=0;compatibility<2;++compatibility) for (unsigned start=0;start<2;++start) {
		unsigned const selected=compatibility && !reg ? 4 : reg, index=start ? size-1 : 0;
		u16 const buffer=0x1200+selected*0x100, st0=compatibility && reg ? 0x608|(reg<<13) : 0x8608;
		u16 const st1=0x900|(compatibility ? 0x20 : 0);
		u32 const base=cases.size()&1 ? 0x20000 : 0;
		// STM BRC; delayed block repeat of a single two-word PORTR instruction.
		auto initial=initial_case("circular-PORTR-under-RPTBD",{0x771a,u16(count-1),0xf272,0x607,0xf495,0xf495,u16(0x74d0|reg),0x27,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_BRC,0},{r::STATE_BK,size},
			{r::STATE_ST0,0x8608},{r::STATE_ST1,st1},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x9876543210ULL}});
		initial.expected.insert(initial.expected.end(),{{r::STATE_READPC,0xffffffff},{r::STATE_EXECPC,0xffffffff}});
		auto final=observation("repeat-drains-with-modulo-writes-and-no-extra-samples",{{r::STATE_BRC,0},{r::STATE_BK,size},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x9876543210ULL}});
		for (unsigned i=0;i<8;++i) {
			initial.initial.push_back({r::STATE_AR0+i,i==selected ? buffer+index : 0x3300+i});
			final.expected.push_back({int(r::STATE_AR0+i),i==selected ? buffer+(index+count)%size : 0x3300+i});
		}
		// Model-regression clocks, not measured silicon: Read12 then a two-clock issue interval.
		unsigned const first=12;
		unsigned const checkpoint=std::array<unsigned,3>{first,first+1,first+2*(count/2)}[cases.size()%3];
		clocked_register_case c{{{0,std::move(initial)},{1,observation("block-active",{})},{96,std::move(final)}},checkpoint,96};
		c.data_events=std::vector<clocked_data_event>{{true,0x1a,u16(count-1),0,false}};
		c.io_events=std::vector<clocked_data_event>{};
		c.volatile_data_space=AS_IO; c.volatile_data_reads={{0x27,0x2468}}; c.volatile_read_increment=1;
		for (unsigned n=0;n<count;++n) {
			clocked_data_event read{false,0x27,u16(0x2468+n),first+2*n};
			read.registers={{r::STATE_READPC,base+0x606}};
			c.io_events->push_back(std::move(read));
			clocked_data_event write{true,u32(buffer+(index+n)%size),u16(0x2468+n),first+2*n+1};
			write.registers={{r::STATE_EXECPC,base+0x606}};
			c.data_events->push_back(std::move(write));
			// Later wraps replace the final word at this address.
			u32 const address=buffer+(index+n)%size;
			bool replaced=false;
			for (auto &word : c.states.back().observation.expected_data) if (word.address==address) { word.value=0x2468+n; replaced=true; }
			if (!replaced) c.states.back().observation.expected_data.push_back({address,u16(0x2468+n)});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
