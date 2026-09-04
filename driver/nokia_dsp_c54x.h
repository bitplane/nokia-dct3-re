// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_NOKIA_DSP_C54X_H
#define MAME_NOKIA_NOKIA_DSP_C54X_H

#include "cpu/tms320c54x/tms320c54x.h"
#include "nokia_cobba.h"
#include "nokia_dsp_backend.h"
#include "nokia_dspif.h"

#include <array>

class nokia_dsp_c54x_device : public device_t, public nokia_dsp_backend_interface
{
public:
	nokia_dsp_c54x_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	virtual void tx_commit_w(int state) override { }
	virtual void service_pending_w(int state) override { }
	virtual void doorbell_w(int state) override;
	virtual void reset_line_w(int released) override;
	virtual void shared_002_write_w(int state) override { }
	virtual void shared_006_write_w(int state) override { }
	virtual void shared_0fe_read_w(int state) override { }
	virtual void shared_0fe_write_w(int state) override { }
	virtual void shared_100_read_w(int state) override { }
	virtual void shared_100_write_w(int state) override { }
	virtual void mcu_shared_write(u16 byte_offset) override;

	virtual u32 tone_frequency1() const override { return 0; }
	virtual u32 tone_frequency2() const override { return 0; }
	virtual u16 tone_amplitude() const override { return 0; }

protected:
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	void program_map(address_map &map) ATTR_COLD;
	void data_map(address_map &map) ATTR_COLD;
	void io_map(address_map &map) ATTR_COLD;
	u16 program_r(offs_t offset);
	void program_w(offs_t offset, u16 data);
	u16 data_r(offs_t offset);
	void data_w(offs_t offset, u16 data);
	u16 io_r(offs_t offset);
	void io_w(offs_t offset, u16 data);
	bool overlay_address(u16 address) const;
	u16 host_request() const;
	void acknowledge_host_command(u16 mask);
	void update_host_command_line();

	required_device<tms320c54x_device> m_cpu;
	required_device<nokia_dspif_device> m_transport;
	required_device<nokia_cobba_device> m_cobba;
	required_region_ptr<u16> m_program_rom;
	required_region_ptr<u16> m_data_rom;
	std::array<u16, 0x10000> m_program{};
	std::array<u16, 0x10000> m_data{};
	std::array<u16, 0x100> m_io{};
	bool m_host_command_line = false;
};

DECLARE_DEVICE_TYPE(NOKIA_DSP_C54X, nokia_dsp_c54x_device)

#endif // MAME_NOKIA_NOKIA_DSP_C54X_H
