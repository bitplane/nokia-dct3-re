// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_GSM_NETWORK_H
#define MAME_NOKIA_NOKIA_GSM_NETWORK_H

#include "gsm_a3a8.h"
#include "gsm_a5.h"
#include "gsm_mobility.h"

#include <array>

class nokia_gsm_network_device : public device_t
{
public:
	enum class cell_profile : u8
	{
		suitable,
		barred,
		unattainable_rxlev,
		unavailable
	};

	enum class paging_profile : u8
	{
		matched,
		wrong_group,
		unmatched_identity,
		malformed_request
	};

	enum class neighbour_fault_profile : u8
	{
		none,
		malformed_system_information,
		unstable_bsic,
		forbidden_plmn,
		access_class_excluded,
		stale_measurement
	};

	enum class assignment_profile : u8
	{
		matched_request,
		mismatched_request_reference
	};

	enum class outgoing_call_outcome : u8
	{
		connect,
		busy,
		no_answer,
		service_reject
	};

	enum class mobility_profile : u8
	{
		single_cell,
		two_cell_same_lac,
		two_cell_different_lac,
		two_cell_loss_recovery,
		two_cell_persistent_loss
	};

	enum class smart_message_profile : u8
	{
		valid,
		missing_second_part,
		mismatched_reference,
		incorrect_total,
		duplicate_first_part,
		second_part_first,
		wrong_destination_port,
		truncated_udh,
		invalid_rtpl_command,
		missing_rtpl_terminator,
		stale_then_valid
	};

	enum class sms_profile : u8
	{
		valid,
		two_sequential,
		duplicate,
		invalid_originating_address,
		unsupported_dcs,
		truncated_user_data,
		inconsistent_user_data_length,
		fill_capacity
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

	void set_cell_profile(cell_profile profile);
	void set_neighbour_cell_profile(cell_profile profile);
	void set_cell(unsigned index, const gsm::mobility::cell &cell)
	{
		m_cells.set(index, cell);
	}
	void use_single_cell() { m_cells.use_single_cell(); }
	unsigned cell_count() const { return m_cells.size(); }
	const gsm::mobility::cell *cell_at(unsigned index) const
	{
		return m_cells.at(index);
	}
	const gsm::mobility::cell *cell_by_arfcn(u16 arfcn) const
	{
		return m_cells.find(arfcn);
	}
	bool cell_receivable(u16 arfcn) const;
	void set_mobility_profile(mobility_profile profile);
	void set_cell_carriers(u16 primary_arfcn, u16 neighbour_arfcn);
	void set_neighbour_bsic(u8 bsic);
	void set_neighbour_fault_profile(neighbour_fault_profile profile);
	u8 synchronization_bsic(u16 arfcn, u32 frame_number) const;
	bool synchronization_stable(u16 arfcn) const;
	bool system_information_decodable(u16 arfcn) const;
	bool configured_neighbour(u16 serving_arfcn, u16 candidate_arfcn) const;
	void stable_camp_observed();
	void neighbour_list_observed();
	void neighbour_bcch_observed(u16 arfcn);
	void downlink_signalling_failed(u16 serving_arfcn);
	void set_paging_profile(paging_profile profile) { m_paging_profile = profile; }
	void set_smart_message_profile(smart_message_profile profile)
	{
		m_smart_message_profile = profile;
	}
	void set_sms_profile(sms_profile profile) { m_sms_profile = profile; }
	void set_assignment_profile(assignment_profile profile)
	{
		m_assignment_profile = profile;
	}
	void set_outgoing_call_outcome(outgoing_call_outcome outcome)
	{
		m_outgoing_call_outcome = outcome;
	}
	void set_cipher_algorithm(gsm::a5::algorithm selected)
	{
		m_cipher_algorithm = selected;
	}
	void set_periodic_update_timer(
			gsm::mobility::periodic_update_timer timer)
	{
		m_periodic_update_timer = timer;
	}
	gsm::a5::algorithm cipher_algorithm() const { return m_cipher_algorithm; }
	outgoing_call_outcome configured_outgoing_call_outcome() const
	{
		return m_outgoing_call_outcome;
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
			const u8 *location_update_request, unsigned length,
			u16 serving_arfcn) const;
	std::array<u8, 19> authentication_request() const;
	std::array<u8, 2> authentication_reject() const;
	bool authentication_response_valid(
			const u8 *information, unsigned length) const;
	gsm::a3a8::result authentication_result() const;
	static const gsm::a3a8::block &laboratory_ki();
	std::array<u8, 3> cipher_mode_command() const;
	std::array<u8, 2> cm_service_accept() const;
	std::array<u8, 3> cm_service_reject() const;
	std::array<u8, 10> mm_information() const;
	std::array<u8, 17> incoming_call_setup() const;
	std::array<u8, 2> call_proceeding(u8 transaction) const;
	std::array<u8, 2> call_alerting(u8 transaction) const;
	std::array<u8, 2> call_connect(u8 transaction) const;
	std::array<u8, 5> call_disconnect(u8 transaction, u8 cause) const;
	std::array<u8, 8> traffic_assignment() const;
	unsigned incoming_sms_message_count() const;
	layer3_message incoming_sms_cp_data(unsigned message_index) const;
	bool incoming_sms_admissible(unsigned message_index) const;
	unsigned incoming_smart_message_part_count() const;
	layer3_message incoming_smart_message_cp_data(unsigned part_index) const;
	std::array<u8, 2> sms_cp_ack(u8 transaction) const;
	std::array<u8, 2> connect_acknowledge(u8 transaction) const;
	std::array<u8, 2> call_release(u8 transaction) const;
	std::array<u8, 2> call_release_complete(u8 transaction) const;
	std::array<u8, 3> channel_release() const;
	s8 serving_rssi(unsigned sample) const;
	s8 cell_rssi(u16 arfcn, unsigned sample) const;

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	cell_profile m_cell_profile = cell_profile::suitable;
	paging_profile m_paging_profile = paging_profile::matched;
	assignment_profile m_assignment_profile =
			assignment_profile::matched_request;
	outgoing_call_outcome m_outgoing_call_outcome =
			outgoing_call_outcome::connect;
	gsm::a5::algorithm m_cipher_algorithm = gsm::a5::algorithm::a5_0;
	gsm::mobility::periodic_update_timer m_periodic_update_timer;
	gsm::mobility::topology m_cells;
	mobility_profile m_mobility_profile = mobility_profile::single_cell;
	smart_message_profile m_smart_message_profile =
			smart_message_profile::valid;
	sms_profile m_sms_profile = sms_profile::valid;
	bool m_stable_camp_seen = false;
	bool m_neighbour_bcch_seen = false;
	bool m_primary_cell_lost = false;
	bool m_all_cells_lost = false;
	bool m_recovery_cell_available = false;
	neighbour_fault_profile m_neighbour_fault =
			neighbour_fault_profile::none;
	bool m_stale_neighbour_lost = false;

	gsm::mobility::cell resolved_cell(u16 arfcn) const;
};

DECLARE_DEVICE_TYPE(NOKIA_GSM_NETWORK, nokia_gsm_network_device)

#endif // MAME_NOKIA_NOKIA_GSM_NETWORK_H
