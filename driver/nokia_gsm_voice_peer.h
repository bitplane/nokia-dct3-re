// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_NOKIA_GSM_VOICE_PEER_H
#define MAME_NOKIA_NOKIA_GSM_VOICE_PEER_H

#include "nokia_gsm_fr_codec.h"

#include <array>

// Network-side speech endpoint. It owns only transcoding between a remote
// 8 kHz PCM source/sink and GSM-FR frames; radio scheduling remains elsewhere.
class nokia_gsm_voice_peer_device : public device_t
{
public:
	using speech_frame = std::array<u8, nokia_gsm_fr_codec::frame_octets>;

	nokia_gsm_voice_peer_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	void set_lab_test_source(bool enabled) { m_lab_test_source = enabled; }
	bool exchange(const speech_frame &uplink, speech_frame &downlink);
	u64 exchanges() const { return m_exchanges; }
	u16 last_uplink_peak() const { return m_last_uplink_peak; }
	u16 last_downlink_peak() const { return m_last_downlink_peak; }

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	static u16 block_peak(const nokia_gsm_fr_codec::pcm_block &block);

	nokia_gsm_fr_codec m_uplink_decoder;
	nokia_gsm_fr_codec m_downlink_encoder;
	bool m_lab_test_source = false;
	bool m_trace_enabled = false;
	u8 m_test_phase = 0;
	u64 m_exchanges = 0;
	u16 m_last_uplink_peak = 0;
	u16 m_last_downlink_peak = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_GSM_VOICE_PEER, nokia_gsm_voice_peer_device)

#endif // MAME_NOKIA_NOKIA_GSM_VOICE_PEER_H
