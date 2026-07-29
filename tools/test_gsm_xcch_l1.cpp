#include "../driver/gsm_xcch_l1.h"

#include <cassert>
#include <iostream>

int main()
{
	const gsm::xcch::block frame = {
		0x03, 0x00, 0x0d, 0x06, 0x35, 0x01, 0x2b, 0x2b,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b
	};
	const gsm::a5::key key = {
		0x12, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
	};
	const gsm::xcch::cipher_context downlink{
		gsm::a5::algorithm::a5_1, key, gsm::a5::direction::downlink
	};
	const auto frames = gsm::xcch::sdcch8_subchannel0_frames(
			104, gsm::a5::direction::downlink);
	// Reference 104 has not completed B(0..3) in the 102-based multiframe;
	// the latest complete block is B(0..3) in the preceding multiframe.
	assert((frames == std::array<std::uint32_t, 4>{ 51, 52, 53, 54 }));
	const auto later = gsm::xcch::sdcch8_subchannel0_frames(
			105, gsm::a5::direction::downlink);
	assert((later == std::array<std::uint32_t, 4>{ 102, 103, 104, 105 }));
	const auto uplink_frames = gsm::xcch::sdcch8_subchannel0_frames(
			120, gsm::a5::direction::uplink);
	assert((uplink_frames ==
			std::array<std::uint32_t, 4>{ 117, 118, 119, 120 }));

	const auto decoded = gsm::xcch::transport(
			frame, later, downlink, downlink);
	assert(decoded.good && decoded.data == frame);

	auto wrong_key = downlink;
	wrong_key.key[3] ^= 0x40;
	assert(!gsm::xcch::transport(
			frame, later, downlink, wrong_key).good);
	auto wrong_direction = downlink;
	wrong_direction.direction = gsm::a5::direction::uplink;
	assert(!gsm::xcch::transport(
			frame, later, downlink, wrong_direction).good);
	auto unsupported = downlink;
	unsupported.algorithm = gsm::a5::algorithm::a5_2;
	assert(!gsm::xcch::transport(
			frame, later, unsupported, unsupported).good);

	const gsm::xcch::cipher_context clear{};
	const auto unciphered = gsm::xcch::transport(frame, later, clear, clear);
	assert(unciphered.good && unciphered.data == frame);
	std::cout << "GSM xCCH Layer 1 tests passed\n";
}
