// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_GSM_SESSION_H
#define MAME_NOKIA_NOKIA_GSM_SESSION_H

#include "nokia_gsm_network.h"

#include <array>

class nokia_gsm_session_device : public device_t
{
public:
	static constexpr unsigned maximum_layer3_length =
			nokia_gsm_network_device::maximum_layer3_length;

	enum class incoming_service : u8
	{
		none,
		call,
		sms,
		smart_message
	};

	enum class downlink_kind : u8
	{
		none,
		location_update_accept,
		channel_release,
		cipher_mode_command,
		mm_information,
		incoming_call_setup,
		traffic_assignment,
		sapi3_establishment,
		incoming_sms_cp_data,
		sms_cp_ack,
		connect_acknowledge,
		call_release,
		release_complete
	};

	struct downlink_message
	{
		u8 kind = u8(downlink_kind::none);
		u8 sapi = 0;
		unsigned length = 0;
		std::array<u8, maximum_layer3_length> data{};
	};

	nokia_gsm_session_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	bool establish_layer3(const u8 *information, unsigned length);
	bool queue_incoming_page(incoming_service service = incoming_service::none);
	downlink_kind contention_resolution_delivered();
	downlink_kind downlink_acknowledged();
	downlink_kind receive_layer3(
			u8 sapi, const u8 *information, unsigned length);
	bool begin_traffic_assignment();

	const downlink_message &pending_downlink() const { return m_pending_downlink; }
	downlink_kind pending_downlink_kind() const
	{
		return downlink_kind(m_pending_downlink.kind);
	}
	std::array<u8, 24> paging_request() const;
	const std::array<u8, 8> &registered_mobile_identity() const
	{
		return m_registered_mobile_identity;
	}
	unsigned registered_mobile_identity_length() const
	{
		return m_registered_mobile_identity_length;
	}
	bool idle() const { return m_state == u8(state::idle); }
	bool awaiting_traffic_assignment() const
	{
		return m_state == u8(state::awaiting_traffic_assignment);
	}
	bool call_connected() const
	{
		return m_state == u8(state::incoming_call_active);
	}

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	enum class state : u8
	{
		idle,
		awaiting_paging_response,
		awaiting_contention_resolution,
		awaiting_paging_contention_resolution,
		awaiting_location_update_accept_acknowledgement,
		awaiting_channel_release_acknowledgement,
		awaiting_cipher_mode_command_acknowledgement,
		awaiting_mm_information_acknowledgement,
		awaiting_incoming_call_setup_acknowledgement,
		incoming_call_active,
		awaiting_traffic_assignment,
		awaiting_assignment_complete,
		awaiting_connect_acknowledgement,
		awaiting_call_release_acknowledgement,
		awaiting_release_complete,
		awaiting_sms_sapi3_establishment,
		awaiting_sms_cp_data_acknowledgement,
		awaiting_sms_handset_response,
		awaiting_sms_cp_ack_acknowledgement
	};

	void clear_pending_downlink();
	downlink_kind queue_downlink(
			downlink_kind kind, const u8 *information, unsigned length,
			u8 sapi = 0);

	required_device<nokia_gsm_network_device> m_network;
	u8 m_state = u8(state::idle);
	std::array<u8, maximum_layer3_length> m_established_layer3{};
	unsigned m_established_layer3_length = 0;
	std::array<u8, 8> m_registered_mobile_identity{};
	unsigned m_registered_mobile_identity_length = 0;
	bool m_release_completes_registration = false;
	u8 m_incoming_service = u8(incoming_service::none);
	u8 m_smart_message_part_index = 0;
	downlink_message m_pending_downlink;
};

DECLARE_DEVICE_TYPE(NOKIA_GSM_SESSION, nokia_gsm_session_device)

#endif // MAME_NOKIA_NOKIA_GSM_SESSION_H
