// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    constexpr std::array<std::array<unsigned,2>,4> shapes{{{0,4},{1,4},{3,4},{5,0}}};
    auto ram_word=[](u16 address)->u16 { return address^0x1357; };
    for (unsigned mode=0;mode<3;++mode) for (unsigned shape=0;shape<shapes.size();++shape) {
        auto const [first,count]=shapes[shape];
        bool const compatibility=(mode+shape)&1,wide=(mode+shape)&2;
        u32 const page=wide ? 0x20000 : 0;
        u16 const opcode=0x7180|(mode<<3)|(compatibility ? 0 : 5);
        auto initial=initial_case("repeated-MVDK-RAM-to-progressing-AR-destinations",{0xf495});
        initial.program[0].value=wide ? 0xf070 : u16(0xec00|count);
        if (wide)initial.program[1].value=count;
        initial.program[wide ? 2 : 1].value=opcode;
        initial.program[wide ? 3 : 2].value=0x10+first;
        initial.program[wide ? 4 : 3].value=0xf4e1;
        for (auto &word:initial.program)word.address+=page;
        std::array<u16,8> ar{};
        for (unsigned n=0;n<ar.size();++n)ar[n]=0x2000+n;
        ar[5]=0x1400;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST0,0xa620},
            {r::STATE_ST1,0x800U|(compatibility ? 0x20U : 0)},{r::STATE_SP,0x3000}});
        for (unsigned n=0;n<ar.size();++n)initial.initial.push_back({r::STATE_AR0+n,ar[n]});
        clocked_register_case c{{{0,std::move(initial)}},wide ? 8U : 6U,32};
        c.data_events=std::vector<clocked_data_event>{};
        std::map<u16,u16> ram;
        for (unsigned n=0;n<=count;++n) {
            u16 const source=ar[5],value=ram_word(source);
            if (source<0x60)throw emu_fatalerror("Invalid repeated MVDK RAM source geometry");
            ram[source]=value;
            ar[5]+=mode==1 ? -1 : mode==2 ? 1 : 0;
            ar[first+n]=value;
            c.data_events->push_back({false,source,value,6+unsigned(wide)+n});
            c.data_events->push_back({true,0x10+first+n,value,6+unsigned(wide)+n});
        }
        for (auto const &[address,value]:ram)c.states[0].observation.data.push_back({address,value});
        auto final=observation("old-source-postmodify-before-mapped-write",{{r::STATE_RC,0},
            {r::STATE_ST0,0xa620},{r::STATE_SP,0x3000},{STATE_GENPC,page+(wide ? 0x605U : 0x604U)}});
        for (unsigned n=0;n<ar.size();++n)final.expected.push_back({int(r::STATE_AR0+n),ar[n]});
        c.states.push_back({32,std::move(final)});
        cases.push_back(std::move(c));
    }
    for (bool mapped_source:{false,true}) {
        u16 const pointer=mapped_source ? 0x10 : 0x1400;
        auto initial=initial_case("repeated-MVDK-preserves-unqualified-mapped-boundaries",
            {0xec00,0x7195,u16(mapped_source ? 0x11 : 0x18),0xf4e1});
        initial.initial.insert(initial.initial.end(),{{r::STATE_AR5,pointer},{r::STATE_SP,0x3000}});
        initial.expected.clear();
        auto final=observation("reject-before-source-pointer-or-destination-effects",
            {{r::STATE_ILLEGAL,1},{r::STATE_AR5,pointer},{r::STATE_SP,0x3000}});
        final.expected.pop_back();
        clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},3,20};
        c.require_idle=false;c.forbid_data_access=true;
        cases.push_back(std::move(c));
    }
    return cases;
}
}
