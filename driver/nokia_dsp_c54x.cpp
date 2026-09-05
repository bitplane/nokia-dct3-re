// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "nokia_dsp_c54x.h"

DEFINE_DEVICE_TYPE(NOKIA_DSP_C54X, nokia_dsp_c54x_device, "nokia_dsp_c54x",
		"Nokia DCT3 C54x DSP backend")

nokia_dsp_c54x_device::nokia_dsp_c54x_device(const machine_config &mconfig,
		const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_DSP_C54X, tag, owner, clock),
	nokia_dsp_backend_interface(mconfig, *this),
	m_cpu(*this, "cpu"),
	m_transport(*this, "^dspif"),
	m_cobba(*this, "^cobba"),
	m_ccont(*this, "^ccont"),
	m_program_rom(*this, "^dsp_program"),
	m_data_rom(*this, "^dsp_data")
{
}

void nokia_dsp_c54x_device::device_add_mconfig(machine_config &config)
{
	TMS320C54X(config, m_cpu, clock());
	m_cpu->set_addrmap(AS_PROGRAM, &nokia_dsp_c54x_device::program_map);
	m_cpu->set_addrmap(AS_DATA, &nokia_dsp_c54x_device::data_map);
	m_cpu->set_addrmap(AS_IO, &nokia_dsp_c54x_device::io_map);
}

void nokia_dsp_c54x_device::device_start()
{
	if (m_program_rom.bytes() < 0x20000 || m_data_rom.bytes() < 0x20000)
		fatalerror("DCT3 C54x backend requires complete 64K-word program and data regions");
	std::copy_n(&m_program_rom[0], m_program.size(), m_program.begin());
	std::copy_n(&m_data_rom[0], m_data.size(), m_data.begin());
	m_slot_timer = timer_alloc(FUNC(nokia_dsp_c54x_device::slot_timer_expired), this);
	m_frame_timer = timer_alloc(FUNC(nokia_dsp_c54x_device::frame_timer_expired), this);
	save_item(NAME(m_program));
	save_item(NAME(m_data));
	save_item(NAME(m_io));
	save_item(NAME(m_host_command_line));
	save_item(NAME(m_reset_released));
	save_item(NAME(m_host_command_vector));
	save_item(NAME(m_slot_frame_length));
	save_item(NAME(m_slot_delay));
	save_item(NAME(m_slot_timer_expiries));
	save_item(NAME(m_frame_timer_expiries));
	save_item(NAME(m_io_trace_count));
	save_item(NAME(m_rf_trace_count));
	save_item(NAME(m_rf_synth_low));
	save_item(NAME(m_rf_synth_high));
	save_item(NAME(m_rf_synth_pairs));
	save_item(NAME(m_completion_strobes));
	save_item(NAME(m_boot_mailbox_writes));
}

void nokia_dsp_c54x_device::device_reset()
{
	std::fill(m_io.begin(), m_io.end(), 0);
	m_host_command_line = false;
	m_reset_released = false;
	m_host_command_vector = 0;
	m_slot_frame_length = 0;
	m_slot_delay = 0;
	m_slot_timer_expiries = 0;
	m_frame_timer_expiries = 0;
	m_io_trace_count = 0;
	m_rf_trace_count = 0;
	m_rf_synth_low = 0;
	m_rf_synth_high = 0;
	m_rf_synth_pairs = 0;
	m_completion_strobes = 0;
	m_boot_mailbox_writes = 0;
	m_slot_timer->adjust(attotime::never);
	m_frame_timer->adjust(attotime::never);
	m_cpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
}

void nokia_dsp_c54x_device::device_stop()
{
	machine().logerror("rom4_interface_summary: completion_strobes=%u mailbox_writes=%u "
			"slot_expiries=%llu frame_expiries=%llu rf_reads=%u rf_tune_pairs=%u "
			"pc=%04x pmst=%04x ifr=%04x imr=%04x mode_aa=%04x mode_ac=%04x\n",
			m_completion_strobes, m_boot_mailbox_writes,
			m_slot_timer_expiries, m_frame_timer_expiries,
			m_rf_trace_count, m_rf_synth_pairs,
			u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_IFR)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_IMR)),
			m_data[0x00aa], m_data[0x00ac]);
}

void nokia_dsp_c54x_device::arm_frame_timer()
{
	// ROM4 CTSI counts quarter-symbols from the 13 MHz reference divided by
	// 12. Hardware starts with a 5000-quarter-symbol GSM frame; a later port
	// 0x0e write replaces the default reload value.
	const u32 quarter_symbols = u32(m_slot_frame_length ? m_slot_frame_length : 4999) + 1;
	m_frame_timer->adjust(attotime::from_ticks(u64(quarter_symbols) * 12, 13'000'000));
}

TIMER_CALLBACK_MEMBER(nokia_dsp_c54x_device::frame_timer_expired)
{
	if (!m_reset_released)
		return;

	++m_frame_timer_expiries;
	const bool enabled = m_ccont->dsp_frame_clock_enabled();
	if (m_frame_timer_expiries <= 64)
		machine().logerror("rom4_frame_timer: expiry=%llu enabled=%u length=%u pc=%04x pmst=%04x ifr=%04x imr=%04x t=%.6f\n",
				m_frame_timer_expiries, enabled, m_slot_frame_length ? m_slot_frame_length : 4999,
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_IFR)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_IMR)),
				machine().time().as_double());
	if (enabled)
	{
		m_cpu->set_input_line(0, HOLD_LINE); // CTSI/GSM frame interrupt
	}
	arm_frame_timer();
}

void nokia_dsp_c54x_device::arm_slot_timer(u16 quarter_symbols)
{
	// CTSI derives this ROM4 one-shot from the 13 MHz reference divided by 12.
	// A zero reload means expiry on the next quarter-symbol tick.
	m_slot_delay = quarter_symbols ? quarter_symbols : 1;
	m_slot_timer->adjust(attotime::from_ticks(u64(m_slot_delay) * 12, 13'000'000));
}

TIMER_CALLBACK_MEMBER(nokia_dsp_c54x_device::slot_timer_expired)
{
	if (!m_reset_released)
		return;
	++m_slot_timer_expiries;
	if (m_slot_timer_expiries <= 16)
		machine().logerror("rom4_slot_timer: expiry=%llu delay=%u pc=%04x ifr=%04x imr=%04x t=%.6f\n",
				m_slot_timer_expiries, m_slot_delay,
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_IFR)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_IMR)),
				machine().time().as_double());
	const u16 command = m_transport->shared_word(0x0a8 / 2);
	m_transport->peer_shared_w(0x0a8 / 2, command | 0x0008);
	update_host_command_line();
}

void nokia_dsp_c54x_device::reset_line_w(int released)
{
	// MAD2 holds the DSP core in reset while leaving its shared/on-chip DARAM
	// intact. Releasing the line restarts at the mask-ROM reset vector.
	m_reset_released = released;
	m_host_command_line = false;
	m_host_command_vector = 0;
	m_cpu->set_input_line(INPUT_LINE_RESET,
			released ? CLEAR_LINE : ASSERT_LINE);
	if (released)
	{
		arm_frame_timer();
		update_host_command_line();
	}
	else
		m_frame_timer->adjust(attotime::never);
}

bool nokia_dsp_c54x_device::overlay_address(u16 address) const
{
	// PMST.OVLY maps the on-chip DARAM at 0x0080..0x27ff into program space.
	return BIT(m_cpu->state_int(tms320c54x_device::STATE_PMST), 5) &&
			address >= 0x0080 && address < 0x2800;
}

u16 nokia_dsp_c54x_device::program_r(offs_t offset)
{
	const u16 address = offset;
	return overlay_address(address) ? data_r(address) : m_program[address];
}

void nokia_dsp_c54x_device::program_w(offs_t offset, u16 data)
{
	const u16 address = offset;
	if (overlay_address(address))
		data_w(address, data);
}

u16 nokia_dsp_c54x_device::data_r(offs_t offset)
{
	const u16 address = offset;
	if (address == 0x0020 && m_cobba->codec_serial_receive_ready())
	{
		const u16 value = m_cobba->codec_serial_receive();
		m_cobba->codec_serial_receive_ack();
		return value;
	}
	if (address >= nokia_dspif_device::hpi_daram_base &&
			address < nokia_dspif_device::hpi_daram_base +
				nokia_dspif_device::hpi_daram_words)
		return m_transport->dsp_data_r(address);
	return m_data[address];
}

void nokia_dsp_c54x_device::data_w(offs_t offset, u16 data)
{
	const u16 address = offset;
	if (address == 0x0022 || address == 0x0032)
	{
		m_cobba->parallel_control_w(data);
		return;
	}
	if (address >= 0xb000 && address <= 0xefff)
	{
		// This address range is the C54x data mask ROM. Writes are bus-visible
		// no-ops; allowing them to alter the backing array corrupts resident
		// dispatch tables and constants with transient response data.
		return;
	}
	if (address == 0x0021)
		m_cobba->codec_serial_transmit(data);
	const bool shared = address >= nokia_dspif_device::hpi_daram_base &&
			address < nokia_dspif_device::hpi_daram_base +
				nokia_dspif_device::hpi_daram_words;
	const u16 old_data = shared ? m_transport->dsp_data_r(address) : m_data[address];
	if (address == 0x0866 && data != old_data)
		machine().logerror("rom4_work_flags: pc=%04x old=%04x new=%04x t=%.6f\n",
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)), old_data, data,
				machine().time().as_double());
	if (shared)
		m_transport->dsp_data_w(address, data);
	else
		m_data[address] = data;
	// The DSP publishes MCU-bound packets by writing the receive-ring producer
	// last. This edge is the physical FIQ0 notification; the shared words alone
	// are not polled by the MCU.
	if (shared && address == nokia_dspif_device::hpi_daram_base + (0x1c8 / 2) &&
			data != old_data)
		m_transport->notify_rx();
	if (address >= 0x0852 && address <= 0x0855)
		update_host_command_line();
}

void nokia_dsp_c54x_device::mcu_shared_write(u16 byte_offset)
{
	if (byte_offset == 0x00fe || byte_offset == 0x0100)
		++m_boot_mailbox_writes;
	if (byte_offset >= 0x0a4 && byte_offset <= 0x0aa)
		update_host_command_line();
}

void nokia_dsp_c54x_device::tx_commit_w(int state)
{
	// MDISND has no separate doorbell. The producer write changes the live
	// ring-not-empty comparator presented on INT2; mcu_shared_write observes the
	// same write and updates that level after the transport has stored it.
	if (state && m_reset_released)
	{
		update_host_command_line();
		m_cpu->set_input_line(9, HOLD_LINE);
	}
}

u16 nokia_dsp_c54x_device::host_request() const
{
	const u16 tx_producer = m_transport->shared_word(0x0a4 / 2);
	const u16 tx_consumer = m_transport->shared_word(0x0a6 / 2);
	const u16 command = m_transport->shared_word(0x0a8 / 2);
	return (command & ~u16(0x0002)) |
			(tx_producer != tx_consumer ? 0x0002 : 0x0000);
}

void nokia_dsp_c54x_device::acknowledge_host_command(u16 mask)
{
	const u16 command = m_transport->shared_word(0x0a8 / 2);
	const u16 acknowledged = mask & ~u16(0x0002);
	if (command & acknowledged)
		m_transport->peer_shared_w(0x0a8 / 2, command & ~acknowledged);
}

void nokia_dsp_c54x_device::update_host_command_line()
{
	if (!m_reset_released)
	{
		m_host_command_line = false;
		m_host_command_vector = 0;
		return;
	}
	// Bit 1 is a live ring-not-empty comparator, not an acknowledgeable latch.
	// Firmware may leave bit 1 set in the in-service mask; it must not suppress
	// a later empty-to-nonempty MDISND transition.
	const u16 acknowledgement = m_transport->shared_word(0x0aa / 2) & ~u16(0x0002);
	// MAD2 presents the firmware's command latch and MDISND ring state as one
	// level-sensitive request to the DSP's edge-sampled INT2 input. Bit 1 is
	// the live ring-not-empty condition rather than persistent RAM state.
	const u16 vector = host_request() & ~acknowledgement;
	const bool level = vector != 0;
	// Re-arm the CPU latch when the effective request changes. Command work and
	// an MDISND commit can overlap without producing an intervening low edge,
	// but are distinct requests to the resident dispatcher.
	if (level && vector != m_host_command_vector)
		m_cpu->set_input_line(2, ASSERT_LINE);
	else if (!level)
		m_cpu->set_input_line(2, CLEAR_LINE);
	m_host_command_line = level;
	m_host_command_vector = vector;
}

u16 nokia_dsp_c54x_device::io_r(offs_t offset)
{
	switch (offset & 0xff)
	{
	case 0x01:
		return host_request();
	case 0x02:
		return m_transport->shared_word(0x0aa / 2);
	case 0x21:
	{
		const u16 value = m_cobba->codec_serial_receive();
		if (m_cobba->codec_serial_receive_ready())
			m_cobba->codec_serial_receive_ack();
		return value;
	}
	case 0x27:
		if (m_rf_trace_count++ < 16)
			machine().logerror("rom4_rf_read: sample=%u pc=%04x t=%.6f\n",
					m_rf_trace_count,
					u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
					machine().time().as_double());
		return m_cobba->rf_receive_sample();
	case 0x2d:
		return m_cobba->control_data_r();
	default:
		return m_io[offset & 0xff];
	}
}

void nokia_dsp_c54x_device::io_w(offs_t offset, u16 data)
{
	const u8 port = offset & 0xff;
	if (port == 0x03 || port == 0x0e || port == 0x0f)
		machine().logerror("rom4_timing_port: port=%02x data=%04x pc=%04x t=%.6f\n",
				port, data, u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				machine().time().as_double());
	if (m_io_trace_count++ < 256)
		machine().logerror("rom4_port_write: port=%02x data=%04x pc=%04x t=%.6f\n",
				port, data, u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				machine().time().as_double());
	m_io[port] = data;
	if (port == 0x02)
	{
		// Port 2 is the DSP's in-service/accept mask.  A set bit also retires
		// the corresponding latched MCU command; the ring comparator is
		// retired only by advancing its consumer pointer.
		m_transport->peer_shared_w(0x0aa / 2, data);
		acknowledge_host_command(data);
		update_host_command_line();
	}
	else if (port == 0x01)
	{
		// Port 1 writes are DSP status/completion strobes, not writes back to
		// the port-1 request latch read above. The same physical strobe is the
		// DSP-to-MCU doorbell; the MCU's FIQ0 handler decides whether the receive
		// ring contains work.
		acknowledge_host_command(data);
		++m_completion_strobes;
		update_host_command_line();
		m_transport->notify_rx();
	}
	else if (port == 0x21)
	{
		m_cobba->codec_serial_transmit(data);
	}
	else if (port == 0x0e)
	{
		m_slot_frame_length = data;
	}
	else if (port == 0x0f)
	{
		arm_slot_timer(data);
	}
	else if (port == 0x2c)
	{
		m_cobba->control_select_w(data);
	}
	else if (port == 0x2d)
	{
		m_cobba->control_data_w(data);
	}
	else if (port == 0x31)
	{
		m_rf_synth_low = data;
	}
	else if (port == 0x32)
	{
		m_rf_synth_high = data;
		++m_rf_synth_pairs;
		if (m_rf_synth_pairs <= 16)
			machine().logerror(
					"rom4_rf_tune: sequence=%u low=%04x high=%04x pc=%04x t=%.6f\n",
					m_rf_synth_pairs, m_rf_synth_low, m_rf_synth_high,
					u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
					machine().time().as_double());
	}
}

void nokia_dsp_c54x_device::doorbell_w(int state)
{
	if (state)
		m_cpu->set_input_line(9, HOLD_LINE); // C54x HPINT, vector 25
}

void nokia_dsp_c54x_device::program_map(address_map &map)
{
	map(0x0000, 0xffff).rw(FUNC(nokia_dsp_c54x_device::program_r),
			FUNC(nokia_dsp_c54x_device::program_w));
}

void nokia_dsp_c54x_device::data_map(address_map &map)
{
	map(0x0000, 0xffff).rw(FUNC(nokia_dsp_c54x_device::data_r),
			FUNC(nokia_dsp_c54x_device::data_w));
}

void nokia_dsp_c54x_device::io_map(address_map &map)
{
	map(0x0000, 0xffff).rw(FUNC(nokia_dsp_c54x_device::io_r),
			FUNC(nokia_dsp_c54x_device::io_w));
}
