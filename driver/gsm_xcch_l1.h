// license:BSD-3-Clause
// copyright-holders:Gaz
// GSM xCCH coded-burst transport, independent of handset packet grammar.

#ifndef MAME_NOKIA_GSM_XCCH_L1_H
#define MAME_NOKIA_GSM_XCCH_L1_H

#include "gsm_a5.h"
#include "gsm_tch_f_l1.h"

#include <array>
#include <cstdint>

namespace gsm::xcch
{

using block = gsm::tch_f::packed_control_block;

struct cipher_context
{
	gsm::a5::algorithm algorithm = gsm::a5::algorithm::a5_0;
	gsm::a5::key key{};
	gsm::a5::direction direction = gsm::a5::direction::downlink;
};

struct decoded_block
{
	block data{};
	bool good = false;
};

// TS 45.002 table 4, SDCCH/8 subchannel 0: downlink B(0..3), uplink
// B(15..18) in each 51-frame multiframe. Return the latest complete block no
// later than reference_frame.
std::array<std::uint32_t, 4> sdcch8_subchannel0_frames(
		std::uint32_t reference_frame, gsm::a5::direction direction);

// Complete independent transmit/receive halves. Differing contexts are useful
// for negative wrong-key/direction/algorithm tests.
decoded_block transport(
		const block &clear, const std::array<std::uint32_t, 4> &frames,
		const cipher_context &transmitter, const cipher_context &receiver);

} // namespace gsm::xcch

#endif // MAME_NOKIA_GSM_XCCH_L1_H
