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
    if(std::getenv("C54X_ROM6_XCCH_BURSTS")) {
        bool const frontend=std::getenv("C54X_ROM6_XCCH_FRONTEND")!=nullptr;
        bool const raw=std::getenv("C54X_ROM6_XCCH_RAW_ADC")!=nullptr;
        bool const erase_all=std::getenv("C54X_ROM6_XCCH_ERASE_ALL")!=nullptr;
        bool const erasures=erase_all || std::getenv("C54X_ROM6_XCCH_ERASE_LAST_TWO")!=nullptr;
        if(erasures && frontend)throw std::runtime_error("soft erasures are not a sample frontend fixture");
        std::ifstream burst_file(std::string(directory)+"/xcch_bursts.le16",std::ios::binary);
        std::vector<unsigned char> burst_bytes((std::istreambuf_iterator<char>(burst_file)),{});
        if(burst_bytes.size()!=912)throw std::runtime_error("four114-word bursts required");
        auto nibble=[&](unsigned i){return u16(burst_bytes[2*i]|(unsigned(burst_bytes[2*i+1])<<8));};
        if(erasures)for(unsigned i=erase_all ? 0 : 228;i<456;++i)
            if(nibble(i))throw std::runtime_error("erased soft buffers must contain no information");
        std::vector<unsigned char> samples;
        if(frontend) {
            std::ifstream sample_file(std::string(directory)+(raw ? "/xcch_raw.le16" : "/xcch_samples.le16"),std::ios::binary);
            samples.assign(std::istreambuf_iterator<char>(sample_file),{});
            if(samples.size()!=4*(raw ? 608 : 304)*2)throw std::runtime_error("wrong normal-burst input length");
        }
        // Laboratory caller supplies four completed normal-burst soft buffers.
        // Native5905/58e0 own all interleaver writes; no decoded block is supplied.
        unsigned pc=0x80;
        for(unsigned burst=0;burst<4;++burst) {
            if(frontend) {
                if(raw) {
                    for(u16 opcode:{0x76f8,0x231,0x2667})initial.program[pc++].value=opcode;
                    for(unsigned block=0;block<19;++block)initial.program[pc++].value=0xf7d0;
                } else {
                    for(unsigned i=0;i<304;++i) {
                        unsigned at=304*burst+i;
                        initial.data.push_back({0x6000+at,u16(samples[2*at]|(unsigned(samples[2*at+1])<<8))});
                    }
                    for(u16 opcode:std::vector<u16>{0x7712,u16(0x6000+304*burst),0x7713,0x2667,
                        0xecff,0xe589,0xec2f,0xe589})initial.program[pc++].value=opcode;
                }
                for(u16 opcode:std::vector<u16>{0x7711,0,0x7715,0,
                    0x76f8,0x2f02,0x50,0xf074,0x743a})initial.program[pc++].value=opcode;
            } else {
                for(unsigned i=0;i<114;++i)initial.data.push_back({0x6000+114*burst+i,nibble(114*burst+i)});
                for(u16 opcode:std::vector<u16>{0x7712,u16(0x6000+114*burst),0x7713,0x160,
                    0xec71,0xe589})initial.program[pc++].value=opcode;
            }
            initial.program[pc++].value=0xf074;
            initial.program[pc++].value=0x5905;
        }
        // Continue through the real caller's unpack/weight/Viterbi sequence.
        // 4ceb supplies this unpack count before the source selection at4cf7.
        initial.program[pc++].value=0x7711;
        initial.program[pc++].value=0x71;
        initial.program[pc++].value=0xf073;
        initial.program[pc].value=0x4cf7;
        initial.initial.push_back({STATE_GENPC,0x80});
        initial.initial.push_back({r::STATE_SP,0x1000});
        initial.data.push_back({0x250f,0x30});
        initial.data.push_back({0x2510,0x50});
        if(frontend) {
            for(auto const &setting:std::vector<memory_word>{{0x524,0xdf85},{0x525,0x6d1b},{0x526,0xdf85},
                    {0x231,0x2797},{0x250b,2},{0x2616,2}})initial.data.push_back(setting);
        }
        if(raw) {
            initial.data.insert(initial.data.end(),{{0x22a,0},{0x22c,2},{0x22f,0xffff}});
            unsigned index=0;
            for(u16 coefficient:{0x0387,0x11a3,0x2ad6,0x4000,0x4000,0x2ad6,0x11a3,0x0387})
                initial.data.push_back({0x3ebd+index++,coefficient});
        }
        // Erasures remove both information and confidence, not decoded output.
        initial.data.push_back({0x264b,0x5000});
        for(unsigned i=0;i<4;++i)initial.data.push_back({0x5000+i,u16(erase_all || (erasures && i>=2) ? 0 : 1)});
        auto final=observation("native-four-burst-deinterleaving",{{STATE_GENPC,0x4d14},{r::STATE_IDLE,1},{r::STATE_SP,0x1000},{r::STATE_ILLEGAL,0}},{{0x261f,0}});
        for(unsigned i=0;i<114;++i) {
            u16 packed=0;
            for(unsigned j=0;j<4;++j)packed|=(erase_all || (erasures && j>=2) ? 0 : word(4*i+j)==32 ? 3 : 12)<<(4*j);
            if(!frontend)final.expected_data.push_back({0x2c2+i,packed});
        }
        for(unsigned i=0;i<15;++i)final.expected_data.push_back({0x2f80+i,word(456+i)});
        unsigned const finish=raw ? 600000 : frontend ? 400000 : 200000;
        clocked_register_case result{{{0,std::move(initial)},{finish,std::move(final)}},1000,finish};
        result.io_events=std::vector<clocked_data_event>{};
        if(raw) {
            result.scripted_io_reads=true;
            for(unsigned block=0;block<76;++block) {
                auto &events=*result.io_events;
                events.push_back({false,0x21,u16(block ? 0x400 : 0),0,false});
                events.push_back({true,0x21,0xc00,0,false});
                for(unsigned i=0;i<32;++i) {
                    unsigned at=32*block+i;
                    events.push_back({false,0x27,u16(samples[2*at]|(unsigned(samples[2*at+1])<<8)),0,false});
                }
                events.push_back({false,0x21,0xc00,0,false});
                events.push_back({true,0x21,0x400,0,false});
            }
        }
        return {std::move(result)};
    }
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
