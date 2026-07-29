// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_a5.h"

namespace gsm::a5
{
namespace
{

constexpr std::uint32_t R1_MASK = 0x07ffff;
constexpr std::uint32_t R2_MASK = 0x3fffff;
constexpr std::uint32_t R3_MASK = 0x7fffff;
constexpr std::uint32_t R1_TAPS = 0x072000; // bits 18, 17, 16, 13
constexpr std::uint32_t R2_TAPS = 0x300000; // bits 21, 20
constexpr std::uint32_t R3_TAPS = 0x700080; // bits 22, 21, 20, 7

std::uint8_t parity(std::uint32_t value)
{
	value ^= value >> 16;
	value ^= value >> 8;
	value ^= value >> 4;
	value &= 0x0f;
	return (0x6996 >> value) & 1;
}

void clock(std::uint32_t &reg, std::uint32_t mask, std::uint32_t taps)
{
	reg = ((reg << 1) & mask) | parity(reg & taps);
}

std::uint8_t majority(std::uint8_t a, std::uint8_t b, std::uint8_t c)
{
	return (a & b) | (a & c) | (b & c);
}

struct a5_1_state
{
	std::uint32_t r1 = 0;
	std::uint32_t r2 = 0;
	std::uint32_t r3 = 0;

	void clock_all()
	{
		clock(r1, R1_MASK, R1_TAPS);
		clock(r2, R2_MASK, R2_TAPS);
		clock(r3, R3_MASK, R3_TAPS);
	}

	void clock_irregular()
	{
		const std::uint8_t vote =
				majority((r1 >> 8) & 1, (r2 >> 10) & 1, (r3 >> 10) & 1);
		if (((r1 >> 8) & 1) == vote)
			clock(r1, R1_MASK, R1_TAPS);
		if (((r2 >> 10) & 1) == vote)
			clock(r2, R2_MASK, R2_TAPS);
		if (((r3 >> 10) & 1) == vote)
			clock(r3, R3_MASK, R3_TAPS);
	}

	std::uint8_t output() const
	{
		return ((r1 >> 18) ^ (r2 >> 21) ^ (r3 >> 22)) & 1;
	}
};

std::array<std::uint8_t, 228> a5_1(const key &kc, std::uint32_t count)
{
	a5_1_state state;
	for (unsigned bit = 0; bit < 64; ++bit)
	{
		state.clock_all();
		const std::uint8_t input = (kc[bit >> 3] >> (bit & 7)) & 1;
		state.r1 ^= input;
		state.r2 ^= input;
		state.r3 ^= input;
	}
	for (unsigned bit = 0; bit < 22; ++bit)
	{
		state.clock_all();
		const std::uint8_t input = (count >> bit) & 1;
		state.r1 ^= input;
		state.r2 ^= input;
		state.r3 ^= input;
	}
	for (unsigned cycle = 0; cycle < 100; ++cycle)
		state.clock_irregular();

	std::array<std::uint8_t, 228> result{};
	for (auto &bit : result)
	{
		state.clock_irregular();
		bit = state.output();
	}
	return result;
}

} // anonymous namespace

std::uint32_t count_from_frame_number(std::uint32_t frame_number)
{
	constexpr std::uint32_t hyperframe = 26 * 51 * 2048;
	frame_number %= hyperframe;
	const std::uint32_t t1 = frame_number / (26 * 51);
	const std::uint32_t t2 = frame_number % 26;
	const std::uint32_t t3 = frame_number % 51;
	return (t1 << 11) | (t3 << 5) | t2;
}

std::optional<burst_keystream> keystream(
		algorithm selected, const key &kc, std::uint32_t count,
		direction link_direction)
{
	burst_keystream result{};
	if (selected == algorithm::a5_0)
		return result;
	if (selected != algorithm::a5_1)
		return std::nullopt;

	const auto both = a5_1(kc, count & 0x3fffff);
	const unsigned offset =
			link_direction == direction::downlink ? 0 : result.size();
	for (unsigned index = 0; index < result.size(); ++index)
		result[index] = both[offset + index];
	return result;
}

bool cipher_mode_complete(unsigned sapi, const std::uint8_t *information,
		unsigned length)
{
	return sapi == 0 && length == 2 && information &&
			information[0] == 0x06 && information[1] == 0x32;
}

bool activation_allowed(algorithm selected, bool key_valid,
		bool command_pending, unsigned sapi, const std::uint8_t *information,
		unsigned length)
{
	return selected == algorithm::a5_1 && key_valid && command_pending &&
			cipher_mode_complete(sapi, information, length);
}

} // namespace gsm::a5
