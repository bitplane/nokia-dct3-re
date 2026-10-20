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
