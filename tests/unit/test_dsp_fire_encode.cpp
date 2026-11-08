// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_dsp_fire_state.h"
#include <cassert>
#include <iostream>

int main()
{
    std::array<std::uint16_t,12> input{};
    std::array<std::uint16_t,3> expected{};
    auto word=[]() { int low=std::cin.get(),high=std::cin.get();assert(low>=0 && high>=0);return std::uint16_t(low|(high<<8)); };
    for(auto &value:input)value=word();
    for(auto &value:expected)value=word();
    assert(std::cin.get()==std::char_traits<char>::eof());
    auto begin=[](auto &state) { state.control_w(3);state.control_w(5); };
    auto finish=[](auto &state) { state.control_w(0xfffc);state.control_w(2);state.command_w(3);state.control_w(3);state.control_w(7); };
    nokia_dsp_fire_state state;
    begin(state);
    for(unsigned at=0;at<7;++at)state.data_w(input[at]);
    auto const saved=state;
    for(unsigned replay=0;replay<2;++replay) {
        state=saved;
        for(unsigned at=7;at<12;++at)state.data_w(input[at]);
        finish(state);
        assert(state.data_r(0xdead,false)==expected[0]);
        assert(state.data_r(0xdead,false)==expected[0]);
        assert(state.data_r(0xdead)==expected[0]);
        auto const cursor=state;
        for(unsigned restore=0;restore<2;++restore) {
            state=cursor;
            assert(state.data_r(0xdead)==expected[1]);
            assert(state.data_r(0xdead)==expected[2]);
            assert(state.data_r(0xdead)==0xdead);
        }
        state.control_w(2);assert(state.data_r(0xbeef)==0xbeef);
    }
    for(unsigned count=0;count<=14;++count)if(count!=12) {
        begin(state);
        for(unsigned at=0;at<count;++at)state.data_w(input[at%12]);
        finish(state);assert(state.data_r(0xabcd)==0xabcd);
    }
    begin(state);for(auto value:input)state.data_w(value);
    state.reset();finish(state);assert(state.data_r(0xbeef)==0xbeef);
    for(unsigned upper:{0U,0xffU}) {
        begin(state);
        for(unsigned at=0;at<12;++at)state.data_w(at==11 ? (input[at]&255)|(upper<<8) : input[at]);
        finish(state);
        for(auto value:expected)assert(state.data_r(0xdead)==value);
    }
}
