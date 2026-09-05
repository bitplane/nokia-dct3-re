// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "emuopts.h"
#include "nokia_mad2.h"

#define LOG_MAD2 (1U << 0)
#define VERBOSE (LOG_MAD2)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(NOKIA_MAD2, nokia_mad2_device, "nokia_mad2", "Nokia MAD2 CTSI controller")

namespace {
constexpr u16 LINE_EXTENDED = 0x100;
constexpr u8 FIQ_ENABLE = 0x01;
constexpr u8 IRQ_ENABLE = 0x04;
constexpr u8 EXT_IRQ_STATUS = 0x20;
constexpr u8 EXT_IRQ_ACK = 0x40;
constexpr u8 FIQ8_MASK = 0x04;
constexpr u16 TIMER0_FIQ = u16(1) << 4;
}

nokia_mad2_device::nokia_mad2_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_MAD2, tag, owner, clock),
	m_fiq_cb(*this),
	m_irq_cb(*this),
	m_irq_ack_cb(*this),
	m_reset_cb(*this),
	m_sleep_cb(*this),
	m_simi_clock_cb(*this),
	m_dsp_reset_cb(*this)
{
}

void nokia_mad2_device::set_dsp_reset_wiring_contract(
		dsp_reset_wiring_contract contract)
{
	if (!contract.valid())
		fatalerror("MAD2: invalid DSP reset wiring status=%02x mask=%02x",
				contract.running_status, contract.release_mask);
	m_dsp_reset_wiring = contract;
}

void nokia_mad2_device::device_start()
{
	m_timer_trace = machine().options().verbose();
	m_interrupt_trace = machine().options().verbose();
	m_clock_trace = machine().options().verbose();
	m_timer0 = timer_alloc(FUNC(nokia_mad2_device::timer0_tick), this);
	m_timer1 = timer_alloc(FUNC(nokia_mad2_device::timer1_tick), this);
	m_fiq8 = timer_alloc(FUNC(nokia_mad2_device::fiq8_tick), this);
	save_item(NAME(m_regs));
	save_item(NAME(m_external_status));
	save_item(NAME(m_fiq_status));
	save_item(NAME(m_irq_status));
	save_item(NAME(m_timer0_counter));
	save_item(NAME(m_timer1_counter));
	save_item(NAME(m_timer1_destination));
	save_item(NAME(m_timer0_divider));
	save_item(NAME(m_timer0_compare_latched));
	save_item(NAME(m_fiq_line_state));
	save_item(NAME(m_irq_line_state));
	save_item(NAME(m_sleeping));
	machine().save().register_postload(save_prepost_delegate(FUNC(nokia_mad2_device::restore_outputs), this));
}

void nokia_mad2_device::device_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_regs[0x01] = 0x01;
	m_regs[0x03] = 0xff;
	m_regs[0x0c] = 0x0a;
	m_fiq_status = 0;
	m_irq_status = 0;
	m_timer0_counter = 0;
	m_timer1_counter = 0;
	m_timer1_destination = 0x7fff;
	m_timer0_divider = 0xff;
	m_timer0_compare_latched = false;
	m_fiq_line_state = false;
	m_irq_line_state = false;
	m_sleeping = false;
	m_timer_trace_count = 0;
	m_interrupt_trace_count = 0;
	m_clock_trace_count = 0;
	m_fiq_cb(0);
	m_irq_cb(0);
	m_sleep_cb(0);
	m_simi_clock_cb(0);
	m_dsp_reset_cb(0);
	m_timer0->adjust(attotime::from_hz(m_timer0_hz), 0, attotime::from_hz(m_timer0_hz));
	m_timer1->adjust(attotime::from_hz(m_timer1_hz), 0, attotime::from_hz(m_timer1_hz));
	m_fiq8->adjust(attotime::from_hz(m_fiq8_hz), 0, attotime::from_hz(m_fiq8_hz));
}

u8 nokia_mad2_device::read(offs_t offset)
{
	offset &= 0x1f;
	switch (offset)
	{
	case 0x00: return 0x40;
	case 0x02:
		// DSP reset/run is not a plain latch on products that expose the DSP
		// clock/ready state here. Firmware supplies the product-specific release
		// line; MAD2 reports the running status only after that line is asserted.
		// When reset is reasserted, ready bit 4 must read low even if a prior
		// read-modify-write copied it into the software-visible latch.
		if (m_dsp_reset_wiring.enabled())
		{
			if (m_regs[offset] & m_dsp_reset_wiring.release_mask)
				return m_dsp_reset_wiring.running_status;
			return m_regs[offset] & ~0x10;
		}
		return m_regs[offset];
	// Timer 1 counts through its fixed terminal destination and then wraps. The
	// paired ROMs guard destination-current arithmetic with FIQ5, so the
	// destination is hardware state rather than a writable firmware latch.
	case 0x04: return m_timer1_counter >> 8;
	case 0x05: return m_timer1_counter;
	case 0x06: return m_timer1_destination >> 8;
	case 0x07: return m_timer1_destination;
	case 0x08: return m_fiq_status;
	case 0x09: return m_irq_status;
	case 0x0c: return (m_regs[offset] & ~EXT_IRQ_STATUS) | ((m_irq_status & LINE_EXTENDED) ? EXT_IRQ_STATUS : 0);
	case 0x0e:
		// All five supported ROMs only read this register. The 3210 power-state
		// machine independently tests bits 0, 1 and 2; no recovered firmware
		// writes it. Exact external pin ownership remains unknown.
		return m_external_status;
	case 0x10: return m_timer0_counter >> 8;
	case 0x11: return m_timer0_counter;
	case 0x16:
	{
		const u8 data = (m_regs[offset] & ~0x02) | ((m_fiq_status >> 7) & 0x02);
		if (m_interrupt_trace && m_interrupt_trace_count++ < 4096)
			LOGMASKED(LOG_MAD2, "mad2_interrupt: event=reg_R off=16 data=%02x fiq=%03x irq=%03x fiqmask=%02x irqmask=%02x ctrl=%02x extctrl=%02x t=%.9f\n",
					data, m_fiq_status, m_irq_status, m_regs[0x0a], m_regs[0x0b],
					m_regs[0x0c], m_regs[0x16], machine().time().as_double());
		return data;
	}
	default: return m_regs[offset];
	}
}

void nokia_mad2_device::write(offs_t offset, u8 data)
{
	offset &= 0x1f;
	const u8 old = m_regs[offset];
	if (offset != 0x0c && offset != 0x0e)
		m_regs[offset] = data;
	switch (offset)
	{
	case 0x02:
		m_dsp_reset_cb(m_dsp_reset_wiring.enabled() ?
				bool(data & m_dsp_reset_wiring.release_mask) : BIT(data, 0));
		break;
	case 0x01:
		// Both 3210 ROMs set bit 2 and then spin without a software exit.
		// MAD2 therefore owns the reset request; the board callback applies the
		// digital-baseband reset domain after the current MMIO transaction.
		if (BIT(data, 2) && !BIT(old, 2))
			m_reset_cb(1);
		break;
	case 0x08: ack_fiq(data); break;
	case 0x09: ack_irq(data); break;
	case 0x0a: update_fiq_line(); break;
	case 0x0b: update_irq_line(); break;
	case 0x0c:
		// Both 3210 IRQ dispatchers read bit 5 as the ninth pending IRQ and
		// write 0x40 as its acknowledgement command.  The dispatcher writes
		// only that command byte, so it must not replace the retained global
		// IRQ/FIQ enable bits in the control latch.
		if (data == EXT_IRQ_ACK)
			ack_irq(LINE_EXTENDED);
		else
			m_regs[offset] = data & ~(EXT_IRQ_STATUS | EXT_IRQ_ACK);
		update_fiq_line();
		update_irq_line();
		break;
	case 0x0d:
		// Both 3210 ROMs use bit 1 as a one-shot clock-stop request. It is
		// written from task 0 only after the scheduler's idle predicates and
		// sleep-timer update; the shutdown path issues the same request after
		// masking every interrupt. It is therefore a command, not retained
		// clock-selection state. Other bits remain ordinary peripheral gates.
		m_regs[offset] = data & ~0x02;
		m_simi_clock_cb(BIT(m_regs[offset], 5));
		if (BIT(data, 1) && m_clock_stop_enabled)
			enter_sleep();
		break;
	case 0x0e:
		// Read-only external pins; software writes have no electrical effect.
		break;
	case 0x0f: m_timer0_divider = data; break;
	case 0x12: m_timer0_compare_latched = false; break;
	case 0x13:
		m_timer0_compare_latched = false;
		if (m_timer0_catchup || m_timer0_counter == ((u16(m_regs[0x12]) << 8) | m_regs[0x13]))
			update_timer0_compare();
		break;
	case 0x16:
		ack_fiq((data << 7) & LINE_EXTENDED);
		update_fiq_line();
		break;
	}
}

void nokia_mad2_device::assert_fiq(unsigned line)
{
	const u16 before = m_fiq_status;
	m_fiq_status |= line < 8 ? u16(1) << line : LINE_EXTENDED;
	if (m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_interrupt: event=assert domain=FIQ line=%u pending_before=%03x pending_after=%03x t=%.9f\n",
				line, before, m_fiq_status, machine().time().as_double());
	update_fiq_line();
}

void nokia_mad2_device::set_fiq_line(unsigned line, bool state)
{
	const u16 mask = line < 8 ? u16(1) << line : LINE_EXTENDED;
	const u16 before = m_fiq_status;
	if (state)
		m_fiq_status |= mask;
	else
		m_fiq_status &= ~mask;
	if (before != m_fiq_status && m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_interrupt: event=levels domain=FIQ line=%u active=%u pending_before=%03x pending_after=%03x t=%.9f\n",
				line, state, before, m_fiq_status, machine().time().as_double());
	update_fiq_line();
}

void nokia_mad2_device::assert_irq(unsigned line)
{
	const u16 before = m_irq_status;
	m_irq_status |= line < 8 ? u16(1) << line : LINE_EXTENDED;
	if (m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_interrupt: event=assert domain=IRQ line=%u pending_before=%03x pending_after=%03x t=%.9f\n",
				line, before, m_irq_status, machine().time().as_double());
	update_irq_line();
}

void nokia_mad2_device::set_irq_line(unsigned line, bool state)
{
	const u16 mask = line < 8 ? u16(1) << line : LINE_EXTENDED;
	const u16 before = m_irq_status;
	if (state)
		m_irq_status |= mask;
	else
		m_irq_status &= ~mask;
	if (before != m_irq_status && m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_interrupt: event=levels domain=IRQ line=%u active=%u pending_before=%03x pending_after=%03x t=%.9f\n",
				line, state, before, m_irq_status, machine().time().as_double());
	update_irq_line();
}

void nokia_mad2_device::ack_fiq(u16 mask)
{
	const u16 before = m_fiq_status;
	m_fiq_status &= ~mask;
	update_fiq_line();
	if (m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_interrupt: event=ack domain=FIQ mask=%03x pending_before=%03x pending_after=%03x t=%.9f\n",
				mask, before, m_fiq_status, machine().time().as_double());
	if (m_timer_trace && m_timer_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_timer: event=ack mask=%03x pending_before=%03x pending_after=%03x t=%.9f\n",
				mask, before, m_fiq_status, machine().time().as_double());
}

void nokia_mad2_device::ack_irq(u16 mask)
{
	const u16 before = m_irq_status;
	m_irq_status &= ~mask;
	if (mask)
		m_irq_ack_cb(mask);
	update_irq_line();
	if (mask && m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_interrupt: event=ack domain=IRQ mask=%03x pending_before=%03x pending_after=%03x t=%.9f\n",
				mask, before, m_irq_status, machine().time().as_double());
}

void nokia_mad2_device::set_fiq_mask_bits(u8 mask)
{
	m_regs[0x0a] |= mask;
	update_fiq_line();
}

void nokia_mad2_device::update_fiq_line()
{
	bool active = false;
	if (m_regs[0x0c] & FIQ_ENABLE)
	{
		active = (m_fiq_status & ~m_regs[0x0a] & 0xff) != 0;
		if ((m_fiq_status & LINE_EXTENDED) && !(m_regs[0x16] & FIQ8_MASK))
			active = true;
	}
	if (active != m_fiq_line_state && m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_interrupt: event=route domain=FIQ active=%u pending=%03x mask=%02x ctrl=%02x extctrl=%02x t=%.9f\n",
				active, m_fiq_status, m_regs[0x0a], m_regs[0x0c], m_regs[0x16], machine().time().as_double());
	m_fiq_line_state = active;
	if (active)
		leave_sleep("FIQ");
	m_fiq_cb(active);
}

void nokia_mad2_device::update_irq_line()
{
	bool active = false;
	if (m_regs[0x0c] & IRQ_ENABLE)
	{
		active = (m_irq_status & ~m_regs[0x0b] & 0xff) != 0;
		if (m_irq_status & LINE_EXTENDED)
			active = true;
	}
	if (active != m_irq_line_state && m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_interrupt: event=route domain=IRQ active=%u pending=%03x mask=%02x ctrl=%02x t=%.9f\n",
				active, m_irq_status, m_regs[0x0b], m_regs[0x0c], machine().time().as_double());
	m_irq_line_state = active;
	if (active)
		leave_sleep("IRQ");
	m_irq_cb(active);
}

void nokia_mad2_device::enter_sleep()
{
	// A request made with an already-routed interrupt pending cannot stop the
	// MCU clock: the wake condition is already true.
	if (m_sleeping || m_fiq_line_state || m_irq_line_state)
	{
		if (m_clock_trace && m_clock_trace_count++ < 4096)
			LOGMASKED(LOG_MAD2, "mad2_sleep: event=request_blocked fiq=%03x irq=%03x t=%.9f\n",
					m_fiq_status, m_irq_status, machine().time().as_double());
		return;
	}
	m_sleeping = true;
	if (m_clock_trace && m_clock_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_sleep: event=request clocks=%02x timer0=%04x timer1=%04x t=%.9f\n",
				m_regs[0x0d], m_timer0_counter, m_timer1_counter,
				machine().time().as_double());
	m_sleep_cb(1);
}

void nokia_mad2_device::leave_sleep(const char *domain)
{
	if (!m_sleeping)
		return;
	m_sleeping = false;
	if (m_clock_trace && m_clock_trace_count++ < 4096)
		LOGMASKED(LOG_MAD2, "mad2_sleep: event=wake domain=%s fiq=%03x irq=%03x timer0=%04x timer1=%04x t=%.9f\n",
				domain, m_fiq_status, m_irq_status, m_timer0_counter,
				m_timer1_counter, machine().time().as_double());
	m_sleep_cb(0);
}

void nokia_mad2_device::restore_outputs()
{
	update_fiq_line();
	update_irq_line();
	m_sleep_cb(m_sleeping ? 1 : 0);
	m_simi_clock_cb(BIT(m_regs[0x0d], 5));
}

bool nokia_mad2_device::timer0_compare_due() const
{
	const u16 compare = (u16(m_regs[0x12]) << 8) | m_regs[0x13];
	if (!compare)
		return false;
	return m_timer0_catchup ? s16(m_timer0_counter - compare) >= 0 : m_timer0_counter == compare;
}

void nokia_mad2_device::update_timer0_compare()
{
	if (m_timer0_compare_latched || !timer0_compare_due())
		return;
	m_timer0_compare_latched = true;
	if (!(m_fiq_status & TIMER0_FIQ))
	{
		assert_fiq(4);
		if (m_timer_trace && m_timer_trace_count++ < 4096)
			LOGMASKED(LOG_MAD2, "mad2_timer: event=assert counter=%04x compare=%04x divider=%02x pending=%03x mask=%02x ctrl=%02x t=%.9f\n",
					m_timer0_counter, (u16(m_regs[0x12]) << 8) | m_regs[0x13],
					m_timer0_divider, m_fiq_status, m_regs[0x0a], m_regs[0x0c], machine().time().as_double());
	}
}

TIMER_CALLBACK_MEMBER(nokia_mad2_device::timer0_tick)
{
	if (m_regs[0x0f])
	{
		m_regs[0x0f]--;
		return;
	}
	m_regs[0x0f] = m_timer0_divider;
	m_timer0_counter++;
	update_timer0_compare();
}

TIMER_CALLBACK_MEMBER(nokia_mad2_device::timer1_tick)
{
	// Nokia's paired ROMs use FIQ5 as the race indication for terminal-count
	// destination-current calculations and acknowledge it through CTSI status
	// bit 0x20. Keeping current within 0x0000..destination is load-bearing for
	// the shutdown timers that consume the unsigned remaining interval.
	m_timer1_counter = (m_timer1_counter + 1) & m_timer1_destination;
	if (m_timer1_counter == m_timer1_destination)
	{
		assert_fiq(5);
		if (m_timer_trace && m_timer_trace_count++ < 4096)
			LOGMASKED(LOG_MAD2, "mad2_timer1: event=destination counter=%04x destination=%04x pending=%03x t=%.9f\n",
					m_timer1_counter, m_timer1_destination, m_fiq_status,
					machine().time().as_double());
	}
}

TIMER_CALLBACK_MEMBER(nokia_mad2_device::fiq8_tick)
{
	if (m_regs[0x16] & 0x01)
		assert_fiq(8);
}

bool nokia_mad2_device::watchdog_tick()
{
	if (m_regs[0x03] == 0xff)
		return false;
	if (m_regs[0x03])
		m_regs[0x03]--;
	return m_regs[0x03] == 0;
}
