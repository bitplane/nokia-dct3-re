// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_EXTERNAL_SERVICE_H
#define MAME_NOKIA_NOKIA_EXTERNAL_SERVICE_H

#include <array>

class nokia_external_service_peer_device : public device_t
{
public:
	enum class application_profile : u8
	{
		none,
		nse8,
		nhm5,
	};

	struct response
	{
		u8 type = 0x8e;
		u8 length = 0;
		bool notify = true;
		std::array<u8, 75> payload = { 0 };
	};

	nokia_external_service_peer_device(
			const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	void set_enabled(bool enabled) { m_enabled = enabled; }
	void set_application_profile(application_profile profile)
	{
		m_application_profile = profile;
	}
	void receive_frame(const u8 *payload, unsigned length);
	void set_service_control_complete();
	void tick();
	bool peek_response(response &result) const;
	void consume_response();

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	struct profile_contract
	{
		unsigned start_delay_ticks;
		u8 registration_result;
		u8 registration_sequence;
		u8 channel_map_byte_a;
		u8 channel_map_mask_a;
		u8 channel_map_byte_b;
		u8 channel_map_mask_b;
		u8 channel_map_sequence;
	};

	static constexpr u8 DISCOVERY_NODE = 0x02;
	static constexpr unsigned QUEUE_SIZE = 8;

	bool queue_response(const u8 *payload, unsigned length, bool notify = true);
	bool queue_transport_ack(const u8 *payload, unsigned length);
	bool queue_service_frame(u8 command, u8 result, u8 sequence);
	const profile_contract *contract() const;
	bool application_enabled() const
	{
		return m_enabled && m_application_profile != application_profile::none;
	}

	bool m_enabled = false;
	application_profile m_application_profile = application_profile::none;
	bool m_trace_enabled = false;
	bool m_discovery_complete = false;
	bool m_service_control_complete = false;
	bool m_registration_sent = false;
	bool m_registration_acknowledged = false;
	bool m_channel_map_sent = false;
	bool m_channel_map_acknowledged = false;
	bool m_empty_ack_sent = false;
	unsigned m_registration_ticks = 0;
	u8 m_queue_type[QUEUE_SIZE] = { 0 };
	u8 m_queue_length[QUEUE_SIZE] = { 0 };
	bool m_queue_notify[QUEUE_SIZE] = { false };
	u8 m_queue_payload[QUEUE_SIZE][75] = { { 0 } };
	u8 m_queue_head = 0;
	u8 m_queue_tail = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_EXTERNAL_SERVICE_PEER, nokia_external_service_peer_device)

#endif // MAME_NOKIA_NOKIA_EXTERNAL_SERVICE_H
