// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// RPT ADD/SUB Xmem,SHFT,src: each issue reads its own Xmem word once and
// accumulates into the live source accumulator in Execute.
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (unsigned sub=0; sub<2; ++sub) for (unsigned src=0; src<2; ++src) for (unsigned mode=0; mode<4; ++mode)
    for (unsigned shift : {0U,1U,13U,15U}) for (unsigned sxm=0; sxm<2; ++sxm) for (unsigned count : {0U,6U,25U}) {
        // Mode 3 is *AR3+0% around thirteen words; the others walk linear RAM.
        bool const circular=mode==3;
        unsigned const size=13, stride=2, start=circular ? 12 : 0x40;
        u32 const page=(cases.size()%2) ? 0x20000 : 0;
        u16 const opcode=u16(0x9000|(sub<<9)|(src<<8)|((mode<<2|1)<<4)|shift);
        auto initial=initial_case("repeated-Xmem-shifted-ADD-SUB",{u16(0xec00|count),opcode,0x7714,0x55,0xf4e1});
        for (auto &word:initial.program) word.address+=page;
        u16 const st1=sxm ? 0x100 : 0;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST1,st1},
            {r::STATE_BK,circular ? size : 0},{r::STATE_AR0,stride},{r::STATE_AR3,0x1200+start},{r::STATE_A,3},{r::STATE_B,0xff80000007ULL}});
        auto value=[](unsigned at) { return at&1 ? u16(0xff00+at) : u16(0x1111*(at%7)+at); };
        for (unsigned at=0; at<(circular ? size : 0x80); ++at) initial.data.push_back({0x1200+at,value(at)});
        std::array<u64,2> expected{3,0xff80000007ULL};
        unsigned cursor=start;
        std::vector<clocked_data_event> reads;
        for (unsigned at=0; at<=count; ++at) {
            s64 const word=sxm ? s64(s16(value(cursor))) : s64(value(cursor));
            u64 const operand=u64(word*(s64(1)<<shift))&0xffffffffffULL;
            expected[src]=(sub ? expected[src]-operand : expected[src]+operand)&0xffffffffffULL;
            clocked_data_event event{false,0x1200+cursor,value(cursor),0,false};
            if (at) event.cycles_from_previous=1;
            reads.push_back(event);
            cursor=circular ? (cursor+stride)%size : mode==1 ? cursor-1 : mode==2 ? cursor+1 : cursor;
        }
        auto final=observation("Xmem-shifted-live-accumulator-feedback",{{r::STATE_IDLE,1},
            {r::STATE_A,expected[0]},{r::STATE_B,expected[1]},
            {r::STATE_AR3,0x1200+cursor},{r::STATE_AR4,0x55},{r::STATE_ST1,st1}});
        unsigned const finish=count+40;
        clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},6+count/2,finish};
        c.retired_instruction=retired_instruction_check{page+0x601,opcode,count+1,0};
        reads.push_back({true,0x14,0x55,0,false});
        c.data_events=std::move(reads);
        cases.push_back(std::move(c));
    }
    return cases;
}
}
