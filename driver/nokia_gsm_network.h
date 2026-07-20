// license:BSD-3-Clause
#pragma once

#include "emu.h"

#include <array>

class nokia_gsm_network_device : public device_t
{
public:
	nokia_gsm_network_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	const std::array<u8, 24> &system_information(unsigned index) const;

protected:
	virtual void device_start() override;
};

DECLARE_DEVICE_TYPE(NOKIA_GSM_NETWORK, nokia_gsm_network_device)
