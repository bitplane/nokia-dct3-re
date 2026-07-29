// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_xcch_l1.h"

namespace gsm::xcch
{

std::array<std::uint32_t, 4> sdcch8_subchannel0_frames(
		std::uint32_t reference_frame, gsm::a5::direction direction)
{
	constexpr std::uint32_t HYPERFRAME = 26 * 51 * 2048;
	const std::uint32_t offset =
			direction == gsm::a5::direction::downlink ? 0 : 15;
	reference_frame %= HYPERFRAME;
	std::uint32_t base = reference_frame - (reference_frame % 51);
	if ((reference_frame % 51) < offset + 3)
		base = (base + HYPERFRAME - 51) % HYPERFRAME;
	return {
		(base + offset) % HYPERFRAME,
		(base + offset + 1) % HYPERFRAME,
		(base + offset + 2) % HYPERFRAME,
		(base + offset + 3) % HYPERFRAME
	};
}

decoded_block transport(
		const block &clear, const std::array<std::uint32_t, 4> &frames,
		const cipher_context &transmitter, const cipher_context &receiver)
{
	auto bursts = gsm::tch_f::interleave_sacch(
			gsm::tch_f::encode_control(gsm::tch_f::unpack_control(clear)));
	const auto training = gsm::tch_f::training_sequence(2);
	for (unsigned index = 0; index < bursts.size(); ++index)
	{
		if (!gsm::a5::apply(bursts[index], transmitter.algorithm,
					transmitter.key,
					gsm::a5::count_from_frame_number(frames[index]),
					transmitter.direction))
			return {};
		const auto air = gsm::tch_f::pack_normal_burst(
				bursts[index], training);
		bursts[index] = gsm::tch_f::unpack_normal_burst(air);
		if (!gsm::a5::apply(bursts[index], receiver.algorithm,
					receiver.key,
					gsm::a5::count_from_frame_number(frames[index]),
					receiver.direction))
			return {};
	}
	const auto decoded = gsm::tch_f::decode_control(
			gsm::tch_f::deinterleave_sacch(bursts));
	return { gsm::tch_f::pack_control(decoded.bits), decoded.good };
}

} // namespace gsm::xcch
