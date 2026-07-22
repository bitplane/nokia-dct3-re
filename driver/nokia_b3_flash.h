// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_B3_FLASH_H
#define MAME_NOKIA_NOKIA_B3_FLASH_H

class intelfsh16_device;

// Transitional adapter for B3 partition/status behavior not yet represented by
// MAME's generic Intel-compatible flash core.  It deliberately owns no Nokia
// firmware policy and can disappear when the generic core gains equivalent
// read-while-write, erase-suspend and block-lock semantics.
class nokia_b3_flash_device : public device_t
{
public:
	nokia_b3_flash_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	void set_enabled(bool enabled) { m_enabled = enabled; }
	void set_status_csr(offs_t offset) { m_status_csr = offset; }

	u16 read(offs_t offset, u16 mem_mask = ~u16(0));
	void write(offs_t offset, u16 data, u16 mem_mask = ~u16(0));

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	TIMER_CALLBACK_MEMBER(erase_complete);

	required_device<intelfsh16_device> m_flash;
	emu_timer *m_erase_timer = nullptr;

	bool m_enabled = false;
	offs_t m_status_csr = 0;
	bool m_lock_command = false;
	bool m_program_data = false;
	bool m_erase_confirm = false;
	bool m_erase_active = false;
	bool m_erase_suspended = false;
	bool m_status_override = false;
	u64 m_erase_remaining_us = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_B3_FLASH, nokia_b3_flash_device)

#endif // MAME_NOKIA_NOKIA_B3_FLASH_H
