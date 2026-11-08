// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_GSM_NORMAL_P27_H
#define MAME_NOKIA_GSM_NORMAL_P27_H

#include "gsm_tch_f_l1.h"
#include <algorithm>

namespace gsm::normal {

// Observed native serializer lane,not an RF demodulator or FIFO-size claim.
inline std::optional<tch_f::burst_payload> decode_p27(
		std::array<std::uint16_t,15> const &words,unsigned tsc)
{
	if (tsc>7) return std::nullopt;
	auto const training=tch_f::training_sequence(tsc);
	std::array<tch_f::bit,180> bits{};
	for (unsigned word=0;word<15;++word)
		for (unsigned bit=0;bit<12;++bit) bits[word*12+bit]=(words[word]>>bit)&1;
	unsigned start=0,matches=0;
	for (unsigned at=0;at<16;++at)
	{
		bool same=true;
		for (unsigned bit=0;bit<3;++bit) same&=!bits[at+bit] && !bits[at+145+bit];
		for (unsigned bit=0;bit<26;++bit) same&=bits[at+61+bit]==training[bit];
		if (same) { start=at;++matches; }
	}
	if (matches!=1 || bits[start+60]!=1 || bits[start+87]!=1) return std::nullopt;
	tch_f::burst_payload result{};
	std::copy_n(bits.begin()+start+3,57,result.data.begin());
	std::copy_n(bits.begin()+start+88,57,result.data.begin()+57);
	result.hl=result.hu=1;
	return result;
}

inline std::optional<tch_f::packed_control_block> decode_control(
		std::array<tch_f::burst_payload,4> const &bursts)
{
	auto const decoded=tch_f::decode_control(tch_f::deinterleave_sacch(bursts));
	// The laboratory bit endpoint is noiseless: never hide a malformed native
	// transmission behind the channel decoder's error correction.
	if (!decoded.good || decoded.corrected_bits) return std::nullopt;
	// Air-interface octets are LSB first. The older traffic helper's
	// pack_control convention is MSB first and is not this port adapter's ABI.
	tch_f::packed_control_block result{};
	for (unsigned bit=0;bit<184;++bit) result[bit/8]|=decoded.bits[bit]<<(bit%8);
	return result;
}

// TS44.006 8.4.1.4: SAPI0 contention resolution returns the SABM information
// unchanged. Both MS command and BS response use C/R=0 (6.3.2).
inline std::optional<tch_f::packed_control_block> contention_ua(
		tch_f::packed_control_block const &sabm)
{
	unsigned const length=sabm[2]>>2;
	if (sabm[0]!=1 || sabm[1]!=0x3f || (sabm[2]&3)!=1 || !length || length>20)
		return std::nullopt;
	tch_f::packed_control_block result{};
	result.fill(0x2b);
	result[0]=1;result[1]=0x73;result[2]=sabm[2];
	std::copy_n(sabm.begin()+3,length,result.begin()+3);
	return result;
}

inline std::optional<tch_f::packed_control_block> disconnect_ua(
		tch_f::packed_control_block const &disc)
{
	if (disc[0]!=1 || disc[1]!=0x53 || disc[2]!=1) return std::nullopt;
	tch_f::packed_control_block result{};
	result.fill(0x2b);
	result[0]=1;result[1]=0x73;result[2]=1;
	return result;
}
} // namespace gsm::normal
#endif // MAME_NOKIA_GSM_NORMAL_P27_H
