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
	save_item(NAME(m_program));
	save_item(NAME(m_data));
	save_item(NAME(m_io));
	save_item(NAME(m_host_command_line));
}

void nokia_dsp_c54x_device::device_reset()
{
	std::fill(m_io.begin(), m_io.end(), 0);
	m_host_command_line = false;
	m_cpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
}

void nokia_dsp_c54x_device::reset_line_w(int released)
{
	// MAD2 holds the DSP core in reset while leaving its shared/on-chip DARAM
	// intact. Releasing the line restarts at the mask-ROM reset vector.
	if (!released)
		m_host_command_line = false;
	m_cpu->set_input_line(INPUT_LINE_RESET,
			released ? CLEAR_LINE : ASSERT_LINE);
	if (released)
		update_host_command_line();
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
	if (address >= nokia_dspif_device::hpi_daram_base &&
			address < nokia_dspif_device::hpi_daram_base +
				nokia_dspif_device::hpi_daram_words)
		return m_transport->dsp_data_r(address);
	return m_data[address];
}

void nokia_dsp_c54x_device::data_w(offs_t offset, u16 data)
{
	const u16 address = offset;
	if (address >= nokia_dspif_device::hpi_daram_base &&
			address < nokia_dspif_device::hpi_daram_base +
				nokia_dspif_device::hpi_daram_words)
		m_transport->dsp_data_w(address, data);
	else
		m_data[address] = data;
	if (address >= 0x0852 && address <= 0x0855)
		update_host_command_line();
}

void nokia_dsp_c54x_device::mcu_shared_write(u16 byte_offset)
{
	if (byte_offset >= 0x0a4 && byte_offset <= 0x0aa)
		update_host_command_line();
}

u16 nokia_dsp_c54x_device::host_request() const
{
	const u16 tx_producer = m_transport->shared_word(0x0a4 / 2);
	const u16 tx_consumer = m_transport->shared_word(0x0a6 / 2);
	const u16 command = m_transport->shared_word(0x0a8 / 2);
	// Host-command bit 1 is a live comparator over the MDISND pointers.  It is
	// not part of the writable command latch.
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
	const u16 acknowledgement = m_transport->shared_word(0x0aa / 2);
	// MAD2 presents the firmware's command latch and MDISND ring state as one
	// level-sensitive request to the DSP's edge-sampled INT2 input. Bit 1 is
	// the live ring-not-empty condition rather than persistent RAM state.
	const bool level = (host_request() & ~acknowledgement) != 0;
	if (level && !m_host_command_line)
		m_cpu->set_input_line(2, HOLD_LINE);
	m_host_command_line = level;
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
	case 0x2d:
		return m_cobba->control_data_r();
	default:
		return m_io[offset & 0xff];
	}
}

void nokia_dsp_c54x_device::io_w(offs_t offset, u16 data)
{
	const u8 port = offset & 0xff;
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
		update_host_command_line();
		m_transport->notify_rx();
	}
	else if (port == 0x21)
	{
		m_cobba->codec_serial_transmit(data);
	}
	else if (port == 0x2c)
	{
		m_cobba->control_select_w(data);
	}
	else if (port == 0x2d)
	{
		m_cobba->control_data_w(data);
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
