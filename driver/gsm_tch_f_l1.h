// license:BSD-3-Clause
// copyright-holders:Gaz
// GSM full-rate traffic-channel coding, independent of any handset model.

#ifndef MAME_NOKIA_GSM_TCH_F_L1_H
#define MAME_NOKIA_GSM_TCH_F_L1_H

#include <array>
#include <cstdint>

namespace gsm::tch_f
{

using bit = std::uint8_t;
using packed_speech_frame = std::array<std::uint8_t, 33>;
using speech_bits = std::array<bit, 260>;
using control_bits = std::array<bit, 184>;
using coded_block = std::array<bit, 456>;

struct burst_payload
{
	std::array<bit, 114> data{};
	bit hl = 0;
	bit hu = 0;
};

struct normal_burst
{
	// 3 tail, 58 payload, 26 training, 58 payload, 3 tail bits.
	std::array<bit, 148> bits{};
};

struct decoded_speech
{
	packed_speech_frame frame{};
	bool good = false;
	unsigned corrected_bits = 0;
};

struct decoded_control
{
	control_bits bits{};
	bool good = false;
	unsigned corrected_bits = 0;
};

// Conversion between libgsm's conventional 33-octet representation and the
// b(1)..b(260) serial speech bits of 3GPP TS 46.010 table 1.1.
bool unpack_speech(const packed_speech_frame &packed, speech_bits &serial);
packed_speech_frame pack_speech(const speech_bits &serial);

// TS 45.003 3.1: subjective-importance order and TCH/FS channel coding.
speech_bits importance_order(const speech_bits &serial);
speech_bits serial_order(const speech_bits &importance);
coded_block encode_speech(const packed_speech_frame &frame);
decoded_speech decode_speech(const coded_block &coded);

// SACCH and FACCH/F share this 184-bit FIRE-code/convolutional-code block.
coded_block encode_control(const control_bits &information);
decoded_control decode_control(const coded_block &coded);

// TS 45.003 3.1.3. The returned element k belongs in burst B0+k.
std::array<burst_payload, 8> interleave(const coded_block &coded);
coded_block deinterleave(const std::array<burst_payload, 8> &bursts);

// The ciphering seam is deliberately the 114 data bits in burst_payload:
// cipher before packing, decipher after unpacking. Stealing flags are clear.
normal_burst pack_normal_burst(
		const burst_payload &payload, const std::array<bit, 26> &training);
burst_payload unpack_normal_burst(const normal_burst &burst);
std::array<bit, 26> training_sequence(unsigned tsc);

// Merge the old block's odd half and new block's even half for one of the
// four burst periods at which a new diagonal-interleaver block begins.
burst_payload combine_diagonal(
		const std::array<burst_payload, 8> &old_block,
		const std::array<burst_payload, 8> &new_block, unsigned phase);

void mark_facch(std::array<burst_payload, 8> &bursts);
bool indicates_facch(const std::array<burst_payload, 8> &bursts);

} // namespace gsm::tch_f

#endif // MAME_NOKIA_GSM_TCH_F_L1_H
