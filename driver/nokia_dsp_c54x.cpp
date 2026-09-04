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
}

void nokia_dsp_c54x_device::device_reset()
{
	std::fill(m_io.begin(), m_io.end(), 0);
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
}

u16 nokia_dsp_c54x_device::io_r(offs_t offset)
{
	return m_io[offset & 0xff];
}

void nokia_dsp_c54x_device::io_w(offs_t offset, u16 data)
{
	m_io[offset & 0xff] = data;
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
