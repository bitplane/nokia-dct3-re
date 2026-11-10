// license:BSD-3-Clause
// copyright-holders:Gaz
// Exact noiseless access coding (TS 45.003 4.6), not an RF demodulator.
#ifndef MAME_NOKIA_GSM_RACH_H
#define MAME_NOKIA_GSM_RACH_H

#include <array>
#include <cstdint>
#include <optional>

namespace gsm::rach
{
inline std::array<unsigned, 36> encode(std::uint8_t ra, unsigned bsic)
{
	std::array<unsigned, 18> input{};
	unsigned polynomial = 0;
	for (unsigned bit = 0; bit < 8; ++bit)
	{
		input[bit] = (ra >> bit) & 1;
		polynomial = (polynomial << 1) | input[bit];
	}
	unsigned remainder = polynomial << 6;
	for (int bit = 13; bit >= 6; --bit)
		if (remainder & (1U << bit)) remainder ^= 0x6fU << (bit - 6);
	unsigned const parity = remainder ^ 63 ^ (bsic & 63);
	for (unsigned bit = 0; bit < 6; ++bit) input[8 + bit] = (parity >> (5 - bit)) & 1;
	std::array<unsigned, 36> coded{};
	for (unsigned bit = 0; bit < input.size(); ++bit)
	{
		auto past = [&](unsigned distance) { return bit >= distance ? input[bit - distance] : 0U; };
		coded[2 * bit] = input[bit] ^ past(3) ^ past(4);
		coded[2 * bit + 1] = input[bit] ^ past(1) ^ past(3) ^ past(4);
	}
	return coded;
}

// Observed serializer framing: fifteen words, low twelve bits LSB first.
// This finite adapter is not a claim about physical FIFO capacity.
inline std::optional<std::uint8_t> decode_p27(std::array<std::uint16_t, 15> const &words, unsigned bsic)
{
	if (bsic >= 64) return std::nullopt;
	constexpr char header[] = "0011101001001011011111111001100110101010001111000";
	std::array<unsigned, 180> bits{};
	for (unsigned word = 0; word < words.size(); ++word)
		for (unsigned bit = 0; bit < 12; ++bit) bits[word * 12 + bit] = (words[word] >> bit) & 1;
	unsigned start = 0, matches = 0;
	for (unsigned at = 0; at < 16; ++at)
	{
		bool same = true;
		for (unsigned bit = 0; bit < 49; ++bit) same &= bits[at + bit] == unsigned(header[bit] - '0');
		if (same) { start = at; ++matches; }
	}
	if (matches != 1 || bits[start + 85] || bits[start + 86] || bits[start + 87]) return std::nullopt;
	for (unsigned ra = 0; ra < 256; ++ra)
	{
		auto const coded = encode(ra, bsic);
		bool same = true;
		for (unsigned bit = 0; bit < coded.size(); ++bit) same &= bits[start + 49 + bit] == coded[bit];
		if (same) return std::uint8_t(ra);
	}
	return std::nullopt;
}
} // namespace gsm::rach
#endif // MAME_NOKIA_GSM_RACH_H
