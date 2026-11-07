// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned header=0;header<3;++header) for (unsigned selector=0;selector<4;++selector)
	for (int shift : {-16,-1,0,1,15}) for (unsigned count : {0U,3U,17U}) {
		u16 const opcode=0xf0e0|(selector<<8)|(shift&31);
		unsigned const body_index=header==1 ? 2 : 1;
		u32 const body=0x600+body_index;
		u16 const st1=cases.size()&1 ? 0xb00 : 0x900;
		auto initial=initial_case("retained-logical-shift-repeat",{u16(header==0 ? 0xec00|count : header==1 ? 0xf070 : 0x4782)});
		if (header==1) initial.program[1].value=count;
		initial.program[body_index].value=opcode;
		initial.program[body_index+1].value=0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x12fedcba98ULL},{r::STATE_B,0xff80000005ULL},
			{r::STATE_ST0,0x1e04},{r::STATE_ST1,st1},{r::STATE_AR2,0x4000}});
		if (header==2) initial.data={{0x4000,u16(count)}};
		std::array<u64,2> accumulators{0x12fedcba98ULL,0xff80000005ULL};
		u16 status=0x1e04;
		std::vector<std::array<u64,3>> states{{accumulators[0],accumulators[1],status}};
		for (unsigned iteration=0;iteration<=count;++iteration) {
			u64 const source=accumulators[selector>>1];
			u64 result=0;
			for (int bit=0;bit<32;++bit) {
				int const from=bit-shift;
				if (from>=0 && from<32 && (source&(u64(1)<<from))) result|=u64(1)<<bit;
			}
			bool const carry=shift && (source&(u64(1)<<(shift<0 ? -shift-1 : 32-shift)));
			status=(status&~0x800)|(carry ? 0x800 : 0);
			accumulators[selector&1]=result;
			states.push_back({accumulators[0],accumulators[1],status});
		}
		unsigned const finish=count+32;
		auto final=observation("all-logical-shifts-and-one-continuation",{{STATE_GENPC,body+2},
			{r::STATE_A,accumulators[0]},{r::STATE_B,accumulators[1]},
			{r::STATE_ST0,status},{r::STATE_ST1,st1},{r::STATE_RC,0},{r::STATE_AR2,0x4000}});
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},3+unsigned(cases.size()%4)*3,finish};
		c.retired_instruction=retired_instruction_check{body,opcode,count+1,0};
		c.retired_accumulator_states=std::move(states);
		c.actions={{header==1 ? 4U : 3U,{},false,{{body,0xe811}}}};
		c.program_events={{false,body,opcode,0,false}};
		c.data_events=std::vector<clocked_data_event>{};
		if (header==2) c.data_events->push_back({false,0x4000,u16(count),0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
