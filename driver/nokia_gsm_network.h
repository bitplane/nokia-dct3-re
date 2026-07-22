// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_GSM_NETWORK_H
#define MAME_NOKIA_NOKIA_GSM_NETWORK_H

#include <array>

class nokia_gsm_network_device : public device_t
{
public:
	nokia_gsm_network_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	const std::array<u8, 24> &system_information(unsigned index) const;
	std::array<u8, 24> immediate_assignment(u8 random_access, u32 frame_number) const;
	std::array<u8, 17> location_update_accept(const std::array<u8, 8> &mobile_identity) const;
	std::array<u8, 3> channel_release() const;
	s8 serving_rssi(unsigned sample) const;

protected:
	virtual void device_start() override;
};

DECLARE_DEVICE_TYPE(NOKIA_GSM_NETWORK, nokia_gsm_network_device)

#endif // MAME_NOKIA_NOKIA_GSM_NETWORK_H
