// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsmfr.h"

#include "libgsm/inc/gsm.h"
#include "libgsm/inc/private.h"
#include "libgsm/inc/unproto.h"

#include <algorithm>
#include <iterator>
#include <limits>

static_assert(sizeof(short) == sizeof(std::int16_t));
static_assert(sizeof(int) == sizeof(std::int32_t));
static_assert(sizeof(long) <= sizeof(std::int64_t));

namespace util {

namespace {

gsm_fr_codec::channel_state export_state(const gsm_state &source)
{
	gsm_fr_codec::channel_state result{};
	std::copy(std::begin(source.dp0), std::end(source.dp0), result.dp0.begin());
	std::copy(std::begin(source.e), std::end(source.e), result.e.begin());
	result.z1 = source.z1;
	result.l_z2 = source.L_z2;
	result.mp = source.mp;
	std::copy(std::begin(source.u), std::end(source.u), result.u.begin());
	for (unsigned bank = 0; bank < 2; ++bank)
		std::copy(std::begin(source.LARpp[bank]),
				std::end(source.LARpp[bank]),
				result.larpp.begin() + bank * 8);
	result.j = source.j;
	result.ltp_cut = source.ltp_cut;
	result.nrp = source.nrp;
	std::copy(std::begin(source.v), std::end(source.v), result.v.begin());
	result.msr = source.msr;
	result.verbose = source.verbose;
	result.fast = source.fast;
	result.wav_fmt = source.wav_fmt;
	result.frame_index = source.frame_index;
	result.frame_chain = source.frame_chain;
	return result;
}

bool import_state(
		const gsm_fr_codec::channel_state &source, gsm_state &result)
{
	// nrp is a 40..120 decoder lag and j selects one of two LAR histories.
	// Reject malformed external state rather than indexing libgsm out of range.
	if (source.j < 0 || source.j > 1 ||
			source.nrp < 40 || source.nrp > 120 ||
			source.l_z2 < std::numeric_limits<std::int32_t>::min() ||
			source.l_z2 > std::numeric_limits<std::int32_t>::max() ||
			source.mp < std::numeric_limits<std::int16_t>::min() ||
			source.mp > std::numeric_limits<std::int16_t>::max() ||
			source.ltp_cut || source.verbose || source.fast || source.wav_fmt ||
			source.frame_index || source.frame_chain)
		return false;

	std::copy(source.dp0.begin(), source.dp0.end(), std::begin(result.dp0));
	std::copy(source.e.begin(), source.e.end(), std::begin(result.e));
	result.z1 = source.z1;
	result.L_z2 = long(source.l_z2);
	result.mp = source.mp;
	std::copy(source.u.begin(), source.u.end(), std::begin(result.u));
	for (unsigned bank = 0; bank < 2; ++bank)
		std::copy(source.larpp.begin() + bank * 8,
				source.larpp.begin() + (bank + 1) * 8,
				std::begin(result.LARpp[bank]));
	result.j = source.j;
	result.ltp_cut = source.ltp_cut;
	result.nrp = source.nrp;
	std::copy(source.v.begin(), source.v.end(), std::begin(result.v));
	result.msr = source.msr;
	result.verbose = source.verbose;
	result.fast = source.fast;
	result.wav_fmt = source.wav_fmt;
	result.frame_index = source.frame_index;
	result.frame_chain = source.frame_chain;
	return true;
}

} // anonymous namespace

// Only this translation unit sees the bundled implementation's private
// structure. Field-wise snapshots remain independent of its ABI and padding.
struct gsm_fr_codec::implementation
{
	gsm encoder = gsm_create();
	gsm decoder = gsm_create();

	~implementation()
	{
		if (encoder)
			gsm_destroy(encoder);
		if (decoder)
			gsm_destroy(decoder);
	}
};

gsm_fr_codec::gsm_fr_codec()
{
	reset();
}

gsm_fr_codec::~gsm_fr_codec() = default;

void gsm_fr_codec::reset()
{
	m_impl = std::make_unique<implementation>();
	if (!available())
		m_impl.reset();
}

bool gsm_fr_codec::available() const
{
	return m_impl && m_impl->encoder && m_impl->decoder;
}

bool gsm_fr_codec::encode(const pcm_block &pcm, speech_frame &frame)
{
	if (!available())
		return false;
	std::array<gsm_signal, pcm_samples> input;
	std::copy(pcm.begin(), pcm.end(), input.begin());
	gsm_encode(m_impl->encoder, input.data(), frame.data());
	return true;
}

bool gsm_fr_codec::decode(const speech_frame &frame, pcm_block &pcm)
{
	if (!available())
		return false;
	auto input = frame;
	std::array<gsm_signal, pcm_samples> output;
	if (gsm_decode(m_impl->decoder, input.data(), output.data()) != 0)
		return false;
	std::copy(output.begin(), output.end(), pcm.begin());
	return true;
}

gsm_fr_codec::state gsm_fr_codec::snapshot() const
{
	state result{};
	if (available())
	{
		result.channels[0] = export_state(*m_impl->encoder);
		result.channels[1] = export_state(*m_impl->decoder);
	}
	return result;
}

bool gsm_fr_codec::restore(const state &state)
{
	if (!available())
		return false;

	// Validate both directions before changing either one.
	gsm_state encoder = *m_impl->encoder;
	gsm_state decoder = *m_impl->decoder;
	if (!import_state(state.channels[0], encoder) ||
			!import_state(state.channels[1], decoder))
		return false;
	*m_impl->encoder = encoder;
	*m_impl->decoder = decoder;
	return true;
}

bool gsm_fr_receiver::decode(gsm_fr_codec &codec,
		const gsm_fr_codec::speech_frame *frame,
		gsm_fr_codec::pcm_block &pcm)
{
	if (frame)
	{
		if (!codec.decode(*frame, pcm))
			return false;
		m_state.last_good = *frame;
		m_state.have_good = 1;
		m_state.lost_frames = 0;
		return true;
	}

	// GSM 06.11 requires a lost frame not to reach the speech decoder as the
	// received payload.  Before the first valid frame there is nothing valid
	// to extrapolate, so emit silence.  Afterwards repeat the last good frame
	// at the decoder input and attenuate progressively to silence no later
	// than 320 ms.  The first loss remains at full level as required.
	if (m_state.lost_frames < mute_after_lost_frames)
		++m_state.lost_frames;
	if (!m_state.have_good)
	{
		pcm.fill(0);
		return true;
	}
	if (!codec.decode(m_state.last_good, pcm))
		return false;

	const unsigned gain =
			m_state.lost_frames >= mute_after_lost_frames
				? 0
				: mute_after_lost_frames - m_state.lost_frames;
	constexpr unsigned denominator = mute_after_lost_frames - 1;
	for (std::int16_t &sample : pcm)
		sample = std::int16_t(
				(std::int32_t(sample) * std::int32_t(gain)) /
				std::int32_t(denominator));
	return true;
}

bool gsm_fr_receiver::restore(const state &saved)
{
	if (saved.have_good > 1 ||
			saved.lost_frames > mute_after_lost_frames)
		return false;
	m_state = saved;
	return true;
}

} // namespace util
