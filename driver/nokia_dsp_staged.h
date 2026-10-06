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
	bool owns_transport() const { return m_active && !(m_runtime_hle && m_observation_halted); }
	void set_program_fragment(u32 flash_offset) { m_fragment_offset = flash_offset; }
	void set_loader2_source(u32 flash_offset, u16 words = 613)
	{
		m_loader2_offset = flash_offset;
		m_loader2_words = words;
	}
	void set_verifier_source_end(u16 end) { m_verifier_source_end = end; }
	void set_loader_control_address(u16 address) { m_loader_control_address = address; }
	void set_cycle_guard_for_loader(bool enable) { m_cycle_guard_for_loader = enable; }
	void set_observe_after_missing_code(bool enable) { m_observe_after_missing_code = enable; }
	void set_runtime_hle_after_loader(bool enable) { m_runtime_hle = enable; }

protected:
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	void program_map(address_map &map);
	void data_map(address_map &map);
	void io_map(address_map &map);
	u16 program_r(offs_t offset);
	void verify_loader2();
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
	required_region_ptr<u16> m_flash;
	std::array<u16, 104> m_fragment{};
	u32 m_fragment_offset = 0;
	u32 m_loader2_offset = 0;
	u16 m_loader2_words = 613;
	u16 m_verifier_source_end = 0xe800;
	u16 m_loader_control_address = 0x087f;
	bool m_cycle_guard_for_loader = false;
	bool m_loader2_verified = false;
	bool m_observe_after_missing_code = false;
	bool m_runtime_hle = false;
	bool m_observation_halted = false;
	std::array<u16, 0x800> m_program_ram{};
	std::array<u8, 0x800> m_program_valid{};
	std::array<u16, 0x10000> m_data{};
	std::array<u16, 0x20> m_control{};
	emu_timer *m_guard = nullptr;
	bool m_active = false;
	bool m_published = false;
	u16 m_program_end = 0x0fdf;
	bool m_verifier = true;
};

DECLARE_DEVICE_TYPE(NOKIA_DSP_STAGED, nokia_dsp_staged_device)

#endif // MAME_NOKIA_NOKIA_DSP_STAGED_H
