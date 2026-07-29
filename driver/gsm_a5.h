// license:BSD-3-Clause
// copyright-holders:Gaz
// GSM A5 burst ciphering, independent of any handset or channel codec.

#ifndef MAME_NOKIA_GSM_A5_H
#define MAME_NOKIA_GSM_A5_H

#include <array>
#include <cstdint>
#include <optional>

namespace gsm::a5
{

using key = std::array<std::uint8_t, 8>;
using burst_keystream = std::array<std::uint8_t, 114>;

enum class algorithm : std::uint8_t
{
	a5_0 = 0,
	a5_1 = 1,
	a5_2 = 2
};

enum class direction : std::uint8_t
{
	downlink,
	uplink
};

// TS 45.002 frame-number decomposition used by A5: COUNT =
// T1[10:0] || T3[5:0] || T2[4:0].
std::uint32_t count_from_frame_number(std::uint32_t frame_number);

// GSM A5 COUNT is the 22-bit TDMA frame number representation supplied to the
// algorithm.  The function is deliberately stateless: a burst is reproducible
// solely from its key, COUNT and direction.
std::optional<burst_keystream> keystream(
		algorithm selected, const key &kc, std::uint32_t count,
		direction link_direction);

// Cipher Mode Complete is an RR message with no information elements.  Keep
// the exact grammar and activation eligibility outside Nokia product code.
bool cipher_mode_complete(unsigned sapi, const std::uint8_t *information,
		unsigned length);
bool activation_allowed(algorithm selected, bool key_valid,
		bool command_pending, unsigned sapi, const std::uint8_t *information,
		unsigned length);

template <typename BurstPayload>
bool apply(BurstPayload &payload, algorithm selected, const key &kc,
		std::uint32_t count, direction link_direction)
{
	const auto stream = keystream(selected, kc, count, link_direction);
	if (!stream)
		return false;
	for (unsigned index = 0; index < stream->size(); ++index)
		payload.data[index] ^= (*stream)[index];
	return true;
}

} // namespace gsm::a5

#endif // MAME_NOKIA_GSM_A5_H
