// license:BSD-3-Clause
// copyright-holders:Gaz

#include "../driver/nokia_gsm_fr_codec.h"

#include <algorithm>
#include <cassert>
#include <cstdint>

int main()
{
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
	assert(!codec.decode(malformed, decoded));

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

	auto invalid_state = codec.snapshot();
	invalid_state.channels[1].nrp = 0;
	assert(!codec.restore(invalid_state));

	codec.reset();
	assert(codec.available());
	return 0;
}
