// license:BSD-3-Clause
// copyright-holders:Gaz
// Explicit acquired-ROM fixture; never part of generic ISA suite discovery.
#ifndef MAME_NOKIA_TEST_ROM6_RECEIVE_KERNEL_H
#define MAME_NOKIA_TEST_ROM6_RECEIVE_KERNEL_H
#include "c54x_section_common.h"
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    auto const *directory=std::getenv("C54X_ROM6_RECEIVE_DIR");
    if (!directory) throw std::runtime_error("C54X_ROM6_RECEIVE_DIR is required for the isolated ROM6 fixture");
    auto load=[&](char const *name) {
        std::ifstream file(std::string(directory)+"/"+name,std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
        if (bytes.size()!=131072) throw std::runtime_error("ROM6 fixture needs a complete LE16 snapshot");
        std::vector<memory_word> words;
        for (unsigned i=0;i<65536;++i) words.push_back({i,u16(bytes[2*i]|(unsigned(bytes[2*i+1])<<8))});
        return words;
    };
    bool const frontend=std::getenv("C54X_ROM6_RECEIVE_FRONTEND")!=nullptr;
    auto initial=initial_case("rom6-sch-uniform-input",{0xf495});
    initial.program=load(frontend ? "rom6_sch_samples_program.le16" : "rom6_sch_decode_program.le16");
    initial.data=load(frontend ? "rom6_sch_samples_data.le16" : "rom6_sch_decode_data.le16");
    initial.data.erase(initial.data.begin(),initial.data.begin()+0x60);
    // The kernel needs acquired DROM tables, not the phone's live RAM state.
    for (auto &word:initial.data) {
        bool const configuration=frontend && ((word.address>=0x524 && word.address<=0x526) ||
            word.address==0x250f || word.address==0x2f02);
        if (word.address<0xb000 && !configuration) word.value=0;
    }
    // The captured zero-correlator frontend supplies +1 soft words, not zeros.
    for (unsigned i=0;i<78;++i) initial.data.push_back({0x2f80+i,1});
    // Stop at the original caller's CRC return, without executing its MDI formatter.
    initial.program[0x4bc5].value=0xf4e1;
    initial.data.push_back({0xfff,0x4bbc});
    initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x9268},{r::STATE_SP,0xfff},
        {r::STATE_A,0x40},{r::STATE_B,0},{r::STATE_T,1},{r::STATE_TRN,0xffff},
        {r::STATE_ST0,0x0e1f},{r::STATE_ST1,0x4100},{r::STATE_PMST,0x07ac},{r::STATE_BK,0x20},
        {r::STATE_AR0,9},{r::STATE_AR1,0x100},{r::STATE_AR2,0xce},{r::STATE_AR3,0x2f82},
        {r::STATE_AR4,0x3210},{r::STATE_AR5,0xdd97},{r::STATE_AR6,0x3222},{r::STATE_AR7,0xd6}});
    unsigned const inputs=frontend ? 368 : 78, source=frontend ? 0x80 : 0x2f80;
    if (frontend) {
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x743a},{r::STATE_A,0xfffffffffaULL},
            {r::STATE_B,0x19a},{r::STATE_T,0},{r::STATE_TRN,0},{r::STATE_ST0,0x161f},
            {r::STATE_ST1,0x4103},{r::STATE_BK,0xfa},{r::STATE_AR0,0},{r::STATE_AR1,0x1221},
            {r::STATE_AR2,0x1f7},{r::STATE_AR3,0x1f5},{r::STATE_AR4,0xffff},{r::STATE_AR5,0},
            {r::STATE_AR6,0x3e9d},{r::STATE_AR7,0x60}});
        initial.data.push_back({0xfff,0x4bb2});
        initial.data.push_back({0x231,0x1f0}); // Declared complete sample-buffer precondition.
        for (unsigned i=0;i<inputs;++i) initial.data.push_back({source+i,0});
    }
    initial.data.push_back({0x2f20,0x55aa});
    auto final=observation("native-uniform-input-failure",{{r::STATE_IDLE,1},{r::STATE_SP,0x1000}},
        {{0x2f80,0},{0x2f81,0},{0x2f82,0},{0x2f20,1}});
    unsigned const finish=frontend ? 200000 : 30000;
    clocked_register_case result{{{0,std::move(initial)},{finish,std::move(final)}},1000,finish};
    result.io_events=std::vector<clocked_data_event>{};
    auto positive=result;
    positive.states[0].observation.id="rom6-sch-independent-soft-input";
    std::ifstream file(std::string(directory)+"/sch_vector.le16",std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    if (bytes.size()!=2*(inputs+4)) throw std::runtime_error("ROM6 SCH vector has the wrong input/output length");
    auto word=[&](unsigned i){return u16(bytes[2*i]|(unsigned(bytes[2*i+1])<<8));};
    for (unsigned i=0;i<inputs;++i) positive.states[0].observation.data.push_back({source+i,word(i)});
    positive.states[1].observation.id="native-positive-sch-crc";
    positive.states[1].observation.expected_data={{0x2f80,word(inputs)},{0x2f81,word(inputs+1)},
        {0x2f82,word(inputs+2)},{0x2f20,word(inputs+3)}};
    return {std::move(result),std::move(positive)};
}
}
#endif // MAME_NOKIA_TEST_ROM6_RECEIVE_KERNEL_H
