// license:BSD-3-Clause
// copyright-holders:Gaz

// Development-only flash conformance fixture, not a handset provisioning path.
#include "emu.h"
#include "machine/intelfsh.h"
#include "nokia_b3_flash.h"
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
class b3_test_state : public driver_device
{
public:
	b3_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_flash(*this, "flash"), m_adapter(*this, "adapter") { }
	void test(machine_config &config)
	{
		ST_M28W320ECT(config, m_flash);
		NOKIA_B3_FLASH(config, m_adapter);
		m_adapter->set_enabled(true);
		m_adapter->set_status_csr(CSR);
	}
private:
	static constexpr u32 CSR = 0x100, ERASE = 0x10000, OTHER = 0x20000;
	required_device<st_m28w320ect_device> m_flash;
	required_device<nokia_b3_flash_device> m_adapter;
	void require(bool value, char const *name)
	{
		if (!value) fatalerror("B3 adapter conformance: %s", name);
	}
	void program(u32 address, u16 value)
	{
		m_adapter->write(address, 0x40);
		m_adapter->write(address, value);
	}
	virtual void machine_start() override
	{
		timer_alloc(FUNC(b3_test_state::check), this)->adjust(attotime::from_msec(1));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		m_adapter->write(0, 0x90);
		require(m_adapter->read(0) == 0x20 && m_adapter->read(1) == 0x88ba, "fitted identity");
		m_adapter->write(0, 0xff);
		program(ERASE, 0);
		program(ERASE + 0x8000, 0x1234);
		m_adapter->write(ERASE, 0x20);
		m_adapter->write(ERASE, 0xd0);
		require(m_adapter->read(CSR) == 0, "active erase status");
		m_adapter->write(0, 0xb0);
		require(m_adapter->read(CSR) == 0xc0, "suspended erase status");
		// A program data word must never be reinterpreted as FF/Read Array.
		// Programming another block is permitted while erase is suspended,
		// not while the erase engine is running.
		program(OTHER, 0x12ff);
		require(m_adapter->read(CSR) == 0xc0, "command-like program data changed erase read mode");
		require(m_adapter->read(ERASE + 0x8000) == 0x1234, "neighbor block isolation");
		require(m_adapter->read(OTHER) == 0x12ff, "programmed data preserved");
		std::ostringstream saved(std::ios::binary);
		require(machine().save().write_stream(saved) == STATERR_NONE, "suspended state save");
		m_adapter->write(0, 0xd0);
		require(m_adapter->read(CSR) == 0, "resume status");
		std::istringstream restore(saved.str(), std::ios::binary);
		require(machine().save().read_stream(restore) == STATERR_NONE, "suspended state restore");
		require(m_adapter->read(CSR) == 0xc0, "restored suspended status");
		m_adapter->write(0, 0xff);
		program(OTHER, 0xfff0);
		m_adapter->write(0, 0xff);
		require(m_adapter->read(OTHER) == 0x12f0, "one-way NOR program");
		osd_printf_info("B3 adapter conformance: PASS id=1 command_like_data=1 nor=1 erase_geometry=1 suspend_resume=1 restore=1 fixed_status_compatibility=1 protection_claim=0 timing_claim=0 handset_claim=0\n");
		machine().schedule_exit();
	}
};
ROM_START(b3test) ROM_END
}
CONS(2026, b3test, 0, 0, test, c3_test, b3_test_state, empty_init,
		"Test", "ST B3 adapter conformance", MACHINE_NO_SOUND_HW | MACHINE_SUPPORTS_SAVE)
