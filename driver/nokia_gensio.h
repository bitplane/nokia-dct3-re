// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#ifndef MAME_NOKIA_NOKIA_GENSIO_H
#define MAME_NOKIA_NOKIA_GENSIO_H

class nokia_gensio_device : public device_t
{
public:
	struct wiring_contract
	{
		u8 ccont_write = 0x2c;
		u8 control = 0x2d;
		u8 lcd_data = 0x2e;
		u8 ccont_read = 0x6c;
		u8 status = 0x6d;
		u8 lcd_command = 0x6e;
		u8 idle_status = 0x03;
		bool ccont_rx_ready_on_write = false;
	};

	nokia_gensio_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto ccont_read_cb() { return m_ccont_read_cb.bind(); }
	auto ccont_write_cb() { return m_ccont_write_cb.bind(); }
	auto ccont_select_cb() { return m_ccont_select_cb.bind(); }
	auto lcd_dc_cb() { return m_lcd_dc_cb.bind(); }
	auto lcd_sdin_cb() { return m_lcd_sdin_cb.bind(); }
	auto lcd_sclk_cb() { return m_lcd_sclk_cb.bind(); }

	void set_wiring_contract(wiring_contract contract) { m_wiring = contract; }
	bool owns(offs_t offset) const;
	u8 read(offs_t offset);
	void write(offs_t offset, u8 data);
	u8 peek(offs_t offset) const;

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	void write_lcd(u8 data, bool is_data);

	devcb_read8 m_ccont_read_cb;
	devcb_write8 m_ccont_write_cb;
	devcb_write_line m_ccont_select_cb;
	devcb_write_line m_lcd_dc_cb;
	devcb_write_line m_lcd_sdin_cb;
	devcb_write_line m_lcd_sclk_cb;
	u8 m_regs[0x100] = {0};
	u8 m_status = 0x03;
	wiring_contract m_wiring;
};

DECLARE_DEVICE_TYPE(NOKIA_GENSIO, nokia_gensio_device)

#endif // MAME_NOKIA_NOKIA_GENSIO_H
