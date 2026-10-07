// license:BSD-3-Clause
// copyright-holders:Gaz

// Isolated original MU4 routine execution, not a DA150 or handset boot profile.
#include "emu.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "machine/nandflash.h"
#include "tms320c54x_dma.h"
#include <vector>
#include <sstream>

namespace {
class mu4_storage_test_state : public driver_device
{
public:
	mu4_storage_test_state(machine_config const &config, device_type type, char const *tag)
				: driver_device(config, type, tag), m_cpu(*this, "cpu"), m_dma(*this, "dma"), m_program_ram(*this, "program_ram"), m_nand(*this, "nand"), m_cinit(*this, "cinit"),
		  m_initdisk(*this, "initdisk"), m_disk_cinit(*this, "disk_cinit"), m_disk_helpers(*this, "disk_helpers"),
		  m_disk_vectors(*this, "disk_vectors"), m_segment(*this, "segment") { }
	void test(machine_config &config)
	{
		// Test clock only; DA150 PLL/physical memory mapping is not claimed here.
		TMS320C54X(config, m_cpu, 13'000'000);
		m_cpu->set_extended_program(true);
		m_cpu->set_addrmap(AS_PROGRAM, &mu4_storage_test_state::program_map);
		m_cpu->set_addrmap(AS_DATA, &mu4_storage_test_state::data_map);
		m_cpu->set_addrmap(AS_IO, &mu4_storage_test_state::io_map);
		m_cpu->bio_in_cb().set(FUNC(mu4_storage_test_state::bio_r));
		TMS320C54X_DMA(config, m_dma, 13'000'000);
		m_dma->set_cpu(m_cpu);
		SAMSUNG_K9K1208U0A(config, m_nand);
		m_nand->rnb_wr_callback().set(FUNC(mu4_storage_test_state::ready_w));
	}
private:
	required_device<tms320c54x_device> m_cpu;
	required_device<tms320c54x_dma_device> m_dma;
	required_shared_ptr<u16> m_program_ram;
	required_device<samsung_k9k1208u0a_device> m_nand;
	required_region_ptr<u16> m_cinit;
	required_region_ptr<u16> m_initdisk, m_disk_cinit, m_disk_helpers, m_disk_vectors;
	required_region_ptr<u8> m_segment;
	unsigned m_segment_cursor = 0, m_segment_waits = 0;
	unsigned m_file_source = 0, m_file_size = 0, m_file_cursor = 0, m_files_checked = 0;
	u16 m_file_marker = 0;
	std::vector<u16> m_verified_files;
	unsigned m_read_words = 0;
	std::stringstream m_saved_dma;
	u16 m_serial_index[2] = {}, m_serial_regs[2][32] = {}, m_serial_word = 0;
	bool m_serial_ready = false;
	unsigned m_serial_reads = 0;
	u8 m_direction = 0, m_gpio = 0;
	bool m_ready = true;
	unsigned m_bio_reads = 0, m_busy_reads = 0, m_data_reads = 0;
	attotime m_read_started, m_first_data;
	std::vector<u16> m_writes;
	emu_timer *m_check = nullptr;
	unsigned m_phase = 25;
	unsigned m_native_entry_reads = 0, m_native_far_reads = 0;
	unsigned m_native_mcbsp_trace = 0, m_native_mcbsp_polls = 0;
	unsigned m_native_adjacent_reads = 0;
	u16 m_native_mcbsp_index = 0, m_native_mcbsp_status = 0;
	unsigned m_page = 0;
	static u16 pattern(unsigned index) { return u16(0x1234 + index * 37); }
	void program_map(address_map &map)
	{
		// This is a routine-test address space, not the unknown DA150 silicon map.
		map(0, 0x7fffff).ram().share("program_ram");
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
		map(0x0054, 0x0057).rw(m_dma, FUNC(tms320c54x_dma_device::read), FUNC(tms320c54x_dma_device::write));
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
		if (m_phase >= 25 && m_phase <= 29)
		{
			program.write_word(0x1809, 0xf4e1);
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x6000, 0x1234); data.write_word(0x6001, 0xabcd);
			program.write_word(0x2ffff, 0xffff); program.write_word(0x20000, 0xffff);
			data.write_word(0x6100, 0xffff);
			data.write_word(0x55, 0);
			data.write_word(0x56, 0x6000);
			data.write_word(0x56, m_phase == 25 ? 0xffff : 0x6100);
			data.write_word(0x56, m_phase == 25 ? 1 : 0);
			data.write_word(0x56, 0);
			data.write_word(0x56, m_phase == 25 ? 0x0144 : 0x0145);
			if (data.read_word(0x55) != 5) fatalerror("MU4 DMA autoincrement mismatch");
			data.write_word(0x55, 0x1e); data.write_word(0x56, 0); data.write_word(0x56, 0xff82);
			data.write_word(0x55, 0x1f);
			if (data.read_word(0x57) != 2 || data.read_word(0x55) != 0x1f) fatalerror("MU4 DMA page/nonincrement mismatch");
			data.write_word(0x54, 1);
			if (!(data.read_word(0x54) & 1) || data.read_word(0x6100) != 0xffff || program.read_word(0x2ffff) != 0xffff)
				fatalerror("MU4 DMA completed synchronously");
			if (m_phase == 27) data.write_word(0x54, 0);
		}
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
		if (m_phase == 6 || m_phase == 19 || m_phase == 21 || m_phase == 24)
		{
			if (m_phase == 19 || m_phase == 21 || m_phase == 24)
			{
				program.install_rom(0x20ae, 0x3679, reinterpret_cast<u16 *>(memregion("library")->base()));
				// Remove InitDisk-only ROM mappings before the consumer DMA writes these addresses.
				program.install_ram(0x367a, 0x4abe, &m_program_ram[0x367a]);
			}
			// Original startup's mount ABI: filesystem context and partition-aware flag.
			u16 const mount[] = {0x7600, 1, 0xf020, 0x3aea, 0xf980, 0x308e, 0xf4e1};
			for (unsigned i = 0; i < std::size(mount); ++i) program.write_word(0x1809 + i, mount[i]);
			if (m_phase == 21 || m_phase == 24)
			{
				// Same directory ABI as startup 0960..096b; no descriptor/file-state injection.
				u16 const directory[] = {0x7600, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x09c6,
					0x7600, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x09fa, 0xf4e1};
				for (unsigned i = 0; i < std::size(directory); ++i) program.write_word(0x180f + i, directory[i]);
				if (m_phase == 24)
				{
					// Loader-safe stack matches original 3538; isolate 2ed4 before its program transfer.
					u16 const load[] = {0x7718, 0x3aea, 0xe800, 0xf980, 0x2ed4, 0xf4e1};
					for (unsigned i = 0; i < std::size(load); ++i) program.write_word(0x181b + i, load[i]);
				}
			}
		}
		if (m_phase == 9)
		{
			u16 const format[] = {0xf020, 0x3aea, 0xf980, 0x051d, 0xf4e1};
			for (unsigned i = 0; i < std::size(format); ++i) program.write_word(0x1809 + i, format[i]);
		}
		if (m_phase == 22)
		{
			unsigned cursor = 0x1809;
			if (!m_file_cursor)
			{
				u16 const seek[] = {0x7600, 0, 0x7601, 0, 0x7602, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x33ea};
				for (u16 word : seek) program.write_word(cursor++, word);
			}
			m_read_words = std::min(256U, (m_file_size - m_file_cursor) / 2);
			u16 const read[] = {0x7600, 0x6000, 0x7601, u16(m_read_words), 0x7602, 0x3aea,
				0xf020, 0x36b0, 0xf980, 0x32d3, 0xf4e1};
			for (u16 word : read) program.write_word(cursor++, word);
		}
		if (m_phase == 23)
		{
			u16 const next[] = {0x7600, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x0a0b, 0xf4e1};
			for (unsigned i = 0; i < std::size(next); ++i) program.write_word(0x1809 + i, next[i]);
		}
		if (m_phase == 7 || m_phase == 19 || m_phase == 21 || m_phase == 24)
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
			if (m_phase == 7)
			{
				u16 const recovery[] = {0xf020, 0x3aea, 0xf980, 0x0880, 0xf4e1};
				for (unsigned i = 0; i < std::size(recovery); ++i) program.write_word(0x1809 + i, recovery[i]);
			}
		}
		if (m_phase == 10 || m_phase == 12 || m_phase == 13 || m_phase == 14 || m_phase == 18)
		{
			// Replace the routine library with unchanged InitDisk code, not a flat overlay image.
			program.install_rom(0x256d, 0x4aa8, &m_initdisk[0]);
			program.install_rom(0x4aa9, 0x4abe, &m_disk_helpers[0]);
			program.install_rom(0x0080, 0x00f7, &m_disk_vectors[0]);
			auto &data = m_cpu->space(AS_DATA);
			if (m_phase == 18)
			{
				// Independent factory-erased input, not media left by earlier routine tests.
				m_nand->nvram_reset();
				for (unsigned address = 0x80; address < 0x10000; ++address) data.write_word(address, 0);
			}
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
			if (m_phase == 18)
			{
				u16 const initialize[] = {0xf074, 0x346a, 0x771d, 0x00e8,
					0xf020, 0x057e, 0xf074, 0x30de, 0xf4e1};
				for (unsigned i = 0; i < std::size(initialize); ++i) program.write_word(0x1809 + i, initialize[i]);
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
		m_check->adjust(m_phase >= 25 ? attotime::from_usec(1) : attotime::from_msec(m_phase == 24 ? 8000 : m_phase == 18 ? 60000 : m_phase == 13 ? 8000 : (m_phase == 7 || m_phase == 14) ? 500 : m_phase == 4 ? 250 : m_phase >= 6 ? 50 : 1));
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
		if (m_phase >= 25 && m_phase <= 29)
		{
			auto &data = m_cpu->space(AS_DATA);
			auto &program = m_cpu->space(AS_PROGRAM);
			if (m_phase == 28)
			{
				if ((data.read_word(0x54) & 1) || data.read_word(0x6100) != 0x1234) fatalerror("MU4 DMA pre-restore transfer failed");
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE || !(data.read_word(0x54) & 1) || data.read_word(0x6100) != 0xffff)
					fatalerror("MU4 DMA pending save restore mismatch");
				m_phase = 29;
				m_check->adjust(attotime::from_usec(1));
				return;
			}
			if (data.read_word(0x54) & 1) fatalerror("MU4 DMA completion bit remains set");
			if (m_phase == 25 && (program.read_word(0x2ffff) != 0x1234 || program.read_word(0x20000) != 0xabcd))
				fatalerror("MU4 DMA program page wrap mismatch");
			if (m_phase == 26 && data.read_word(0x6100) != 0x1234) fatalerror("MU4 DMA zero-count single-word mismatch");
			if (m_phase == 27 && data.read_word(0x6100) != 0xffff) fatalerror("MU4 DMA canceled transfer wrote data");
			if (m_phase == 27)
			{
				data.write_word(0x54, 1);
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 DMA pending save failed");
				m_phase = 28;
				m_check->adjust(attotime::from_usec(1));
				return;
			}
			if (m_phase == 29)
			{
				if (data.read_word(0x6100) != 0x1234) fatalerror("MU4 DMA restored transfer failed");
				m_saved_dma.str(std::string());
				logerror("mu4_dma_conformance: PASS deferred=1 page_wrap=1 zero_count=1 cancel=1 pending_restore=1\n");
				m_phase = 0;
			}
			else ++m_phase;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 24)
		{
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x3aea)
				fatalerror("MU4 original loader failed pc=%04x sp=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
			if (m_cpu->space(AS_DATA).read_word(0x54) & 1) fatalerror("MU4 loader returned before DMA completion");
			auto word = [this](unsigned offset) -> u16
			{
				if (offset + 1 >= m_segment.length()) fatalerror("MU4 loader source bounds");
				return (u16(m_segment[offset]) << 8) | m_segment[offset + 1];
			};
			unsigned segment = 0;
			while (word(segment) != 0xaa22)
				segment += 10 + (unsigned(word(segment + 2)) << 16) + word(segment + 4);
			unsigned cursor = segment + 6, records = 0, checked = 0;
			unsigned const end = cursor + (unsigned(word(segment + 2)) << 16) + word(segment + 4);
			while (word(cursor))
			{
				unsigned const count = word(cursor);
				u32 const destination = (u32(word(cursor + 2)) << 16) | word(cursor + 4);
				cursor += 6;
				if (cursor + count * 2 > end) fatalerror("MU4 loader section bounds");
				bool const program = destination > 0xffff;
				// Independent expected rule from original 2f00..2f0e and 2f60..2f70.
				u32 const address = (destination & 0x8000) ? destination : destination & 0xffff;
				for (unsigned i = 0; i < count; ++i)
				{
					u32 const target = (program ? address & 0x7f0000 : 0) | u16(address + i);
					u16 const actual = m_cpu->space(program ? AS_PROGRAM : AS_DATA).read_word(target);
					if (actual != word(cursor + i * 2))
						fatalerror("MU4 DMA destination mismatch space=%s address=%06x actual=%04x expected=%04x", program ? "program" : "data", target, actual, word(cursor + i * 2));
				}
				cursor += count * 2;
				checked += count;
				++records;
			}
			if (cursor + 2 != end || checked != 51921) fatalerror("MU4 loader coverage mismatch words=%u", checked);
			logerror("mu4_loader: PASS original_loader=1 dma_complete=1 records=%u words=%u data_and_program=1\n", records, checked);
			// Observe unchanged 3538 -> loader -> 2000 -> page-2 common-window entry.
			m_cpu->space(AS_PROGRAM).install_read_tap(0x2000, 0x2000, "mu4_native_entry",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_entry_reads; });
			m_cpu->space(AS_PROGRAM).install_read_tap(0x6d62, 0x6d62, "mu4_native_far_entry",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_far_reads; });
			m_cpu->space(AS_DATA).install_write_tap(0x48, 0x49, "mu4_native_mcbsp_config",
				[this](offs_t offset, u16 &value, u16)
				{
					if (machine().side_effects_disabled()) return;
					if (offset == 0x48) m_native_mcbsp_index = value;
					if (m_native_mcbsp_trace++ < 64)
						logerror("mu4_native_mcbsp: write address=%04x index=%04x value=%04x pc=%06x\n", unsigned(offset), m_native_mcbsp_index, value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_DATA).install_read_tap(0x49, 0x49, "mu4_native_mcbsp_status",
				[this](offs_t, u16 &value, u16)
				{
					if (machine().side_effects_disabled()) return;
					if (m_native_mcbsp_index == 1)
					{
						++m_native_mcbsp_polls;
						m_native_mcbsp_status = value;
					}
				});
			m_cpu->space(AS_DATA).install_read_tap(0x4a, 0x4a, "mu4_native_adjacent_status",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_adjacent_reads; });
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x3538);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 30;
			m_check->adjust(attotime::from_seconds(8));
			return;
		}
		if (m_phase == 30)
		{
			if (!m_native_entry_reads || !m_native_far_reads)
				fatalerror("MU4 original program transfer missing entry=%u far=%u pc=%06x", m_native_entry_reads, m_native_far_reads, unsigned(m_cpu->state_int(STATE_GENPC)));
			logerror("mu4_native_entry: PASS original_transfer=1 entry_reads=%u far_reads=%u pc=%06x illegal=%u idle=%u pmst=%04x\n",
				m_native_entry_reads, m_native_far_reads, unsigned(m_cpu->state_int(STATE_GENPC)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
			logerror("mu4_native_mcbsp_boundary: index=%04x status=%04x polls=%u adjacent_reads=%u config_writes=%u controller_modeled=0\n",
				m_native_mcbsp_index, m_native_mcbsp_status, m_native_mcbsp_polls, m_native_adjacent_reads, m_native_mcbsp_trace);
			machine().schedule_exit();
			return;
		}
		if (m_phase == 20)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL))
				fatalerror("MU4 segment illegal opcode pc=%04x cursor=%u", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), m_segment_cursor);
			// XF is the original firmware's external busy indication, not a RAM-state override.
			if (m_serial_ready || BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1), 13))
			{
				if (++m_segment_waits > 10000) fatalerror("MU4 segment serial/busy timeout cursor=%u pc=%04x", m_segment_cursor, unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
			}
			else if (m_segment_cursor < m_segment.length())
			{
				deliver_serial(m_segment[m_segment_cursor++]);
				m_segment_waits = 0;
			}
			else if (!data.read_word(0x1f6) && data.read_word(0x4bbf) == data.read_word(0x4bc0))
			{
				if (m_serial_reads != m_segment.length() + 2 || data.read_word(0x1a2) != 0x40 || data.read_word(0x1f9) != 0x40 || data.read_word(0x584) != 7)
					fatalerror("MU4 original segment checksum/receive mismatch reads=%u checksum=%04x calculated=%04x", m_serial_reads, data.read_word(0x1a2), data.read_word(0x1f9));
				logerror("mu4_initdisk_segment: PASS segments=7 original_bytes=%u final_checksum=0040 ring_drained=1\n", m_segment_cursor);
				m_phase = 19;
				machine().schedule_soft_reset();
				return;
			}
			else if (++m_segment_waits > 10000)
				fatalerror("MU4 segment completion timeout pc=%04x state=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), data.read_word(0x1f6));
			// Bounded word-ingress cadence, not recovered physical McBSP timing.
			m_check->adjust(attotime::from_msec(1));
			return;
		}
		if (m_phase == 18)
		{
			auto &data = m_cpu->space(AS_DATA);
			unsigned const pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
			if (!((pc >= 0x31f0 && pc <= 0x3203) || (pc >= 0x3499 && pc <= 0x34a4)) ||
				m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) || data.read_word(0x1f6) || data.read_word(0x1f5) ||
				data.read_word(0x4bbf) != 0xff || data.read_word(0x4bc0) != 0xff ||
				data.read_word(0x10d) != 0xaa55 || data.read_word(0x19d) != 0xbbcc)
				fatalerror("MU4 original initialization did not reach receive boundary pc=%04x", pc);
			logerror("mu4_initdisk_initialize: PASS original_init=1 erased_input=1 receiver_wait=1 no_received_words=1 writes=%u reads=%u\n",
				unsigned(m_writes.size()), m_data_reads);
			m_phase = 20;
			m_check->adjust(attotime::from_msec(1));
			return;
		}
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
			m_phase = 18;
			machine().schedule_soft_reset();
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
		if (m_phase == 19)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_data_reads || !m_busy_reads)
				fatalerror("MU4 InitData mount of original InitDisk medium failed a=%llx sp=%04x",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
			logerror("mu4_initdisk_mount: PASS original_initdata_mount=1 original_initdisk_media=1\n");
			m_phase = 21;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 21 || m_phase == 23)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 directory reader did not return with balanced stack");
			logerror("mu4_directory: a=%llx sp=%04x name=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x ext=%04x,%04x,%04x length=%04x,%04x\n",
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
				data.read_word(0x36b0), data.read_word(0x36b1), data.read_word(0x36b2), data.read_word(0x36b3),
				data.read_word(0x36b4), data.read_word(0x36b5), data.read_word(0x36b6), data.read_word(0x36b7),
				data.read_word(0x36b9), data.read_word(0x36ba), data.read_word(0x36bb), data.read_word(0x36c2), data.read_word(0x36c3));
			if (m_phase == 23 && m_cpu->state_int(tms320c54x_device::STATE_A) == 1)
			{
				if (m_files_checked != 6) fatalerror("MU4 file count mismatch files=%u", m_files_checked);
				logerror("mu4_file_readback: PASS files=6 original_readers=1 complete_payloads=1\n");
				m_phase = 24;
				machine().schedule_soft_reset();
				return;
			}
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_A) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 original directory reader failed pc=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
			m_file_size = (unsigned(data.read_word(0x36c2)) << 16) | data.read_word(0x36c3);
			m_file_source = 0;
			for (unsigned offset = 0; offset < m_segment.length(); )
			{
				unsigned const size = (unsigned(m_segment[offset + 2]) << 24) | (unsigned(m_segment[offset + 3]) << 16) |
					(unsigned(m_segment[offset + 4]) << 8) | m_segment[offset + 5];
				if (size == m_file_size)
				{
					if (m_file_source) fatalerror("MU4 ambiguous source size");
					m_file_source = offset + 6;
					m_file_marker = (u16(m_segment[offset]) << 8) | m_segment[offset + 1];
				}
				offset += size + 10;
			}
			if (!m_file_source || (m_file_size & 1)) fatalerror("MU4 file size has no original segment size=%u", m_file_size);
			char const *name = nullptr;
			switch (m_file_marker)
			{
			case 0xaa22: name = "MCUSI16 "; break;
			case 0xaa44: name = "MP3SI16 "; break;
			case 0xaa88: name = "RERSI16 "; break;
			case 0xaabb: name = "AACSI16 "; break;
			case 0xaadd: name = "USBSI16 "; break;
			case 0xaa99: name = "RELSI16 "; break;
			default: fatalerror("MU4 unexpected file marker %04x", m_file_marker);
			}
			for (unsigned i = 0; i < 9; ++i)
				if (data.read_word(0x36b0 + i) != u8(name[i])) fatalerror("MU4 directory name mismatch marker=%04x", m_file_marker);
			if (data.read_word(0x36b9) != 'B' || data.read_word(0x36ba) != 'I' || data.read_word(0x36bb) != 'N')
				fatalerror("MU4 directory extension mismatch");
			m_file_cursor = 0;
			m_phase = 22;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 22)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_A) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 original file reader failed pc=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
			for (unsigned i = 0; i < m_read_words; ++i)
			{
				unsigned const source = m_file_source + m_file_cursor + i * 2;
				u16 const expected = (u16(m_segment[source]) << 8) | m_segment[source + 1];
				if (data.read_word(0x6000 + i) != expected)
					fatalerror("MU4 file mismatch marker=%04x byte=%u actual=%04x expected=%04x", m_file_marker, m_file_cursor + i * 2, data.read_word(0x6000 + i), expected);
			}
			m_file_cursor += m_read_words * 2;
			if (m_file_cursor == m_file_size)
			{
				if (std::find(m_verified_files.begin(), m_verified_files.end(), m_file_marker) != m_verified_files.end())
					fatalerror("MU4 directory repeats segment %04x", m_file_marker);
				m_verified_files.push_back(m_file_marker);
				++m_files_checked;
				logerror("mu4_file_payload: PASS marker=%04x bytes=%u\n", m_file_marker, m_file_size);
				m_phase = 23;
			}
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
	ROM_REGION(741916, "segment", 0)
	ROM_LOAD("mu4_initdata_container.bin", 0, 741916, CRC(e0c05bf2) SHA1(5ff0b99c8d93b6ef2cda0bcd002810a4ab7a0e8f))
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
