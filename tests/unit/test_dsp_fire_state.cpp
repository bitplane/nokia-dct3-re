// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_dsp_fire_state.h"
#include <cassert>
#include <iostream>

int main()
{
    std::array<std::uint16_t,14> words{};
    for(auto &word:words) {
        int const low=std::cin.get(),high=std::cin.get();
        assert(low>=0 && high>=0);word=low|(high<<8);
    }
    assert(std::cin.get()==std::char_traits<char>::eof());
    auto begin=[](auto &state){state.control_w(3);state.control_w(5);};
    auto finish=[](auto &state){state.control_w(0xfffc);state.control_w(2);state.command_w(5);};
    nokia_dsp_fire_state state;
    begin(state);
    for(unsigned i=0;i<7;++i)state.data_w(words[i]);
    auto const saved=state;
    for(unsigned replay=0;replay<2;++replay) {
        state=saved;
        for(unsigned i=7;i<14;++i)state.data_w(words[i]);
        assert((state.status_r()&0xe0)==0);
        finish(state);assert((state.status_r()&0xe0)==0x20);
    }
    for(unsigned bit=0;bit<224;++bit) {
        begin(state);
        for(unsigned i=0;i<14;++i)state.data_w(words[i]^(i==bit/16 ? 1U<<(bit%16) : 0));
        finish(state);assert((state.status_r()&0xe0)==0);
    }
    for(unsigned length=0;length<14;++length) {
        begin(state);
        for(unsigned i=0;i<length;++i)state.data_w(words[i]);
        finish(state);assert((state.status_r()&0xe0)==0);
    }
    state=saved;state.reset();finish(state);assert(state.status_r()==5);
    begin(state);for(auto word:words)state.data_w(word);
    state.data_w(0);finish(state);assert((state.status_r()&0xe0)==0);
    state.command_w(0x20);assert((state.status_r()&0xe0)==0);
    begin(state);for(auto word:words)state.data_w(word);
    state.control_w(0xfffc);state.control_w(2);state.command_w(9);
    assert((state.status_r()&0xe0)==0);
    state.command_w(0x10);begin(state);assert((state.status_r()&0x10)==0x10);
    for(auto word:words)state.data_w(word);
    state.control_w(0xfffc);state.control_w(2);state.command_w(0x15);
    assert(state.status_r()==0x35);
    std::cout<<"FIRE check: valid,224 single-bit corruptions,partial,reset,overflow,unsupported and state-copy replay PASS\n";
}
