// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "nokia_lapdm_link.h"

DEFINE_DEVICE_TYPE(NOKIA_LAPDM_LINK, nokia_lapdm_link_device,
		"nokia_lapdm_link", "Nokia DCT3 LAPDm link")

nokia_lapdm_link_device::nokia_lapdm_link_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_LAPDM_LINK, tag, owner, clock)
{
}

void nokia_lapdm_link_device::device_start()
{
	save_item(NAME(m_layer3_information));
	save_item(NAME(m_layer3_length));
	save_item(NAME(m_sapi));
	save_item(NAME(m_layer3_more_data));
	save_item(NAME(m_downlink_send_sequence));
	save_item(NAME(m_next_uplink_receive_sequence));
	save_item(NAME(m_pending_receive_sequence));
	save_item(NAME(m_established));
	save_item(NAME(m_awaiting_establishment));
	save_item(NAME(m_downlink_segmentation_pending));
	save_item(NAME(m_downlink_acknowledgement_pending));
}

void nokia_lapdm_link_device::device_reset()
{
	m_layer3_information.fill(0);
	m_layer3_length = 0;
	m_sapi = 0;
	m_layer3_more_data = false;
	m_downlink_send_sequence.fill(0);
	m_next_uplink_receive_sequence.fill(0);
	m_pending_receive_sequence.fill(0);
	m_established.fill(false);
	m_awaiting_establishment.fill(false);
	m_downlink_segmentation_pending.fill(false);
	m_downlink_acknowledgement_pending.fill(false);
	m_last_downlink_acknowledged = false;
}

nokia_lapdm_link_device::uplink_result nokia_lapdm_link_device::receive_uplink(
		const u8 *frame, unsigned length)
{
	m_last_downlink_acknowledged = false;
	if (length < 3 || !(frame[0] & 0x01) || !(frame[2] & 0x01))
		return uplink_result::ignored;

	const u8 sapi = (frame[0] >> 2) & 0x07;
	const u8 control = frame[1];
	const unsigned information_length = frame[2] >> 2;
	if (sapi >= link_count)
		return uplink_result::ignored;

	if ((control & 0xef) == 0x2f)
	{
		if ((frame[2] & 0x02) || information_length == 0 ||
				information_length > maximum_information_length ||
				length < 3 + information_length)
			return uplink_result::ignored;

		// Registration has organically established only the RR/MM link on SAPI 0.
		// SAPI 3 becomes admissible when an SMS transaction reaches this boundary.
		if (sapi != 0)
			return uplink_result::ignored;
		m_sapi = sapi;
		m_layer3_information.fill(0);
		std::copy_n(frame + 3, information_length, m_layer3_information.begin());
		m_layer3_length = information_length;
		m_layer3_more_data = false;
		m_downlink_send_sequence[sapi] = 0;
		m_next_uplink_receive_sequence[sapi] = 0;
		m_pending_receive_sequence[sapi] = 0;
		m_established[sapi] = false;
		m_awaiting_establishment[sapi] = false;
		m_downlink_segmentation_pending[sapi] = false;
		m_downlink_acknowledgement_pending[sapi] = false;
		return uplink_result::establish_indication;
	}

	if (control == 0x73 && frame[2] == 0x01 &&
			m_awaiting_establishment[sapi])
	{
		m_sapi = sapi;
		m_established[sapi] = true;
		m_awaiting_establishment[sapi] = false;
		return uplink_result::establish_confirmation;
	}

	if (!m_established[sapi])
		return uplink_result::ignored;

	const bool receive_ready = (control & 0x0f) == 0x01 &&
			frame[2] == 0x01;
	const bool information_frame = !(control & 0x01) &&
			!(frame[2] & 0x02) &&
			information_length <= maximum_information_length &&
			length >= 3 + information_length;
	if (!receive_ready && !information_frame)
		return uplink_result::ignored;

	const u8 receive_sequence = (control >> 5) & 0x07;
	const bool acknowledges_downlink =
			m_downlink_acknowledgement_pending[sapi] &&
			receive_sequence == m_pending_receive_sequence[sapi];

	bool accepted_information = false;
	if (information_frame)
	{
		const u8 send_sequence = (control >> 1) & 0x07;
		if (send_sequence == m_next_uplink_receive_sequence[sapi])
		{
			m_sapi = sapi;
			m_layer3_information.fill(0);
			std::copy_n(frame + 3, information_length,
					m_layer3_information.begin());
			m_layer3_length = information_length;
			m_layer3_more_data = bool(frame[2] & 0x02);
			m_next_uplink_receive_sequence[sapi] =
					(m_next_uplink_receive_sequence[sapi] + 1) & 0x07;
			accepted_information = true;
		}
	}

	if (acknowledges_downlink)
	{
		m_sapi = sapi;
		m_downlink_acknowledgement_pending[sapi] = false;
		m_last_downlink_acknowledged = true;
	}

	if (accepted_information)
		return uplink_result::information_indication;
	return acknowledges_downlink ?
			uplink_result::downlink_acknowledgement :
			uplink_result::ignored;
}

std::array<u8, nokia_lapdm_link_device::frame_length>
nokia_lapdm_link_device::build_ua()
{
	std::array<u8, frame_length> frame;
	frame.fill(0x2b);
	frame[0] = (m_sapi << 2) | 0x01;
	frame[1] = 0x73; // UA response with final bit set
	frame[2] = (m_layer3_length << 2) | 0x01;
	std::copy_n(m_layer3_information.begin(), m_layer3_length, frame.begin() + 3);
	m_established[m_sapi] = true;
	m_downlink_segmentation_pending[m_sapi] = false;
	m_downlink_acknowledgement_pending[m_sapi] = false;
	return frame;
}

std::array<u8, nokia_lapdm_link_device::frame_length>
nokia_lapdm_link_device::build_sabm_command(u8 sapi)
{
	std::array<u8, frame_length> frame;
	frame.fill(0x2b);
	if (sapi >= link_count)
		return frame;
	m_downlink_send_sequence[sapi] = 0;
	m_next_uplink_receive_sequence[sapi] = 0;
	m_pending_receive_sequence[sapi] = 0;
	m_established[sapi] = false;
	m_awaiting_establishment[sapi] = true;
	m_downlink_segmentation_pending[sapi] = false;
	m_downlink_acknowledgement_pending[sapi] = false;
	frame[0] = (sapi << 2) | 0x03;
	frame[1] = 0x3f;
	frame[2] = 0x01;
	return frame;
}

std::array<u8, nokia_lapdm_link_device::frame_length>
nokia_lapdm_link_device::build_receive_ready(u8 sapi)
{
	std::array<u8, frame_length> frame;
	frame.fill(0x2b);
	if (sapi >= link_count || !m_established[sapi])
		return frame;
	frame[0] = (sapi << 2) | 0x01;
	frame[1] = (m_next_uplink_receive_sequence[sapi] << 5) | 0x01;
	frame[2] = 0x01;
	return frame;
}

std::array<u8, nokia_lapdm_link_device::frame_length>
nokia_lapdm_link_device::build_information_frame(
		u8 sapi, const u8 *information, unsigned length, bool more_data)
{
	std::array<u8, frame_length> frame;
	frame.fill(0x2b);
	if (sapi >= link_count || !m_established[sapi] ||
			m_downlink_acknowledgement_pending[sapi] ||
			length > maximum_information_length)
		return frame;

	frame[0] = (sapi << 2) | 0x03;
	frame[1] = (m_next_uplink_receive_sequence[sapi] << 5) |
			(m_downlink_send_sequence[sapi] << 1);
	frame[2] = (length << 2) | (more_data ? 0x03 : 0x01);
	std::copy_n(information, length, frame.begin() + 3);
	m_downlink_send_sequence[sapi] =
			(m_downlink_send_sequence[sapi] + 1) & 0x07;
	m_pending_receive_sequence[sapi] = m_downlink_send_sequence[sapi];
	m_downlink_segmentation_pending[sapi] = more_data;
	m_downlink_acknowledgement_pending[sapi] = true;
	return frame;
}
