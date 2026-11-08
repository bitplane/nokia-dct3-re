// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_DSP_FIRE_STATE_H
#define MAME_NOKIA_NOKIA_DSP_FIRE_STATE_H
#include <array>
#include <cstdint>

// Partial MAD2 block-coder contract: observed184-bit FIRE encode/224-bit check.
// No error correction,cipher operation or physical busy-time claim.
struct nokia_dsp_fire_state
{
    enum : std::uint8_t { idle, reset_seen, loading, sealed, ready, complete, readback };
    std::array<std::uint16_t,14> input{};
    std::uint16_t command=0,status=0;
    std::uint8_t count=0,phase=idle;
    std::array<std::uint16_t,3> output{};
    std::uint8_t output_cursor=0;
    bool output_valid=false;
    void reset() { *this={}; }
    void control_w(std::uint16_t value)
    {
        status=0;
        if(value==3) { input.fill(0);count=0;phase=reset_seen; }
        else if(value==5 && phase==reset_seen) { phase=loading;output_valid=false; }
        else if(value==7 && phase==reset_seen && output_valid) { phase=readback;output_cursor=0; }
        else if(value==0xfffc && phase==loading && (count==12 || count==input.size()))phase=sealed;
        else if(value==2 && phase==sealed)phase=ready;
        else if(value==2 && phase==readback) { phase=idle;output_valid=false; }
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
        output_valid=false;
        if(phase==ready && count==input.size() && (value&~std::uint16_t(0x10))==5) {
            status=syndrome()==0 ? 0x20 : 0;
            phase=complete; // Functional completion; physical latency is unknown.
        } else if(phase==ready && count==12 && (value&~std::uint16_t(0x10))==3) {
            constexpr std::uint64_t generator=(1ULL<<40)|(1ULL<<26)|(1ULL<<23)|(1ULL<<17)|(1ULL<<3)|1;
            std::uint64_t remainder=0;
            for(unsigned bit=0;bit<224;++bit) {
                remainder=(remainder<<1)|(bit<184 ? (input[bit/16]>>(bit%16))&1 : 0);
                if(remainder&(1ULL<<40))remainder^=generator;
            }
            output.fill(0);
            // Resident807f complements these words and packs the first32
            // bits low-first,then the final8 from the third word's high byte.
            // That third word's unused low byte is not physically qualified.
            for(unsigned bit=0;bit<40;++bit)
                output[bit/16]|=((remainder>>(39-bit))&1)<<(bit<32 ? bit%16 : bit%16+8);
            output_valid=true;phase=complete;
        } else phase=idle;
    }
    std::uint16_t data_r(std::uint16_t opaque,bool advance=true)
    {
        if(phase!=readback || !output_valid || output_cursor>=output.size())return opaque;
        auto const value=output[output_cursor];
        if(advance)++output_cursor;
        return value;
    }
    std::uint16_t status_r() const { return command|status; }
};
#endif // MAME_NOKIA_NOKIA_DSP_FIRE_STATE_H
