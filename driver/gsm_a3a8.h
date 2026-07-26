// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_GSM_A3A8_H
#define MAME_NOKIA_GSM_A3A8_H

#include <array>
#include <cstdint>

namespace gsm::a3a8
{

using block = std::array<std::uint8_t, 16>;

struct result
{
	std::array<std::uint8_t, 4> sres;
	std::array<std::uint8_t, 8> kc;
};

// 3GPP TS 55.205 section 5's deliberately simple example: encrypt RAND with
// AES-128 under Ki, take TEMP[0..31] as SRES and TEMP[64..127] as Kc.
result aes_example(const block &ki, const block &rand);

} // namespace gsm::a3a8

#endif // MAME_NOKIA_GSM_A3A8_H
