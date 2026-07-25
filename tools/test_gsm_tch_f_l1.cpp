#include "../driver/gsm_tch_f_l1.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

using namespace gsm::tch_f;

namespace
{

packed_speech_frame patterned_frame()
{
	speech_bits bits{};
	for (unsigned k = 0; k < bits.size(); ++k)
		bits[k] = ((k * 73 + 19) % 101) < 49;
	return pack_speech(bits);
}

void test_serial_packing()
{
	speech_bits bits{};
	for (unsigned k = 0; k < bits.size(); ++k)
		bits[k] = (k % 7) == 1 || (k % 11) == 5;
	const auto packed = pack_speech(bits);
	assert((packed[0] >> 4) == 0x0d);
	speech_bits recovered{};
	assert(unpack_speech(packed, recovered));
	assert(recovered == bits);
	auto invalid = packed;
	invalid[0] = 0;
	assert(!unpack_speech(invalid, recovered));
}

void test_importance_permutation()
{
	speech_bits original{};
	for (unsigned selected = 0; selected < original.size(); ++selected)
	{
		original.fill(0);
		original[selected] = 1;
		assert(serial_order(importance_order(original)) == original);
	}
}

void test_clean_and_degraded_round_trip()
{
	speech_bits zero_bits{};
	const auto zero_coded = encode_speech(pack_speech(zero_bits));
	for (unsigned k = 0; k < zero_coded.size(); ++k)
	{
		const bool expected =
				k == 182 || k == 183 || k == 184 || k == 186 ||
				k == 188 || k == 194 || k == 195;
		assert(bool(zero_coded[k]) == expected);
	}

	const auto frame = patterned_frame();
	const auto coded = encode_speech(frame);
	auto decoded = decode_speech(coded);
	assert(decoded.good);
	assert(decoded.corrected_bits == 0);
	assert(decoded.frame == frame);

	// The K=5, rate-1/2 code corrects representative isolated hard errors.
	auto degraded = coded;
	for (unsigned position : {0U, 37U, 128U, 233U, 377U})
		degraded[position] ^= 1;
	decoded = decode_speech(degraded);
	assert(decoded.good);
	assert(decoded.frame == frame);
	assert(decoded.corrected_bits == 5);

	// Class-2 bits are unprotected and therefore faithfully pass corruption.
	degraded = coded;
	degraded[455] ^= 1;
	decoded = decode_speech(degraded);
	assert(decoded.good);
	assert(decoded.frame != frame);
}

void test_interleaving_and_bursts()
{
	coded_block coded{};
	for (unsigned k = 0; k < coded.size(); ++k)
		coded[k] = ((k * 29 + 7) % 43) < 20;
	auto payloads = interleave(coded);
	assert(deinterleave(payloads) == coded);

	std::array<bit, 26> training{};
	for (unsigned k = 0; k < training.size(); ++k)
		training[k] = (k * 5 + 1) & 1;
	for (const auto &payload : payloads)
	{
		const auto air = pack_normal_burst(payload, training);
		assert(unpack_normal_burst(air).data == payload.data);
		assert(std::all_of(air.bits.begin(), air.bits.begin() + 3,
				[](bit value) { return value == 0; }));
		assert(std::all_of(air.bits.end() - 3, air.bits.end(),
				[](bit value) { return value == 0; }));
		assert(std::equal(training.begin(), training.end(), air.bits.begin() + 61));
	}
	const auto tsc2 = training_sequence(2);
	assert(tsc2[0] == 0 && tsc2[1] == 1 && tsc2[25] == 0);

	coded_block next_coded{};
	for (unsigned k = 0; k < next_coded.size(); ++k)
		next_coded[k] = !coded[k];
	const auto next = interleave(next_coded);
	for (unsigned phase = 0; phase < 4; ++phase)
	{
		const auto combined = combine_diagonal(payloads, next, phase);
		for (unsigned j = 0; j < 114; ++j)
		{
			const bit expected =
					payloads[4 + phase].data[j] | next[phase].data[j];
			assert(combined.data[j] == expected);
		}
	}

	assert(!indicates_facch(payloads));
	mark_facch(payloads);
	assert(indicates_facch(payloads));
	for (unsigned k = 0; k < 4; ++k)
		assert(payloads[k].hu == 1);
	for (unsigned k = 4; k < 8; ++k)
		assert(payloads[k].hl == 1);
}

void test_facch_control_coding()
{
	control_bits information{};
	for (unsigned k = 0; k < information.size(); ++k)
		information[k] = ((k * 31 + 9) % 53) < 25;
	auto coded = encode_control(information);
	auto decoded = decode_control(coded);
	assert(decoded.good);
	assert(decoded.bits == information);
	assert(decoded.corrected_bits == 0);

	for (unsigned position : {4U, 89U, 214U, 331U, 450U})
		coded[position] ^= 1;
	decoded = decode_control(coded);
	assert(decoded.good);
	assert(decoded.bits == information);
	assert(decoded.corrected_bits == 5);
}

} // anonymous namespace

int main()
{
	test_serial_packing();
	test_importance_permutation();
	test_clean_and_degraded_round_trip();
	test_interleaving_and_bursts();
	test_facch_control_coding();
	std::cout << "GSM TCH/FS Layer 1 tests passed\n";
	return 0;
}
