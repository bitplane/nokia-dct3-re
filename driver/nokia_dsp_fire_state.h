// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_DSP_FIRE_STATE_H
#define MAME_NOKIA_NOKIA_DSP_FIRE_STATE_H
#include <array>
#include <cstdint>

// Partial MAD2 block-coder contract: the observed224-bit FIRE check only.
// No encoder, error correction, cipher operation or busy-time claim.
struct nokia_dsp_fire_state
{
    enum : std::uint8_t { idle, reset_seen, loading, sealed, ready, complete };
    std::array<std::uint16_t,14> input{};
    std::uint16_t command=0,status=0;
    std::uint8_t count=0,phase=idle;
    void reset() { *this={}; }
    void control_w(std::uint16_t value)
    {
        status=0;
        if(value==3) { input.fill(0);count=0;phase=reset_seen; }
        else if(value==5 && phase==reset_seen)phase=loading;
        else if(value==0xfffc && phase==loading && count==input.size())phase=sealed;
        else if(value==2 && phase==sealed)phase=ready;
        else phase=idle;
    }
    void data_w(std::uint16_t value)
    {
        if(phase!=loading || count>=input.size()) { phase=idle;status=0;return; }
        input[count++]=value;
    }
    std::uint64_t syndrome() const
    {
        // GSM05.03: g(x)=(x^23+1)(x^17+x^3+1). The native caller
        // has already complemented the transmitted40 parity bits.
        constexpr std::uint64_t generator=(1ULL<<40)|(1ULL<<26)|(1ULL<<23)|(1ULL<<17)|(1ULL<<3)|1;
        std::uint64_t remainder=0;
        for(unsigned bit=0;bit<224;++bit) {
            remainder=(remainder<<1)|((input[bit/16]>>(bit%16))&1);
            if(remainder&(1ULL<<40))remainder^=generator;
        }
        return remainder;
    }
    void command_w(std::uint16_t value)
    {
        command=value&~std::uint16_t(0xe0); // Result/busy bits cannot be supplied by software.
        status=0;
        if(phase==ready && count==input.size() && (value&~std::uint16_t(0x10))==5) {
            status=syndrome()==0 ? 0x20 : 0;
            phase=complete; // Functional completion; physical latency is unknown.
        } else phase=idle;
    }
    std::uint16_t status_r() const { return command|status; }
};
#endif // MAME_NOKIA_NOKIA_DSP_FIRE_STATE_H
