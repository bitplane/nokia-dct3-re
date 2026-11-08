// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (unsigned source=0;source<2;++source) for (unsigned destination=0;destination<2;++destination)
    for (bool fractional:{false,true}) for (unsigned shape=0;shape<3;++shape) {
        unsigned const count=shape==1 ? 128 : 8;
        bool const wide=shape!=1;
        u8 const x=shape==1 ? 12 : 8,y=shape==1 ? 13 : shape==2 ? 8 : 9;
        u16 const opcode=0xcc00|(source<<9)|(destination<<8)|(x<<4)|y;
        auto initial=initial_case("repeated-parallel-MPY-retained-X-T-live-store",{0xec00,opcode,0xf4e1});
        initial.program[0].value=wide ? 0xf070 : u16(0xec00|(count-1));
        if (wide) {
            initial.program[1].value=count-1;initial.program[2].value=opcode;initial.program[3].value=0xf4e1;
        }
        std::array<u16,4> ar{u16(shape==1 ? 0x1407 : 0x1400),u16(shape==1 ? 0x1606 : 0x1600),0x1800,0x1900};
        std::array<u64,2> accumulators{0x123456789aULL,0xffabcdef01ULL};
        initial.initial.insert(initial.initial.end(),{{r::STATE_A,accumulators[0]},{r::STATE_B,accumulators[1]},
            {r::STATE_T,0xfffd},{r::STATE_ST0,0xa002},{r::STATE_ST1,0x100U|(fractional ? 0x40U : 0)},
            {r::STATE_PMST,0},{r::STATE_AR0,2},{r::STATE_BK,8}});
        for (unsigned n=0;n<ar.size();++n)initial.initial.push_back({r::STATE_AR2+n,ar[n]});
        clocked_register_case c{{{0,std::move(initial)}},shape==1 ? 40U : 9U,count+32};
        c.data_events=std::vector<clocked_data_event>{};
        std::map<u16,u16> memory,seeded;
        auto sample=[&](u16 address)->u16 {
            if (!memory.count(address))memory[address]=seeded[address]=0x10+(address&7);
            return memory[address];
        };
        auto modify=[&](u16 address)->u16 {return shape==1 ? u16((address&~u16(15))|((address+2)%8)) : u16(address+1);};
        u16 previous_address=0,previous_word=0;
        unsigned const read=6+unsigned(wide);
        for (unsigned n=0;n<count;++n) {
            u16 const xa=ar[x&3],ya=ar[y&3],word=sample(xa);
            c.data_events->push_back({false,xa,word,read+n});
            if (n) {
                memory[previous_address]=previous_word;
                c.data_events->push_back({true,previous_address,previous_word,read+n});
            }
            ar[x&3]=modify(xa);if ((x&3)!=(y&3))ar[y&3]=modify(ya);
            previous_address=ya;previous_word=u16(accumulators[source]>>16);
            accumulators[destination]=u64(s64(s16(word))*(n&1 ? 3 : -3)*(fractional ? 2 : 1))&0xffffffffffULL;
            // Change T after Read: this copy retains its sample; the next
            // must acquire the new one instead of reusing the first issue.
            c.actions.push_back({read+n,{{r::STATE_T,n+1==count ? 0x55aaU : n&1 ? 0xfffdU : 3U}}});
        }
        memory[previous_address]=previous_word;
        c.data_events->push_back({true,previous_address,previous_word,read+count});
        for (auto const &[address,value]:seeded)c.states[0].observation.data.push_back({address,value});
        auto final=observation("all-products-and-old-source-stores-once-per-iteration",
            {{r::STATE_A,accumulators[0]},{r::STATE_B,accumulators[1]},{r::STATE_T,0x55aa},
             {r::STATE_RC,0},{r::STATE_ST0,0xa002},{r::STATE_BK,8},
             {STATE_GENPC,wide ? 0x604U : 0x603U}});
        for (unsigned n=0;n<ar.size();++n)final.expected.push_back({int(r::STATE_AR2+n),ar[n]});
        for (auto const &[address,value]:memory)final.expected_data.push_back({address,value});
        c.states.push_back({count+32,std::move(final)});
        cases.push_back(std::move(c));
    }
    for (bool mapped_source:{false,true}) {
        auto initial=initial_case("repeated-parallel-MPY-retains-mapped-boundary",{0xec00,0xcc89,0xf4e1});
        initial.initial.insert(initial.initial.end(),{{r::STATE_AR2,mapped_source ? 0x10U : 0x1400U},
            {r::STATE_AR3,mapped_source ? 0x1600U : 0x20U}});
        initial.expected.clear();
        auto final=observation("reject-before-DAGEN-or-bus-effects",{{r::STATE_ILLEGAL,1},
            {r::STATE_AR2,mapped_source ? 0x10U : 0x1400U},{r::STATE_AR3,mapped_source ? 0x1600U : 0x20U}});
        final.expected.pop_back();
        clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},3,20};
        c.require_idle=false;c.forbid_data_access=true;cases.push_back(std::move(c));
    }
    return cases;
}
}
