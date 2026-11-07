// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned header=0;header<3;++header) for (unsigned selector=0;selector<4;++selector)
	for (unsigned count : {0U,4U,15U}) for (bool negative : {false,true}) {
		unsigned const index=header==1 ? 2 : 1;
		u32 const body=0x600+index;
		u16 const opcode=0x5092|(selector<<8);
		auto initial=initial_case("repeated-paired-add",{u16(header==0 ? 0xec00|count : header==1 ? 0xf070 : 0x4783)});
		if (header==1) initial.program[1].value=count;
		initial.program[index].value=opcode;
		initial.program[index+1].value=0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x10000},{r::STATE_B,0x20000},
			{r::STATE_ST0,0x1000},{r::STATE_ST1,0x900},{r::STATE_AR2,0x4000},{r::STATE_AR3,0x5000}});
		if (header==2) initial.data.push_back({0x5000,u16(count)});
		std::array<u64,2> acc{0x10000,0x20000};
		std::vector<std::array<u64,3>> states{{acc[0],acc[1],0x1000}};
		std::vector<clocked_data_event> reads;
		if (header==2) reads.push_back({false,0x5000,u16(count),0,false});
		u16 status=0x1000;
		for (unsigned n=0;n<=count;++n) {
			u32 const value=negative ? u32(-s32(n+1)) : (n+1)*0x10003U;
			u16 const address=0x4000+2*n;
			initial.data.push_back({address,u16(value>>16)});
			initial.data.push_back({u32(address^1),u16(value)});
			reads.push_back({false,address,u16(value>>16),0,false});
			reads.push_back({false,u32(address^1),u16(value),0,false});
			u64 const source=acc[selector>>1];
			bool const carry=u64(u32(source))+value>0xffffffffULL;
			acc[selector&1]=(source+s64(s32(value)))&0xffffffffffULL;
			status=0x1000|(carry ? 0x800 : 0);
			states.push_back({acc[0],acc[1],status});
		}
		unsigned const finish=count+32;
		auto final=observation("paired-reads-and-live-accumulator-per-iteration",{{STATE_GENPC,body+2},
			{r::STATE_A,acc[0]},{r::STATE_B,acc[1]},{r::STATE_ST0,status},{r::STATE_ST1,0x900},
			{r::STATE_AR2,0x4000+2*(count+1)},{r::STATE_RC,0}});
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},3+unsigned(cases.size()%4)*3,finish};
		c.retired_instruction=retired_instruction_check{body,opcode,count+1,0};
		c.retired_accumulator_states=std::move(states);
		c.data_events=std::move(reads);
		c.actions={{header==1 ? 4U : 3U,{},false,{{body,0xe811}}}};
		c.program_events={{false,body,opcode,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
