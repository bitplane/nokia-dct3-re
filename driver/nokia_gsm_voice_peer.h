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
	static constexpr unsigned host_queue_depth = 8;

	nokia_gsm_voice_peer_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	void set_lab_test_source(bool enabled) { m_lab_test_source = enabled; }
	void start_call();
	void begin_host_media(u32 request_id);
	void end_host_media();
	bool submit_host_downlink(
			u32 request_id, u32 sequence, const speech_frame &frame);
	bool take_host_uplink(
			u32 &request_id, u32 &sequence, u64 &time_us,
			bool &good, speech_frame &frame);
	bool host_media_active() const { return m_host_request_id != 0; }
	u32 host_next_uplink_sequence() const
	{
		return m_host_uplink_sequence - m_host_uplink_count;
	}
	u32 host_next_downlink_sequence() const
	{
		return m_host_downlink_sequence;
	}
	// Null uplink denotes BFI/FACCH substitution at the network receiver.
	// Downlink generation remains independent and still produces one frame.
	bool exchange(const speech_frame *uplink, speech_frame &downlink);
	u64 exchanges() const { return m_exchanges; }
	u16 last_uplink_peak() const { return m_last_uplink_peak; }
	u16 last_downlink_peak() const { return m_last_downlink_peak; }

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	static u16 block_peak(const nokia_gsm_fr_codec::pcm_block &block);
	void prepare_codec_save();
	void restore_codec_state();
	static bool frame_queue_push(
			std::array<speech_frame, host_queue_depth> &queue,
			u8 &head, u8 &count, const speech_frame &frame);
	static bool frame_queue_pop(
			std::array<speech_frame, host_queue_depth> &queue,
			u8 &head, u8 &count, speech_frame &frame);

	nokia_gsm_fr_codec m_uplink_decoder;
	nokia_gsm_fr_codec m_downlink_encoder;
	nokia_gsm_fr_codec m_host_downlink_decoder;
	nokia_gsm_fr_receiver m_uplink_receiver;
	nokia_gsm_fr_codec::state m_uplink_decoder_state{};
	nokia_gsm_fr_codec::state m_downlink_encoder_state{};
	nokia_gsm_fr_codec::state m_host_downlink_decoder_state{};
	nokia_gsm_fr_receiver::state m_uplink_receiver_state{};
	bool m_lab_test_source = false;
	bool m_trace_enabled = false;
	u8 m_test_phase = 0;
	u64 m_exchanges = 0;
	u64 m_concealed_uplink_frames = 0;
	u64 m_muted_uplink_frames = 0;
	u16 m_last_uplink_peak = 0;
	u16 m_last_downlink_peak = 0;
	u32 m_host_request_id = 0;
	u32 m_host_uplink_sequence = 0;
	u32 m_host_downlink_sequence = 0;
	std::array<speech_frame, host_queue_depth> m_host_uplink{};
	std::array<u64, host_queue_depth> m_host_uplink_time_us{};
	std::array<u8, host_queue_depth> m_host_uplink_good{};
	u8 m_host_uplink_head = 0;
	u8 m_host_uplink_count = 0;
	std::array<speech_frame, host_queue_depth> m_host_downlink{};
	u8 m_host_downlink_head = 0;
	u8 m_host_downlink_count = 0;
	u64 m_host_uplink_overruns = 0;
	u64 m_host_downlink_underruns = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_GSM_VOICE_PEER, nokia_gsm_voice_peer_device)

#endif // MAME_NOKIA_NOKIA_GSM_VOICE_PEER_H
