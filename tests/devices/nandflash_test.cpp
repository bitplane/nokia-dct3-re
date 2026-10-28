// license:BSD-3-Clause
// copyright-holders:Gaz

// Development-only tests for the fitted MU4 NAND, independent of Nokia firmware.
#include "emu.h"
#include "machine/nandflash.h"
#include <sstream>

namespace {
class nandflash_test_state : public driver_device
{
public:
	nandflash_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_nand(*this, "nand"), m_legacy(*this, "legacy") { }
	void test(machine_config &config)
	{
		SAMSUNG_K9K1208U0A(config, m_nand);
		m_nand->rnb_wr_callback().set(FUNC(nandflash_test_state::ready_w));
		SAMSUNG_K9F5608U0D(config, m_legacy);
	}
private:
	required_device<samsung_k9k1208u0a_device> m_nand;
	required_device<samsung_k9f5608u0d_device> m_legacy;
	emu_timer *m_check = nullptr;
	unsigned m_phase = 0, m_checks = 0;
	bool m_ready = true;
	std::stringstream m_saved;
	void ready_w(int state) { m_ready = bool(state); }
	void expect(bool condition, char const *name)
	{
		if (!condition) fatalerror("NAND conformance phase %u: %s", m_phase, name);
		++m_checks;
	}
	void address(u32 row, u8 column = 0)
	{
		m_nand->address_w(column);
		m_nand->address_w(row);
		m_nand->address_w(row >> 8);
		m_nand->address_w(row >> 16);
	}
	void read(u32 row, u8 command = 0, u8 column = 0)
	{
		m_nand->command_w(command); address(row, column);
		expect(m_nand->is_busy() && !m_ready, "read transfer asserts busy");
	}
	void program(u32 row, u8 value)
	{
		m_nand->command_w(0); m_nand->command_w(0x80); address(row);
		for (unsigned i = 0; i < 528; ++i) m_nand->data_w(value);
		m_nand->command_w(0x10);
		expect(m_nand->is_busy() && !m_ready, "program asserts busy");
		m_nand->command_w(0x70);
		expect(!(m_nand->data_r() & 0x40), "status remains available while busy");
	}
	void next(unsigned phase, unsigned usec)
	{
		m_phase = phase;
		m_check->adjust(attotime::from_usec(usec));
	}
	virtual void machine_start() override
	{
		m_check = timer_alloc(FUNC(nandflash_test_state::check), this);
		m_check->adjust(attotime::from_usec(1));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		switch (m_phase)
		{
		case 0:
			m_nand->command_w(0x90); m_nand->address_w(0);
			expect(m_nand->data_r() == 0xec && m_nand->data_r() == 0x76, "exact fitted ID");
			m_legacy->command_w(0x90); m_legacy->address_w(0);
			expect(m_legacy->data_r() == 0xec && m_legacy->data_r() == 0x75, "legacy ID unchanged");
			program(0x10000, 0xa5); next(1, 199); return;
		case 1:
			expect(m_nand->is_busy() && !m_ready, "program not complete before 200 us");
			m_saved.str(std::string()); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "save pending program");
			next(2, 2); return;
		case 2:
			expect(!m_nand->is_busy() && m_ready, "program completes after 200 us");
			program(0x10000, 0); next(19, 201); return;
		case 19:
			expect(!m_nand->is_busy(), "disturb media before restore");
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved) == STATERR_NONE, "restore pending program");
			expect(m_nand->is_busy() && !m_ready, "pending busy/output restored");
			next(3, 2); return;
		case 3:
			expect(!m_nand->is_busy(), "restored timer completes");
			read(0x10000); next(4, 11); return;
		case 4:
			for (unsigned i = 0; i < 528; ++i) expect(m_nand->data_r() == 0xa5, "data and spare program replay");
			expect(m_nand->is_busy(), "sequential next row has transfer latency");
			next(5, 11); return;
		case 5:
			expect(m_nand->data_r() == 0xff, "next row remains erased");
			m_nand->ce_w(1); m_nand->ce_w(0);
			expect(m_nand->data_r() == 0xff, "CE terminates sequential read");
			read(0); next(6, 11); return;
		case 6:
			expect(m_nand->data_r() == 0xff, "third row byte does not alias row zero");
			program(0x10000, 0x0f); next(7, 201); return;
		case 7:
			read(0x10000, 0x50); next(8, 11); return;
		case 8:
			expect(m_nand->data_r() == 5, "spare select and NAND AND programming");
			m_nand->command_w(0x60);
			m_nand->address_w(0); m_nand->address_w(0); m_nand->address_w(1);
			m_nand->command_w(0xd0); next(9, 1999); return;
		case 9:
			expect(m_nand->is_busy(), "erase not complete before 2 ms");
			next(10, 2); return;
		case 10:
			expect(!m_nand->is_busy() && m_ready, "erase completes after 2 ms");
			read(0x10000); next(11, 11); return;
		case 11:
			expect(m_nand->data_r() == 0xff, "block erase restores programmed data");
			read(31); next(12, 11); return;
		case 12:
			for (unsigned i = 0; i < 528; ++i) expect(m_nand->data_r() == 0xff, "last page read");
			expect(!m_nand->is_busy(), "no automatic transfer into next block");
			expect(m_nand->data_r() == 0xff, "block boundary exhausted");
			m_nand->ce_w(1); m_nand->command_w(0x90); m_nand->ce_w(0);
			expect(m_nand->data_r() == 0xff, "deselected command ignored");
			program(32, 0x36); next(13, 201); return;
		case 13:
			read(31); next(14, 11); return;
		case 14:
			for (unsigned i = 0; i < 528; ++i) expect(m_nand->data_r() == 0xff, "boundary row erased");
			expect(m_nand->data_r() == 0xff && !m_nand->is_busy(), "does not enter programmed next block");
			read(32); next(15, 11); return;
		case 15:
			expect(m_nand->data_r() == 0x36, "next block accessible by explicit address");
			m_nand->ce_w(1); m_nand->ce_w(0);
			expect(m_nand->data_r() == 0xff, "CE terminates non-erased stream");
			program(4, 0x42);
			m_nand->command_w(0xff); next(16, 9); return;
		case 16:
			expect(m_nand->is_busy(), "program reset remains busy for 10 us");
			next(17, 2); return;
		case 17:
			expect(!m_nand->is_busy() && m_ready, "reset completes and cancels pending program");
			read(4); next(18, 11); return;
		case 18:
			expect(m_nand->data_r() == 0xff, "aborted program not committed by reset");
			m_legacy->command_w(0); m_legacy->command_w(0x80);
			m_legacy->address_w(0); m_legacy->address_w(0); m_legacy->address_w(0);
			m_legacy->data_w(0x5a); m_legacy->command_w(0x10);
			expect(!m_legacy->is_busy(), "legacy programming remains synchronous");
			m_legacy->command_w(0);
			m_legacy->address_w(0); m_legacy->address_w(0); m_legacy->address_w(0);
			expect(m_legacy->data_r() == 0x5a, "legacy programmed data unchanged");
			logerror("nandflash_conformance: PASS checks=%u\n", m_checks);
			machine().schedule_exit(); return;
		}
	}
};
static INPUT_PORTS_START(nandflash_test) INPUT_PORTS_END
ROM_START(nandtest) ROM_END
} // anonymous namespace
CONS(2026, nandtest, 0, 0, test, nandflash_test, nandflash_test_state, empty_init,
		"Test", "MU4 NAND controller conformance", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)
