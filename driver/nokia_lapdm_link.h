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
	std::array<u8, frame_length> build_receive_ready(u8 sapi);

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
	bool m_last_downlink_acknowledged = false;
};

DECLARE_DEVICE_TYPE(NOKIA_LAPDM_LINK, nokia_lapdm_link_device)

#endif // MAME_NOKIA_NOKIA_LAPDM_LINK_H
