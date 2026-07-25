// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_NOKIA_GSM_FR_CODEC_H
#define MAME_NOKIA_NOKIA_GSM_FR_CODEC_H

#include <array>
#include <cstdint>

// DSP-side ETSI GSM full-rate codec boundary.  It deals only in one 20 ms
// PCM block and one serial GSM 06.10 frame; radio scheduling, channel coding
// and COBBA routing belong to separate components.
class nokia_gsm_fr_codec
{
public:
	static constexpr unsigned pcm_samples = 160;
	static constexpr unsigned frame_octets = 33;
	using pcm_block = std::array<std::int16_t, pcm_samples>;
	using speech_frame = std::array<std::uint8_t, frame_octets>;

	nokia_gsm_fr_codec();
	~nokia_gsm_fr_codec();

	nokia_gsm_fr_codec(const nokia_gsm_fr_codec &) = delete;
	nokia_gsm_fr_codec &operator=(const nokia_gsm_fr_codec &) = delete;

	bool available() const { return m_encoder && m_decoder; }
	bool encode(const pcm_block &pcm, speech_frame &frame);
	bool decode(const speech_frame &frame, pcm_block &pcm);
	void reset();

private:
	void release();
	void *m_encoder = nullptr;
	void *m_decoder = nullptr;
};

#endif // MAME_NOKIA_NOKIA_GSM_FR_CODEC_H
