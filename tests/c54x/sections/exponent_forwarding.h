// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u64 value : {0ULL,1ULL,2ULL,0x7fffULL,0x8000ULL,0x7fffffffULL,0x80000000ULL,0xffffffffULL,
		0x0100000000ULL,0x7fffffffffULL,0x8000000000ULL,0xffffffffffULL,0xffffffffcbULL,
		0x0785432105ULL,0xff80000000ULL,0xff00000000ULL})
	for (unsigned b=0;b<2;++b) for (unsigned gap=0;gap<3;++gap)
	for (unsigned flags=0;flags<8;++flags) for (unsigned producer=0;producer<2;++producer) {
		u64 const source=producer ? u16(value) : value;
		u64 magnitude=source & 0x8000000000ULL ? (~source)&0xffffffffffULL : source;
		unsigned significant=0; while (magnitude) { ++significant; magnitude>>=1; }
		u16 const exponent=source ? u16(31-int(significant)) : 0;
		u16 const st0=0xa602|(flags&4 ? 0x1800 : 0),st1=0x800|((flags&3)<<8);
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		auto initial=initial_case("EXP-T-publication-and-LDM-forwarding",{0xf495});
		for (auto &word : initial.program) word.address+=base;
		auto word=[&](unsigned offset,u16 opcode) { initial.program[offset].value=opcode; };
		if (producer) word(0,b ? 0x1350 : 0x1250);
		word(producer,b ? 0xf58e : 0xf48e);
		word(1+producer+gap,b ? 0x480e : 0x490e); word(2+producer+gap,0xf4e1);
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_T,0x8123},{r::STATE_A,b ? 0x123456789aULL : producer ? 0x5555555555ULL : value},
			{r::STATE_B,!b ? 0x123456789aULL : producer ? 0x5555555555ULL : value}});
		initial.data={{0x150,u16(value)}};
		clocked_register_case c{{{0,std::move(initial)},
			{5+producer,observation("EXP-before-Execute",{{r::STATE_T,0x8123}})},
			{6+producer,observation("EXP-publishes-signed-exponent",{{r::STATE_T,exponent}})},
			{7+producer,observation("consumer-and-later-T-mutation",{})},
			{16,observation("EXP-source-flags-and-consumer-result",{{r::STATE_IDLE,1},
				{int(b ? r::STATE_B : r::STATE_A),source},{int(b ? r::STATE_A : r::STATE_B),gap ? exponent : 0x8123U},
				{r::STATE_T,0x4321},{r::STATE_ST0,st0},{r::STATE_ST1,st1}})}},
			std::array<unsigned,4>{3,5+producer,6+producer+gap,8+producer+gap}[cases.size()%4],16};
		if (producer) c.actions.push_back({5,{},false,{},{},{{0x150,0xdead}}});
		c.actions.push_back({6+producer+gap,{{r::STATE_T,0x4321}}});
		c.data_events=std::vector<clocked_data_event>{};
		if (producer) c.data_events->push_back({false,0x150,u16(value),5});
		c.data_events->push_back({false,0xe,u16(gap ? exponent : 0x8123),6+producer+gap});
		c.program_events.push_back({false,base+0x600+producer,u16(b ? 0xf58e : 0xf48e),0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
