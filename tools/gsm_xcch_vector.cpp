// license:BSD-3-Clause
// copyright-holders:Gaz
// Export the existing channel encoder without any handset/device dependency.
#include "../driver/gsm_tch_f_l1.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc,char **argv)
{
    try {
        unsigned const tsc=argc==2 ? std::stoul(argv[1]) : 2;
        if (argc>2 || tsc>7) throw std::runtime_error("TSC must be0..7");
        gsm::tch_f::packed_control_block bytes{};
        std::cin.read(reinterpret_cast<char *>(bytes.data()),bytes.size());
        if (std::cin.gcount()!=23 || std::cin.peek()!=std::char_traits<char>::eof())
            throw std::runtime_error("stdin must contain exactly23 system-information octets");
        gsm::tch_f::control_bits information{};
        // Radio octets are LSB first; the library's packed-control helper
        // is a separate MSB-first internal representation.
        for (unsigned i=0;i<information.size();++i) information[i]=(bytes[i/8]>>(i%8))&1;
        auto const coded=gsm::tch_f::encode_control(information);
        auto const decoded=gsm::tch_f::decode_control(coded);
        if (!decoded.good || decoded.bits!=information) throw std::runtime_error("encoder self-check failed");
        auto emit=[](auto const &bits){for(auto bit:bits)std::cout<<unsigned(bit);};
        std::cout<<"{\"octets\":\""<<std::hex<<std::setfill('0');
        for (auto byte:bytes) std::cout<<std::setw(2)<<unsigned(byte);
        std::cout<<std::dec<<"\",\"tsc\":"<<tsc<<",\"information\":\"";
        emit(information);std::cout<<"\",\"coded\":\"";emit(coded);
        std::cout<<"\",\"normal_bursts\":[";
        auto const bursts=gsm::tch_f::interleave_sacch(coded);
        for (unsigned i=0;i<4;++i) {
            if(i)std::cout<<',';
            std::cout<<'"';emit(gsm::tch_f::pack_normal_burst(bursts[i],gsm::tch_f::training_sequence(tsc)).bits);std::cout<<'"';
        }
        std::cout<<"]}\n";
    } catch (std::exception const &error) {
        std::cerr<<"GSM xCCH vector rejected: "<<error.what()<<'\n';return 1;
    }
}
