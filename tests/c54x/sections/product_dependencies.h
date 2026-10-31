// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned group=0;group<6;++group)for(unsigned operation=0;operation<(group==0 ? 18U : group==3 ? 6U : group==5 ? 1U : 14U);++operation)
	for(unsigned layout=0;layout<6;++layout)for(unsigned pattern=0;pattern<8;++pattern){
		unsigned flags=(group+operation+layout+pattern)%16;if(group==5)flags&=~4U; // POLY SMUL is not qualified.
		bool const memory=group==1 || group==2 || group==4 || group==5,immediate=group==3,poly=group==5,mapped=group==2;
		unsigned const length=group==3 || group==4 ? 2 : 1,read=6+length;
		u32 const base=(group+operation+layout+pattern)&1 ? 0x20000 : 0;
		u16 const opcode=group==0 ? (operation<2 ? 0xf48c|(operation<<8) : 0xf488|((operation-2)&3)|(((operation-2)&4)<<6)|(((operation-2)&8)<<6)) :
			group==3 ? std::array<u16,6>{0xf066,0xf166,0xf067,0xf167,0xf267,0xf367}[operation] : poly ? 0x3681 :
			std::array<u16,14>{0x2000,0x2100,0x2200,0x2300,0x2400,0x2500,0x2800,0x2900,0x2a00,0x2b00,0x2c00,0x2d00,0x2e00,0x2f00}[operation]|(group==4 ? 0xf8 : mapped ? 0xe : 0x81);
		u16 const st0=(0x1804|(pattern&1 ? 0x600 : 0))&u16(mapped ? ~0x1ff : 0xffff),st1=0x800|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0)|(flags&8 ? 0x100 : 0);
		u64 const initial_a=std::array<u64,8>{0,0x12340000,0xff87651111,0x0080000000,0x0180000000,0xff80000000,0xff00000000,0x7fffffffff}[pattern];
		u64 const b=std::array<u64,8>{0x1234,0x7fff8000,0x3200000,0xff80000000,0x7fffffff,0xffffffffff,0x80aa001234,0x7fffffffff}[pattern],a=layout==3 ? 1 : initial_a;
		u16 const initial_t=std::array<u16,8>{0,1,0x4567,0x8000,0xffff,0x7fff,0x8001,0x1234}[pattern];
		u64 magnitude=initial_a&0x8000000000ULL ? (~initial_a)&0xffffffffffULL : initial_a;unsigned significant=0;while(magnitude){++significant;magnitude>>=1;}
		u16 const t=layout==1 || layout==2 ? u16(b) : layout==5 ? (initial_a ? u16(31-int(significant)) : 0) : initial_t,operand=mapped ? t : 0x8001;
		unsigned const family=(opcode>>8)&0xe;int const lane=(a>>16)&0x1ffff; s64 const high=lane&0x10000 ? lane-0x20000 : lane;
		s64 product=(immediate ? s64(s16(t))*s16(operand) : memory && !poly ? family==4 ? s64(t)*operand : s64(s16(t))*s16(operand) : high*s16(t))*(flags&1 ? 2 : 1);
		bool const multiply=!poly && (immediate ? (opcode&0xfeff)==0xf066 : memory ? family<=4 : (opcode&0xfeff)==0xf48c);
		if(!multiply && (flags&7)==7)product=std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
		u64 const source=poly || (opcode&(memory && !poly ? 0x100 : 0x200)) ? b : a;
		s64 const signed_source=source&0x8000000000ULL ? s64(source)-0x10000000000LL : s64(source);
		s64 result=multiply ? product : signed_source+(!poly && !immediate && (memory ? family>=12 : bool(opcode&2)) ? -product : product);
		if(poly || (!immediate && (memory ? family==2 || family==10 || family==14 : !multiply && (opcode&1))))result=(result+32768)&~s64(65535);
		bool const overflow=result<-2147483648LL || result>2147483647LL;
		u64 const stored=u64(overflow && (flags&2) ? std::clamp(result,s64(-2147483648LL),s64(2147483647LL)) : result)&0xffffffffffULL;
		u64 const expected_a=poly ? stored : opcode&0x100 ? a : stored,expected_b=poly ? u64((flags&8 ? s64(s16(operand)) : operand)*65536)&0xffffffffffULL : opcode&0x100 ? stored : b;
		u16 const expected_st0=st0|(overflow ? opcode&0x100 ? 0x200 : 0x400 : 0);
		auto initial=initial_case("product-dependency-Read-T-and-Execute-accumulator-ownership",{0xf495});
		for(auto &word : initial.program)word.address+=base;
		if(layout==1 || layout==2)initial.program.push_back({base+0x600+unsigned(layout==1),0x890e});
		if(layout==3)initial.program.push_back({base+0x601,0xe801});
		if(layout==5)initial.program.push_back({base+0x601,0xf48e});
		initial.program.push_back({base+0x602,opcode});if(length==2)initial.program.push_back({base+0x603,u16(immediate ? 0x8001 : 0x1800)});
		if(layout==4)initial.program.insert(initial.program.end(),{{base+0x602+length,0x770e},{base+0x603+length,0xbeef},{base+0x604+length,0xf4e1}});
		else initial.program.push_back({base+0x602+length,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_PMST,flags&4 ? 2U : 0U},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_A,initial_a},{r::STATE_B,b},{r::STATE_T,initial_t},{r::STATE_AR1,0x1800}});initial.data={{0x1800,0x8001}};
		auto final=observation("product-retains-operands-and-preserves-later-T",{{r::STATE_A,expected_a},{r::STATE_B,expected_b},{r::STATE_ST0,expected_st0},{r::STATE_ST1,st1},{r::STATE_T,layout==4 ? 0xbeefU : 0xdeadU},
			{STATE_GENPC,base+(layout==4 ? 0x605 : 0x603)+length}});
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},std::array<unsigned,3>{4,6+length,7+length}[(group+operation+layout+pattern)%3],24};
		clocked_register_action poison{};poison.tick=read;poison.values={{r::STATE_T,0xdead}};if(memory && !mapped)poison.data={{0x1800,0xdead}};if(length==2)poison.program={{base+0x603,0xdead}};c.actions.push_back(poison);
		c.data_events=std::vector<clocked_data_event>{};if(layout==2 || (layout==1 && (!memory || mapped || length==2)))c.data_events->push_back({true,0xe,u16(b),0,false});
		if(memory)c.data_events->push_back({false,mapped ? 0xeU : 0x1800U,operand,read,true});
		if(layout==1 && memory && !mapped && length==1)c.data_events->push_back({true,0xe,u16(b),0,false});
		if(layout==4)c.data_events->push_back({true,0xe,0xbeef,0,false});
		c.program_events={{false,base+0x602,opcode,0,false}};if(length==2)c.program_events.push_back({false,base+0x603,u16(immediate ? 0x8001 : 0x1800),0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
