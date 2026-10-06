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
		: driver_device(config, type, tag), m_cpu(*this, "cpu"), m_nand(*this, "nand") { }
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
		map(0x20ae, 0x3679).rom().region("library", 0);
	}
	void data_map(address_map &map)
	{
		map(0, 0xffff).ram();
		map(0x003c, 0x003d).rw(FUNC(mu4_storage_test_state::gpio_r), FUNC(mu4_storage_test_state::gpio_w));
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
		// Disable the unrelated core timer so it cannot wake the completion IDLE.
		program.write_word(0x17fe, 0x7726); program.write_word(0x17ff, 0x0010);
		program.write_word(0xff80, 0xf073); program.write_word(0xff81, 0x17fe);
		m_check->adjust(attotime::from_msec(m_phase == 4 ? 250 : 1));
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
		if (m_phase == 1 || m_phase == 3)
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
			machine().schedule_exit();
			return;
		}
		u16 const pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
		if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) ||
				!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
			fatalerror("MU4 original storage routine did not finish: pc=%04x illegal=%u writes=%u reads=%u",
				pc, unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), unsigned(m_writes.size()), m_data_reads);
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
	ROM_REGION16_LE(6758, "entry", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_entry.bin", 0, 6758, CRC(aa68038b) SHA1(c4d0c3b8211569ee4ec23d2945a9855b5423ec89))
	ROM_REGION16_LE(11160, "library", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_library.bin", 0, 11160, CRC(f3b1ddd5) SHA1(e6d082993f823ebbff46f33bc36402a8e8c0c5d8))
ROM_END
} // anonymous namespace
CONS(2026, mu4nand, 0, 0, test, mu4_storage_test, mu4_storage_test_state, empty_init,
		"Test", "MU4 original NAND routine boundary", MACHINE_NO_SOUND_HW)
