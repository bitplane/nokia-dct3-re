// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "nokia_gsm_session.h"

DEFINE_DEVICE_TYPE(NOKIA_GSM_SESSION, nokia_gsm_session_device,
		"nokia_gsm_session", "Nokia DCT3 GSM Layer 3 session")

nokia_gsm_session_device::nokia_gsm_session_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_GSM_SESSION, tag, owner, clock),
	m_network(*this, "^gsm_network")
{
}

void nokia_gsm_session_device::device_start()
{
	save_item(NAME(m_state));
	save_item(NAME(m_established_layer3));
	save_item(NAME(m_established_layer3_length));
	save_item(NAME(m_registered_mobile_identity));
	save_item(NAME(m_registered_mobile_identity_length));
	save_item(NAME(m_release_completes_registration));
	save_item(NAME(m_incoming_service));
	save_item(NAME(m_smart_message_part_index));
	save_item(NAME(m_pending_downlink.kind));
	save_item(NAME(m_pending_downlink.sapi));
	save_item(NAME(m_pending_downlink.length));
	save_item(NAME(m_pending_downlink.data));
}

void nokia_gsm_session_device::device_reset()
{
	m_state = u8(state::idle);
	m_established_layer3.fill(0);
	m_established_layer3_length = 0;
	m_registered_mobile_identity.fill(0);
	m_registered_mobile_identity_length = 0;
	m_release_completes_registration = false;
	m_incoming_service = u8(incoming_service::none);
	m_smart_message_part_index = 0;
	clear_pending_downlink();
}

bool nokia_gsm_session_device::establish_layer3(
		const u8 *information, unsigned length)
{
	if (length < 2 || length > maximum_layer3_length)
		return false;

	const bool location_update_request =
			m_state == u8(state::idle) &&
			(information[0] & 0x0f) == 0x05 &&
			(information[1] & 0x3f) == 0x08;
	const bool paging_response =
			m_state == u8(state::awaiting_paging_response) &&
			(information[0] & 0x0f) == 0x06 &&
			(information[1] & 0x3f) == 0x27;
	if (!location_update_request && !paging_response)
		return false;

	m_established_layer3.fill(0);
	std::copy_n(information, length, m_established_layer3.begin());
	m_established_layer3_length = length;
	clear_pending_downlink();
	m_release_completes_registration = location_update_request;
	m_state = u8(location_update_request ?
			state::awaiting_contention_resolution :
			state::awaiting_paging_contention_resolution);
	return true;
}

bool nokia_gsm_session_device::queue_incoming_page(incoming_service service)
{
	if (m_state == u8(state::awaiting_paging_response))
		return true;
	if (m_state != u8(state::idle) || m_registered_mobile_identity_length != 8)
		return false;
	m_incoming_service = u8(service);
	if (service != incoming_service::smart_message)
		m_smart_message_part_index = 0;
	m_state = u8(state::awaiting_paging_response);
	return true;
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::contention_resolution_delivered()
{
	if (m_state == u8(state::awaiting_paging_contention_resolution))
	{
		if (m_incoming_service != u8(incoming_service::none))
		{
			// This laboratory connection remains unciphered. Supply the network
			// time on the newly established MM connection before entering call
			// control, matching the ordering expected by this firmware.
			const auto information = m_network->mm_information();
			m_state = u8(state::awaiting_mm_information_acknowledgement);
			return queue_downlink(downlink_kind::mm_information,
					information.data(), information.size());
		}
		const auto release = m_network->channel_release();
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
	}

	if (m_state != u8(state::awaiting_contention_resolution))
		return downlink_kind::none;

	const auto accept = m_network->location_update_accept(
			m_established_layer3.data(), m_established_layer3_length);
	m_state = u8(state::awaiting_location_update_accept_acknowledgement);
	return queue_downlink(downlink_kind::location_update_accept,
			accept.data(), accept.size());
}

std::array<u8, 24> nokia_gsm_session_device::paging_request() const
{
	return m_network->paging_request(
			m_registered_mobile_identity.data(),
			m_registered_mobile_identity_length);
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::downlink_acknowledged()
{
	if (m_state == u8(state::awaiting_location_update_accept_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::location_update_accept))
	{
		const auto release = m_network->channel_release();
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
	}

	if (m_state == u8(state::awaiting_incoming_call_setup_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::incoming_call_setup))
	{
		clear_pending_downlink();
		m_state = u8(state::incoming_call_active);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_mm_information_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::mm_information))
	{
		if (m_incoming_service == u8(incoming_service::call))
		{
			const auto setup = m_network->incoming_call_setup();
			m_state = u8(state::awaiting_incoming_call_setup_acknowledgement);
			return queue_downlink(downlink_kind::incoming_call_setup,
					setup.data(), setup.size());
		}
		if (m_incoming_service == u8(incoming_service::sms) ||
				m_incoming_service == u8(incoming_service::smart_message))
		{
			clear_pending_downlink();
			m_state = u8(state::awaiting_sms_sapi3_establishment);
			return queue_downlink(downlink_kind::sapi3_establishment,
					nullptr, 0, 3);
		}
	}

	if (m_state == u8(state::awaiting_sms_sapi3_establishment) &&
			m_pending_downlink.kind == u8(downlink_kind::sapi3_establishment))
	{
		m_state = u8(state::awaiting_sms_cp_data_acknowledgement);
		if (m_incoming_service == u8(incoming_service::smart_message))
		{
			const auto data = m_network->incoming_smart_message_cp_data(
					m_smart_message_part_index);
			return queue_downlink(downlink_kind::incoming_sms_cp_data,
					data.data.data(), data.length, 3);
		}
		const auto data = m_network->incoming_sms_cp_data();
		return queue_downlink(downlink_kind::incoming_sms_cp_data,
				data.data(), data.size(), 3);
	}

	if (m_state == u8(state::awaiting_sms_cp_data_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::incoming_sms_cp_data))
	{
		clear_pending_downlink();
		m_state = u8(state::awaiting_sms_handset_response);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_sms_cp_ack_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::sms_cp_ack))
	{
		const auto release = m_network->channel_release();
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
	}

	if (m_state == u8(state::awaiting_connect_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::connect_acknowledge))
	{
		clear_pending_downlink();
		m_state = u8(state::incoming_call_active);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_call_release_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::call_release))
	{
		// Close the dedicated RR channel after RELEASE is acknowledged. The
		// handset emits CC Release Complete while that release is in flight.
		const auto release = m_network->channel_release();
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
	}

	if (m_state == u8(state::awaiting_channel_release_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::channel_release))
	{
		if (m_release_completes_registration &&
				m_established_layer3_length >= 18 &&
				m_established_layer3[9] == 8)
		{
			std::copy_n(m_established_layer3.begin() + 10,
					m_registered_mobile_identity.size(),
					m_registered_mobile_identity.begin());
			m_registered_mobile_identity_length =
					m_registered_mobile_identity.size();
		}
		clear_pending_downlink();
		const bool more_smart_message_parts =
				m_incoming_service == u8(incoming_service::smart_message) &&
				m_smart_message_part_index + 1 <
						m_network->incoming_smart_message_part_count();
		if (more_smart_message_parts)
			++m_smart_message_part_index;
		if (m_incoming_service == u8(incoming_service::call))
		{
			m_state = u8(state::awaiting_release_complete);
			return downlink_kind::none;
		}
		m_established_layer3.fill(0);
		m_established_layer3_length = 0;
		m_release_completes_registration = false;
		if (!more_smart_message_parts)
		{
			m_incoming_service = u8(incoming_service::none);
			m_smart_message_part_index = 0;
		}
		m_state = u8(state::idle);
		return downlink_kind::release_complete;
	}

	return downlink_kind::none;
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::receive_layer3(
		u8 sapi, const u8 *information, unsigned length)
{
	if (length < 2)
		return downlink_kind::none;

	const u8 protocol_discriminator = information[0] & 0x0f;
	const u8 message_type = information[1] & 0x3f;
	if (sapi == 0 &&
			(m_state == u8(state::incoming_call_active) ||
			m_state == u8(state::awaiting_incoming_call_setup_acknowledgement)) &&
			protocol_discriminator == 0x03)
	{
		if (message_type == 0x08 || message_type == 0x01)
			return downlink_kind::none;
		if (message_type == 0x07)
		{
			const auto acknowledge =
					m_network->connect_acknowledge(information[0]);
			m_state = u8(state::awaiting_connect_acknowledgement);
			return queue_downlink(downlink_kind::connect_acknowledge,
					acknowledge.data(), acknowledge.size());
		}
		if (message_type == 0x25)
		{
			const auto release = m_network->call_release(information[0]);
			m_state = u8(state::awaiting_call_release_acknowledgement);
			return queue_downlink(downlink_kind::call_release,
					release.data(), release.size());
		}
	}

	if (m_state == u8(state::awaiting_release_complete) &&
			protocol_discriminator == 0x03 &&
			message_type == 0x2a)
	{
		m_established_layer3.fill(0);
		m_established_layer3_length = 0;
		m_release_completes_registration = false;
		m_incoming_service = u8(incoming_service::none);
		m_state = u8(state::idle);
		return downlink_kind::release_complete;
	}

	if (sapi == 3 && m_state == u8(state::awaiting_sms_handset_response) &&
			protocol_discriminator == 0x09)
	{
		// CP-ACK confirms CP-DATA delivery. The following CP-DATA carries the
		// RP-ACK for the current multipart message reference.
		if (message_type == 0x04)
			return downlink_kind::none;
		if (message_type == 0x01 && length >= 6 &&
				information[2] == 0x01 && information[3] == 0x02 &&
				information[4] == 0x02 &&
				information[5] == 0x40 + m_smart_message_part_index)
		{
			const auto acknowledge = m_network->sms_cp_ack(information[0]);
			m_state = u8(state::awaiting_sms_cp_ack_acknowledgement);
			return queue_downlink(downlink_kind::sms_cp_ack,
					acknowledge.data(), acknowledge.size(), 3);
		}
	}

	return downlink_kind::none;
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::queue_downlink(
		downlink_kind kind, const u8 *information, unsigned length, u8 sapi)
{
	if (length > m_pending_downlink.data.size())
		return downlink_kind::none;
	clear_pending_downlink();
	m_pending_downlink.kind = u8(kind);
	m_pending_downlink.sapi = sapi;
	m_pending_downlink.length = length;
	if (length != 0)
		std::copy_n(information, length, m_pending_downlink.data.begin());
	return kind;
}

void nokia_gsm_session_device::clear_pending_downlink()
{
	m_pending_downlink.kind = u8(downlink_kind::none);
	m_pending_downlink.sapi = 0;
	m_pending_downlink.length = 0;
	m_pending_downlink.data.fill(0);
}
