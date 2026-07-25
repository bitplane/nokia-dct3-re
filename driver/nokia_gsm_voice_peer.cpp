// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "nokia_gsm_voice_peer.h"

#define LOG_VOICE_PEER (1U << 0)
#define VERBOSE (LOG_VOICE_PEER)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(NOKIA_GSM_VOICE_PEER, nokia_gsm_voice_peer_device,
		"nokia_gsm_voice_peer", "Nokia laboratory GSM voice peer")

nokia_gsm_voice_peer_device::nokia_gsm_voice_peer_device(
		const machine_config &mconfig, const char *tag,
		device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_GSM_VOICE_PEER, tag, owner, clock)
{
}

void nokia_gsm_voice_peer_device::device_start()
{
	m_trace_enabled = machine().options().verbose();
	save_item(NAME(m_lab_test_source));
	save_item(NAME(m_test_phase));
	save_item(NAME(m_exchanges));
	save_item(NAME(m_concealed_uplink_frames));
	save_item(NAME(m_muted_uplink_frames));
	save_item(NAME(m_last_uplink_peak));
	save_item(NAME(m_last_downlink_peak));
	auto save_codec_state =
			[this](nokia_gsm_fr_codec::state &state, int index)
	{
		save_item(STRUCT_MEMBER(state.channels, dp0), index);
		save_item(STRUCT_MEMBER(state.channels, e), index);
		save_item(STRUCT_MEMBER(state.channels, z1), index);
		save_item(STRUCT_MEMBER(state.channels, l_z2), index);
		save_item(STRUCT_MEMBER(state.channels, mp), index);
		save_item(STRUCT_MEMBER(state.channels, u), index);
		save_item(STRUCT_MEMBER(state.channels, larpp), index);
		save_item(STRUCT_MEMBER(state.channels, j), index);
		save_item(STRUCT_MEMBER(state.channels, ltp_cut), index);
		save_item(STRUCT_MEMBER(state.channels, nrp), index);
		save_item(STRUCT_MEMBER(state.channels, v), index);
		save_item(STRUCT_MEMBER(state.channels, msr), index);
		save_item(STRUCT_MEMBER(state.channels, verbose), index);
		save_item(STRUCT_MEMBER(state.channels, fast), index);
		save_item(STRUCT_MEMBER(state.channels, wav_fmt), index);
		save_item(STRUCT_MEMBER(state.channels, frame_index), index);
		save_item(STRUCT_MEMBER(state.channels, frame_chain), index);
	};
	save_codec_state(m_uplink_decoder_state, 0);
	save_codec_state(m_downlink_encoder_state, 1);
	save_item(NAME(m_uplink_receiver_state.last_good));
	save_item(NAME(m_uplink_receiver_state.have_good));
	save_item(NAME(m_uplink_receiver_state.lost_frames));
	machine().save().register_presave(
			save_prepost_delegate(
				FUNC(nokia_gsm_voice_peer_device::prepare_codec_save), this));
	machine().save().register_postload(
			save_prepost_delegate(
				FUNC(nokia_gsm_voice_peer_device::restore_codec_state), this));
}

void nokia_gsm_voice_peer_device::device_reset()
{
	start_call();
}

void nokia_gsm_voice_peer_device::start_call()
{
	m_uplink_decoder.reset();
	m_downlink_encoder.reset();
	m_uplink_receiver.reset();
	m_test_phase = 0;
	m_exchanges = 0;
	m_concealed_uplink_frames = 0;
	m_muted_uplink_frames = 0;
	m_last_uplink_peak = 0;
	m_last_downlink_peak = 0;
}

void nokia_gsm_voice_peer_device::prepare_codec_save()
{
	m_uplink_decoder_state = m_uplink_decoder.snapshot();
	m_downlink_encoder_state = m_downlink_encoder.snapshot();
	m_uplink_receiver_state = m_uplink_receiver.snapshot();
}

void nokia_gsm_voice_peer_device::restore_codec_state()
{
	if (!m_uplink_decoder.restore(m_uplink_decoder_state) ||
			!m_downlink_encoder.restore(m_downlink_encoder_state) ||
			!m_uplink_receiver.restore(m_uplink_receiver_state))
		fatalerror("GSM voice peer: invalid codec state in save image");
}

u16 nokia_gsm_voice_peer_device::block_peak(
		const nokia_gsm_fr_codec::pcm_block &block)
{
	u16 peak = 0;
	for (s16 sample : block)
		peak = std::max<u16>(
				peak, u16(sample < 0 ? -s32(sample) : s32(sample)));
	return peak;
}

bool nokia_gsm_voice_peer_device::exchange(
		const speech_frame *uplink, speech_frame &downlink)
{
	nokia_gsm_fr_codec::speech_frame encoded_uplink{};
	const nokia_gsm_fr_codec::speech_frame *decoder_frame = nullptr;
	if (uplink)
	{
		std::copy(uplink->begin(), uplink->end(), encoded_uplink.begin());
		decoder_frame = &encoded_uplink;
	}
	nokia_gsm_fr_codec::pcm_block remote_receiver{};
	if (!m_uplink_receiver.decode(
			m_uplink_decoder, decoder_frame, remote_receiver))
		return false;
	m_last_uplink_peak = block_peak(remote_receiver);
	if (!uplink)
	{
		++m_concealed_uplink_frames;
		if (m_uplink_receiver.lost_frames() >=
				nokia_gsm_fr_receiver::mute_after_lost_frames)
			++m_muted_uplink_frames;
	}

	nokia_gsm_fr_codec::pcm_block remote_microphone{};
	if (m_lab_test_source)
	{
		// Service documentation specifies audio levels at 1 kHz. At 8 kHz,
		// this eight-sample signed sine vector is exactly one test period.
		static constexpr std::array<s16, 8> sine_1khz = {
			0, 2896, 4096, 2896, 0, -2896, -4096, -2896
		};
		for (s16 &sample : remote_microphone)
		{
			sample = sine_1khz[m_test_phase];
			m_test_phase = (m_test_phase + 1) & 7;
		}
	}
	m_last_downlink_peak = block_peak(remote_microphone);

	nokia_gsm_fr_codec::speech_frame encoded_downlink{};
	if (!m_downlink_encoder.encode(remote_microphone, encoded_downlink))
		return false;
	std::copy(encoded_downlink.begin(), encoded_downlink.end(), downlink.begin());
	++m_exchanges;
	if (m_trace_enabled &&
			(m_exchanges <= 3 || (m_exchanges % 50) == 0))
		LOGMASKED(LOG_VOICE_PEER,
				"gsm_voice_peer: exchange=%llu uplink_peak=%u "
				"downlink_peak=%u source=%s uplink_good=%u "
				"concealed=%llu muted=%llu t=%.6f\n",
				m_exchanges, m_last_uplink_peak, m_last_downlink_peak,
				m_lab_test_source ? "lab-1khz" : "silence",
				uplink != nullptr, m_concealed_uplink_frames,
				m_muted_uplink_frames,
				machine().time().as_double());
	return true;
}
