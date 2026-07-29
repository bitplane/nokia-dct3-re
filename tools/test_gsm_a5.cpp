#include "../driver/gsm_a5.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace
{

std::array<std::uint8_t, 114> unpack(
		const std::array<std::uint8_t, 15> &packed)
{
	std::array<std::uint8_t, 114> bits{};
	for (unsigned index = 0; index < bits.size(); ++index)
		bits[index] = (packed[index >> 3] >> (7 - (index & 7))) & 1;
	return bits;
}

void known_answer()
{
	// Published A5/1 reference vector: Kc=1223456789abcdef, COUNT=0x134.
	const gsm::a5::key key = {
		0x12, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
	};
	const std::array<std::uint8_t, 15> downlink = {
		0x53, 0x4e, 0xaa, 0x58, 0x2f, 0xe8, 0x15, 0x1a,
		0xb6, 0xe1, 0x85, 0x5a, 0x72, 0x8c, 0x00
	};
	const std::array<std::uint8_t, 15> uplink = {
		0x24, 0xfd, 0x35, 0xa3, 0x5d, 0x5f, 0xb6, 0x52,
		0x6d, 0x32, 0xf9, 0x06, 0xdf, 0x1a, 0xc0
	};
	assert(gsm::a5::keystream(
			gsm::a5::algorithm::a5_1, key, 0x134,
			gsm::a5::direction::downlink).value() == unpack(downlink));
	assert(gsm::a5::keystream(
			gsm::a5::algorithm::a5_1, key, 0x134,
			gsm::a5::direction::uplink).value() == unpack(uplink));
}

void boundaries()
{
	assert(gsm::a5::count_from_frame_number(0) == 0);
	assert(gsm::a5::count_from_frame_number(26 * 51) == 0x800);
	assert(gsm::a5::count_from_frame_number(26) == 26 * 32);
	assert(gsm::a5::count_from_frame_number(51) == 25);

	const gsm::a5::key key = { 1, 2, 3, 4, 5, 6, 7, 8 };
	struct payload
	{
		std::array<std::uint8_t, 114> data{};
		std::uint8_t hl = 1;
		std::uint8_t hu = 1;
	};
	payload clear;
	gsm::a5::apply(clear, gsm::a5::algorithm::a5_0, key, 42,
			gsm::a5::direction::downlink);
	assert((clear.data == std::array<std::uint8_t, 114>{}));
	assert(clear.hl == 1 && clear.hu == 1);

	payload encrypted;
	gsm::a5::apply(encrypted, gsm::a5::algorithm::a5_1, key, 42,
			gsm::a5::direction::downlink);
	const payload ciphertext = encrypted;
	gsm::a5::apply(encrypted, gsm::a5::algorithm::a5_1, key, 42,
			gsm::a5::direction::downlink);
	assert((encrypted.data == std::array<std::uint8_t, 114>{}));
	assert(encrypted.hl == 1 && encrypted.hu == 1);
	assert(ciphertext.data != encrypted.data);
	payload wrong_key = ciphertext;
	auto other_key = key;
	other_key[0] ^= 0x80;
	gsm::a5::apply(wrong_key, gsm::a5::algorithm::a5_1, other_key, 42,
			gsm::a5::direction::downlink);
	assert(wrong_key.data != encrypted.data);
	payload wrong_count = ciphertext;
	gsm::a5::apply(wrong_count, gsm::a5::algorithm::a5_1, key, 43,
			gsm::a5::direction::downlink);
	assert(wrong_count.data != encrypted.data);
	payload wrong_direction = ciphertext;
	gsm::a5::apply(wrong_direction, gsm::a5::algorithm::a5_1, key, 42,
			gsm::a5::direction::uplink);
	assert(wrong_direction.data != encrypted.data);
	assert(gsm::a5::keystream(gsm::a5::algorithm::a5_1, key, 42,
			gsm::a5::direction::downlink).value() !=
			gsm::a5::keystream(gsm::a5::algorithm::a5_1, key, 43,
			gsm::a5::direction::downlink).value());
	assert(gsm::a5::keystream(gsm::a5::algorithm::a5_1, key, 42,
			gsm::a5::direction::downlink).value() !=
			gsm::a5::keystream(gsm::a5::algorithm::a5_1, key, 42,
			gsm::a5::direction::uplink).value());
	assert(!gsm::a5::keystream(gsm::a5::algorithm::a5_2, key, 42,
			gsm::a5::direction::downlink));

	const std::array<std::uint8_t, 2> complete{0x06, 0x32};
	const std::array<std::uint8_t, 3> extended{0x06, 0x32, 0x00};
	const std::array<std::uint8_t, 2> malformed{0x06, 0x33};
	assert(gsm::a5::cipher_mode_complete(
			0, complete.data(), complete.size()));
	assert(!gsm::a5::cipher_mode_complete(
			0, extended.data(), extended.size()));
	assert(!gsm::a5::cipher_mode_complete(
			0, malformed.data(), malformed.size()));
	assert(!gsm::a5::cipher_mode_complete(
			3, complete.data(), complete.size()));
	assert(gsm::a5::activation_allowed(gsm::a5::algorithm::a5_1,
			true, true, 0, complete.data(), complete.size()));
	assert(!gsm::a5::activation_allowed(gsm::a5::algorithm::a5_1,
			true, false, 0, complete.data(), complete.size()));
	assert(!gsm::a5::activation_allowed(gsm::a5::algorithm::a5_1,
			false, true, 0, complete.data(), complete.size()));
	assert(!gsm::a5::activation_allowed(gsm::a5::algorithm::a5_0,
			true, true, 0, complete.data(), complete.size()));
	assert(!gsm::a5::activation_allowed(gsm::a5::algorithm::a5_2,
			true, true, 0, complete.data(), complete.size()));
}

} // anonymous namespace

int main()
{
	known_answer();
	boundaries();
	std::cout << "GSM A5 tests passed\n";
}
