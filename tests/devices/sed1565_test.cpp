// license:BSD-3-Clause
// copyright-holders:Gaz

// Development-only executable conformance checks for the SED1565 controller.
#include "emu.h"
#include "video/sed1520.h"
#include "screen.h"
#include "emupal.h"

namespace {
class sed1565_test_state : public driver_device
{
public:
	sed1565_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_lcd(*this, "lcd"), m_screen(*this, "screen") { }
	void test(machine_config &config)
	{
		SED1565(config, m_lcd);
		SCREEN(config, m_screen, SCREEN_TYPE_LCD);
		m_screen->set_refresh_hz(60);
		m_screen->set_size(132, 65);
		m_screen->set_visarea(0, 131, 0, 64);
		m_screen->set_screen_update("lcd", FUNC(sed1565_device::screen_update));
		m_screen->set_palette("palette");
		PALETTE(config, "palette", palette_device::MONOCHROME_INVERTED);
	}
private:
	required_device<sed1565_device> m_lcd;
	required_device<screen_device> m_screen;
	unsigned m_checks = 0;
	void expect(bool condition, char const *name)
	{
		if (!condition) fatalerror("SED1565 conformance: %s", name);
		++m_checks;
	}
	void address(u8 page, u8 column)
	{
		m_lcd->control_write(0xb0 | page);
		m_lcd->control_write(0x10 | (column >> 4));
		m_lcd->control_write(column & 15);
	}
	void serial(u8 value, bool data)
	{
		m_lcd->dc_w(data);
		for (int bit = 7; bit >= 0; --bit)
		{
			m_lcd->sclk_w(0);
			m_lcd->sdin_w(BIT(value, bit));
			m_lcd->sclk_w(1);
		}
	}
	virtual void machine_start() override
	{
		timer_alloc(FUNC(sed1565_test_state::check), this)->adjust(attotime::from_usec(1));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		bitmap_ind16 bitmap(132, 65);
		rectangle const clip(0, 131, 0, 64);
		auto pixel = [&](unsigned x, unsigned y) {
			m_lcd->screen_update(*m_screen, bitmap, clip);
			return bitmap.pix(y, x);
		};
		serial(0xaf, false);
		expect((m_lcd->status_read() & 0x20) == 0, "serial display on/status polarity");
		address(0, 18);
		serial(1, true);
		expect(pixel(18, 0) == 1 && pixel(19, 0) == 0, "MSB-first serial data/address");
		m_lcd->control_write(0x81);
		m_lcd->control_write(0xa7); // Volume argument, not reverse-display command.
		expect(pixel(18, 0) == 1 && pixel(19, 0) == 0, "two-byte command consumes argument");
		m_lcd->control_write(0xa1);
		expect(pixel(113, 0) == 1 && pixel(18, 0) == 0, "segment direction");
		m_lcd->control_write(0xa0);
		m_lcd->control_write(0xc8);
		expect(pixel(18, 63) == 1, "common direction");
		m_lcd->control_write(0xc0);
		m_lcd->control_write(0x41);
		expect(pixel(18, 63) == 1 && pixel(18, 0) == 0, "start-line rotation");
		m_lcd->control_write(0x40);
		address(8, 20); m_lcd->data_write(0xff);
		expect(pixel(20, 64) == 1, "ninth page visible bit");
		address(9, 20); m_lcd->data_write(0xff);
		expect(pixel(20, 0) == 0, "invalid page must not alias");
		address(1, 131); m_lcd->data_write(1); m_lcd->data_write(2);
		expect(pixel(131, 8) == 0 && pixel(131, 9) == 1 && pixel(0, 16) == 0,
				"column saturation/no page carry");
		address(0, 30); m_lcd->control_write(0xe0);
		m_lcd->data_write(1); m_lcd->data_write(1); m_lcd->control_write(0xee);
		m_lcd->data_write(2);
		expect(pixel(30, 0) == 0 && pixel(30, 1) == 1 && pixel(31, 0) == 1,
				"read-modify-write restores column");
		m_lcd->control_write(0xa7);
		expect(pixel(18, 0) == 0 && pixel(19, 0) == 1, "reverse display");
		m_lcd->control_write(0xa5);
		expect(pixel(18, 0) == 1 && pixel(19, 0) == 1, "all-points priority");
		m_lcd->control_write(0xae);
		expect(pixel(18, 0) == 0, "display off/power save blanks output");
		m_lcd->control_write(0xa4); m_lcd->control_write(0xa6);
		m_lcd->control_write(0xaf); m_lcd->control_write(0xa1);
		m_lcd->control_write(0xe2);
		expect(pixel(113, 0) == 1 && (m_lcd->status_read() & 0x60) == 0,
				"software reset preserves RAM/display-on/segment direction");
		m_lcd->control_write(0xa0);
		m_lcd->cs_w(1); address(0, 40); serial(0xff, true); m_lcd->cs_w(0);
		expect(pixel(40, 0) == 0, "deselected serial input ignored");
		m_lcd->reset_w(0); serial(0xff, true); m_lcd->reset_w(1);
		m_lcd->control_write(0xaf);
		expect(pixel(18, 0) == 1 && pixel(40, 0) == 0, "reset pin preserves RAM/blocks input");
		m_lcd->set_panel_window(18, 96, 65);
		expect(pixel(0, 0) == 1, "panel segment crop");
		logerror("sed1565_conformance: PASS checks=%u\n", m_checks);
		machine().schedule_exit();
	}
};
static INPUT_PORTS_START(sed1565_test) INPUT_PORTS_END
ROM_START(sed1565t) ROM_END
} // anonymous namespace
CONS(2026, sed1565t, 0, 0, test, sed1565_test, sed1565_test_state, empty_init,
		"Test", "SED1565 controller conformance", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)
