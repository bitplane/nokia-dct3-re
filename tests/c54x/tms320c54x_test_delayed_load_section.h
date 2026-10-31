// license:BSD-3-Clause
// copyright-holders:Gaz
#include "tms320c54x_test_delayed_memory_section.h"
namespace c54x_test {
inline std::vector<clocked_register_case> delayed_load_cases(bool paired)
{
	std::vector<clocked_register_case> cases;
	for(unsigned control=0;control<6;++control)for(unsigned form=0;form<4;++form)for(unsigned ar=0;ar<8;++ar)
	for(bool compatibility : {false,true})for(bool alternate : {false,true})for(unsigned destination=0;destination<2;++destination){
		if(form==3 && ar)continue; // Absolute addressing has no AR selector.
		unsigned const selector=control+form+ar+compatibility+alternate+destination, pattern=selector%4, flags=(control+form+ar+destination)%4;
		u32 const caller=selector&1 ? 0x20000 : 0;
		unsigned const selected=compatibility && !ar ? 5 : ar;
		u16 const address=paired ? (alternate ? 0x5007 : 0x5004) : (alternate ? 0x5004 : 0x5006), literal=form==3 ? address : paired ? (alternate ? 2 : 0xffff) : (alternate ? 0xffff : 1);
		u16 const memory=std::array<u16,4>{0,1,0x8000,0xffff}[pattern], low=std::array<u16,4>{0x1234,0xffff,0,0x8001}[pattern];
		u16 const st0=0xa608, final_st0=compatibility && ar && form!=3 ? (st0&0x1fff)|(ar<<13) : st0;
		u16 const st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? (paired ? 0x80 : 0x200) : 0)|(compatibility ? 0x20 : 0);
		u64 const a=control==1 ? 0 : control==2 ? 1 : 0x123456789aULL,b=0xfedcba9876ULL;
		u64 const result=paired ? ((u64(memory)<<16)|low|((flags&1)&&(memory&0x8000) ? 0xff00000000ULL : 0)) : u64(flags&1 ? s64(s16(memory)) : s64(memory))&0xffffffffffULL;
		delayed_memory_contract f{};f.control=control;f.destination=destination;f.caller=caller;
		f.opcode=(paired ? 0x5600 : 0x1000)|(destination<<8)|(form==3 ? 0xf8 : (0xe0+form*8)|ar);
		f.literal=literal;f.address=address;f.memory=memory;f.st0=st0;f.final_st0=final_st0;f.st1=st1;f.a=a;f.b=b;f.result=result;f.paired=paired;f.low=low;
		for(unsigned i=0;i<8;++i)f.pointers[i]=0x5100+i;
		f.pointers[selected]=0x5005;f.updated=f.pointers;if(form==1 || form==2)f.updated[selected]=address;
		auto c=delayed_memory_case(f,selector);
		cases.push_back(std::move(c));
	}
	return cases;
}
}
