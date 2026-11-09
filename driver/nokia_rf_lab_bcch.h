// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_RF_LAB_BCCH_H
#define MAME_NOKIA_NOKIA_RF_LAB_BCCH_H
#include "nokia_rf_lab_signal.h"
#include "gsm_tch_f_l1.h"

namespace nokia_rf_lab {
// Valid pages for unassigned TMSIs make each laboratory source block unique.
// TS 44.018 9.1.22 / TS 24.008 10.5.1.4; not a handset timing correction.
inline gsm::tch_f::packed_control_block frame_paging(unsigned frame)
{
	if (frame>=hyperframe || (frame%51!=6 && frame%51!=12 && frame%51!=16))
		throw std::invalid_argument("frame-tagged page outside combined CCCH block starts");
	gsm::tch_f::packed_control_block block{};
	block.fill(0x2b);
	block[0]=0x25;block[1]=6;block[2]=0x21;block[3]=0x10;
	block[4]=5;block[5]=0xf4;
	std::uint32_t const identity=0x80000000U|frame;
	for (unsigned byte=0;byte<4;++byte) block[6+byte]=identity>>(24-8*byte);
	return block;
}

// TS0 BCCH block positions. Payload scheduling belongs to the lab composition.
constexpr bool bcch_active(std::uint64_t tick)
{
	unsigned const frame = (tick / 60'000) % 51;
	return frame >= 2 && frame <= 5 && tick % 60'000 < 148 * 48;
}

inline std::array<std::array<unsigned, 148>, 4> bcch_bits(
		gsm::tch_f::packed_control_block const &octets, unsigned tsc)
{
	if (tsc > 7) throw std::invalid_argument("BCCH training sequence outside0..7");
	gsm::tch_f::control_bits information{};
	for (unsigned bit=0; bit<information.size(); ++bit)
		information[bit]=(octets[bit/8]>>(bit%8))&1;
	auto const bursts=gsm::tch_f::interleave_sacch(gsm::tch_f::encode_control(information));
	std::array<std::array<unsigned, 148>, 4> result{};
	for (unsigned at=0; at<4; ++at)
	{
		auto const packed=gsm::tch_f::pack_normal_burst(bursts[at],gsm::tch_f::training_sequence(tsc));
		for (unsigned bit=0; bit<148; ++bit) result[at][bit]=packed.bits[bit];
	}
	return result;
}
}
#endif // MAME_NOKIA_NOKIA_RF_LAB_BCCH_H
