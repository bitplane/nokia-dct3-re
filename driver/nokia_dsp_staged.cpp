// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "nokia_dsp_staged.h"

DEFINE_DEVICE_TYPE(NOKIA_DSP_STAGED, nokia_dsp_staged_device, "nokia_dsp_staged", "Nokia uploaded DSP program research composition")

nokia_dsp_staged_device::nokia_dsp_staged_device(const machine_config &config, const char *tag, device_t *owner, u32 clock) :
	device_t(config, NOKIA_DSP_STAGED, tag, owner, clock),
	m_cpu(*this, "cpu"), m_transport(*this, "^dspif"), m_cobba(*this, "^cobba"), m_flash(*this, "^flash")
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
	if (!m_fragment_offset || m_fragment_offset + m_fragment.size() * 2 > m_flash.bytes())
		fatalerror("Staged DSP requires a configured product-local program fragment");
	std::copy_n(&m_flash[m_fragment_offset / 2], m_fragment.size(), m_fragment.begin());
	save_item(NAME(m_program_ram));
	save_item(NAME(m_program_valid));
	save_item(NAME(m_loader2_verified));
	save_item(NAME(m_observation_halted));
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
	m_program_ram.fill(0);
	m_program_valid.fill(0);
	m_loader2_verified = false;
	m_observation_halted = false;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
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
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_observation_halted = false;
		m_active = false;
		m_guard->adjust(attotime::never);
		m_cpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
		return;
	}
	if (m_active)
		return;
	const bool verifier = m_transport->dsp_data_r(0x087b) == 0x0100 &&
			m_transport->dsp_data_r(0x087c) == 0x0300 &&
			m_transport->dsp_data_r(0x087e) == m_verifier_source_end &&
			m_transport->dsp_data_r(0x0881) == 0x0200;
	const bool loader = m_transport->dsp_data_r(0x087b) == 0xfd00 &&
			m_transport->dsp_data_r(0x087c) == 0xff80 &&
			m_transport->dsp_data_r(0x087d) == 0x027e &&
			m_transport->dsp_data_r(0x087e) == 0x0500 &&
			m_transport->dsp_data_r(m_loader_control_address) == 0x0078;
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
	if (loader)
		m_loader2_verified = false;
	m_program_end = verifier ? 0x0fdf : 0x0f7e;
	m_cpu->set_input_line(INPUT_LINE_RESET, CLEAR_LINE);
	// CPU input-line changes are synchronized by MAME. Set the uploaded
	// entry after reset has actually completed, not before it overwrites PC.
	machine().scheduler().synchronize(timer_expired_delegate(FUNC(nokia_dsp_staged_device::begin_execution), this));
	machine().scheduler().perfect_quantum(attotime::from_usec(100));
	machine().scheduler().abort_timeslice();
	logerror("staged_dsp: release entry=0f00 words=%u prom_input=%04x clock=%u stage=%s t=%.6f\n",
			m_program_end - 0x0f00, m_fragment[7], clock(), verifier ? "verifier" : "loader", machine().time().as_double());
}

TIMER_CALLBACK_MEMBER(nokia_dsp_staged_device::begin_execution)
{
	if (!m_active)
		return;
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0f00);
	// A coarse observation tick can miss CALL into absent mask code before
	// program_r fails closed. Cycle-scale isolation never supplies that opcode.
	attotime const cadence = (!m_verifier && m_cycle_guard_for_loader) ?
			attotime::from_hz(clock()) : attotime::from_usec(1);
	m_guard->adjust(cadence, 0, cadence);
}

void nokia_dsp_staged_device::verify_loader2()
{
	if (!m_loader2_verified)
	{
		if (!m_loader2_offset || !m_loader2_words || m_loader2_words > 0x600 ||
				m_loader2_offset + m_loader2_words * 2 > m_flash.bytes())
			fatalerror("Staged DSP requires a configured loader2 source");
		for (unsigned index = 0; index != m_loader2_words; ++index)
			if (m_transport->dsp_data_r(0x0a00 + index) != m_flash[m_loader2_offset / 2 + index])
				throw emu_fatalerror(1, "Staged DSP loader2 differs from product flash at word %u", index);
		m_loader2_verified = true;
		logerror("staged_dsp: loader2_verified words=%u entry=0a00 t=%.6f\n", m_loader2_words, machine().time().as_double());
	}
}

u16 nokia_dsp_staged_device::program_r(offs_t offset)
{
	if (!m_verifier && offset == 0x0a00)
		verify_loader2();
	// Firmware-contained bootstrap fragment, not a fitted-mask dump. The
	// product profile chooses its source; no missing words are fabricated.
	if (offset >= 0xff80 && offset < 0xff80 + m_fragment.size())
		return m_fragment[offset - 0xff80];
	if (offset < m_program_ram.size() && m_program_valid[offset])
		return m_program_ram[offset];
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
	if (offset >= 0xff80 && offset < 0xff80 + m_fragment.size())
	{
		// The verifier deliberately writes version 6 before reading this
		// resident ROM cell. The declared immutable fragment rejects writes;
		// do not turn that probe into a writable version-selection register.
		logerror("staged_dsp: readonly_program_write address=%04x data=%04x pc=%04x\n",
				u16(offset), data, u16(m_cpu->state_int(tms320c54x_device::STATE_PC)));
	}
	else if (offset >= 0x0800 && offset < 0x1000)
		m_transport->dsp_data_w(offset, data);
	else if (offset < m_program_ram.size())
	{
		m_program_ram[offset] = data;
		m_program_valid[offset] = 1;
	}
	else
		throw emu_fatalerror(1, "Staged DSP needs unmodelled program write: address=%04x data=%04x", u16(offset), data);
}

u16 nokia_dsp_staged_device::data_r(offs_t offset)
{
	return offset >= 0x0800 && offset < 0x1000 ? m_transport->dsp_data_r(offset) : m_data[offset];
}

void nokia_dsp_staged_device::data_w(offs_t offset, u16 data)
{
	const u16 previous = data_r(offset);
	if (offset >= 0x0800 && offset < 0x1000)
		m_transport->dsp_data_w(offset, data);
	else
		m_data[offset] = data;
	if (offset == 0x0029 && BIT(previous ^ data, 3))
	{
		// Native loader code publishes a selector before stroking this wire.
		// MAD2/DSPIF owns routing; no MCU task or message is injected here.
		m_transport->service_irq_w(BIT(data, 3));
		if (BIT(data, 3))
			logerror("staged_dsp: request selector=%04x ack=%04x remaining=%04x destination=%04x pc=%04x t=%.6f\n",
				m_transport->dsp_data_r(0x0871), m_transport->dsp_data_r(0x0872),
				m_transport->dsp_data_r(0x087d), m_transport->dsp_data_r(0x087b),
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)), machine().time().as_double());
	}
}

u16 nokia_dsp_staged_device::io_r(offs_t offset)
{
	if (offset == 0x002d)
		return m_cobba->control_data_r();
	if (offset == 0x0000 || offset == 0x000c || offset == 0x000e || offset == 0x001c)
		return m_control[offset];
	throw emu_fatalerror(1, "Staged DSP needs unsupported port read %04x", u16(offset));
}

void nokia_dsp_staged_device::io_w(offs_t offset, u16 data)
{
	if (offset == 0x002c)
		m_cobba->control_select_w(data);
	else if (offset == 0x002d)
		m_cobba->control_data_w(data);
	else if (offset == 0x0000 || offset == 0x0002 || offset == 0x000c || offset == 0x000e || offset == 0x001c)
	{
		// Uploaded code initializes these CTSI control/frame registers but
		// does not read them or await their interrupts in this bounded stage.
		// Retain writes; do not invent timer or radio completion behavior.
		// NSM-2 loader 0a37 reads port 1c, ORs 0200 and writes it back.
		// Storage supports that RMW only; bit meaning/reset value is unverified.
		m_control[offset] = data;
		logerror("staged_dsp: control_write port=%04x data=%04x\n", u16(offset), data);
	}
	else
		throw emu_fatalerror(1, "Staged DSP needs unsupported port write %04x", u16(offset));
}

TIMER_CALLBACK_MEMBER(nokia_dsp_staged_device::check_execution)
{
	const u16 pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
	// A cycle guard can observe the loader entry before its first fetch.
	if (!m_verifier && pc == 0x0a00)
		verify_loader2();
	if ((pc < 0x0f00 || pc >= m_program_end) &&
			!(m_loader2_verified && pc >= 0x0a00 && pc < 0x0a00 + m_loader2_words))
	{
		unsigned installed = 0;
		unsigned first = m_program_valid.size();
		unsigned last = 0;
		for (unsigned index = 0; index < m_program_valid.size(); ++index)
			if (m_program_valid[index])
			{
				++installed;
				first = std::min(first, index);
				last = index;
			}
		logerror("staged_dsp: installed_program words=%u first=%04x last=%04x target_data=%04x pmst=%04x\n",
				installed, first, last, data_r(pc), u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
		logerror("staged_dsp: outside_uploaded_code pc=%04x t=%.6f\n", pc, machine().time().as_double());
		if (m_runtime_hle && !m_verifier && m_loader2_verified && pc == 0x2c75)
		{
			// Selected hybrid backend: no missing instruction is executed and
			// no native return/cookie is fabricated. HLE owns later transport.
			m_observation_halted = true;
			m_guard->adjust(attotime::never);
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			machine().scheduler().abort_timeslice();
			logerror("staged_dsp: runtime_hle_handoff pc=%04x native_suspended=1\n", pc);
			return;
		}
		if (m_observe_after_missing_code)
		{
			// Diagnostic isolation, not a substitute helper or hardware claim.
			// Keep ownership active so HLE cannot manufacture acknowledgements
			// while the MCU is observed with an explicitly silent DSP.
			m_observation_halted = true;
			m_guard->adjust(attotime::never);
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			machine().scheduler().abort_timeslice();
			logerror("staged_dsp: observation_halt pc=%04x ownership_retained=1\n", pc);
			return;
		}
		throw emu_fatalerror(1, "Staged DSP escaped uploaded program: pc=%04x", pc);
	}
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
