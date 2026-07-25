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
	auto inverted = payloads[0];
	invert_data_bits(inverted);
	for (unsigned k = 0; k < inverted.data.size(); ++k)
		assert(inverted.data[k] != payloads[0].data[k]);
	assert(inverted.hl == payloads[0].hl && inverted.hu == payloads[0].hu);

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
	assert(unpack_control(pack_control(information)) == information);

	const auto sacch = interleave_sacch(encode_control(information));
	for (const auto &burst : sacch)
		assert(burst.hl == 1 && burst.hu == 1);
	decoded = decode_control(deinterleave_sacch(sacch));
	assert(decoded.good);
	assert(decoded.bits == information);
}

void test_stateful_diagonal_stream()
{
	const auto speech_frame = patterned_frame();
	control_bits facch{};
	for (unsigned k = 0; k < facch.size(); ++k)
		facch[k] = (k % 9) == 2;

	diagonal_transmitter transmitter;
	diagonal_receiver receiver;
	assert(transmitter.enqueue(
			{encode_speech(speech_frame), traffic_block_kind::speech}));
	assert(transmitter.substitute_facch(encode_control(facch)));
	assert(transmitter.enqueue(
			{encode_speech(speech_frame), traffic_block_kind::speech}));

	std::array<received_traffic_block, 4> received{};
	unsigned count = 0;
	// Four queued/empty block starts flush the three useful diagonal blocks.
	for (unsigned burst = 0; burst < 16; ++burst)
	{
		const auto decoded = receiver.receive(transmitter.next_burst());
		if (decoded)
			received[count++] = *decoded;
	}
	assert(count == 3);
	assert(received[0].kind == traffic_block_kind::facch);
	assert(received[0].control.good);
	assert(received[0].control.bits == facch);
	assert(received[1].kind == traffic_block_kind::speech);
	assert(received[1].speech.good);
	assert(received[1].speech.frame == speech_frame);
	assert(received[2].kind == traffic_block_kind::speech);
	assert(!received[2].speech.good);

	transmitter.reset();
	receiver.reset();
	// No queued traffic produces an erased/bad speech block, not synthetic
	// codec payload or an accidentally valid FACCH.
	std::optional<received_traffic_block> erased;
	for (unsigned burst = 0; burst < 8; ++burst)
	{
		auto result = receiver.receive(transmitter.next_burst());
		if (result)
			erased = result;
	}
	assert(erased);
	assert(erased->kind == traffic_block_kind::speech);
	assert(!erased->speech.good);
}

void test_diagonal_snapshot_continuation()
{
	const auto speech_frame = patterned_frame();
	control_bits facch{};
	for (unsigned k = 0; k < facch.size(); ++k)
		facch[k] = ((k * 23 + 5) % 41) < 19;

	diagonal_transmitter original_transmitter;
	diagonal_receiver original_receiver;
	assert(original_transmitter.enqueue(
			{encode_speech(speech_frame), traffic_block_kind::speech}));
	assert(original_transmitter.substitute_facch(encode_control(facch)));
	assert(original_transmitter.enqueue(
			{encode_speech(speech_frame), traffic_block_kind::speech}));

	// Save at a deliberately non-block-aligned point, while old and new
	// diagonal halves are both live.
	for (unsigned burst = 0; burst < 5; ++burst)
		original_receiver.receive(original_transmitter.next_burst());

	diagonal_transmitter restored_transmitter;
	diagonal_receiver restored_receiver;
	restored_transmitter.restore(original_transmitter.snapshot());
	restored_receiver.restore(original_receiver.snapshot());

	unsigned decoded_blocks = 0;
	for (unsigned burst = 5; burst < 20; ++burst)
	{
		const auto original_burst = original_transmitter.next_burst();
		const auto restored_burst = restored_transmitter.next_burst();
		assert(original_burst.data == restored_burst.data);
		assert(original_burst.hl == restored_burst.hl);
		assert(original_burst.hu == restored_burst.hu);

		const auto original =
				original_receiver.receive(original_burst);
		const auto restored =
				restored_receiver.receive(restored_burst);
		assert(bool(original) == bool(restored));
		if (!original)
			continue;

		++decoded_blocks;
		assert(original->kind == restored->kind);
		if (original->kind == traffic_block_kind::facch)
		{
			assert(original->control.good == restored->control.good);
			assert(original->control.bits == restored->control.bits);
			assert(original->control.corrected_bits ==
					restored->control.corrected_bits);
		}
		else
		{
			assert(original->speech.good == restored->speech.good);
			assert(original->speech.frame == restored->speech.frame);
			assert(original->speech.corrected_bits ==
					restored->speech.corrected_bits);
		}
	}
	assert(decoded_blocks >= 3);
}

void test_full_rate_schedule()
{
	unsigned traffic = 0;
	unsigned sacch = 0;
	unsigned idle = 0;
	for (unsigned fn = 0; fn < 104; ++fn)
	{
		switch (full_rate_slot(fn, 1))
		{
		case tdma_slot_kind::traffic: ++traffic; break;
		case tdma_slot_kind::sacch: ++sacch; break;
		case tdma_slot_kind::idle: ++idle; break;
		}
	}
	assert(traffic == 96);
	assert(sacch == 4);
	assert(idle == 4);
	assert(full_rate_slot(12, 1) == tdma_slot_kind::idle);
	assert(full_rate_slot(25, 1) == tdma_slot_kind::sacch);
	assert(full_rate_slot(12, 0) == tdma_slot_kind::sacch);
	assert(full_rate_slot(25, 0) == tdma_slot_kind::idle);

	for (unsigned timeslot = 0; timeslot < 8; ++timeslot)
	{
		std::array<bool, 4> phases{};
		for (unsigned fn = 0; fn < 104; ++fn)
		{
			if (full_rate_slot(fn, timeslot) == tdma_slot_kind::sacch)
				phases[sacch_burst_index(fn, timeslot)] = true;
		}
		assert(std::all_of(
				phases.begin(), phases.end(), [](bool value) { return value; }));
	}
	assert(sacch_burst_index(12, 0) == 0);
	assert(sacch_burst_index(25, 1) == 0);
	assert(sacch_burst_index(12, 2) == 3);
	assert(sacch_burst_index(25, 7) == 1);
}

void test_stateful_sacch_stream()
{
	control_bits information{};
	for (unsigned k = 0; k < information.size(); ++k)
		information[k] = ((k * 17 + 3) % 31) < 14;
	sacch_transmitter transmitter;
	sacch_receiver receiver;
	assert(!transmitter.next_burst(0));
	assert(transmitter.enqueue(information));
	assert(!transmitter.enqueue(information));
	assert(!transmitter.next_burst(2));

	std::optional<decoded_control> decoded;
	unsigned traffic_slots = 0;
	unsigned idle_slots = 0;
	for (unsigned fn = 0; fn < 104; ++fn)
	{
		switch (full_rate_slot(fn, 1))
		{
		case tdma_slot_kind::traffic:
			++traffic_slots;
			break;
		case tdma_slot_kind::idle:
			++idle_slots;
			break;
		case tdma_slot_kind::sacch:
		{
			const auto burst = transmitter.next_burst(
					sacch_burst_index(fn, 1));
			assert(burst);
			auto result = receiver.receive(*burst);
			if (result)
				decoded = result;
			break;
		}
		}
	}
	assert(traffic_slots == 96 && idle_slots == 4);
	assert(decoded && decoded->good && decoded->bits == information);
	assert(!transmitter.next_burst(0));
}

void test_sacch_snapshot_continuation()
{
	control_bits information{};
	for (unsigned k = 0; k < information.size(); ++k)
		information[k] = ((k * 13 + 7) % 37) < 18;

	sacch_transmitter original_transmitter;
	sacch_receiver original_receiver;
	assert(original_transmitter.enqueue(information));
	for (unsigned phase = 0; phase < 2; ++phase)
	{
		const auto burst = original_transmitter.next_burst(phase);
		assert(burst);
		assert(!original_receiver.receive(*burst));
	}

	sacch_transmitter restored_transmitter;
	sacch_receiver restored_receiver;
	restored_transmitter.restore(original_transmitter.snapshot());
	restored_receiver.restore(original_receiver.snapshot());

	for (unsigned phase = 2; phase < 4; ++phase)
	{
		const auto original_burst =
				original_transmitter.next_burst(phase);
		const auto restored_burst =
				restored_transmitter.next_burst(phase);
		assert(original_burst && restored_burst);
		assert(original_burst->data == restored_burst->data);
		assert(original_burst->hl == restored_burst->hl);
		assert(original_burst->hu == restored_burst->hu);

		const auto original =
				original_receiver.receive(*original_burst);
		const auto restored =
				restored_receiver.receive(*restored_burst);
		assert(bool(original) == bool(restored));
		if (phase == 3)
		{
			assert(original && restored);
			assert(original->good == restored->good);
			assert(original->bits == restored->bits);
			assert(original->corrected_bits ==
					restored->corrected_bits);
			assert(restored->good && restored->bits == information);
		}
	}
}

} // anonymous namespace

int main()
{
	test_serial_packing();
	test_importance_permutation();
	test_clean_and_degraded_round_trip();
	test_interleaving_and_bursts();
	test_facch_control_coding();
	test_stateful_diagonal_stream();
	test_diagonal_snapshot_continuation();
	test_full_rate_schedule();
	test_stateful_sacch_stream();
	test_sacch_snapshot_continuation();
	std::cout << "GSM TCH/FS Layer 1 tests passed\n";
	return 0;
}
