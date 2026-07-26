// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_a3a8.h"

#include <algorithm>

namespace gsm::a3a8
{
namespace
{

std::uint8_t multiply(std::uint8_t left, std::uint8_t right)
{
	std::uint8_t result = 0;
	while (right != 0)
	{
		if (right & 1)
			result ^= left;
		left = (left << 1) ^ ((left & 0x80) ? 0x1b : 0);
		right >>= 1;
	}
	return result;
}

std::uint8_t power(std::uint8_t value, unsigned exponent)
{
	std::uint8_t result = 1;
	while (exponent != 0)
	{
		if (exponent & 1)
			result = multiply(result, value);
		value = multiply(value, value);
		exponent >>= 1;
	}
	return result;
}

std::uint8_t rotate_left(std::uint8_t value, unsigned count)
{
	return std::uint8_t((value << count) | (value >> (8 - count)));
}

std::uint8_t substitute(std::uint8_t value)
{
	// AES's S-box is the multiplicative inverse in GF(2^8), followed by its
	// specified affine transform.  Computing it avoids an opaque lookup blob.
	const std::uint8_t inverse = value == 0 ? 0 : power(value, 254);
	return inverse ^ rotate_left(inverse, 1) ^ rotate_left(inverse, 2) ^
			rotate_left(inverse, 3) ^ rotate_left(inverse, 4) ^ 0x63;
}

block expand_round_key(const block &previous, unsigned round)
{
	block result;
	std::array<std::uint8_t, 4> word = {
		substitute(previous[13]), substitute(previous[14]),
		substitute(previous[15]), substitute(previous[12])
	};
	std::uint8_t rcon = 1;
	for (unsigned index = 1; index < round; ++index)
		rcon = multiply(rcon, 2);
	word[0] ^= rcon;
	for (unsigned index = 0; index < 4; ++index)
		result[index] = previous[index] ^ word[index];
	for (unsigned index = 4; index < result.size(); ++index)
		result[index] = previous[index] ^ result[index - 4];
	return result;
}

void add_round_key(block &state, const block &key)
{
	for (unsigned index = 0; index < state.size(); ++index)
		state[index] ^= key[index];
}

void substitute_bytes(block &state)
{
	std::transform(state.begin(), state.end(), state.begin(), substitute);
}

void shift_rows(block &state)
{
	const block previous = state;
	for (unsigned row = 0; row < 4; ++row)
		for (unsigned column = 0; column < 4; ++column)
			state[4 * column + row] =
					previous[4 * ((column + row) & 3) + row];
}

void mix_columns(block &state)
{
	for (unsigned column = 0; column < 4; ++column)
	{
		const unsigned base = 4 * column;
		const std::array<std::uint8_t, 4> value = {
			state[base], state[base + 1], state[base + 2], state[base + 3]
		};
		state[base] = multiply(value[0], 2) ^ multiply(value[1], 3) ^
				value[2] ^ value[3];
		state[base + 1] = value[0] ^ multiply(value[1], 2) ^
				multiply(value[2], 3) ^ value[3];
		state[base + 2] = value[0] ^ value[1] ^
				multiply(value[2], 2) ^ multiply(value[3], 3);
		state[base + 3] = multiply(value[0], 3) ^ value[1] ^
				value[2] ^ multiply(value[3], 2);
	}
}

block encrypt(const block &key, const block &plaintext)
{
	block state = plaintext;
	block round_key = key;
	add_round_key(state, round_key);
	for (unsigned round = 1; round <= 10; ++round)
	{
		substitute_bytes(state);
		shift_rows(state);
		if (round != 10)
			mix_columns(state);
		round_key = expand_round_key(round_key, round);
		add_round_key(state, round_key);
	}
	return state;
}

} // anonymous namespace

result aes_example(const block &ki, const block &rand)
{
	const block temporary = encrypt(ki, rand);
	result output;
	std::copy_n(temporary.begin(), output.sres.size(), output.sres.begin());
	std::copy_n(temporary.begin() + 8, output.kc.size(), output.kc.begin());
	return output;
}

} // namespace gsm::a3a8
