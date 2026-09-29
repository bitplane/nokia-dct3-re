// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_UTIL_GSMFR_H
#define MAME_UTIL_GSMFR_H

#include <array>
#include <cstdint>
#include <memory>

namespace util {

// ETSI GSM full-rate codec boundary.  It deals only in one 20 ms
// PCM block and one serial GSM 06.10 frame; radio scheduling, channel coding
// and COBBA routing belong to separate components.
class gsm_fr_codec
{
public:
	static constexpr unsigned pcm_samples = 160;
	static constexpr unsigned frame_octets = 33;
	using pcm_block = std::array<std::int16_t, pcm_samples>;
	using speech_frame = std::array<std::uint8_t, frame_octets>;

	// Complete algorithmic state of one libgsm 1.0.24 direction, expressed
	// in fixed-width values rather than opaque allocator bytes. This keeps
	// emulator save states independent of pointers and structure padding.
	struct channel_state
	{
		std::array<std::int16_t, 280> dp0{};
		std::array<std::int16_t, 50> e{};
		std::int16_t z1 = 0;
		std::int64_t l_z2 = 0;
		std::int32_t mp = 0;
		std::array<std::int16_t, 8> u{};
		std::array<std::int16_t, 16> larpp{};
		std::int16_t j = 0;
		std::int16_t ltp_cut = 0;
		std::int16_t nrp = 40;
		std::array<std::int16_t, 9> v{};
		std::int16_t msr = 0;
		std::int8_t verbose = 0;
		std::int8_t fast = 0;
		std::int8_t wav_fmt = 0;
		std::uint8_t frame_index = 0;
		std::uint8_t frame_chain = 0;
	};

	struct state
	{
		// Encoder and decoder histories are intentionally independent.
		std::array<channel_state, 2> channels{};
	};

	gsm_fr_codec();
	~gsm_fr_codec();

	gsm_fr_codec(const gsm_fr_codec &) = delete;
	gsm_fr_codec &operator=(const gsm_fr_codec &) = delete;

	bool available() const;
	bool encode(const pcm_block &pcm, speech_frame &frame);
	bool decode(const speech_frame &frame, pcm_block &pcm);
	void reset();
	state snapshot() const;
	bool restore(const state &state);

private:
	struct implementation;
	std::unique_ptr<implementation> m_impl;
};

// Generic GSM-FR receive-side bad-frame substitution.  Layer 1 supplies the
// BFI; this component substitutes a previous valid speech frame at the decoder
// input and progressively mutes its PCM result.  It owns neither radio timing
// nor hardware audio routing.
class gsm_fr_receiver
{
public:
	static constexpr unsigned mute_after_lost_frames = 16; // 320 ms at 20 ms

	struct state
	{
		gsm_fr_codec::speech_frame last_good{};
		std::uint8_t have_good = 0;
		std::uint8_t lost_frames = 0;
	};

	// A null frame is a BFI/erasure.  A valid frame resets the muting sequence.
	bool decode(gsm_fr_codec &codec,
			const gsm_fr_codec::speech_frame *frame,
			gsm_fr_codec::pcm_block &pcm);
	void reset() { m_state = {}; }
	state snapshot() const { return m_state; }
	bool restore(const state &saved);
	unsigned lost_frames() const { return m_state.lost_frames; }
	bool concealed() const { return m_state.lost_frames != 0; }

private:
	state m_state{};
};

} // namespace util

#endif // MAME_UTIL_GSMFR_H
