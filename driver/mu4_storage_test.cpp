// license:BSD-3-Clause
// copyright-holders:Gaz

// Isolated original MU4 routine execution, not a DA150 or handset boot profile.
#include "emu.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "machine/nandflash.h"
#include <vector>

namespace {
class mu4_storage_test_state : public driver_device
{
public:
	mu4_storage_test_state(machine_config const &config, device_type type, char const *tag)
		: driver_device(config, type, tag), m_cpu(*this, "cpu"), m_nand(*this, "nand"), m_cinit(*this, "cinit"),
		  m_initdisk(*this, "initdisk"), m_disk_cinit(*this, "disk_cinit"), m_disk_helpers(*this, "disk_helpers"),
		  m_disk_vectors(*this, "disk_vectors") { }
	void test(machine_config &config)
	{
		// Test clock only; DA150 PLL/physical memory mapping is not claimed here.
		TMS320C54X(config, m_cpu, 13'000'000);
		m_cpu->set_extended_program(true);
		m_cpu->set_addrmap(AS_PROGRAM, &mu4_storage_test_state::program_map);
		m_cpu->set_addrmap(AS_DATA, &mu4_storage_test_state::data_map);
		m_cpu->set_addrmap(AS_IO, &mu4_storage_test_state::io_map);
		m_cpu->bio_in_cb().set(FUNC(mu4_storage_test_state::bio_r));
		SAMSUNG_K9K1208U0A(config, m_nand);
		m_nand->rnb_wr_callback().set(FUNC(mu4_storage_test_state::ready_w));
	}
private:
	required_device<tms320c54x_device> m_cpu;
	required_device<samsung_k9k1208u0a_device> m_nand;
	required_region_ptr<u16> m_cinit;
	required_region_ptr<u16> m_initdisk, m_disk_cinit, m_disk_helpers, m_disk_vectors;
	u16 m_serial_index[2] = {}, m_serial_regs[2][32] = {}, m_serial_word = 0;
	bool m_serial_ready = false;
	unsigned m_serial_reads = 0;
	u8 m_direction = 0, m_gpio = 0;
	bool m_ready = true;
	unsigned m_bio_reads = 0, m_busy_reads = 0, m_data_reads = 0;
	attotime m_read_started, m_first_data;
	std::vector<u16> m_writes;
	emu_timer *m_check = nullptr;
	unsigned m_phase = 0;
	unsigned m_page = 0;
	static u16 pattern(unsigned index) { return u16(0x1234 + index * 37); }
	void program_map(address_map &map)
	{
		// This is a routine-test address space, not the unknown DA150 silicon map.
		map(0, 0xffff).ram();
		map(0x0200, 0x0f32).rom().region("entry", 0);
		map(0x2080, 0x20ad).rom().region("trampolines", 0);
		map(0x20ae, 0x3679).rom().region("library", 0);
	}
	void data_map(address_map &map)
	{
		map(0, 0xffff).ram();
		map(0x0031, 0x0031).r(FUNC(mu4_storage_test_state::serial_r));
		map(0x0034, 0x0035).rw(FUNC(mu4_storage_test_state::serial_config_r), FUNC(mu4_storage_test_state::serial_config_w));
		map(0x0038, 0x0039).rw(FUNC(mu4_storage_test_state::serial_config0_r), FUNC(mu4_storage_test_state::serial_config0_w));
		map(0x003c, 0x003d).rw(FUNC(mu4_storage_test_state::gpio_r), FUNC(mu4_storage_test_state::gpio_w));
	}
	// Word-level receive fixture only; serial clocks/framing are not modeled here.
	u16 serial_r()
	{
		if (!m_serial_ready) fatalerror("MU4 serial read without delivered word");
		m_serial_ready = false;
		m_serial_regs[0][0] &= ~u16(2);
		++m_serial_reads;
		return m_serial_word;
	}
	u16 serial_config_r(offs_t offset) { return offset ? m_serial_regs[0][m_serial_index[0]] : m_serial_index[0]; }
	void serial_config_w(offs_t offset, u16 value)
	{
		if (offset) m_serial_regs[0][m_serial_index[0]] = value;
		else m_serial_index[0] = value & 31;
	}
	u16 serial_config0_r(offs_t offset) { return offset ? m_serial_regs[1][m_serial_index[1]] : m_serial_index[1]; }
	void serial_config0_w(offs_t offset, u16 value)
	{
		if (offset) m_serial_regs[1][m_serial_index[1]] = value;
		else m_serial_index[1] = value & 31;
	}
	void deliver_serial(u8 value)
	{
		if (m_serial_ready || !(m_serial_regs[0][0] & 1)) fatalerror("MU4 serial fixture not receive-ready");
		m_serial_word = value;
		m_serial_ready = true;
		m_serial_regs[0][0] |= 2;
		m_cpu->set_input_line(6, ASSERT_LINE);
		m_cpu->set_input_line(6, CLEAR_LINE);
	}
	void io_map(address_map &map)
	{
		map(0x4000, 0x7fff).rw(FUNC(mu4_storage_test_state::nand_r), FUNC(mu4_storage_test_state::nand_w));
	}
	u16 gpio_r(offs_t offset) { return offset ? m_gpio : m_direction; }
	void gpio_w(offs_t offset, u16 value)
	{
		if (offset) m_gpio = value; else m_direction = value;
		if (BIT(m_direction, 2)) m_nand->ce_w(BIT(m_gpio, 2));
	}
	void ready_w(int state)
	{
		m_ready = bool(state);
		if (!state) m_read_started = machine().time();
	}
	int bio_r()
	{
		++m_bio_reads;
		if (!m_ready) ++m_busy_reads;
		return m_ready;
	}
	void nand_w(offs_t offset, u16 value)
	{
		// ADD_H is held enabled by this fixture; U202's lower 14 address bits alias.
		if ((m_direction & 7) != 7) fatalerror("MU4 storage: strobe before GPIO outputs enabled");
		m_writes.push_back((u16(m_gpio & 7) << 8) | (value & 0xff));
		if (BIT(m_gpio, 2)) return;
		if (BIT(m_gpio, 0)) m_nand->command_w(value);
		else if (BIT(m_gpio, 1)) m_nand->address_w(value);
		else m_nand->data_w(value);
	}
	u16 nand_r()
	{
		if (!m_data_reads) m_first_data = machine().time();
		++m_data_reads;
		return m_nand->data_r();
	}
	virtual void machine_start() override
	{
		m_check = timer_alloc(FUNC(mu4_storage_test_state::check), this);
	}
	virtual void machine_reset() override
	{
		m_direction = m_gpio = 0;
		m_ready = true;
		m_bio_reads = m_busy_reads = m_data_reads = 0;
		m_writes.clear();
		auto &program = m_cpu->space(AS_PROGRAM);
		// Synthetic wrapper initializes the routine ABI, then calls untouched code.
		u16 const wrapper[] = {
			0x7718, 0x1200, // STM #1200,SP.
			0xf7be,         // SSBX CPL: stack-relative direct operands.
			0x76f8, 0x003c, 7, 0x76f8, 0x003d, 7,
			0xf6b8, 0xf020, u16(m_phase ? 0xffff : 0), // Unsigned logical row; 3035 adds one.
			0xf980, 0x3035, 0xf980, 0x306c, 0xf4e1
		};
		for (unsigned i = 0; i < std::size(wrapper); ++i) program.write_word(0x1800 + i, wrapper[i]);
		if (m_phase == 4)
		{
			// Flush is called with NAND already selected by the storage reader.
			program.write_word(0x1808, 3);
			// Routine ABI fixture: dirty cache contents and its physical row base.
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x3b12, 0); data.write_word(0x3b13, 64);
			data.write_word(0x3b14, 1);
			for (unsigned i = 0; i < 8192; ++i) data.write_word(0x4000 + i, pattern(i));
			program.write_word(0x1809, 0xf980); program.write_word(0x180a, 0x0725);
			program.write_word(0x180b, 0xf4e1);
		}
		if (m_phase == 6)
		{
			// Original startup's mount ABI: filesystem context and partition-aware flag.
			u16 const mount[] = {0x7600, 1, 0xf020, 0x3aea, 0xf980, 0x308e, 0xf4e1};
			for (unsigned i = 0; i < std::size(mount); ++i) program.write_word(0x1809 + i, mount[i]);
		}
		if (m_phase == 9)
		{
			u16 const format[] = {0xf020, 0x3aea, 0xf980, 0x051d, 0xf4e1};
			for (unsigned i = 0; i < std::size(format); ++i) program.write_word(0x1809 + i, format[i]);
		}
		if (m_phase == 7)
		{
			// Original C globals are routine inputs, not an invented disk image.
			auto &data = m_cpu->space(AS_DATA);
			unsigned cursor = 0;
			while (cursor < m_cinit.length())
			{
				unsigned const count = m_cinit[cursor++];
				if (!count) break;
				if (cursor + count + 1 > m_cinit.length()) fatalerror("MU4 fixture truncated cinit");
				unsigned const destination = m_cinit[cursor++];
				if (destination + count > 0x10000) fatalerror("MU4 fixture wrapping cinit");
				for (unsigned i = 0; i < count; ++i) data.write_word(destination + i, m_cinit[cursor++]);
			}
			if (cursor != m_cinit.length()) fatalerror("MU4 fixture incomplete cinit");
			u16 const recovery[] = {0xf020, 0x3aea, 0xf980, 0x0880, 0xf4e1};
			for (unsigned i = 0; i < std::size(recovery); ++i) program.write_word(0x1809 + i, recovery[i]);
		}
		if (m_phase == 10 || m_phase == 12 || m_phase == 13 || m_phase == 14)
		{
			// Replace the routine library with unchanged InitDisk code, not a flat overlay image.
			program.install_rom(0x256d, 0x4aa8, &m_initdisk[0]);
			program.install_rom(0x4aa9, 0x4abe, &m_disk_helpers[0]);
			program.install_rom(0x0080, 0x00f7, &m_disk_vectors[0]);
			auto &data = m_cpu->space(AS_DATA);
			unsigned cursor = 0;
			while (cursor < m_disk_cinit.length())
			{
				unsigned const count = m_disk_cinit[cursor++];
				if (!count) break;
				if (cursor + count + 1 > m_disk_cinit.length()) fatalerror("MU4 InitDisk truncated cinit");
				unsigned const destination = m_disk_cinit[cursor++];
				if (destination + count > 0x10000) fatalerror("MU4 InitDisk wrapping cinit");
				for (unsigned i = 0; i < count; ++i) data.write_word(destination + i, m_disk_cinit[cursor++]);
			}
			if (cursor != m_disk_cinit.length()) fatalerror("MU4 InitDisk incomplete cinit");
			// Original ABI: block zero in A, original NAND geometry context on the stack.
			u16 const marker[] = {0xf074, 0x377a, 0x7600, 0x0585, 0xf020, 0, 0xf074, 0x2889, 0xf4e1};
			for (unsigned i = 0; i < std::size(marker); ++i) program.write_word(0x1809 + i, marker[i]);
			if (m_phase == 13)
			{
				u16 const scan[] = {0xf074, 0x377a, 0x7600, 0x2000, 0x7601, 0x2100,
					0xf020, 0x068c, 0xf074, 0x36b9, 0xf4e1};
				for (unsigned i = 0; i < std::size(scan); ++i) program.write_word(0x1809 + i, scan[i]);
			}
			if (m_phase == 14)
			{
				u16 const receive[] = {0xf074, 0x346a, 0x771d, 0x00e8, 0x7700, 0x0040,
					0xf6bb, 0xf4e1, 0xf073, 0x1810};
				for (unsigned i = 0; i < std::size(receive); ++i) program.write_word(0x1809 + i, receive[i]);
			}
		}
		if (m_phase == 17)
		{
			u16 const consume[] = {0xf074, 0x34c0, 0xf4e1};
			for (unsigned i = 0; i < std::size(consume); ++i) program.write_word(0x1809 + i, consume[i]);
		}
		// Disable the unrelated core timer so it cannot wake the completion IDLE.
		program.write_word(0x17fe, 0x7726); program.write_word(0x17ff, 0x0010);
		program.write_word(0xff80, 0xf073); program.write_word(0xff81, 0x17fe);
		m_check->adjust(attotime::from_msec(m_phase == 13 ? 8000 : (m_phase == 7 || m_phase == 14) ? 500 : m_phase == 4 ? 250 : m_phase >= 6 ? 50 : 1));
	}
	void start_media_read()
	{
		m_nand->command_w(0);
		m_nand->address_w(0); m_nand->address_w(64 + m_page);
		m_nand->address_w(0); m_nand->address_w(0);
		m_check->adjust(attotime::from_usec(11));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		if (m_phase == 14 || m_phase == 15 || m_phase == 16)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 serial ISR did not return to idle phase=%u pc=%04x sp=%04x illegal=%u", m_phase,
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
			if (m_phase == 14)
			{
				if (data.read_word(0x4bbf) != 0xff || data.read_word(0x4bc0) != 0xff)
					fatalerror("MU4 original serial ring initialization mismatch");
				deliver_serial(0x12);
			}
			else
			{
				unsigned const index = m_phase - 15;
				if (m_serial_ready || m_serial_reads != index + 1 || data.read_word(0x4bbf) != index ||
					data.read_word(0x4abf + index) != (index ? 0x34 : 0x12))
					fatalerror("MU4 original serial ISR ring delivery mismatch");
				if (m_phase == 16)
				{
					m_phase = 17;
					machine().schedule_soft_reset();
					return;
				}
				deliver_serial(0x34);
			}
			++m_phase;
			m_check->adjust(attotime::from_msec(1));
			return;
		}
		if (m_phase == 8)
		{
			auto &data = m_cpu->space(AS_DATA);
			for (unsigned i = 0; i < 512; ++i)
			{
				u16 const word = data.read_word(0x1746 + i / 2);
				u8 const expected = (i & 1) ? u8(word) : u8(word >> 8);
				if (m_nand->data_r() != expected) fatalerror("MU4 recovery template mismatch byte=%u", i);
			}
			logerror("mu4_storage_template: PASS original_template=512 physical_row=32\n");
			m_phase = 9;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 1 || m_phase == 3 || m_phase == 11)
		{
			if (m_nand->is_busy()) fatalerror("MU4 storage test pattern program did not complete");
			++m_phase;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 5)
		{
			if (m_nand->is_busy()) fatalerror("MU4 flush media read still busy");
			for (unsigned i = 0; i < 256; ++i)
			{
				u16 const low = m_nand->data_r();
				u16 const value = low | (u16(m_nand->data_r()) << 8);
				if (value != pattern(m_page * 256 + i))
					fatalerror("MU4 flush media mismatch page=%u word=%u actual=%04x", m_page, i, value);
			}
			for (unsigned i = 0; i < 16; ++i)
				if (m_nand->data_r() != 0xff) fatalerror("MU4 flush spare fill mismatch");
			if (++m_page < 32) { start_media_read(); return; }
			logerror("mu4_storage_flush: PASS pages=32 words=8192 spare=512 erase=1 program=32 readback=32\n");
			m_phase = 6;
			machine().schedule_soft_reset();
			return;
		}
		u16 const pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
		if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) ||
				!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
			fatalerror("MU4 original storage routine did not finish: pc=%04x illegal=%u writes=%u reads=%u",
				pc, unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), unsigned(m_writes.size()), m_data_reads);
		if (m_phase == 9)
		{
			unsigned mutations = 0;
			for (u16 value : m_writes) mutations += value == 0x0160 || value == 0x0180;
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 1 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_data_reads || mutations)
				fatalerror("MU4 format prerequisite contract mismatch");
			logerror("mu4_storage_format: PASS missing_boot_record rejected=1 media_mutations=0\n");
			m_phase = 10;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 10 || m_phase == 12)
		{
			unsigned spare_reads = 0, status_reads = 0;
			for (u16 value : m_writes) { spare_reads += value == 0x0150; status_reads += value == 0x0170; }
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != (m_phase == 10 ? 0xff : 0) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_busy_reads ||
				spare_reads != (m_phase == 10 ? 2 : 1) || status_reads != spare_reads)
				fatalerror("MU4 InitDisk marker contract mismatch a=%llx sp=%04x spare=%u status=%u reads=%u busy=%u",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), spare_reads, status_reads, m_data_reads, m_busy_reads);
			if (m_phase == 12)
			{
				logerror("mu4_initdisk_marker: PASS erased=ff bad_marker=00 original_spare_reader=1\n");
				m_phase = 13;
				machine().schedule_soft_reset();
				return;
			}
			// External bad-block fixture: program only spare byte 5 of physical page zero.
			m_nand->command_w(0x50); m_nand->command_w(0x80);
			m_nand->address_w(5); m_nand->address_w(0); m_nand->address_w(0); m_nand->address_w(0);
			m_nand->data_w(0); m_nand->command_w(0x10);
			m_phase = 11;
			m_check->adjust(attotime::from_usec(201));
			return;
		}
		if (m_phase == 13)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || data.read_word(0x2000))
				fatalerror("MU4 InitDisk scan return/reserved bitmap mismatch");
			for (unsigned i = 0; i < 256; ++i)
				// 256d byte-swaps its source bitmap in place before NAND writeback.
				if (data.read_word(0x2100 + i) != (i == 0 ? 0x0100 : 0))
					fatalerror("MU4 InitDisk bitmap mismatch group=%u value=%04x", i, data.read_word(0x2100 + i));
			logerror("mu4_initdisk_scan: PASS blocks=4096 bad_block=0 reserved_bad=0 reads=%u busy_reads=%u\n", m_data_reads, m_busy_reads);
			m_phase = 14;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 17)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0x1234 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 ||
				m_cpu->space(AS_DATA).read_word(0x4bc0) != 1)
				fatalerror("MU4 original serial word consumer mismatch");
			logerror("mu4_initdisk_serial: PASS vector=00d8 register=0031 words=2 value=1234 firmware_ring=1\n");
			machine().schedule_exit();
			return;
		}
		if (m_phase == 6)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 1 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_busy_reads || !m_data_reads)
				fatalerror("MU4 erased mount contract mismatch a=%llx sp=%04x reads=%u busy=%u",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), m_data_reads, m_busy_reads);
			logerror("mu4_storage_mount: PASS erased_boot_sector rejected=1 reads=%u busy_reads=%u\n", m_data_reads, m_busy_reads);
			m_phase = 7;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 7)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_busy_reads)
				fatalerror("MU4 original template writer failed");
			m_nand->command_w(0); m_nand->address_w(0); m_nand->address_w(32);
			m_nand->address_w(0); m_nand->address_w(0);
			m_phase = 8;
			m_check->adjust(attotime::from_usec(11));
			return;
		}
		if (m_phase == 4)
		{
			unsigned erase = 0, program = 0;
			for (u16 value : m_writes) { erase += value == 0x0160; program += value == 0x0180; }
			if (erase != 1 || program != 32 || !m_busy_reads ||
				m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 ||
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x4000) ||
				m_cpu->space(AS_DATA).read_word(0x3b14) != 0)
				fatalerror("MU4 flush failed a=%llx sp=%04x erase=%u program=%u busy_reads=%u",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), erase, program, m_busy_reads);
			m_phase = 5;
			start_media_read();
			return;
		}
		std::vector<u16> const expected = m_phase ?
			std::vector<u16>{0x0100, 0x0200, 0x0200, 0x0200, 0x0201} :
			std::vector<u16>{0x0100, 0x0200, 0x0201, 0x0200, 0x0200};
		if (m_writes != expected || m_data_reads != 2 || !m_busy_reads ||
				m_cpu->state_int(tms320c54x_device::STATE_A) != (m_phase ? 0x3412 : 0xffff) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 ||
				m_first_data - m_read_started < attotime::from_usec(10))
		{
			for (u16 word : m_writes) logerror("mu4_storage_write=%04x\n", word);
			fatalerror("MU4 storage contract mismatch: pc=%04x a=%010llx sp=%04x busy_reads=%u data_reads=%u",
				pc, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), m_busy_reads, m_data_reads);
		}
		if (!m_phase)
		{
			logerror("mu4_storage_erased: PASS row=1 value=ffff bio_reads=%u busy_reads=%u\n", m_bio_reads, m_busy_reads);
			// External device-conformance pattern, not a provisioned filesystem or firmware state.
			m_nand->command_w(0); m_nand->command_w(0x80);
			m_nand->address_w(0); m_nand->address_w(0); m_nand->address_w(0); m_nand->address_w(1);
			m_nand->data_w(0x12); m_nand->data_w(0x34); m_nand->command_w(0x10);
			m_phase = 1;
			m_check->adjust(attotime::from_usec(201));
			return;
		}
		logerror("mu4_storage_original: PASS rows=1,65536 values=ffff,3412 writes=5 reads=2 bio_reads=%u busy_reads=%u\n",
			m_bio_reads, m_busy_reads);
		// Non-erased marker makes the original block erase necessary before programming.
		m_nand->command_w(0); m_nand->command_w(0x80);
		m_nand->address_w(0); m_nand->address_w(64); m_nand->address_w(0); m_nand->address_w(0);
		m_nand->data_w(0); m_nand->command_w(0x10);
		m_phase = 3;
		m_check->adjust(attotime::from_usec(201));
	}
};
static INPUT_PORTS_START(mu4_storage_test) INPUT_PORTS_END
ROM_START(mu4nand)
	ROM_REGION16_LE(240, "disk_vectors", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_vectors.bin", 0, 240, CRC(2f1b5de4) SHA1(145048a918768dbee2d5fce5213a6e26ece50edf))
	ROM_REGION16_LE(44, "disk_helpers", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_helpers.bin", 0, 44, CRC(6e84f71a) SHA1(fa38ceaa537490d6d57c84a6406990aad5b869a6))
	ROM_REGION16_LE(19064, "initdisk", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_library.bin", 0, 19064, CRC(e9ecd12c) SHA1(748672ed6cac0e864fe0e70a9026b8cafaf8a7bc))
	ROM_REGION16_LE(2522, "disk_cinit", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_cinit.bin", 0, 2522, CRC(535a37c1) SHA1(78d90095d93a4cbed326dff901671ece0004462e))
	ROM_REGION16_LE(546, "cinit", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_cinit.bin", 0, 546, CRC(9d1fb99f) SHA1(d08dd94e471519ed69869b3685a8a6c9cd393f58))
	ROM_REGION16_LE(92, "trampolines", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_trampolines.bin", 0, 92, CRC(0de68a60) SHA1(3b2b1ac46f7c0cef5c5c1288a7cb0bcf1bb817de))
	ROM_REGION16_LE(6758, "entry", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_entry.bin", 0, 6758, CRC(aa68038b) SHA1(c4d0c3b8211569ee4ec23d2945a9855b5423ec89))
	ROM_REGION16_LE(11160, "library", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_library.bin", 0, 11160, CRC(f3b1ddd5) SHA1(e6d082993f823ebbff46f33bc36402a8e8c0c5d8))
ROM_END
} // anonymous namespace
CONS(2026, mu4nand, 0, 0, test, mu4_storage_test, mu4_storage_test_state, empty_init,
		"Test", "MU4 original NAND routine boundary", MACHINE_NO_SOUND_HW)
