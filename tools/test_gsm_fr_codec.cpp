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

	codec.reset();
	assert(codec.available());
	return 0;
}
