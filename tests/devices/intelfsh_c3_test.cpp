// license:BSD-3-Clause
// copyright-holders:Gaz

// Development-only flash conformance fixture, not a handset provisioning path.
#include "emu.h"
#include "machine/intelfsh.h"
#include "util/ioprocs.h"
#include <sstream>

namespace {
class c3_test_state : public driver_device
{
public:
	c3_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_flash(*this, "flash") { }
	void test(machine_config &config) { INTEL_28F160C3T(config, m_flash); }
private:
	required_device<intel_28f160c3t_device> m_flash;
	emu_timer *m_timer = nullptr;
	unsigned m_phase = 0;
	std::string m_checkpoint;
	void require(bool condition, char const *name)
	{
		if (!condition) fatalerror("C3 flash fixture failed: %s", name);
	}
	u16 array(u32 address)
	{
		m_flash->write(address, 0xff);
		return m_flash->read(address);
	}
	u16 locks(u32 address)
	{
		m_flash->write(address, 0x90);
		return m_flash->read((address & ~0xffU) + 2);
	}
	void lock(u32 address, u16 command)
	{
		m_flash->write(address, 0x60);
		m_flash->write(address, command);
	}
	void program(u32 address, u16 data)
	{
		m_flash->write(address, 0x40);
		m_flash->write(address, data);
	}
	void erase(u32 address)
	{
		m_flash->write(address, 0x20);
		m_flash->write(address, 0xd0);
	}
	u16 protection(u32 address)
	{
		m_flash->write(0, 0x90);
		return m_flash->read(address);
	}
	void protection_program(u32 address, u16 data)
	{
		m_flash->write(0, 0xc0);
		m_flash->write(address, data);
	}
	void check_query()
	{
		// Intel 290645-011 Appendix C, Tables 17--21; x16 word offsets.
		constexpr u8 expected[] = {
			0x51, 0x52, 0x59, 0x03, 0x00, 0x35, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x27, 0x36, 0xb4, 0xc6, 0x05,
			0x00, 0x0a, 0x00, 0x04, 0x00, 0x03, 0x00, 0x15,
			0x01, 0x00, 0x00, 0x00, 0x02, 0x1e, 0x00, 0x00,
			0x01, 0x07, 0x00, 0x20, 0x00, 0x50, 0x52, 0x49,
			0x31, 0x30, 0x66, 0x00, 0x00, 0x00, 0x01, 0x03,
			0x00, 0x33, 0xc0, 0x01, 0x80, 0x00, 0x03, 0x03
		};
		m_flash->write(0x55, 0x98);
		for (unsigned index = 0; index < std::size(expected); ++index)
			require(m_flash->read(0x10 + index) == expected[index], "CFI exact table and zero upper byte");
		require(m_flash->read(0) == 0x89 && m_flash->read(1) == 0x88c2, "CFI identity plane");
		u32 capacity = 0;
		for (unsigned region = 0; region < m_flash->read(0x2c); ++region)
		{
			u32 const offset = 0x2d + region * 4;
			u32 const blocks = 1 + m_flash->read(offset) + (m_flash->read(offset + 1) << 8);
			u32 const bytes = 256 * (m_flash->read(offset + 2) + (m_flash->read(offset + 3) << 8));
			require(blocks == (region ? 8 : 31) && bytes == (region ? 8192 : 65536), "CFI top-boot region ordering");
			capacity += blocks * bytes;
		}
		require(capacity == 0x200000 && capacity == (1U << m_flash->read(0x27)), "CFI density equals erase geometry");
		require(m_flash->read(0x1f0000 / 2 + 2) == 1, "CFI top parameter block lock");
		std::ostringstream saved(std::ios::binary);
		require(machine().save().write_stream(saved) == STATERR_NONE, "save query mode");
		m_flash->write(0, 0x70);
		require(m_flash->read(0x10) == 0x80, "CFI exits to status");
		std::istringstream restore(saved.str(), std::ios::binary);
		require(machine().save().read_stream(restore) == STATERR_NONE && m_flash->read(0x10) == 'Q', "restore query mode");
		m_flash->write(0, 0x90);
		require(m_flash->read(1) == 0x88c2, "CFI exits to identity");
		m_flash->write(0, 0x98);
		m_flash->reset();
		require(m_flash->read(0x10) == 0xffff, "reset exits query to unchanged array");
		osd_printf_info("C3 flash CFI: PASS bytes=56 x16=1 top_regions=31x65536,8x8192 restore=1 array_unchanged=1 handset_claim=0\n");
	}
	virtual void machine_start() override
	{
		m_timer = timer_alloc(FUNC(c3_test_state::step), this);
	}
	virtual void machine_reset() override
	{
		m_timer->adjust(attotime::from_msec(1));
	}
	TIMER_CALLBACK_MEMBER(step)
	{
		constexpr u32 A = 0x1f0000 / 2, B = 0x1f2000 / 2, MAIN = 0x1e0000 / 2;
		if (m_phase == 0)
		{
			check_query();
			if (protection(0x85) != 0xffff)
			{
				require(protection(0x85) == 0x1200 && protection(0x86) == 0xffff
						&& protection(0x80) == 0xfffc, "cold OTP data and irreversible lock");
				protection_program(0x85, 0);
				require(m_flash->read(0) == 0x92 && protection(0x85) == 0x1200, "cold OTP protection");
				require(array(B) == 0x5678, "cold array retained beside OTP");
				osd_printf_info("C3 flash cold OTP: PASS user=1200 lock=fffc array=5678 handset_claim=0\n");
				machine().schedule_exit();
				return;
			}
			m_flash->write(0, 0x90);
			require(m_flash->read(0) == 0x89 && m_flash->read(1) == 0x88c2, "identity");
			require(m_flash->read(0x80) == 0xfffe, "OTP protection word");
			for (unsigned address = 0x85; address <= 0x88; ++address)
				require(m_flash->read(address) == 0xffff, "erased user OTP read plane");
			require(locks(A) == 1 && locks(B) == 1, "power-up locked");
			program(A, 0x1234);
			require(m_flash->read(A) == 0x92 && array(A) == 0xffff, "locked program rejected");
			erase(A);
			require(m_flash->read(A) == 0xa2, "locked erase rejected");
			lock(A, 0xd0); lock(B, 0xd0); lock(MAIN, 0xd0);
			require(locks(A) == 0 && locks(B) == 0, "independent unlock");
			require(locks(MAIN + 0x1000) == 0, "main-block lock spans its full geometry");
			m_flash->write(0, 0x98);
			require(m_flash->read(A + 2) == 0 && m_flash->read(B + 2) == 0, "CFI reports changed block locks");
			m_flash->write(0, 0xff);
			program(A, 0x1234); program(B, 0x5678); program(MAIN, 0x9abc);
			program(A, 0xff00);
			require(array(A) == 0x1200, "NOR only clears bits");
			erase(A);
			require(array(A) == 0xffff && array(B) == 0x5678 && array(MAIN) == 0x9abc, "parameter erase isolation");
			m_phase = 1;
			m_timer->adjust(attotime::from_msec(1100));
			return;
		}
		if (m_phase == 1)
		{
			program(A, 0xabcd);
			erase(MAIN);
			require(array(MAIN) == 0xffff && array(A) == 0xabcd && array(B) == 0x5678, "main erase isolation");
			m_phase = 2;
			m_timer->adjust(attotime::from_msec(1100));
			return;
		}
		if (m_phase == 2)
		{
			protection_program(0x85, 0x1234);
			protection_program(0x85, 0xff00);
			require(protection(0x85) == 0x1200 && array(0x85) == 0xffff, "OTP one-way program separate from array");
			protection_program(0x81, 0);
			require(m_flash->read(0) == 0x92 && protection(0x81) == 0xffff, "factory OTP immutable");
			protection_program(0x89, 0);
			require(array(0x89) == 0xffff, "reserved OTP address cannot mutate array");
			lock(B, 0x2f); lock(B, 0xd0);
			require(locks(B) == 3, "WP-low lockdown rejects unlock");
			m_flash->wp_w(1); lock(B, 0xd0);
			require(locks(B) == 2, "WP-high overrides lockdown");
			m_flash->wp_w(0); program(B, 0);
			require(m_flash->read(B) == 0x92 && array(B) == 0x5678, "WP-low restores lockdown protection");
			std::ostringstream saved(std::ios::binary);
			require(machine().save().write_stream(saved) == STATERR_NONE, "save");
			m_checkpoint = saved.str();
			for (unsigned tail : {0U, 17U, 18U, 19U})
			{
				std::vector<u8> input(0x200000 + tail, 0xff);
				auto stream = util::ram_read(input.data(), input.size());
				require(bool(stream) && m_flash->nvram_load(*stream) == (tail == 0), "raw import and malformed OTP-tail bounds");
			}
			protection_program(0x86, 0);
			protection_program(0x80, 0xfffd);
			protection_program(0x80, 0xffff);
			require(protection(0x80) == 0xfffc, "user OTP lock cannot be set back to one");
			m_flash->wp_w(1);
			m_flash->reset();
			require(locks(A) == 1 && locks(B) == 1, "reset relocks and clears lockdown");
			require(protection(0x80) == 0xfffc && protection(0x86) == 0, "reset cannot erase or unlock OTP");
			std::istringstream restore(m_checkpoint, std::ios::binary);
			require(machine().save().read_stream(restore) == STATERR_NONE, "restore");
			require(locks(A) == 0 && locks(B) == 2 && array(B) == 0x5678, "saved locks and array restored");
			program(B, 0);
			require(m_flash->read(B) == 0x92, "WP pin state restored");
			require(protection(0x80) == 0xfffe && protection(0x85) == 0x1200
					&& protection(0x86) == 0xffff, "OTP state restored");
			protection_program(0x80, 0xfffd);
			protection_program(0x85, 0);
			require(m_flash->read(0) == 0x92 && protection(0x85) == 0x1200, "user OTP lock enforced");
			osd_printf_info("C3 flash conformance: PASS id=1 locked_rejection=1 parameter_isolation=1 main_isolation=1 nor=1 lockdown=1 reset=1 restore=1 otp=1 handset_claim=0\n");
			machine().schedule_exit();
		}
	}
};
static INPUT_PORTS_START(c3_test) INPUT_PORTS_END
ROM_START(c3test) ROM_END
}
CONS(2026, c3test, 0, 0, test, c3_test, c3_test_state, empty_init,
		"Test", "Intel C3 flash conformance", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)


namespace {
class st_test_state : public driver_device
{
public:
	st_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_flash(*this, "flash") { }
	void test(machine_config &config) { ST_M28W320ECT(config, m_flash); }
private:
	static constexpr u32 A = 0x10000, B = 0x20000, P = 0x1f8000;
	required_device<st_m28w320ect_device> m_flash;
	emu_timer *m_timer = nullptr;
	unsigned m_phase = 0, m_checks = 0;
	std::string m_saved, m_active_erase_saved, m_page_saved;
	void check_query()
	{
		// Independent transcription of ST full-datasheet Tables 27--30.
		constexpr u8 expected[] = {
			0x51, 0x52, 0x59, 0x03, 0x00, 0x35, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x27, 0x36, 0xb4, 0xc6, 0x04,
			0x04, 0x0a, 0x00, 0x05, 0x05, 0x03, 0x00, 0x16,
			0x01, 0x00, 0x03, 0x00, 0x02, 0x3e, 0x00, 0x00,
			0x01, 0x07, 0x00, 0x20, 0x00, 0x50, 0x52, 0x49,
			0x31, 0x30, 0x66, 0x00, 0x00, 0x00, 0x01, 0x03,
			0x00, 0x30, 0xc0, 0x01, 0x80, 0x00, 0x03, 0x03
		};
		m_flash->write(0x55, 0x98);
		for (unsigned i = 0; i < std::size(expected); ++i)
			if (m_flash->read(0x10 + i) != expected[i])
				fatalerror("ST CFI mismatch offset=%02x", 0x10 + i);
		if (m_flash->read(0) != 0x20 || m_flash->read(1) != 0x88ba)
			fatalerror("ST CFI identity plane");
		u32 capacity = 0;
		for (unsigned region = 0; region < m_flash->read(0x2c); ++region)
		{
			const u32 offset = 0x2d + region * 4;
			const u32 blocks = 1 + m_flash->read(offset) + (m_flash->read(offset + 1) << 8);
			const u32 bytes = 256 * (m_flash->read(offset + 2) + (m_flash->read(offset + 3) << 8));
			if (blocks != (region ? 8 : 63) || bytes != (region ? 8192 : 65536))
				fatalerror("ST CFI erase-region ordering");
			capacity += blocks * bytes;
		}
		if (capacity != 0x400000 || capacity != (1U << m_flash->read(0x27)))
			fatalerror("ST CFI capacity");
		std::ostringstream saved(std::ios::binary);
		if (machine().save().write_stream(saved) != STATERR_NONE) fatalerror("ST CFI save");
		m_flash->write(0, 0x70);
		if (m_flash->read(0x10) != 0x80) fatalerror("ST CFI mode exit");
		std::istringstream restore(saved.str(), std::ios::binary);
		if (machine().save().read_stream(restore) != STATERR_NONE || m_flash->read(0x10) != 0x51)
			fatalerror("ST CFI mode restore");
		m_flash->write(0, 0xff);
		if (raw(0x10) != 0xffff) fatalerror("ST CFI changed array");
		osd_printf_info("ST flash CFI: PASS bytes=56 x16=1 capacity=4194304 regions=63x65536,8x8192 page_bytes=8 restore=1 array_unchanged=1 full_datasheet=1 handset_claim=0\n");
	}
	void page_snapshot(bool load)
	{
		if (load)
		{
			std::istringstream stream(m_page_saved, std::ios::binary);
			if (machine().save().read_stream(stream) != STATERR_NONE) fatalerror("ST page restore");
		}
		else
		{
			std::ostringstream stream(std::ios::binary);
			if (machine().save().write_stream(stream) != STATERR_NONE) fatalerror("ST page save");
			m_page_saved = stream.str();
		}
	}
	void require(bool value, char const *name)
	{
		++m_checks;
		if (!value) fatalerror("ST flash conformance phase=%u: %s", m_phase, name);
	}
	u16 raw(u32 address) { return (u16(m_flash->base()[address * 2]) << 8) | m_flash->base()[address * 2 + 1]; }
	u16 array(u32 address) { m_flash->write(0, 0xff); return m_flash->read(address); }
	u16 status(u32 address = 0) { m_flash->write(0, 0x70); return m_flash->read(address); }
	u16 locks(u32 address) { m_flash->write(0, 0x90); return m_flash->read(address + 2); }
	void lock(u32 address, u16 data) { m_flash->write(0, 0x60); m_flash->write(address, data); }
	void program(u32 address, u16 data) { m_flash->write(0, 0x40); m_flash->write(address, data); }
	void erase(u32 address) { m_flash->write(0, 0x20); m_flash->write(address, 0xd0); }
	void save()
	{
		std::ostringstream stream(std::ios::binary);
		require(machine().save().write_stream(stream) == STATERR_NONE, "save pending operation");
		m_saved = stream.str();
	}
	void restore()
	{
		std::istringstream stream(m_saved, std::ios::binary);
		require(machine().save().read_stream(stream) == STATERR_NONE, "restore pending operation");
	}
	void next(attotime delay) { ++m_phase; m_timer->adjust(delay); }
	virtual void machine_start() override
	{
		m_timer = timer_alloc(FUNC(st_test_state::check), this);
		m_timer->adjust(attotime::from_msec(1));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		switch (m_phase)
		{
		case 0:
			m_flash->wp_w(1);
			check_query();
			m_flash->write(0, 0x90);
			require(m_flash->read(0) == 0x20 && m_flash->read(1) == 0x88ba, "identity");
			require(m_flash->read(0x100) == 0x20 && m_flash->read(0x100101) == 0x88ba, "identity address don't-care bits");
			for (unsigned i = 0; i < 71; ++i)
				require(locks(i < 63 ? i * 0x8000 : P + (i - 63) * 0x1000) == 1, "all 71 blocks reset locked");
			program(A, 0);
			require(status(A) == 0x92 && status(B) == 0x92 && raw(A) == 0xffff, "locked program/status any address");
			erase(A);
			require(status() == 0xb2 && raw(A) == 0xffff, "sticky locked erase");
			m_flash->write(0, 0x50);
			require(status() == 0x80, "clear sticky errors");
			lock(A, 0xd0); lock(B, 0xd0); lock(P, 0xd0);
			require(locks(A) == 0 && locks(A + 0x8000) == 1 && locks(P) == 0 && locks(P + 0x1000) == 1, "main/parameter lock isolation");
			require(locks(A + 0x100) == 0, "block signature address don't-care bits");
			program(A, 0x12ff);
			require(status(A) == 0 && status(B) == 0 && raw(A) == 0xffff, "deferred program/data opcode");
			// While busy, read-array/ID/setup commands cannot escape status mode.
			m_flash->write(0, 0xff); m_flash->write(0, 0x90); m_flash->write(0, 0x40);
			require(m_flash->read(A) == 0, "busy command admission");
			next(attotime::from_usec(2)); break;
		case 1:
			require(raw(A) == 0xffff && status() == 0, "program still pending");
			save(); next(attotime::from_usec(20)); break;
		case 2:
			require(status() == 0x80 && array(A) == 0x12ff, "program completion");
			restore();
			require(status(B) == 0 && raw(A) == 0xffff, "pending program array restored");
			next(attotime::from_usec(20)); break;
		case 3:
			require(array(A) == 0x12ff, "program replay completion");
			program(A, 0xfff0);
			next(attotime::from_usec(20)); break;
		case 4:
			require(array(A) == 0x12f0, "NOR cannot set cleared bits");
			program(B, 0x5678);
			next(attotime::from_usec(20)); break;
		case 5:
			require(array(B) == 0x5678, "neighbor program");
			erase(A);
			require(status(B) == 0 && raw(A) == 0x12f0, "deferred erase/global busy");
			next(attotime::from_msec(100)); break;
		case 6:
			save(); m_active_erase_saved = m_saved;
			m_flash->write(0, 0xb0);
			require(status(B) == 0xc0 && raw(A) == 0x12f0 && array(B) == 0x5678, "suspend/other block read");
			save(); next(attotime::from_msec(1100)); break;
		case 7:
			require(status() == 0xc0 && raw(A) == 0x12f0, "suspended operation does not complete");
			program(B + 1, 0x12ff);
			require(status() == 0x40 && raw(B + 1) == 0xffff, "program during erase suspend");
			next(attotime::from_usec(20)); break;
		case 8:
			require(status() == 0xc0 && array(B + 1) == 0x12ff, "nested program completion");
			restore();
			require(status() == 0xc0 && raw(B + 1) == 0xffff, "suspended erase restoration");
			{
				const std::string suspended = m_saved;
				m_saved = m_active_erase_saved; restore();
				require(status() == 0 && raw(A) == 0x12f0, "active erase restoration");
				m_saved = suspended; restore();
				require(status() == 0xc0, "suspended erase replay");
			}
			m_flash->write(0, 0xd0);
			require(status(P) == 0, "erase resume");
			next(attotime::from_msec(901)); break;
		case 9:
			require(status() == 0x80 && array(A) == 0xffff && array(B) == 0x5678, "main erase isolation/completion");
			program(P, 0x1234);
			next(attotime::from_usec(20)); break;
		case 10:
			require(array(P) == 0x1234, "parameter program");
			lock(P + 0x1000, 0xd0); program(P + 0x1000, 0xabcd);
			next(attotime::from_usec(20)); break;
		case 11:
			erase(P);
			next(attotime::from_msec(801)); break;
		case 12:
			require(array(P) == 0xffff && array(P + 0x1000) == 0xabcd, "8KiB erase isolation");
			lock(B, 0x2f); m_flash->wp_w(0); lock(B, 0xd0);
			require(locks(B) == 3, "WP low blocks lockdown unlock");
			program(B, 0);
			require(status() == 0x92 && raw(B) == 0x5678, "lockdown blocks program");
			m_flash->write(0, 0x50); m_flash->wp_w(1); lock(B, 0xd0);
			require(locks(B) == 2, "WP high permits unlock retaining lockdown");
			m_flash->wp_w(0);
			require(locks(B) == 3, "WP low restores effective lock");
			m_flash->wp_w(1); lock(B, 0xd0);
			program(B + 2, 0x2468);
			next(attotime::from_usec(2)); break;
		case 13:
			m_flash->write(0, 0xb0);
			require(status() == 0x84 && raw(B + 2) == 0xffff, "program suspend");
			next(attotime::from_msec(2)); break;
		case 14:
			require(status() == 0x84 && raw(B + 2) == 0xffff, "program stays suspended");
			m_flash->write(0, 0xd0); next(attotime::from_usec(20)); break;
		case 15:
			require(array(B + 2) == 0x2468, "program resume");
			m_flash->write(0, 0x30); m_flash->write(A, 0x40ff); m_flash->write(A + 1, 0x60b0);
			require(status() == 0 && raw(A) == 0xffff, "double word deferred");
			next(attotime::from_usec(20)); break;
		case 16:
			require(array(A) == 0x40ff && array(A + 1) == 0x60b0, "double word program");
			m_flash->write(0, 0x20); m_flash->write(A, 0x40);
			require(status() == 0xb0 && raw(A) == 0x40ff, "invalid erase confirm errors");
			m_flash->write(0, 0x50); require(status() == 0x80, "error clear after bad sequence");
			program(A + 3, 0);
			m_flash->reset();
			require(locks(A) == 1 && locks(B) == 1 && status() == 0x80, "reset locks and cancels operation");
			next(attotime::from_msec(1100)); break;
		case 17:
			require(array(A + 3) == 0xffff && array(A) == 0x40ff, "reset cancellation policy/no late commit");
			lock(A, 0xd0); erase(A);
			require(status() == 0, "erase pending before reset");
			m_flash->reset(); next(attotime::from_msec(1100)); break;
		case 18:
			require(array(A) == 0x40ff && locks(A) == 1 && status() == 0x80, "erase reset cancellation/no late commit");
			osd_printf_info("ST flash engine: PASS checks=%u blocks=71 status_anywhere=1 sticky_errors=1 deferred_program=1 deferred_erase=1 suspend_resume=1 nested_program=1 restore=4 lockdown_wp=1 double_word=1 reset_abort=2 physical_cell_claim=0 handset_claim=0\n", m_checks);
			lock(A, 0xd0);
			m_flash->write(0, 0x56);
			m_flash->write(A + 6, 0x4090); m_flash->write(A + 4, 0x60b0);
			page_snapshot(false);
			m_flash->reset(); page_snapshot(true);
			m_flash->write(A + 7, 0x98ff); m_flash->write(A + 5, 0x30d0);
			if (status() != 0 || raw(A + 4) != 0xffff) fatalerror("ST page not deferred");
			page_snapshot(false);
			next(attotime::from_usec(20)); break;
		case 19:
			if (array(A + 4) != 0x60b0 || array(A + 5) != 0x30d0 ||
					array(A + 6) != 0x4090 || array(A + 7) != 0x98ff || array(A + 8) != 0xffff)
				fatalerror("ST page program/neighbor isolation");
			page_snapshot(true);
			if (raw(A + 4) != 0xffff || status() != 0) fatalerror("ST pending page restore");
			next(attotime::from_usec(20)); break;
		case 20:
			if (array(A + 4) != 0x60b0 || array(A + 7) != 0x98ff) fatalerror("ST restored page completion");
			m_flash->write(0, 0x56); m_flash->write(A + 8, 0); m_flash->write(A + 8, 0);
			if (array(A + 8) != 0xffff) fatalerror("ST duplicate page changed array");
			m_flash->write(0, 0x56); m_flash->write(A + 8, 0); m_flash->write(A + 12, 0);
			if (array(A + 8) != 0xffff || array(A + 12) != 0xffff) fatalerror("ST cross-page changed array");
			lock(A, 0x01); m_flash->write(0, 0x56);
			for (unsigned i = 0; i < 4; ++i) m_flash->write(A + 8 + i, 0);
			if (status() != 0x92 || raw(A + 8) != 0xffff) fatalerror("ST locked page accepted");
			m_flash->write(0, 0x50); lock(P, 0xd0); erase(P);
			next(attotime::from_msec(350)); break;
		case 21:
			if (status() != 0) fatalerror("ST parameter erase completed before nominal interval");
			next(attotime::from_msec(100)); break;
		case 22:
			if (status() != 0x80 || array(P) != 0xffff) fatalerror("ST parameter erase exceeded nominal interval");
			osd_printf_info("ST flash page: PASS words=4 deferred=1 arbitrary_order=1 opcode_data=1 restore=2 locked_rejection=1 malformed_rejection=2 parameter_erase_ms=400 physical_cell_claim=0 handset_claim=0\n");
			lock(A, 0xd0); m_flash->vpp_valid_w(0);
			program(A + 16, 0);
			if (status() != 0x88 || raw(A + 16) != 0xffff) fatalerror("ST VPP word refusal");
			m_flash->write(0, 0x50);
			m_flash->write(0, 0x30); m_flash->write(A + 24, 0); m_flash->write(A + 25, 0);
			if (status() != 0x88 || raw(A + 24) != 0xffff || raw(A + 25) != 0xffff) fatalerror("ST VPP double-word refusal");
			m_flash->write(0, 0x50); m_flash->write(0, 0x56);
			for (unsigned i = 0; i < 4; ++i) m_flash->write(A + 20 + i, 0);
			if (status() != 0x88) fatalerror("ST VPP page refusal");
			for (unsigned i = 0; i < 4; ++i) if (raw(A + 20 + i) != 0xffff) fatalerror("ST VPP page changed array");
			m_flash->write(0, 0x50); erase(A);
			if (status() != 0x88 || raw(A) != 0x40ff) fatalerror("ST VPP erase refusal");
			if (array(A) != 0x40ff) fatalerror("ST VPP lockout blocked array read");
			m_flash->write(0, 0x90); if (m_flash->read(0) != 0x20) fatalerror("ST VPP lockout blocked signature read");
			m_flash->write(0, 0x98); if (m_flash->read(0x10) != 0x51) fatalerror("ST VPP lockout blocked CFI read");
			m_flash->write(0, 0x50); m_flash->write(0, 0xc0); m_flash->write(0x85, 0x1234);
			if (status() != 0x88) fatalerror("ST VPP OTP refusal");
			m_flash->write(0, 0x90); if (m_flash->read(0x85) != 0xffff) fatalerror("ST VPP changed OTP");
			m_flash->reset(); lock(A, 0xd0); program(A + 16, 0);
			if (status() != 0x88) fatalerror("ST reset altered external VPP input");
			m_flash->vpp_valid_w(1); program(A + 16, 0x5a3c);
			if (status() != 0x08) fatalerror("ST VPP recovery cleared sticky error");
			m_flash->vpp_valid_w(0); page_snapshot(false);
			next(attotime::from_usec(20)); break;
		case 23:
			if (status() != 0x88 || raw(A + 16) != 0x5a3c) fatalerror("ST VPP low-range start sample");
			m_flash->vpp_valid_w(1); page_snapshot(true);
			if (status() != 0x08 || raw(A + 16) != 0xffff) fatalerror("ST VPP pending restore");
			next(attotime::from_usec(20)); break;
		case 24:
			if (status() != 0x88 || raw(A + 16) != 0x5a3c) fatalerror("ST VPP restored operation");
			m_flash->write(0, 0x50); program(A + 17, 0);
			if (status() != 0x88 || raw(A + 17) != 0xffff) fatalerror("ST saved VPP input not restored");
			m_flash->reset(); if (status() != 0x80) fatalerror("ST reset did not clear VPP error");
			m_flash->vpp_valid_w(1); lock(A, 0xd0); program(A + 17, 0x5678);
			next(attotime::from_usec(20)); break;
		case 25:
			if (status() != 0x80 || raw(A + 17) != 0x5678) fatalerror("ST VPP valid recovery");
			osd_printf_info("ST flash VPP: PASS refused_families=5 read_planes=3 sticky=1 start_sample=1 reset_input=1 restore=1 recovery=1 analogue_claim=0 handset_claim=0\n");
			machine().schedule_exit(); break;
		}
	}
};
ROM_START(sttest) ROM_END
}
CONS(2026, sttest, 0, 0, test, c3_test, st_test_state, empty_init,
		"Test", "ST M28W320ECT engine conformance", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)

namespace {
class st_otp_test_state : public driver_device
{
public:
	st_otp_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_flash(*this, "flash"), m_seeded(*this, "seeded") { }
	void test(machine_config &config) { ST_M28W320ECT(config, m_flash); ST_M28W320ECT(config, m_seeded); }
private:
	class capture_stream : public util::write_stream
	{
	public:
		std::string bytes;
		std::size_t limit = 0x400000 + 26;
		virtual std::error_condition finalize() noexcept override { return {}; }
		virtual std::error_condition flush() noexcept override { return {}; }
		virtual std::error_condition write_some(void const *buffer, std::size_t length, std::size_t &actual) noexcept override
		{
			actual = 0;
			if (bytes.size() >= limit) return std::make_error_condition(std::errc::no_space_on_device);
			length = std::min(length, limit - bytes.size());
			try { bytes.append(static_cast<char const *>(buffer), length); }
			catch (...) { return std::make_error_condition(std::errc::not_enough_memory); }
			actual = length;
			return {};
		}
	};
	required_device<st_m28w320ect_device> m_flash, m_seeded;
	emu_timer *m_timer = nullptr;
	unsigned m_phase = 0;
	std::string m_saved;
	void require(bool ok, char const *name) { if (!ok) fatalerror("ST OTP phase=%u: %s", m_phase, name); }
	u16 otp(st_m28w320ect_device &device, u32 address, bool query = false)
	{
		device.write(0, query ? 0x98 : 0x90);
		return device.read(address);
	}
	u16 otp(u32 address) { return otp(*m_flash, address); }
	u16 status() { m_flash->write(0, 0x70); return m_flash->read(0); }
	void program(u32 address, u16 data) { m_flash->write(0, 0xc0); m_flash->write(address, data); }
	void next(attotime delay) { ++m_phase; m_timer->adjust(delay); }
	void snapshot(bool load)
	{
		if (load)
		{
			std::istringstream stream(m_saved, std::ios::binary);
			require(machine().save().read_stream(stream) == STATERR_NONE, "restore");
		}
		else
		{
			std::ostringstream stream(std::ios::binary);
			require(machine().save().write_stream(stream) == STATERR_NONE, "save");
			m_saved = stream.str();
		}
	}
	std::string persist(st_m28w320ect_device &device)
	{
		capture_stream stream;
		require(device.nvram_save(stream), "NVRAM write");
		return std::move(stream.bytes);
	}
	bool load(st_m28w320ect_device &device, std::string const &bytes)
	{
		auto stream = util::ram_read(bytes.data(), bytes.size());
		return stream && device.nvram_load(*stream);
	}
	void check_persistence()
	{
		auto good = persist(*m_flash);
		require(good.size() == 0x400000 + 26, "ST tail has thirteen BE halfwords");
		require(u8(good[0x400001]) == 0xfd && u8(good[0x40000a]) == 0x12 && u8(good[0x40000b]) == 0,
				"serialized lock and user data");
		// Malformed imports must not partially publish the new flash array.
		for (unsigned kind = 0; kind < 7; ++kind)
		{
			auto bad = good;
			bad[0] ^= 0x80;
			if (kind < 3) bad.resize(0x400000 + (kind == 0 ? 1 : kind == 1 ? 25 : 27), char(0xff));
			else if (kind == 3) bad[0x400001] &= ~4; // Forbidden DQ2.
			else if (kind == 4) bad[0x400002] = 0; // Invented factory identity.
			else if (kind == 5) bad[0x400000] = 0; // Reserved lock bits.
			else bad.resize(0x400000 - 1); // Truncated array, no publication.
			require(!load(*m_flash, bad) && persist(*m_flash) == good, "atomic malformed import refusal");
		}
		auto raw = good.substr(0, 0x400000);
		require(load(*m_flash, raw) && otp(0x85) == 0xffff && (otp(0x80) & 2), "legacy raw-array import");
		require(persist(*m_flash).size() == 0x400000, "unchanged OTP keeps raw-array format");
		require(load(*m_flash, good) && persist(*m_flash) == good, "ST tail round trip");
		for (std::size_t limit : {std::size_t(0x400000 - 1), std::size_t(0x400000 + 25)})
		{
			capture_stream short_write;
			short_write.limit = limit;
			require(!m_flash->nvram_save(short_write), "short array/tail write must fail");
		}
		auto seed = persist(*m_seeded);
		require(seed.size() == 0x400000 && otp(*m_seeded, 0x81) == 0x1234, "explicit factory fixture seed");
		auto wrong = good;
		require(!load(*m_seeded, wrong) && persist(*m_seeded) == seed && otp(*m_seeded, 0x81) == 0x1234,
				"foreign factory tail refused");
		require(load(*m_seeded, seed) && otp(*m_seeded, 0x81) == 0x1234, "raw import preserves configured factory seed");
		// A seeded user zero cannot be turned back into one by importing NVRAM.
		auto seed_tail = seed;
		seed_tail.append(26, char(0xff));
		seed_tail[0x400002] = 0x12; seed_tail[0x400003] = 0x34;
		seed_tail[0x40000a] = 0x7f;
		require(load(*m_seeded, seed_tail) && otp(*m_seeded, 0x85) == 0x7fff, "valid explicit user seed");
		seed_tail[0x40000a] = char(0xff);
		require(!load(*m_seeded, seed_tail) && otp(*m_seeded, 0x85) == 0x7fff, "seeded user zero cannot be imported as one");
	}
	void check_words()
	{
		for (u32 address = 0x81; address <= 0x84; ++address)
		{
			require(otp(address) == 0xffff, "undumped factory words stay erased");
			require(otp(*m_seeded, address) == (address == 0x81 ? 0x1234 : 0xffff), "explicit factory fixture words");
		}
		for (u32 address = 0x85; address <= 0x8c; ++address)
		{
			const u16 expected = address == 0x85 ? 0x1200 : address == 0x8c ? 0x9880 : 0x9800 + address;
			require(otp(address) == expected && otp(*m_flash, address, true) == expected, "all eight user words in both planes");
		}
	}
	virtual void machine_start() override
	{
		m_timer = timer_alloc(FUNC(st_otp_test_state::check), this);
		m_timer->adjust(attotime::from_msec(1));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		switch (m_phase)
		{
		case 0:
			require(otp(0x81) == 0xffff && otp(*m_seeded, 0x81) == 0x1234, "undumped versus explicit factory fixture");
			if (otp(0x85) == 0x1200)
			{
				check_words();
				require(otp(*m_seeded, 0x85) == 0x7f00, "cold nested OTP data");
				require(!(otp(0x80) & 2) && otp(0x8c) == 0x9880 && m_flash->base()[0] == 0x12, "cold persisted lock/data/array");
				program(0x85, 0); require(status() == 0x92 && otp(0x85) == 0x1200, "cold permanent lock enforced");
				m_flash->reset(); require(!(otp(0x80) & 2), "cold reset cannot unlock");
				osd_printf_info("ST flash OTP cold: PASS lock=1 user=8 factory_immutable=1 array=1 handset_claim=0\n");
				machine().schedule_exit(); break;
			}
			require(persist(*m_flash).size() == 0x400000, "fresh raw-array persistence");
			for (u32 address = 0x85; address <= 0x8c; ++address) require(otp(address) == 0xffff, "all eight user words erased");
			m_flash->write(0, 0xc0); snapshot(false);
			m_flash->reset(); snapshot(true); // Restore the second-cycle ownership.
			m_flash->write(0x85, 0x12ff); // FFh is data, not a read-array command.
			require(status() == 0 && m_flash->base()[0x10a] == 0xff, "OTP deferred and separate from array");
			m_flash->write(0, 0xb0); require(status() == 0, "OTP cannot suspend");
			snapshot(false); next(attotime::from_usec(20)); break;
		case 1:
			require(otp(0x85) == 0x12ff, "OTP first program");
			snapshot(true); require(status() == 0, "pending OTP restored");
			next(attotime::from_usec(20)); break;
		case 2:
			require(otp(0x85) == 0x12ff && otp(0x185) == 0x12ff && otp(*m_flash, 0x85, true) == 0x12ff, "OTP signature alias and CFI plane");
			program(0x85, 0xff00); next(attotime::from_usec(20)); break;
		case 3:
			require(otp(0x85) == 0x1200, "OTP one-way bits");
			for (u32 address = 0x81; address <= 0x84; ++address)
			{
				program(address, 0); require(status() == 0x92 && otp(address) == 0xffff, "factory write refused");
			}
			m_flash->write(0, 0x50);
			m_seeded->write(0, 0xc0); m_seeded->write(0x81, 0);
			require(otp(*m_seeded, 0x81) == 0x1234, "configured factory write refused");
			program(0x8d, 0); require(m_flash->base()[0x11a] == 0xff, "reserved OTP write cannot alter array");
			program(0x80, 0xfff9); require(otp(0x80) == 0xffff, "DQ2 prohibition cannot lock user segment");
			// Program the seven remaining user words.
			program(0x86, 0x9886); next(attotime::from_usec(20)); break;
		case 4: case 5: case 6: case 7: case 8: case 9:
			require(otp(0x82 + m_phase) == 0x9880 + 0x82 + m_phase - 0x80, "user word program");
			program(0x83 + m_phase, 0x9800 + 0x83 + m_phase);
			next(attotime::from_usec(20)); break;
		case 10:
			require(otp(0x8c) == 0x988c, "last user word");
			program(0x8c, 0xfff0); next(attotime::from_usec(20)); break;
		case 11:
			require(otp(0x8c) == 0x9880, "last user word one-way");
			program(0x87, 0); m_flash->reset();
			next(attotime::from_usec(20)); break;
		case 12:
			require(otp(0x87) == 0x9887, "reset aborts pending OTP without late commit");
			program(0x80, 0xfffd); next(attotime::from_usec(20)); break;
		case 13:
			require(otp(0x80) == 0xfffd, "only lock DQ1 programs");
			m_flash->wp_w(1); m_flash->reset();
			require(otp(0x80) == 0xfffd, "reset and WP cannot unlock OTP");
			program(0x80, 0xffff); require(status() == 0x92 && otp(0x80) == 0xfffd, "OTP lock cannot reverse");
			m_flash->write(0, 0x50); program(0x85, 0);
			require(status() == 0x92 && otp(0x85) == 0x1200, "permanent lock protects user data");
			m_flash->reset(); m_flash->base()[0] = 0x12;
			check_words();
			check_persistence();
			m_seeded->write(0, 0x50);
			m_seeded->write(0, 0x60); m_seeded->write(0x10000, 0xd0);
			m_seeded->write(0, 0x40); m_seeded->write(0x10000, 0);
			next(attotime::from_usec(20)); break;
		case 14:
			m_seeded->write(0, 0xff); require(m_seeded->read(0x10000) == 0, "nested erase precondition");
			m_seeded->write(0, 0x20); m_seeded->write(0x10000, 0xd0);
			next(attotime::from_msec(1)); break;
		case 15:
			m_seeded->write(0, 0xb0); require(m_seeded->read(0) == 0xc0, "erase suspended for OTP");
			m_seeded->write(0, 0xc0); m_seeded->write(0x85, 0x7f00);
			m_seeded->write(0, 0xb0); require(m_seeded->read(0) == 0x40, "nested OTP cannot suspend");
			snapshot(false); next(attotime::from_usec(20)); break;
		case 16:
			require(otp(*m_seeded, 0x85) == 0x7f00, "nested OTP completion");
			snapshot(true); require(m_seeded->read(0) == 0x40, "nested OTP pending restored");
			next(attotime::from_usec(20)); break;
		case 17:
			require(otp(*m_seeded, 0x85) == 0x7f00, "restored nested OTP completion");
			m_seeded->write(0, 0x70); require(m_seeded->read(0) == 0xc0, "erase stays suspended after OTP");
			m_seeded->write(0, 0xd0); next(attotime::from_msec(1100)); break;
		case 18:
			m_seeded->write(0, 0xff); require(m_seeded->read(0x10000) == 0xffff, "erase resumes after nested OTP");
			require(otp(*m_seeded, 0x85) == 0x7f00, "array erase cannot erase OTP");
			osd_printf_info("ST flash OTP: PASS user_words=8 factory_words=4 deferred=1 one_way=1 immutable_factory=1 permanent_lock=1 wp_reset=1 nonsuspendable=1 restore=3 nested_erase=1 reset_abort=1 reserved_refusal=2 atomic_import_refusal=7 short_write_refusal=2 raw_compat=1 seed_binding=1 tail_bytes=26 handset_claim=0\n");
			machine().schedule_exit(); break;
		}
	}
};
ROM_START(stotptst)
	// Synthetic test-only identity, never supplied by handset configurations.
	ROM_REGION(26, "seeded:otp", ROMREGION_ERASEFF)
	ROM_FILL(2, 1, 0x12)
	ROM_FILL(3, 1, 0x34)
	ROM_FILL(10, 1, 0x7f)
ROM_END
}
CONS(2026, stotptst, 0, 0, test, c3_test, st_otp_test_state, empty_init,
		"Test", "ST M28W320ECT OTP conformance", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)
