// license:BSD-3-Clause
// copyright-holders:Gaz
// Acquired-ROM laboratory fixture; excluded from generic ISA discovery.
#ifndef MAME_NOKIA_TEST_ROM6_XCCH_KERNEL_H
#define MAME_NOKIA_TEST_ROM6_XCCH_KERNEL_H
#include "c54x_section_common.h"
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    auto const *directory=std::getenv("C54X_ROM6_RECEIVE_DIR");
    if (!directory) throw std::runtime_error("C54X_ROM6_RECEIVE_DIR required");
    auto load=[&](char const *name) {
        std::ifstream file(std::string(directory)+"/"+name,std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
        if (bytes.size()!=131072) throw std::runtime_error("complete LE16 snapshot required");
        std::vector<memory_word> words;
        for (unsigned i=0;i<65536;++i) words.push_back({i,u16(bytes[2*i]|(unsigned(bytes[2*i+1])<<8))});
        return words;
    };
    auto initial=initial_case("rom6-xcch-independent-soft-input",{0xf495});
    initial.program=load("rom6_sch_decode_program.le16");
    initial.data=load("rom6_sch_decode_data.le16");
    initial.data.erase(initial.data.begin(),initial.data.begin()+0x60);
    for (auto &word:initial.data) if(word.address<0xb000)word.value=0;
    bool const fire=std::getenv("C54X_ROM6_FIRE_TRANSACTION")!=nullptr;
    // Optionally observe the request, but always halt before reading its verdict.
    initial.program[fire ? 0x3ade : 0x4d13].value=0xf4e1;
    initial.data.push_back({0xfff,0x4d06});
    initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x924c},{r::STATE_SP,0xfff},
        {r::STATE_A,0},{r::STATE_B,0},{r::STATE_T,1},{r::STATE_TRN,0xffff},
        {r::STATE_ST0,0x0e1f},{r::STATE_ST1,0x4100},{r::STATE_PMST,0x07ac},{r::STATE_BK,0x20},
        {r::STATE_AR0,9},{r::STATE_AR1,0x100},{r::STATE_AR2,0xce},{r::STATE_AR3,0x3147},
        {r::STATE_AR4,0x3210},{r::STATE_AR5,0xdd97},{r::STATE_AR6,0x3222},{r::STATE_AR7,0xd6}});
    std::ifstream file(std::string(directory)+"/xcch_vector.le16",std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    if(bytes.size()!=2*(456+15))throw std::runtime_error("wrong xCCH vector length");
    auto word=[&](unsigned i){return u16(bytes[2*i]|(unsigned(bytes[2*i+1])<<8));};
    for(unsigned i=0;i<456;++i)initial.data.push_back({0x2f80+i,word(i)});
    auto final=observation("native-xcch-before-fire",{{r::STATE_IDLE,1}},{});
    if(!fire)final.expected.push_back({r::STATE_SP,0x1000});
    for(unsigned i=0;i<15;++i)final.expected_data.push_back({0x2f80+i,word(456+i)});
    clocked_register_case result{{{0,std::move(initial)},{200000,std::move(final)}},1000,200000};
    result.io_events=std::vector<clocked_data_event>{};
    if(fire) {
        auto &events=*result.io_events;
        events.push_back({true,0x3c,3,0,false});
        events.push_back({true,0x3c,5,0,false});
        for(unsigned i=0;i<14;++i)events.push_back({true,0x39,word(456+i),0,false});
        events.push_back({true,0x3c,0xfffc,0,false});
        events.push_back({true,0x3c,2,0,false});
        events.push_back({false,0,0,0,false});
        events.push_back({true,0,0x60,0,false});
        events.push_back({false,0x38,0,0,false});
        events.push_back({true,0x38,5,0,false});
    }
    return {std::move(result)};
}
}
#endif // MAME_NOKIA_TEST_ROM6_XCCH_KERNEL_H
