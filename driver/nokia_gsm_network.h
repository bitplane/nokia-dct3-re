// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_GSM_NETWORK_H
#define MAME_NOKIA_NOKIA_GSM_NETWORK_H

#include "gsm_a3a8.h"

#include <array>

class nokia_gsm_network_device : public device_t
{
public:
	enum class cell_profile : u8
	{
		suitable,
		barred,
		unattainable_rxlev
	};

	enum class paging_profile : u8
	{
		matched,
		wrong_group,
		unmatched_identity,
		malformed_request
	};

	enum class assignment_profile : u8
	{
		matched_request,
		mismatched_request_reference
	};

	static constexpr unsigned maximum_layer3_length = 176;
	static constexpr unsigned smart_message_single_part_capacity = 133;
	static constexpr unsigned smart_message_multipart_part_capacity = 128;
	static constexpr unsigned smart_message_maximum_parts = 3;
	static constexpr unsigned smart_message_ringtone_payload_length = 251;

	struct layer3_message
	{
		std::array<u8, maximum_layer3_length> data{};
		unsigned length = 0;
	};

	struct paging_group
	{
		u8 multiframe_phase;
		u8 frame_offset;

		constexpr bool operator==(const paging_group &) const = default;
	};

	nokia_gsm_network_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	void set_cell_profile(cell_profile profile) { m_cell_profile = profile; }
	void set_paging_profile(paging_profile profile) { m_paging_profile = profile; }
	void set_assignment_profile(assignment_profile profile)
	{
		m_assignment_profile = profile;
	}
	std::array<u8, 24> system_information(unsigned index, u16 serving_arfcn) const;
	std::array<u8, 24> paging_fill() const;
	std::array<u8, 24> paging_request(
			const u8 *mobile_identity, unsigned length) const;
	paging_group subscriber_paging_group(
			const u8 *mobile_identity, unsigned length) const;
	paging_group paging_request_group(
			const u8 *mobile_identity, unsigned length) const;
	bool paging_request_monitored(
			const u8 *mobile_identity, unsigned length) const
	{
		return paging_request_group(mobile_identity, length) ==
				subscriber_paging_group(mobile_identity, length);
	}
	std::array<u8, 24> immediate_assignment(
			u8 random_access, u32 frame_number, u16 serving_arfcn) const;
	std::array<u8, 17> location_update_accept(
			const u8 *location_update_request, unsigned length) const;
	std::array<u8, 19> authentication_request() const;
	std::array<u8, 2> authentication_reject() const;
	bool authentication_response_valid(
			const u8 *information, unsigned length) const;
	static const gsm::a3a8::block &laboratory_ki();
	std::array<u8, 3> cipher_mode_command() const;
	std::array<u8, 10> mm_information() const;
	std::array<u8, 17> incoming_call_setup() const;
	std::array<u8, 8> traffic_assignment() const;
	std::array<u8, 36> incoming_sms_cp_data() const;
	unsigned incoming_smart_message_part_count() const;
	layer3_message incoming_smart_message_cp_data(unsigned part_index) const;
	std::array<u8, 2> sms_cp_ack(u8 transaction) const;
	std::array<u8, 2> connect_acknowledge(u8 transaction) const;
	std::array<u8, 2> call_release(u8 transaction) const;
	std::array<u8, 3> channel_release() const;
	s8 serving_rssi(unsigned sample) const;

protected:
	virtual void device_start() override;

private:
	cell_profile m_cell_profile = cell_profile::suitable;
	paging_profile m_paging_profile = paging_profile::matched;
	assignment_profile m_assignment_profile =
			assignment_profile::matched_request;
};

DECLARE_DEVICE_TYPE(NOKIA_GSM_NETWORK, nokia_gsm_network_device)

#endif // MAME_NOKIA_NOKIA_GSM_NETWORK_H
