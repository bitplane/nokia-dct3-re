// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "nokia_dsp_staged.h"

DEFINE_DEVICE_TYPE(NOKIA_DSP_STAGED, nokia_dsp_staged_device, "nokia_dsp_staged", "Nokia uploaded DSP program research composition")

nokia_dsp_staged_device::nokia_dsp_staged_device(const machine_config &config, const char *tag, device_t *owner, u32 clock) :
	device_t(config, NOKIA_DSP_STAGED, tag, owner, clock),
	m_cpu(*this, "cpu"), m_transport(*this, "^dspif"), m_cobba(*this, "^cobba")
{
}

void nokia_dsp_staged_device::device_add_mconfig(machine_config &config)
{
	TMS320C54X(config, m_cpu, clock());
	m_cpu->set_addrmap(AS_PROGRAM, &nokia_dsp_staged_device::program_map);
	m_cpu->set_addrmap(AS_DATA, &nokia_dsp_staged_device::data_map);
	m_cpu->set_addrmap(AS_IO, &nokia_dsp_staged_device::io_map);
}

void nokia_dsp_staged_device::device_start()
{
	m_guard = timer_alloc(FUNC(nokia_dsp_staged_device::check_execution), this);
	save_item(NAME(m_data));
	save_item(NAME(m_control));
	save_item(NAME(m_active));
	save_item(NAME(m_published));
	save_item(NAME(m_program_end));
	save_item(NAME(m_verifier));
}

void nokia_dsp_staged_device::device_reset()
{
	m_data.fill(0);
	m_control.fill(0);
	m_active = false;
	m_published = false;
	m_program_end = 0x0fdf;
	m_verifier = true;
	m_guard->adjust(attotime::never);
	m_cpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
}

void nokia_dsp_staged_device::reset_line_w(int released)
{
	if (!released)
	{
		m_active = false;
		m_guard->adjust(attotime::never);
		m_cpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
		return;
	}
	if (m_active)
		return;
	const bool verifier = m_transport->dsp_data_r(0x087b) == 0x0100 &&
			m_transport->dsp_data_r(0x087c) == 0x0300 &&
			m_transport->dsp_data_r(0x087e) == 0xe800 &&
			m_transport->dsp_data_r(0x0881) == 0x0200;
	const bool loader = m_transport->dsp_data_r(0x087b) == 0xfd00 &&
			m_transport->dsp_data_r(0x087c) == 0xff80 &&
			m_transport->dsp_data_r(0x087d) == 0x027e &&
			m_transport->dsp_data_r(0x087e) == 0x0500 &&
			m_transport->dsp_data_r(0x087f) == 0x0078;
	// Only uploaded programs recovered independently from this flash are
	// supported. Missing mask instructions/data are never filled with stubs.
	if (!verifier && !loader)
		throw emu_fatalerror(1, "Staged DSP release needs another contract: fields=%04x/%04x/%04x/%04x/%04x/%04x/%04x program0=%04x t=%.6f",
			m_transport->dsp_data_r(0x087b), m_transport->dsp_data_r(0x087c),
			m_transport->dsp_data_r(0x087d), m_transport->dsp_data_r(0x087e),
			m_transport->dsp_data_r(0x087f), m_transport->dsp_data_r(0x0880),
			m_transport->dsp_data_r(0x0881), m_transport->dsp_data_r(0x0f00), machine().time().as_double());
	m_active = true;
	m_published = false;
	m_verifier = verifier;
	m_program_end = verifier ? 0x0fdf : 0x0f7e;
	m_cpu->set_input_line(INPUT_LINE_RESET, CLEAR_LINE);
	// CPU input-line changes are synchronized by MAME. Set the uploaded
	// entry after reset has actually completed, not before it overwrites PC.
	machine().scheduler().synchronize(timer_expired_delegate(FUNC(nokia_dsp_staged_device::begin_execution), this));
	machine().scheduler().perfect_quantum(attotime::from_usec(100));
	machine().scheduler().abort_timeslice();
	logerror("staged_dsp: release entry=0f00 words=%u prom_input=0006 clock=%u stage=%s t=%.6f\n",
			m_program_end - 0x0f00, clock(), verifier ? "verifier" : "loader", machine().time().as_double());
}

TIMER_CALLBACK_MEMBER(nokia_dsp_staged_device::begin_execution)
{
	if (!m_active)
		return;
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0f00);
	m_guard->adjust(attotime::from_usec(1), 0, attotime::from_usec(1));
}

u16 nokia_dsp_staged_device::program_r(offs_t offset)
{
	// Declared HLE input, not a claim that the fitted mask has been acquired.
	if (offset == 0xff87)
		return 6;
	if (offset >= 0x0800 && offset < 0x1000)
		return m_transport->dsp_data_r(offset);
	if (m_active)
	{
		logerror("staged_dsp: unavailable_program address=%04x pc=%04x stage=%s t=%.6f\n",
				u16(offset), u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				m_verifier ? "verifier" : "loader", machine().time().as_double());
		throw emu_fatalerror(1, "Staged DSP needs unavailable program word: address=%04x pc=%04x stage=%s t=%.6f",
				u16(offset), u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				m_verifier ? "verifier" : "loader", machine().time().as_double());
	}
	return 0xffff;
}

void nokia_dsp_staged_device::program_w(offs_t offset, u16 data)
{
	if (offset >= 0x0800 && offset < 0x1000)
		m_transport->dsp_data_w(offset, data);
}

u16 nokia_dsp_staged_device::data_r(offs_t offset)
{
	return offset >= 0x0800 && offset < 0x1000 ? m_transport->dsp_data_r(offset) : m_data[offset];
}

void nokia_dsp_staged_device::data_w(offs_t offset, u16 data)
{
	if (offset >= 0x0800 && offset < 0x1000)
		m_transport->dsp_data_w(offset, data);
	else
		m_data[offset] = data;
}

u16 nokia_dsp_staged_device::io_r(offs_t offset)
{
	if (offset == 0x002d)
		return m_cobba->control_data_r();
	throw emu_fatalerror(1, "Staged DSP needs unsupported port read %04x", u16(offset));
}

void nokia_dsp_staged_device::io_w(offs_t offset, u16 data)
{
	if (offset == 0x002c)
		m_cobba->control_select_w(data);
	else if (offset == 0x002d)
		m_cobba->control_data_w(data);
	else if (offset == 0x0000 || offset == 0x000c || offset == 0x000e)
	{
		// Uploaded code initializes these CTSI control/frame registers but
		// does not read them or await their interrupts in this bounded stage.
		// Retain writes; do not invent timer or radio completion behavior.
		m_control[offset] = data;
		logerror("staged_dsp: control_write port=%04x data=%04x\n", u16(offset), data);
	}
	else
		throw emu_fatalerror(1, "Staged DSP needs unsupported port write %04x", u16(offset));
}

TIMER_CALLBACK_MEMBER(nokia_dsp_staged_device::check_execution)
{
	const u16 pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
	if (pc < 0x0f00 || pc >= m_program_end)
		throw emu_fatalerror(1, "Staged DSP escaped uploaded program: pc=%04x", pc);
	if (m_verifier && !m_published && m_transport->dsp_data_r(0x0801) != 0xffff)
	{
		m_published = true;
		logerror("staged_dsp: publication word0=%04x word1=%04x word2=%04x word3=%04x pc=%04x t=%.6f\n",
			m_transport->dsp_data_r(0x0800), m_transport->dsp_data_r(0x0801),
			m_transport->dsp_data_r(0x0802), m_transport->dsp_data_r(0x0803), pc, machine().time().as_double());
	}
}

void nokia_dsp_staged_device::program_map(address_map &map)
{
	map(0, 0xffff).rw(FUNC(nokia_dsp_staged_device::program_r), FUNC(nokia_dsp_staged_device::program_w));
}

void nokia_dsp_staged_device::data_map(address_map &map)
{
	map(0, 0xffff).rw(FUNC(nokia_dsp_staged_device::data_r), FUNC(nokia_dsp_staged_device::data_w));
}

void nokia_dsp_staged_device::io_map(address_map &map)
{
	map(0, 0xffff).rw(FUNC(nokia_dsp_staged_device::io_r), FUNC(nokia_dsp_staged_device::io_w));
}
