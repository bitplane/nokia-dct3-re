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
	save_item(NAME(m_last_uplink_peak));
	save_item(NAME(m_last_downlink_peak));
}

void nokia_gsm_voice_peer_device::device_reset()
{
	m_uplink_decoder.reset();
	m_downlink_encoder.reset();
	m_test_phase = 0;
	m_exchanges = 0;
	m_last_uplink_peak = 0;
	m_last_downlink_peak = 0;
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
		const speech_frame &uplink, speech_frame &downlink)
{
	nokia_gsm_fr_codec::speech_frame encoded_uplink{};
	std::copy(uplink.begin(), uplink.end(), encoded_uplink.begin());
	nokia_gsm_fr_codec::pcm_block remote_receiver{};
	if (!m_uplink_decoder.decode(encoded_uplink, remote_receiver))
		return false;
	m_last_uplink_peak = block_peak(remote_receiver);

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
				"downlink_peak=%u source=%s t=%.6f\n",
				m_exchanges, m_last_uplink_peak, m_last_downlink_peak,
				m_lab_test_source ? "lab-1khz" : "silence",
				machine().time().as_double());
	return true;
}
