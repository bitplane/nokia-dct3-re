// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_CONTEXT_POP_POISON
#include "c54x_section_common.h"
namespace c54x_test {
enum class context_access { mapped, pmst, indirect };
inline std::vector<clocked_register_case> active_context_cases(context_access kind)
{
	bool const pmst=kind==context_access::pmst, indirect=kind==context_access::indirect;
	std::vector<clocked_register_case> cases;
	for (bool replace : {false,true}) for (bool span : {false,true}) for (unsigned count : {0U,1U,2U,7U}) for (u32 page : {0U,0x20000U}) {
		u32 const sp=0x3000+((count+unsigned(span)+unsigned(replace)+(page>>16))&1);
		u16 const old_end=0x60b+unsigned(span), start=replace ? span ? 0x1800 : 0x800 : 0x604, end=replace ? start+2 : old_end;
		unsigned const copies=count+1+unsigned(replace), calls=replace ? 1 : count+1;
		auto initial=initial_case("active-repeat-context-software-save-restore-or-replacement",{0xf272,old_end}); initial.program.clear();
		for (u32 base : {0x600U,0x700U,u32(start)}) for (unsigned i=0;i<32;++i) initial.program.push_back({page+base+i,0xf495});
		auto code=[&](u32 address,u16 value) { initial.program.push_back({page+address,value}); };
		code(0x600,0xf272);code(0x601,old_end);code(0x604,0xe589);code(0x605,0xe800);code(0x606,0xfb45);code(0x607,0x700);code(0x60a,0x6d94);code(old_end+1,0x6d97);code(old_end+2,0xf4e1);
		if (replace) {
			code(0x700,0xeefd);code(0x701,0x8a1c);code(0x702,0x8a1b);code(0x703,0x8a1a);
			code(0x704,0x76f8);code(0x705,sp-1);code(0x706,start);code(0x70d,0xfc00);
			code(start,0xe589);code(start+1,0x6d94);code(end+1,0x6d97);code(end+2,0xf4e1);
			initial.data={{sp-4,end},{sp-3,start},{sp-2,u16(count)}};
		} else {
			for (unsigned i=0;i<3;++i) { code(0x700+i,0x4a1a+i);code(0x703+i,0x8a1c-i); }
			code(0x70c,0xfc00);
		}
		if (indirect) {
			unsigned const first=replace ? 0x701 : 0x705;
			if (!replace) {code(0x703,0xf495);code(0x704,0xf495);code(0x70c,0xf495);code(0x70e,0xfc00);}
			code(first,0x8b80);code(first+1,0x8b81);code(first+2,0x8b85);
		}
		if (pmst) {code(replace ? 0x70d : 0x70c,0xf495);code(0x70e,0x4a1d);code(0x711,0x8b80);code(0x718,0xfc00);}
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_SP,sp},{r::STATE_BRC,replace ? 3U : count},
			{r::STATE_A,0},{r::STATE_B,0xfedcba9876ULL},{r::STATE_AR2,0x5000},{r::STATE_AR3,0x6000},{r::STATE_AR4,0x1400},{r::STATE_AR7,0x1700},
			{r::STATE_AR0,pmst ? 0x1dU : indirect ? 0x1cU : 0U},{r::STATE_AR1,0x1b},{r::STATE_AR5,0x1a}});
		auto final=observation("restored-controller-and-balanced-call-frame",{{STATE_GENPC,page+end+3},{r::STATE_SP,sp},{r::STATE_A,0},{r::STATE_B,0xfedcba9876ULL},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_BRC,0},{r::STATE_RSA,start},{r::STATE_REA,end},{r::STATE_PMST,0},
			{r::STATE_AR2,0x5000+copies},{r::STATE_AR3,0x6000+copies},{r::STATE_AR4,0x1401+count},{r::STATE_AR7,0x1701}});
		for (unsigned i=0;i<copies;++i) {u16 const value=0x1357^(i*0x2345);initial.data.push_back({0x5000+i,value});initial.data.push_back({0x6000+i,0xdead});final.expected_data.push_back({0x6000+i,value});}
		clocked_register_case c{{{0,std::move(initial)},{600,std::move(final)}},4,600};c.data_events=std::vector<clocked_data_event>{};
		std::array<unsigned,5> boundaries{};
		for (unsigned call=0;call<calls;++call) {
			u16 const value=0x1357^(call*0x2345);
			c.data_events->push_back({false,0x5000+call,value,0,false});c.data_events->push_back({true,0x6000+call,value,0,false});c.data_events->push_back({true,sp-1,0x60a,0,false});
			if (!call) boundaries[0]=c.data_events->size();
			std::array<u16,3> const regs{0x1a,0x1b,0x1c}, values{u16(replace ? count : count-call),start,end};
			if (!replace) {
				for (unsigned i=0;i<3;++i) { c.data_events->push_back({false,regs[i],values[i],0,false});if(i) c.data_events->push_back({true,sp-1-i,values[i-1],0,false}); }
				c.data_events->push_back({true,sp-4,values[2],0,false});
			}
			if (!call) boundaries[1]=c.data_events->size()+1;
			for (unsigned i=0;i<3;++i) {unsigned const saved=2-i;c.data_events->push_back({false,sp-4+i,values[saved],0,false});if(i)c.data_events->push_back({true,regs[saved+1],values[saved+1],0,false});}
			c.data_events->push_back({true,regs[0],values[0],0,false});
			if (!call) boundaries[2]=c.data_events->size();
			if(replace)c.data_events->push_back({true,sp-1,start,0,false});
			if(pmst) {c.data_events->push_back({false,0x1d,0,0,false});c.data_events->push_back({true,sp-2,0,0,false});c.data_events->push_back({false,sp-2,0,0,false});c.data_events->push_back({true,0x1d,0,0,false});}
			if (!call) boundaries[3]=c.data_events->size();
			c.data_events->push_back({false,sp-1,replace ? start : u16(0x60a),0,false});
			if (!call) boundaries[4]=c.data_events->size();
		}
		if(replace)for(unsigned i=1;i<copies;++i){u16 const value=0x1357^(i*0x2345);c.data_events->push_back({false,0x5000+i,value,0,false});c.data_events->push_back({true,0x6000+i,value,0,false});}
		unsigned const cohort=cases.size()%6;if(cohort)c.checkpoint_after_data_events=boundaries[cohort-1];
		cases.push_back(std::move(c));
	}
	return cases;
}
}
