// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "nokia_gsm_session.h"

DEFINE_DEVICE_TYPE(NOKIA_GSM_SESSION, nokia_gsm_session_device,
		"nokia_gsm_session", "Nokia DCT3 GSM Layer 3 session")

nokia_gsm_session_device::nokia_gsm_session_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_GSM_SESSION, tag, owner, clock),
	m_network(*this, "^gsm_network"),
	m_call_alerting_output(*this, "nokia_gsm_call_alerting")
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
	save_item(NAME(m_call_alerting));
	save_item(NAME(m_traffic_assignment_issued));
	save_item(NAME(m_mobile_originated_call));
	save_item(NAME(m_call_transaction));
	save_item(NAME(m_incoming_service));
	save_item(NAME(m_smart_message_part_index));
	save_item(NAME(m_pending_downlink.kind));
	save_item(NAME(m_pending_downlink.sapi));
	save_item(NAME(m_pending_downlink.length));
	save_item(NAME(m_pending_downlink.data));
	machine().save().register_postload(
			save_prepost_delegate(
				FUNC(nokia_gsm_session_device::publish_call_alerting_output),
				this));
}

void nokia_gsm_session_device::device_reset()
{
	m_state = u8(state::idle);
	m_established_layer3.fill(0);
	m_established_layer3_length = 0;
	m_registered_mobile_identity.fill(0);
	m_registered_mobile_identity_length = 0;
	m_release_completes_registration = false;
	m_call_alerting = false;
	m_traffic_assignment_issued = false;
	m_mobile_originated_call = false;
	m_call_transaction = 0;
	publish_call_alerting_output();
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
	const bool cm_service_request =
			m_state == u8(state::idle) &&
			(information[0] & 0x0f) == 0x05 &&
			(information[1] & 0x3f) == 0x24 &&
			length >= 6 &&
			(information[2] & 0x0f) == 0x01;
	if (!location_update_request && !paging_response && !cm_service_request)
		return false;
	if (cm_service_request)
	{
		// GSM 04.08 9.2.9: accept only a structurally complete mobile-
		// originating call request. Other CM service types belong to their own
		// sessions and must not accidentally enter call control.
		const unsigned classmark_length = information[3];
		const unsigned identity_length_offset = 4 + classmark_length;
		if (identity_length_offset >= length)
			return false;
		const unsigned identity_length = information[identity_length_offset];
		if (identity_length == 0 || identity_length > 8 ||
				identity_length_offset + 1 + identity_length > length)
			return false;
	}

	m_established_layer3.fill(0);
	std::copy_n(information, length, m_established_layer3.begin());
	m_established_layer3_length = length;
	clear_pending_downlink();
	m_release_completes_registration = location_update_request;
	m_mobile_originated_call = cm_service_request;
	m_call_transaction = 0;
	m_traffic_assignment_issued = false;
	m_state = u8(paging_response ?
			state::awaiting_paging_contention_resolution :
			state::awaiting_contention_resolution);
	return true;
}

bool nokia_gsm_session_device::queue_incoming_page(incoming_service service)
{
	if (m_state == u8(state::awaiting_paging_response))
		return true;
	if (m_state != u8(state::idle) || m_registered_mobile_identity_length != 8)
		return false;
	m_incoming_service = u8(service);
	m_traffic_assignment_issued = false;
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
			// Exercise the firmware's real DSP cipher-control publication while
			// keeping this laboratory connection explicitly unciphered.
			const auto information = m_network->cipher_mode_command();
			m_state = u8(state::awaiting_cipher_mode_command_acknowledgement);
			return queue_downlink(downlink_kind::cipher_mode_command,
					information.data(), information.size());
		}
		const auto release = m_network->channel_release();
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
	}

	if (m_state != u8(state::awaiting_contention_resolution))
		return downlink_kind::none;

	if ((m_established_layer3[0] & 0x0f) == 0x05 &&
			(m_established_layer3[1] & 0x3f) == 0x24)
	{
		const auto accept = m_network->cm_service_accept();
		m_state = u8(state::awaiting_cm_service_accept_acknowledgement);
		return queue_downlink(downlink_kind::cm_service_accept,
				accept.data(), accept.size());
	}

	if (m_authentication_required)
	{
		const auto request = m_network->authentication_request();
		m_state = u8(state::awaiting_authentication_request_acknowledgement);
		return queue_downlink(downlink_kind::authentication_request,
				request.data(), request.size());
	}

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
	if (m_state == u8(state::awaiting_cm_service_accept_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::cm_service_accept))
	{
		clear_pending_downlink();
		m_state = u8(state::awaiting_outgoing_call_setup);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_call_proceeding_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::call_proceeding))
	{
		m_traffic_assignment_issued = true;
		const auto assignment = m_network->traffic_assignment();
		m_state = u8(state::awaiting_traffic_assignment);
		return queue_downlink(downlink_kind::traffic_assignment,
				assignment.data(), assignment.size());
	}

	if (m_state == u8(state::awaiting_call_alerting_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::call_alerting))
	{
		const auto connect = m_network->call_connect(m_call_transaction);
		m_state = u8(state::awaiting_outgoing_connect_acknowledgement);
		return queue_downlink(downlink_kind::call_connect,
				connect.data(), connect.size());
	}

	if (m_state == u8(state::awaiting_outgoing_connect_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::call_connect))
	{
		clear_pending_downlink();
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_authentication_request_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::authentication_request))
	{
		clear_pending_downlink();
		m_state = u8(state::awaiting_authentication_response);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_authentication_reject_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::authentication_reject))
	{
		const auto release = m_network->channel_release();
		m_release_completes_registration = false;
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
	}

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

	if (m_state == u8(state::awaiting_traffic_assignment) &&
			m_pending_downlink.kind == u8(downlink_kind::traffic_assignment))
	{
		// The mobile may acknowledge this I frame before locally releasing the
		// old SDCCH. Assignment completion itself belongs to the new link.
		clear_pending_downlink();
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_cipher_mode_command_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::cipher_mode_command))
	{
		// Supply deterministic network time before entering call control or
		// SAPI-3 SMS, matching the ordering expected by this firmware.
		const auto information = m_network->mm_information();
		m_state = u8(state::awaiting_mm_information_acknowledgement);
		return queue_downlink(downlink_kind::mm_information,
				information.data(), information.size());
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
	if (sapi == 0 && m_state == u8(state::awaiting_outgoing_call_setup) &&
			protocol_discriminator == 0x03 && message_type == 0x05)
	{
		bool speech_bearer = false;
		bool called_party = false;
		for (unsigned offset = 2; offset < length;)
		{
			const u8 identifier = information[offset];
			if (BIT(identifier, 7))
			{
				++offset;
				continue;
			}
			if (offset + 1 >= length)
				return downlink_kind::none;
			const unsigned value_length = information[offset + 1];
			if (offset + 2 + value_length > length)
				return downlink_kind::none;
			if (identifier == 0x04 && value_length >= 1)
				speech_bearer = (information[offset + 2] & 0x07) == 0x00;
			else if (identifier == 0x5e && value_length >= 2)
				called_party = true;
			offset += 2 + value_length;
		}
		if (!speech_bearer || !called_party || BIT(information[0], 7))
			return downlink_kind::none;
		m_call_transaction = information[0];
		const auto proceeding =
				m_network->call_proceeding(m_call_transaction);
		m_state = u8(state::awaiting_call_proceeding_acknowledgement);
		return queue_downlink(downlink_kind::call_proceeding,
				proceeding.data(), proceeding.size());
	}

	if (sapi == 0 && m_state == u8(state::awaiting_authentication_response))
	{
		if (m_network->authentication_response_valid(information, length))
		{
			const auto accept = m_network->location_update_accept(
					m_established_layer3.data(),
					m_established_layer3_length);
			m_state = u8(state::awaiting_location_update_accept_acknowledgement);
			return queue_downlink(downlink_kind::location_update_accept,
					accept.data(), accept.size());
		}

		const auto reject = m_network->authentication_reject();
		m_state = u8(state::awaiting_authentication_reject_acknowledgement);
		return queue_downlink(downlink_kind::authentication_reject,
				reject.data(), reject.size());
	}

	if (sapi == 0 &&
			(m_state == u8(state::incoming_call_active) ||
			m_state == u8(state::awaiting_traffic_assignment) ||
			m_state == u8(state::awaiting_assignment_complete) ||
			m_state == u8(state::awaiting_incoming_call_setup_acknowledgement)) &&
			protocol_discriminator == 0x03)
	{
		if (message_type == 0x08)
		{
			// Some handsets repeat Call Confirmed after the dedicated channel
			// assignment has completed.  It is still the same CC transaction,
			// so it must not start a second RR assignment.
			if (m_state != u8(state::incoming_call_active) ||
					m_traffic_assignment_issued)
				return downlink_kind::none;
			m_traffic_assignment_issued = true;
			const auto assignment = m_network->traffic_assignment();
			m_state = u8(state::awaiting_traffic_assignment);
			return queue_downlink(downlink_kind::traffic_assignment,
					assignment.data(), assignment.size());
		}
		if (message_type == 0x01)
		{
			m_call_alerting = true;
			publish_call_alerting_output();
			return downlink_kind::none;
		}
		if (message_type == 0x07)
		{
			m_call_alerting = false;
			publish_call_alerting_output();
			const auto acknowledge =
					m_network->connect_acknowledge(information[0]);
			m_state = u8(state::awaiting_connect_acknowledgement);
			return queue_downlink(downlink_kind::connect_acknowledge,
					acknowledge.data(), acknowledge.size());
		}
		if (message_type == 0x25)
		{
			m_call_alerting = false;
			publish_call_alerting_output();
			const auto release = m_network->call_release(information[0]);
			m_state = u8(state::awaiting_call_release_acknowledgement);
			return queue_downlink(downlink_kind::call_release,
					release.data(), release.size());
		}
	}

	if (sapi == 0 &&
			m_state == u8(state::awaiting_assignment_complete) &&
			protocol_discriminator == 0x06 && message_type == 0x29)
	{
		if (m_mobile_originated_call)
		{
			m_call_alerting = true;
			publish_call_alerting_output();
			const auto alerting =
					m_network->call_alerting(m_call_transaction);
			m_state = u8(state::awaiting_call_alerting_acknowledgement);
			return queue_downlink(downlink_kind::call_alerting,
					alerting.data(), alerting.size());
		}
		m_state = u8(state::incoming_call_active);
		return downlink_kind::none;
	}

	if (sapi == 0 &&
			m_state == u8(state::awaiting_outgoing_connect_acknowledgement) &&
			protocol_discriminator == 0x03 && message_type == 0x0f)
	{
		clear_pending_downlink();
		m_call_alerting = false;
		publish_call_alerting_output();
		m_state = u8(state::incoming_call_active);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_release_complete) &&
			protocol_discriminator == 0x03 &&
			message_type == 0x2a)
	{
		m_established_layer3.fill(0);
		m_established_layer3_length = 0;
		m_release_completes_registration = false;
		m_call_alerting = false;
		m_traffic_assignment_issued = false;
		m_mobile_originated_call = false;
		m_call_transaction = 0;
		publish_call_alerting_output();
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

void nokia_gsm_session_device::publish_call_alerting_output()
{
	m_call_alerting_output = m_call_alerting;
}

bool nokia_gsm_session_device::begin_traffic_assignment()
{
	if (m_state != u8(state::awaiting_traffic_assignment))
		return false;
	clear_pending_downlink();
	m_state = u8(state::awaiting_assignment_complete);
	return true;
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
