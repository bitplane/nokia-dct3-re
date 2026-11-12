// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// RPT ADD/SUB/LD Smem,SHIFT,src,dst (6fxx + extension, ROM4 8206:
// RPT #3; ADD *AR7+0,14,A). The extension is fetched once; each issue reads
// its own Smem word and Execute consumes the live source accumulator.
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    static constexpr std::array<u16,4> modes{0x83,0x93,0xb3,0xab}; // *AR3, *AR3+, *AR3+0, *AR3-0
    static constexpr std::array<int,5> shifts{14,0,-8,15,-16};
    for (unsigned op=0; op<3; ++op) for (unsigned selector=0; selector<4; ++selector) for (unsigned mode=0; mode<4; ++mode)
    for (unsigned shift_index=0; shift_index<shifts.size(); ++shift_index) for (unsigned sxm=0; sxm<2; ++sxm) for (unsigned count : {0U,3U,17U}) {
        unsigned const src=selector>>1, dst=selector&1;
        int const shift=shifts[shift_index];
        u32 const page=(cases.size()%2) ? 0x20000 : 0;
        u16 const opcode=u16(0x6f00|modes[mode]);
        // LD has no source field; its selector pairs only vary the destination.
        u16 const extension=u16(0x0c00|(op<<5)|(op<2 ? src<<9 : 0)|(dst<<8)|(shift&0x1f));
        auto initial=initial_case("repeated-extended-shift-ADD-SUB-LD",{u16(0xec00|count),opcode,extension,0x7714,0x55,0xf4e1});
        for (auto &word:initial.program) word.address+=page;
        u16 const st1=sxm ? 0x100 : 0;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST1,st1},{r::STATE_BK,0},
            {r::STATE_AR0,8},{r::STATE_AR3,0x1300},{r::STATE_A,3},{r::STATE_B,0xff80000007ULL}});
        auto value=[](unsigned at) { return at&1 ? u16(0xff00+at) : u16(0x1111*(at%7)+at); };
        for (unsigned at=0; at<0x200; ++at) initial.data.push_back({0x1200+at,value(at)});
        std::array<u64,2> expected{3,0xff80000007ULL};
        unsigned cursor=0x100;
        std::vector<clocked_data_event> reads;
        for (unsigned at=0; at<=count; ++at) {
            s64 const word=sxm ? s64(s16(value(cursor))) : s64(value(cursor));
            s64 const shifted=shift<0 ? word>>-shift : word*(s64(1)<<shift);
            u64 const operand=u64(shifted)&0xffffffffffULL;
            expected[dst]=(op==2 ? operand : op ? expected[src]-operand : expected[src]+operand)&0xffffffffffULL;
            clocked_data_event event{false,0x1200+cursor,value(cursor),0,false};
            if (at) event.cycles_from_previous=1;
            reads.push_back(event);
            cursor=mode==1 ? cursor+1 : mode==2 ? cursor+8 : mode==3 ? cursor-8 : cursor;
        }
        auto final=observation("extended-shift-live-accumulator-feedback",{{r::STATE_IDLE,1},
            {r::STATE_A,expected[0]},{r::STATE_B,expected[1]},
            {r::STATE_AR3,0x1200+cursor},{r::STATE_AR4,0x55},{r::STATE_ST1,st1}});
        unsigned const finish=count+40;
        clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},6+count/2,finish};
        c.retired_instruction=retired_instruction_check{page+0x601,opcode,count+1,0};
        c.retained_program_reads={{page+0x602,extension}};
        reads.push_back({true,0x14,0x55,0,false});
        c.data_events=std::move(reads);
        cases.push_back(std::move(c));
    }
    return cases;
}
}
