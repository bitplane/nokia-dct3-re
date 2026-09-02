// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_NOKIA_GSM_CALL_ADAPTER_H
#define MAME_NOKIA_NOKIA_GSM_CALL_ADAPTER_H

#include "nokia_gsm_session.h"
#include "nokia_gsm_voice_peer.h"

#include <atomic>
#include <memory>

class nokia_radio_peer_device;

class nokia_gsm_call_adapter_device : public device_t
{
public:
	nokia_gsm_call_adapter_device(
			const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);
	virtual ~nokia_gsm_call_adapter_device();

	void set_enabled(bool enabled);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;

private:
	struct host_state;

	TIMER_CALLBACK_MEMBER(poll_host);
	void postload();
	void publish_request();
	void publish_ready();
	void publish_state(const char *phase);
	void publish_incoming_state(const char *phase);
	void publish_uplink_media(
			u32 request_id, u32 sequence, u64 time_us, bool good,
			const nokia_gsm_voice_peer_device::speech_frame &frame);

	required_device<nokia_gsm_session_device> m_session;
	required_device<nokia_gsm_voice_peer_device> m_voice_peer;
	required_device<nokia_radio_peer_device> m_radio_peer;
	std::unique_ptr<host_state> m_host;
	emu_timer *m_poll_timer = nullptr;
	bool m_enabled = false;
	u32 m_last_published_request_id = 0;
	bool m_last_published_connected = false;
	bool m_last_published_alerting = false;
	u32 m_incoming_request_id = 0;
	std::array<u8, 20> m_incoming_digits{};
	unsigned m_incoming_digits_length = 0;
	bool m_incoming_page_accepted = false;
	bool m_last_incoming_connected = false;
	bool m_last_incoming_alerting = false;
	bool m_incoming_started = false;
	bool m_incoming_connected_once = false;
	std::atomic<u32> m_transport_epoch{1};
};

DECLARE_DEVICE_TYPE(NOKIA_GSM_CALL_ADAPTER, nokia_gsm_call_adapter_device)

#endif // MAME_NOKIA_NOKIA_GSM_CALL_ADAPTER_H
