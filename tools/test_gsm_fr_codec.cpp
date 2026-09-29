// license:BSD-3-Clause
// copyright-holders:Gaz

#include "../driver/nokia_gsm_fr_codec.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

namespace {

void check_reference_vectors()
{
	// FNV-1a over 64 frames and little-endian decoded PCM for each deterministic
	// input. Captured from the original pinned libgsm 1.0.24 integration; these
	// are compatibility vectors, not an ETSI conformance claim.
	constexpr std::uint64_t expected_frames[] = {
		0x71fe46fc24e03765ULL, 0xbaf759748531aaabULL, 0xf72ab9b235e7a50dULL,
		0xaa2e0c25868d2ad1ULL, 0xf47fccdc04e8e41bULL
	};
	constexpr std::uint64_t expected_pcm[] = {
		0x39bfe49685b5083dULL, 0x853d11de0aaade31ULL, 0x744d693c57ae37b0ULL,
		0xe70530259aa7c17aULL, 0x29e3fccf83718cacULL
	};
	for (unsigned pattern = 0; pattern < std::size(expected_frames); ++pattern)
	{
		nokia_gsm_fr_codec codec;
		std::uint64_t frames = 14695981039346656037ULL;
		std::uint64_t pcm = frames;
		std::uint32_t random = 0x12345678;
		for (unsigned block = 0; block < 64; ++block)
		{
			nokia_gsm_fr_codec::pcm_block input{};
			for (unsigned i = 0; i < input.size(); ++i)
			{
				random = random * 1664525 + 1013904223;
				switch (pattern)
				{
				case 0: input[i] = 0; break;
				case 1: input[i] = i == block % 160 ? 32767 : 0; break;
				case 2: input[i] = i & 1 ? 32767 : -32768; break;
				case 3: input[i] = std::int16_t(std::int32_t((i + block * 160) % 65536) - 32768); break;
				case 4: input[i] = std::int16_t(std::int32_t(random >> 16) - 32768); break;
				}
			}
			nokia_gsm_fr_codec::speech_frame frame{};
			nokia_gsm_fr_codec::pcm_block output{};
			assert(codec.encode(input, frame));
			assert(codec.decode(frame, output));
			for (auto byte : frame)
				frames = (frames ^ byte) * 1099511628211ULL;
			for (auto sample : output)
			{
				const auto word = std::uint16_t(sample);
				pcm = (pcm ^ (word & 255)) * 1099511628211ULL;
				pcm = (pcm ^ (word >> 8)) * 1099511628211ULL;
			}
		}
		assert(frames == expected_frames[pattern]);
		assert(pcm == expected_pcm[pattern]);
	}
}

} // anonymous namespace

int main()
{
	check_reference_vectors();
	nokia_gsm_fr_codec codec;
	assert(codec.available());

	nokia_gsm_fr_codec::pcm_block source{};
	for (unsigned sample = 0; sample < source.size(); ++sample)
		source[sample] = (sample / 4) & 1 ? std::int16_t(12'000) : std::int16_t(-12'000);

	nokia_gsm_fr_codec::speech_frame encoded{};
	assert(codec.encode(source, encoded));
	// Conventional GSM 06.10 serial frames identify RPE-LTP with magic 0xd.
	assert((encoded[0] >> 4) == 0x0d);

	nokia_gsm_fr_codec::pcm_block decoded{};
	assert(codec.decode(encoded, decoded));
	assert(std::any_of(decoded.begin(), decoded.end(),
			[](std::int16_t sample) { return sample != 0; }));

	auto malformed = encoded;
	malformed[0] = 0;
	decoded.fill(1234);
	assert(!codec.decode(malformed, decoded));
	assert(std::all_of(decoded.begin(), decoded.end(),
			[](std::int16_t sample) { return sample == 1234; }));

	// Predictor histories must continue identically after an emulator
	// save/load, independently in the encoder and decoder directions.
	nokia_gsm_fr_codec original;
	std::array<nokia_gsm_fr_codec::speech_frame, 8> source_frames{};
	for (unsigned frame = 0; frame < source_frames.size(); ++frame)
	{
		nokia_gsm_fr_codec::pcm_block input{};
		for (unsigned sample = 0; sample < input.size(); ++sample)
		{
			const std::int32_t phase =
					std::int32_t((sample * (frame + 3) + frame * 29) % 127);
			input[sample] = std::int16_t((phase - 63) * 173);
		}
		assert(original.encode(input, source_frames[frame]));
		nokia_gsm_fr_codec::pcm_block output{};
		assert(original.decode(source_frames[frame], output));
		if (frame == 3)
		{
			const auto saved = original.snapshot();
			nokia_gsm_fr_codec restored;
			assert(restored.restore(saved));

			for (unsigned future = frame + 1;
					future < source_frames.size(); ++future)
			{
				nokia_gsm_fr_codec::pcm_block future_input{};
				for (unsigned sample = 0; sample < future_input.size(); ++sample)
				{
					const std::int32_t phase = std::int32_t(
							(sample * (future + 3) + future * 29) % 127);
					future_input[sample] = std::int16_t((phase - 63) * 173);
				}
				nokia_gsm_fr_codec::speech_frame original_frame{};
				nokia_gsm_fr_codec::speech_frame restored_frame{};
				assert(original.encode(future_input, original_frame));
				assert(restored.encode(future_input, restored_frame));
				assert(original_frame == restored_frame);

				nokia_gsm_fr_codec::pcm_block original_pcm{};
				nokia_gsm_fr_codec::pcm_block restored_pcm{};
				assert(original.decode(original_frame, original_pcm));
				assert(restored.decode(restored_frame, restored_pcm));
				assert(original_pcm == restored_pcm);
			}
			break;
		}
	}

	const auto before_invalid_state = codec.snapshot();
	auto invalid_state = before_invalid_state;
	invalid_state.channels[1].nrp = 0;
	invalid_state.channels[0].dp0[0] ^= 1;
	assert(!codec.restore(invalid_state));
	// A bad decoder state must not partially install the encoder history.
	assert(codec.snapshot().channels[0].dp0 == before_invalid_state.channels[0].dp0);
	invalid_state = codec.snapshot();
	invalid_state.channels[0].wav_fmt = 1;
	assert(!codec.restore(invalid_state));
	invalid_state = codec.snapshot();
	invalid_state.channels[0].l_z2 =
			std::int64_t(std::numeric_limits<std::int32_t>::max()) + 1;
	assert(!codec.restore(invalid_state));

	// GSM 06.11 receive-side substitution is independent of Layer 1.  The
	// first BFI repeats a valid speech frame, subsequent BFIs fade, and the
	// output reaches silence at 320 ms before recovering on a clean frame.
	nokia_gsm_fr_codec receive_codec;
	nokia_gsm_fr_receiver receiver;
	nokia_gsm_fr_codec::pcm_block concealed{};
	assert(receiver.decode(receive_codec, nullptr, concealed));
	assert(std::all_of(concealed.begin(), concealed.end(),
			[](std::int16_t sample) { return sample == 0; }));
	assert(receiver.decode(receive_codec, &encoded, concealed));
	std::uint16_t clean_peak = 0;
	for (std::int16_t sample : concealed)
		clean_peak = std::max<std::uint16_t>(
				clean_peak, std::uint16_t(
						sample < 0 ? -std::int32_t(sample) : sample));
	assert(clean_peak != 0);
	for (unsigned loss = 1;
			loss <= nokia_gsm_fr_receiver::mute_after_lost_frames; ++loss)
	{
		assert(receiver.decode(receive_codec, nullptr, concealed));
		std::uint16_t peak = 0;
		for (std::int16_t sample : concealed)
			peak = std::max<std::uint16_t>(
					peak, std::uint16_t(
							sample < 0 ? -std::int32_t(sample) : sample));
		if (loss == 1)
			assert(peak != 0);
		if (loss == nokia_gsm_fr_receiver::mute_after_lost_frames)
			assert(peak == 0);
	}
	assert(receiver.decode(receive_codec, &encoded, concealed));
	assert(receiver.lost_frames() == 0);
	assert(std::any_of(concealed.begin(), concealed.end(),
			[](std::int16_t sample) { return sample != 0; }));

	// Receiver substitution history and the underlying decoder predictor must
	// branch together across a save/load in the middle of a loss sequence.
	assert(receiver.decode(receive_codec, nullptr, concealed));
	assert(receiver.decode(receive_codec, nullptr, concealed));
	const auto saved_receiver = receiver.snapshot();
	const auto saved_receive_codec = receive_codec.snapshot();
	nokia_gsm_fr_codec restored_receive_codec;
	nokia_gsm_fr_receiver restored_receiver;
	assert(restored_receive_codec.restore(saved_receive_codec));
	assert(restored_receiver.restore(saved_receiver));
	for (unsigned loss = 0; loss < 4; ++loss)
	{
		nokia_gsm_fr_codec::pcm_block original_concealed{};
		nokia_gsm_fr_codec::pcm_block restored_concealed{};
		assert(receiver.decode(
				receive_codec, nullptr, original_concealed));
		assert(restored_receiver.decode(
				restored_receive_codec, nullptr, restored_concealed));
		assert(original_concealed == restored_concealed);
	}

	auto receiver_state = receiver.snapshot();
	receiver_state.lost_frames =
			nokia_gsm_fr_receiver::mute_after_lost_frames + 1;
	assert(!receiver.restore(receiver_state));

	codec.reset();
	assert(codec.available());
	return 0;
}
