// license:BSD-3-Clause
// copyright-holders:Gaz

/* Development-only execution tests for the clean-room TMS320C54x core. */

#include "emu.h"
#include "cpu/tms320c54x/tms320c54x.h"

namespace {

class tms320c54x_test_state : public driver_device
{
public:
	tms320c54x_test_state(const machine_config &mconfig, device_type type,
			const char *tag) :
		driver_device(mconfig, type, tag),
		m_cpu(*this, "maincpu")
	{
	}

	void test(machine_config &config);

private:
	virtual void machine_start() override
	{
		m_check_timer = timer_alloc(FUNC(tms320c54x_test_state::check_results), this);
	}

	virtual void machine_reset() override
	{
		auto &program = m_cpu->space(AS_PROGRAM);
		auto &data = m_cpu->space(AS_DATA);

		// CALL/RET, then a three-word RPT copy.
		program.write_word(0x0100, 0xf074);
		program.write_word(0x0101, 0x0200);
		program.write_word(0x0102, 0xec02);
		program.write_word(0x0103, 0xe598);

		// Reset source/destination pointers and execute a two-word CALL at the
		// end of an RPTB block three times.
		program.write_word(0x0104, 0x7712);
		program.write_word(0x0105, 0x0500);
		program.write_word(0x0106, 0x7714);
		program.write_word(0x0107, 0x0600);
		program.write_word(0x0108, 0x771a);
		program.write_word(0x0109, 0x0002);
		program.write_word(0x010a, 0xf072);
		program.write_word(0x010b, 0x010d);
		program.write_word(0x010c, 0xf074);
		program.write_word(0x010d, 0x0210);

		program.write_word(0x0200, 0x7680);
		program.write_word(0x0201, 0xbeef);
		program.write_word(0x0202, 0xfc00);
		program.write_word(0x0210, 0x1092);
		program.write_word(0x0211, 0x8094);
		program.write_word(0x0212, 0xfc00);

		data.write_word(0x0500, 0x1111);
		data.write_word(0x0501, 0x2222);
		data.write_word(0x0502, 0x3333);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0100);
		m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0700);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0400);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0500);

		m_check_timer->adjust(attotime::from_usec(100));
	}

	void program_map(address_map &map) ATTR_COLD
	{
		map(0x0000, 0xffff).ram();
	}

	void data_map(address_map &map) ATTR_COLD
	{
		map(0x0000, 0xffff).ram();
	}

	void expect(bool condition, const char *message)
	{
		if (!condition)
			throw emu_fatalerror("TMS320C54x core conformance: %s", message);
	}

	TIMER_CALLBACK_MEMBER(check_results)
	{
		auto &data = m_cpu->space(AS_DATA);
		osd_printf_info("TMS320C54x test state: pc=%04x sp=%04x brc=%04x "
				"rpt=%04x,%04x,%04x block=%04x,%04x,%04x\n",
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_SP)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_BRC)),
				data.read_word(0x0400), data.read_word(0x0401), data.read_word(0x0402),
				data.read_word(0x0600), data.read_word(0x0601), data.read_word(0x0602));
		expect(data.read_word(0x0700) == 0xbeef, "CALL/RET continuation");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
				"balanced system stack");
		expect(data.read_word(0x0400) == 0x1111 &&
				data.read_word(0x0401) == 0x2222 &&
				data.read_word(0x0402) == 0x3333, "RPT memory transfer");
		expect(data.read_word(0x0600) == 0x1111 &&
				data.read_word(0x0601) == 0x2222 &&
				data.read_word(0x0602) == 0x3333, "RPTB multiword CALL");
		expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0,
				"RPTB terminal count");

		osd_printf_info("TMS320C54x core conformance: PASS\n");
		throw emu_fatalerror(0, "TMS320C54x core tests complete");
	}

	required_device<tms320c54x_device> m_cpu;
	emu_timer *m_check_timer = nullptr;
};

void tms320c54x_test_state::test(machine_config &config)
{
	TMS320C54X(config, m_cpu, 13'000'000);
	m_cpu->set_addrmap(AS_PROGRAM, &tms320c54x_test_state::program_map);
	m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::data_map);
}

ROM_START(tms54test)
ROM_END

} // anonymous namespace

SYST(2026, tms54test, 0, 0, test, 0, tms320c54x_test_state, empty_init,
		"MAME", "TMS320C54x core conformance tests",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
