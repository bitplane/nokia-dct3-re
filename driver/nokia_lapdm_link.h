// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_LAPDM_LINK_H
#define MAME_NOKIA_NOKIA_LAPDM_LINK_H

#include <array>

class nokia_lapdm_link_device : public device_t
{
public:
	static constexpr unsigned frame_length = 24;
	static constexpr unsigned maximum_information_length = 20;
	static constexpr unsigned maximum_layer3_length = 251;
	// GSM 04.06 T200 on an SDCCH is one second. Express it in the radio
	// peer's exact 60/13 ms TDMA-frame clock so save/load cannot move expiry.
	static constexpr unsigned t200_control_frames = 217;
	static constexpr unsigned n200_control_attempts = 3;

	enum class uplink_result : u8
	{
		ignored,
		establish_indication,
		establish_confirmation,
		release_indication,
		downlink_acknowledgement,
		information_segment,
		information_indication
	};
	enum class expiry_kind : u8
	{
		none,
		retransmission,
		establishment_failure,
		acknowledgement_failure,
		reassembly_failure
	};
	struct expiry_result
	{
		expiry_kind kind = expiry_kind::none;
		u8 sapi = 0;
	};

	nokia_lapdm_link_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	uplink_result receive_uplink(const u8 *frame, unsigned length);
	void begin_mobile_establishment(u8 sapi);
	std::array<u8, frame_length> build_ua();
	std::array<u8, frame_length> build_release_ua();
	std::array<u8, frame_length> build_sabm_command(u8 sapi);
	std::array<u8, frame_length> build_information_frame(
			u8 sapi, const u8 *information, unsigned length,
			bool more_data = false);
	std::array<u8, frame_length> build_ui_frame(
			u8 sapi, const u8 *information, unsigned length);
	std::array<u8, frame_length> build_receive_ready(u8 sapi);
	expiry_result advance_frame();
	bool retransmission_pending() const;
	std::array<u8, frame_length> take_retransmission();

	const std::array<u8, maximum_layer3_length> &layer3_information() const
	{
		return m_layer3_information;
	}
	unsigned layer3_length() const { return m_layer3_length; }
	u8 layer3_sapi() const { return m_sapi; }
	bool layer3_more_data() const { return m_layer3_more_data; }
	bool established(u8 sapi = 0) const
	{
		return sapi < link_count && m_established[sapi];
	}
	bool awaiting_establishment(u8 sapi) const
	{
		return sapi < link_count && m_awaiting_establishment[sapi];
	}
	bool downlink_acknowledgement_pending(u8 sapi = 0) const
	{
		return sapi < link_count && m_downlink_acknowledgement_pending[sapi];
	}
	bool downlink_segmentation_pending(u8 sapi = 0) const
	{
		return sapi < link_count && m_downlink_segmentation_pending[sapi];
	}
	u8 pending_receive_sequence(u8 sapi = 0) const
	{
		return sapi < link_count ? m_pending_receive_sequence[sapi] : 0;
	}
	bool last_downlink_acknowledged() const
	{
		return m_last_downlink_acknowledged;
	}

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	static constexpr unsigned link_count = 4;
	void arm_transaction(u8 sapi, const std::array<u8, frame_length> &frame);
	void clear_transaction(u8 sapi);
	void clear_link(u8 sapi);

	std::array<u8, maximum_layer3_length> m_layer3_information{};
	unsigned m_layer3_length = 0;
	u8 m_sapi = 0;
	bool m_layer3_more_data = false;
	bool m_uplink_segmentation_active = false;
	std::array<u8, link_count> m_downlink_send_sequence{};
	std::array<u8, link_count> m_next_uplink_receive_sequence{};
	std::array<u8, link_count> m_pending_receive_sequence{};
	std::array<bool, link_count> m_established{};
	std::array<bool, link_count> m_mobile_establishment_expected{};
	std::array<bool, link_count> m_awaiting_establishment{};
	std::array<bool, link_count> m_downlink_segmentation_pending{};
	std::array<bool, link_count> m_downlink_acknowledgement_pending{};
	std::array<bool, link_count> m_uplink_final_response_pending{};
	std::array<u16, link_count> m_transaction_frames{};
	std::array<u8, link_count> m_transaction_attempts{};
	std::array<bool, link_count> m_retransmission_pending{};
	std::array<std::array<u8, frame_length>, link_count> m_last_downlink_frame{};
	u16 m_reassembly_frames = 0;
	bool m_last_downlink_acknowledged = false;
};

DECLARE_DEVICE_TYPE(NOKIA_LAPDM_LINK, nokia_lapdm_link_device)

#endif // MAME_NOKIA_NOKIA_LAPDM_LINK_H
