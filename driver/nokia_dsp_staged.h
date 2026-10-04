// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_NOKIA_DSP_STAGED_H
#define MAME_NOKIA_NOKIA_DSP_STAGED_H

#include "cpu/tms320c54x/tms320c54x.h"
#include "nokia_cobba.h"
#include "nokia_dspif.h"

// Research composition: execute MCU-uploaded code without inventing a mask ROM.
class nokia_dsp_staged_device : public device_t
{
public:
	nokia_dsp_staged_device(const machine_config &config, const char *tag, device_t *owner, u32 clock);
	void reset_line_w(int released);
	bool active() const { return m_active; }

protected:
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	void program_map(address_map &map);
	void data_map(address_map &map);
	void io_map(address_map &map);
	u16 program_r(offs_t offset);
	void program_w(offs_t offset, u16 data);
	u16 data_r(offs_t offset);
	void data_w(offs_t offset, u16 data);
	u16 io_r(offs_t offset);
	void io_w(offs_t offset, u16 data);
	TIMER_CALLBACK_MEMBER(check_execution);
	TIMER_CALLBACK_MEMBER(begin_execution);
	required_device<tms320c54x_device> m_cpu;
	required_device<nokia_dspif_device> m_transport;
	required_device<nokia_cobba_device> m_cobba;
	std::array<u16, 0x10000> m_data{};
	std::array<u16, 0x10> m_control{};
	emu_timer *m_guard = nullptr;
	bool m_active = false;
	bool m_published = false;
};

DECLARE_DEVICE_TYPE(NOKIA_DSP_STAGED, nokia_dsp_staged_device)

#endif // MAME_NOKIA_NOKIA_DSP_STAGED_H
