// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "nokia_cobba.h"

namespace {

// Execute the stock flash-staged verifier without supplying a DSP mask ROM.
// This is a protocol investigation fixture, not an NSM-3 compatibility backend.
class nsm3_verifier_state : public driver_device
{
public:
	nsm3_verifier_state(const machine_config &config, device_type type, const char *tag) :
		driver_device(config, type, tag), m_cpu(*this, "maincpu"), m_cobba(*this, "cobba") { }
	void verifier(machine_config &config)
	{
		TMS320C54X(config, m_cpu, 13'000'000);
		NOKIA_COBBA(config, m_cobba, 0);
		m_cpu->set_addrmap(AS_PROGRAM, &nsm3_verifier_state::program_map);
		m_cpu->set_addrmap(AS_DATA, &nsm3_verifier_state::data_map);
		m_cpu->set_addrmap(AS_IO, &nsm3_verifier_state::io_map);
	}
	void nse5_verifier(machine_config &config)
	{
		verifier(config);
		m_cpu->set_addrmap(AS_PROGRAM, &nsm3_verifier_state::nse5_program_map);
	}

private:
	bool npe3() const { return !strcmp(machine().system().name, "npe3verify"); }
	bool nhm3() const { return !strcmp(machine().system().name, "nhm3verify"); }
	bool nse5() const { return !strcmp(machine().system().name, "nse5verify"); }
	unsigned block_count() const { return nse5() ? 228 : (npe3() || nhm3()) ? 232 : 116; }
	void program_map(address_map &map)
	{
		map(0, 0xffff).ram();
		// ROM4's acquired PROM has the immutable version word here. This
		// fixture supplies an explicit version input, not missing ROM code.
		map(0xff87, 0xff87).rw(FUNC(nsm3_verifier_state::version_r), FUNC(nsm3_verifier_state::version_w));
	}
	void nse5_program_map(address_map &map)
	{
		map(0, 0x7fff).ram();
		map(0x8000, 0xffff).rom().region("mask", 0x10000);
	}
	u16 version_r() { return !strcmp(machine().options().bios(), "rom4") ? 4 : 6; }
	void version_w(u16 value)
	{
		logerror("nsm3_verifier: immutable_version_write=%04x\n", value);
	}
	void data_map(address_map &map) { map(0, 0xffff).ram(); }
	void io_map(address_map &map)
	{
		map(0, 0xffff).rw(FUNC(nsm3_verifier_state::io_r), FUNC(nsm3_verifier_state::io_w));
	}
	bool using_cobba() const
	{
		return !strcmp(machine().options().bios(), "cobba") ||
				!strcmp(machine().options().bios(), "cobba_alt") ||
				!strcmp(machine().options().bios(), "rom4");
	}
	u16 io_r(offs_t offset)
	{
		if (using_cobba() && offset == 0x2d)
		{
			u16 const value = m_cobba->control_data_r();
			logerror("nsm3_verifier: port_read=%04x data=%04x blocks=%u\n", u16(offset), value, m_block);
			return value;
		}
		throw emu_fatalerror(1, "NSM3 verifier requires peripheral read: port=%04x pc=%04x blocks=%u",
			u16(offset), u16(m_cpu->state_int(tms320c54x_device::STATE_PC)), m_block);
	}
	void io_w(offs_t offset, u16 data)
	{
		logerror("nsm3_verifier: port_write=%04x data=%04x blocks=%u\n", u16(offset), data, m_block);
		if (using_cobba())
		{
			if (offset == 0x2c)
				m_cobba->control_select_w(data);
			else if (offset == 0x2d)
				m_cobba->control_data_w(data);
		}
	}
	void machine_start() override
	{
		m_timer = timer_alloc(FUNC(nsm3_verifier_state::step), this);
	}
	void machine_reset() override
	{
		auto &program = m_cpu->space(AS_PROGRAM);
		auto &data = m_cpu->space(AS_DATA);
		for (unsigned i = 0; i != (nse5() ? 0x8000 : 0x10000); ++i)
			if (i != 0xff87)
				program.write_word(i, 0xffff);
		u16 const *const staged = &memregion("verifier")->as_u16();
		for (unsigned i = 0; i != (nse5() ? 210 : 223); ++i)
			program.write_word(0x0f00 + i, staged[i]);
		// Values written by the MCU at 0x2cac80..0x2cacee, not a mask-ROM snapshot.
		data.write_word(0x0800, 0);
		data.write_word(0x0801, 0xffff);
		data.write_word(0x0802, 0xffff);
		data.write_word(0x0803, 0xffff);
		data.write_word(0x087b, 0x0100);
		data.write_word(0x087c, 0x0300);
		data.write_word(0x087d, (npe3() || nhm3() || nse5()) ? 1 : 0);
		data.write_word(0x087e, nse5() ? 0xc800 : (npe3() || nhm3()) ? 0xd000 : 0xe800);
		data.write_word(0x087f, 1);
		data.write_word(0x0880, 1);
		data.write_word(0x0881, 0x0200);
		m_block = 0;
		m_ticks = 0;
		// Metamorphic peripheral input, not a proposed handset identity.
		if (!strcmp(machine().options().bios(), "cobba_alt"))
		{
			m_cobba->control_data_w(0x0016);
			m_cobba->control_select_w(0x000f);
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0f00);
		m_timer->adjust(attotime::from_usec(1), 0, attotime::from_usec(1));
	}
	TIMER_CALLBACK_MEMBER(step)
	{
		auto &data = m_cpu->space(AS_DATA);
		unsigned const pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
		if ((pc < 0x0f00 || pc >= (nse5() ? 0x0fd2 : 0x0fdf)) &&
				!(nse5() && pc >= 0x8000))
			throw emu_fatalerror(1, "NSM3 verifier escaped staged image: pc=%04x block=%u", pc, m_block);
		if (data.read_word(0x0801) != 0xffff)
			throw emu_fatalerror(0, "NSM3 verifier publication: blocks=%u word0=%04x word1=%04x word2=%04x word3=%04x pc=%04x fingerprint=%04x%04x pmst=%04x",
				m_block, data.read_word(0x0800), data.read_word(0x0801), data.read_word(0x0802), data.read_word(0x0803), pc,
				data.read_word(nse5() ? 0x1f0e : 0x04f7), data.read_word(nse5() ? 0x1f0f : 0x04f8), u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
		if (++m_ticks == 2000000)
			throw emu_fatalerror(1, "NSM3 verifier timeout: pc=%04x blocks=%u flags=%04x/%04x",
				pc, m_block, data.read_word(0x087f), data.read_word(0x0880));
		bool const second = BIT(m_block, 0);
		unsigned const flag = second ? 0x0880 : 0x087f;
		bool const polling = second ? (pc == 0x0f33 || pc == 0x0f35) : (pc == 0x0f17 || pc == 0x0f19);
		if (m_block == block_count() || !polling || data.read_word(flag) != 1)
			return;
		u8 const *const flash = memregion("flash")->base();
		unsigned const base = second ? 0x0b00 : 0x0900;
		for (unsigned i = 0; i != 512; ++i)
		{
			unsigned const offset = 0x40 + (m_block * 512 + i) * 0x20;
			u16 const value = (m_block == block_count() - 1 && i >= 510) ? 0xffff :
				(u16(flash[offset]) << 8) | flash[offset + 1];
			data.write_word(base + i, value);
		}
		data.write_word(flag, 0);
		logerror("nsm3_verifier: block=%u flag=%04x pc=%04x\n", m_block, flag, pc);
		++m_block;
	}
	required_device<tms320c54x_device> m_cpu;
	required_device<nokia_cobba_device> m_cobba;
	emu_timer *m_timer = nullptr;
	unsigned m_block = 0;
	unsigned m_ticks = 0;
};

ROM_START(nsm3verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "Compare existing COBBA register model (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture (not handset identity)")
	ROM_SYSTEM_BIOS(3, "rom4", "Immutable PROM version sensitivity fixture (not NSM-3 hardware)")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x1d0000, "flash", 0)
	ROM_LOAD("8210_5.31ppm_c.fls", 0, 0x1d0000,
		CRC(927022b1) SHA1(c1a0fe95cedb89a92b19654208cc4855e1a4988e))
ROM_END

// Same staged program and observed geometry, distinct stock 8250 flash input.
// Peripheral variants remain sensitivity fixtures, not fitted NSM-3D identity.
ROM_START(nsm3dverify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x1d0000, "flash", 0)
	ROM_LOAD("8250-502mcuppmk.fls", 0, 0x1d0000,
		CRC(2c58e48b) SHA1(f26c98ffcfffbbd5714889e10cfa41c5f6dd2529))
ROM_END

// Own 3310 v6.39 input and MCU-derived 116-block geometry; no donor mask ROM.
ROM_START(nhm5verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x200000, "flash", 0)
	ROM_LOAD("3310f639e.fls", 0, 0x200000,
		CRC(13430c77) SHA1(d5da65f417595200314eb0115bf46ca1fbf53128))
ROM_END

// NPE-3 additionally supplies a different block count and source extent.
ROM_START(npe3verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x3a0000, "flash", 0)
	ROM_LOAD("6210_556c.fls", 0, 0x3a0000,
		CRC(203fb962) SHA1(3d9ea319503e78ec69b60d72cda23e461e118ea9))
ROM_END

// 6250's own staged bytes and sparse flash stream; no handset verdict supplied.
ROM_START(nhm3verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x3a0000, "flash", 0)
	ROM_LOAD("6250-503mcuppmc.fls", 0, 0x3a0000,
		CRC(8dffb91b) SHA1(95607ce39c383bda75f1e6aeae67a214b787b0a1))
ROM_END

// Stock NSE-5 upload plus independently recovered ROM4 CRC routines. COBBA
// variants are explicit peripheral fixtures, not a measured 7110 publication.
ROM_START(nse5verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_REGION16_LE(420, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nse5_verifier.bin", 0, 420,
		CRC(e6c77fdc) SHA1(caca7599d9ca1a7dddf2df37f32be4aacd420deb))
	ROM_REGION(0x390000, "flash", 0)
	ROM_LOAD("7110f501_ppmc.fls", 0, 0x390000,
		CRC(919ac753) SHA1(53af8324919f455ba8199d2c05f7a921cfb811d5))
	ROM_REGION16_LE(0x20000, "mask", ROMREGION_ERASEFF)
	ROM_LOAD16_WORD_SWAP("dsp_full.bin", 0, 0x1fffe,
		CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739))
ROM_END

} // anonymous namespace

SYST(2026, nsm3verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NSM-3 stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nsm3dverify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NSM-3D stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, npe3verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NPE-3 stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nhm5verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "3310 own stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nhm3verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "6250 stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nse5verify, 0, 0, nse5_verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NSE-5 stock staged DSP verifier with ROM4 CRC routines",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
