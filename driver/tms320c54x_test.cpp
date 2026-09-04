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
	void rom4(machine_config &config);

private:
	virtual void machine_start() override
	{
		m_check_timer = timer_alloc(FUNC(tms320c54x_test_state::check_results), this);
	}

	virtual void machine_reset() override
	{
		if (!strcmp(machine().system().name, "tms54rom4"))
		{
			auto &data = m_cpu->space(AS_DATA);
			u16 const *const initial = &memregion("dspdata")->as_u16();
			for (unsigned address = 0; address != 0x10000; ++address)
				data.write_word(address, initial[address]);
			m_phase = 2;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x4b73);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x201f);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x2103);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0000004b73);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfffffffffe);
			static constexpr u16 ar[] = {
				0x0001, 0xb0bc, 0x0825, 0x06e3,
				0x001a, 0x12ca, 0x06e3, 0x0000
			};
			for (unsigned i = 0; i != std::size(ar); ++i)
				m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, ar[i]);
			m_check_timer->adjust(attotime::from_usec(100));
			return;
		}

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

		// Final ROM4 challenge-transform loop. These are observed operands and
		// generic instruction encodings, not firmware code or a canned result.
		program.write_word(0x0300, 0x7712);
		program.write_word(0x0301, 0x13d9);
		program.write_word(0x0302, 0x7713);
		program.write_word(0x0303, 0x13d7);
		program.write_word(0x0304, 0x7714);
		program.write_word(0x0305, 0x1208);
		program.write_word(0x0306, 0x771a);
		program.write_word(0x0307, 0x0005);
		program.write_word(0x0308, 0xf072);
		program.write_word(0x0309, 0x030b);
		program.write_word(0x030a, 0xf074);
		program.write_word(0x030b, 0x0320);
		program.write_word(0x0320, 0x108a);
		program.write_word(0x0321, 0xf493);
		program.write_word(0x0322, 0x1a8b);
		program.write_word(0x0323, 0x6d8c);
		program.write_word(0x0324, 0x1c84);
		program.write_word(0x0325, 0x8084);
		program.write_word(0x0326, 0xfc00);

		data.write_word(0x0500, 0x1111);
		data.write_word(0x0501, 0x2222);
		data.write_word(0x0502, 0x3333);
		data.write_word(0x13d2, 0x6d4d);
		data.write_word(0x13d3, 0xc431);
		data.write_word(0x13d4, 0xbfe4);
		data.write_word(0x13d5, 0x5d91);
		data.write_word(0x13d6, 0x71b1);
		data.write_word(0x13d7, 0x9ac9);
		data.write_word(0x13d8, 0x6d4d);
		data.write_word(0x13d9, 0xc431);
		data.write_word(0x1202, 0x71b1);
		data.write_word(0x1203, 0x9ac9);
		data.write_word(0x1204, 0x6d4d);
		data.write_word(0x1205, 0xc431);
		data.write_word(0x1206, 0xbfe4);
		data.write_word(0x1207, 0x5d91);
		m_phase = 0;
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

	void rom4_program_map(address_map &map) ATTR_COLD
	{
		map(0x0000, 0xffff).rom().region("dspprg", 0);
	}

	void rom4_data_map(address_map &map) ATTR_COLD
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
		if (m_phase == 2)
		{
			const u16 pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
			osd_printf_info("TMS320C54x ROM4 execution frontier: pc=%04x "
					"header=%04x,%04x ar2=%04x ar3=%04x\n", pc,
					data.read_word(0x1200), data.read_word(0x1201),
					u16(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
					u16(m_cpu->state_int(tms320c54x_device::STATE_AR3)));
			// PC has advanced past unsupported MVDD opcode e50b at 4b84.
			expect(pc == 0x4b85, "ROM4 first unsupported instruction");
			expect(data.read_word(0x1200) == 0x3532 &&
					data.read_word(0x1201) == 0x0000,
					"ROM4 challenge header construction");
			throw emu_fatalerror(0, "TMS320C54x ROM4 frontier complete");
		}
		if (m_phase)
		{
			static constexpr u16 expected[] = {
				0x1cee, 0x7cb6, 0xd2a3, 0xb986, 0x4c57, 0xe65e
			};
			osd_printf_info("TMS320C54x transform result: %04x,%04x,%04x,%04x,%04x,%04x\n",
					data.read_word(0x1202), data.read_word(0x1203),
					data.read_word(0x1204), data.read_word(0x1205),
					data.read_word(0x1206), data.read_word(0x1207));
			for (unsigned i = 0; i != std::size(expected); ++i)
				expect(data.read_word(0x1202 + i) == expected[i],
						"ROM4 challenge transform terminal loop");
			expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0,
					"ROM4 challenge transform repeat count");
			osd_printf_info("TMS320C54x core conformance: PASS\n");
			throw emu_fatalerror(0, "TMS320C54x core tests complete");
		}

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

		m_phase = 1;
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0300);
		m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
		m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
		m_check_timer->adjust(attotime::from_usec(100));
	}

	required_device<tms320c54x_device> m_cpu;
	emu_timer *m_check_timer = nullptr;
	unsigned m_phase = 0;
};

void tms320c54x_test_state::test(machine_config &config)
{
	TMS320C54X(config, m_cpu, 13'000'000);
	m_cpu->set_addrmap(AS_PROGRAM, &tms320c54x_test_state::program_map);
	m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::data_map);
}

void tms320c54x_test_state::rom4(machine_config &config)
{
	TMS320C54X(config, m_cpu, 13'000'000);
	m_cpu->set_addrmap(AS_PROGRAM, &tms320c54x_test_state::rom4_program_map);
	m_cpu->set_addrmap(AS_DATA, &tms320c54x_test_state::rom4_data_map);
}

ROM_START(tms54test)
ROM_END

ROM_START(tms54rom4)
	ROM_REGION16_LE(0x80000, "dspprg", 0)
	ROM_LOAD16_WORD_SWAP("transform_entry_prog.bin", 0, 0x80000,
			CRC(99757118) SHA1(0a1da67d21f4c333acd331271c3d9f08e896008f))
	ROM_REGION16_LE(0x20000, "dspdata", 0)
	ROM_LOAD16_WORD_SWAP("transform_entry_data.bin", 0, 0x20000,
			CRC(bef92101) SHA1(1c547eb7fd457d95cdf7956462e80a40dbadb46e))
ROM_END

} // anonymous namespace

SYST(2026, tms54test, 0, 0, test, 0, tms320c54x_test_state, empty_init,
		"MAME", "TMS320C54x core conformance tests",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, tms54rom4, 0, 0, rom4, 0, tms320c54x_test_state, empty_init,
		"MAME", "TMS320C54x ROM4 private execution fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
