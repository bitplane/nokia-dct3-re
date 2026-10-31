// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_COUNT_RETIREMENTS
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> memory_repeat_cases(bool store)
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,15U,32U,255U})for(unsigned form : {0U,1U,2U,3U,5U})for(bool long_count : {false,true})
	for(unsigned destination=0;destination<(store ? 1U : 2U);++destination)for(unsigned pattern=0;pattern<4;++pattern)for(unsigned irq=0;irq<3;++irq){
		unsigned const seed=count+form+long_count+destination+pattern+irq,flags=seed%(store ? 8 : 4),first=long_count ? 8 : 7;
		u32 const base=seed&1 ? 0x20000 : 0,body=base+(long_count ? 0x602 : 0x601);
		u16 const word=store ? std::array<u16,4>{0,1,0x8000,0xffff}[pattern] : std::array<u16,4>{7,1,0x1234,0xffff}[pattern];
		u64 const dividend=store ? 0 : std::array<u64,4>{0x41,0x7fff,0x12345678,0x1fffffff}[pattern];
		u16 const st1=(irq==1 ? 0 : 0x800)|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0)|(form==1 ? 0x4000 : 0);
		u16 const opcode=(store ? 0x7600 : 0x1e00|(destination<<8))|std::array<u16,6>{2,2,0x91,0x81,0x99,0x89}[form];
		auto initial=initial_case("memory-repeat-retains-header-and-defers-physical-interrupt",{u16(long_count ? 0xf070 : 0xec00|count)});
		for(auto &entry : initial.program)entry.address+=base;
		if(long_count)initial.program.push_back({base+0x601,u16(count)});
		initial.program.insert(initial.program.end(),{{body,opcode},{body+(store ? 3U : 2U),0xf4e1}});if(store)initial.program.push_back({body+1,word});
		int const line=irq==2 ? INPUT_LINE_NMI : 0;u32 const vector=base+(irq==2 ? 4U : 0x40U);
		initial.program.insert(initial.program.end(),{{vector,0x4a1e},{vector+1,0xf495},{vector+2,0xf4e5}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_PMST,store && (flags&4) ? 1U : 0U},{r::STATE_ST0,0x1804},{r::STATE_ST1,st1},
			{r::STATE_SP,0x1000},{r::STATE_AR1,form==5 ? 0x400U : 0x200U},{r::STATE_A,destination ? 0x1234 : dividend},{r::STATE_B,destination ? dividend : 0x1234},{r::STATE_IMR,irq==1 ? 1U : 0U}});
		for(unsigned at=0x100;at<=0x400;++at)initial.data.push_back({at,word});
		initial.data.push_back({0x1002,word});
		unsigned const last=store ? first+1+2*count : first+count,finish=last+30;
		u64 accumulator=dividend;u16 status=0x1804;
		std::vector<std::array<u64,3>> states{{destination ? 0x1234 : accumulator,destination ? accumulator : 0x1234,status}};
		if(!store)for(unsigned i=0;i<=count;++i){
			s64 const left=accumulator&0x8000000000ULL ? s64(accumulator)-0x10000000000LL : s64(accumulator),divisor=flags&1 ? s16(word) : word,difference=left-divisor*32768;
			status=(status&~0x800)|(difference>=0 ? 0x800 : 0);if(difference<-2147483648LL || difference>2147483647LL)status|=destination ? 0x200 : 0x400;
			accumulator=u64(difference>=0 ? difference*2+1 : left*2)&0xffffffffffULL;
			states.push_back({destination ? 0x1234 : accumulator,destination ? accumulator : 0x1234,status});
		}
		auto final=observation("complete-repeat-and-FRETE-restore-without-extra-transactions",{{STATE_GENPC,body+(store ? 4U : 3U)},{r::STATE_SP,0x1000},{r::STATE_RC,0},{r::STATE_PMST,store && (flags&4) ? 1U : 0U},
			{r::STATE_AR1,form==2 ? 0x201+count : form==5 ? 0x3ff-count : 0x200},{r::STATE_A,destination ? 0x1234 : accumulator},{r::STATE_B,destination ? accumulator : 0x1234},{r::STATE_ST0,status},{r::STATE_ST1,irq ? st1&~0x800U : st1}});
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},std::array<unsigned,4>{3,first,first+count/2,last}[seed%4],finish};
		c.retired_instruction=retired_instruction_check{body,opcode,count+1,store ? 0U : first};if(!store)c.retired_accumulator_states=std::move(states);
		if(store)for(unsigned i=0;i<=count;++i){
			c.states.insert(c.states.end()-1,{first+1+2*i,observation("store-publication-count-and-address-progress",{{r::STATE_RC,count-i},{r::STATE_AR1,form==2 ? 0x201+i : form==5 ? 0x3ff-i : 0x200},{r::STATE_ST0,0x1804}})});
			if(i<count)c.states.insert(c.states.end()-1,{first+2+2*i,observation("next-address-precedes-next-store",{{r::STATE_RC,count-i},{r::STATE_AR1,form==2 ? 0x202+i : form==5 ? 0x3fe - i : 0x200}})});
		}
		c.initial_input_lines={{0,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
		if(irq){clocked_register_action pulse{};pulse.tick=2;pulse.input_lines={{line,ASSERT_LINE},{line,CLEAR_LINE}};c.actions.push_back(pulse);}
		clocked_register_action poison{};poison.tick=long_count ? 4 : 3;poison.program={{body,0xe811}};c.actions.push_back(poison);
		if(store){clocked_register_action literal{};literal.tick=long_count ? 6 : 5;literal.program={{body+1,0xdead}};c.actions.push_back(literal);}
		c.program_events={{false,body,opcode,0,false}};if(store)c.program_events.push_back({false,body+1,word,0,false});
		c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<=count;++i){u32 const address=form==0 ? 0x202 : form==1 ? 0x1002 : form==5 ? 0x400-i : form==2 ? 0x200+i : 0x200;
			c.data_events->push_back({store,address,word,store ? first+1+2*i : !i ? first-1 : first+i,true});}
		if(irq){clocked_data_event frame{true,0xfff,u16(body+(store ? 2 : 1)),0,false};frame.value_register=r::STATE_RTN;c.data_events->push_back(frame);
			c.data_events->push_back({false,0x1e,u16(base>>16),0,false});c.data_events->push_back({true,0xffe,u16(base>>16),0,false});
			c.data_events->push_back({false,0xffe,u16(base>>16),0,false});frame.write=false;c.data_events->push_back(frame);c.irqs={{line,last+1,~0U,count+1,u16(body+(store ? 2 : 1)),{{r::STATE_RC,0}}}};}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
