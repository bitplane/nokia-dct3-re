#include "emu.h"
#include "nokia_mad2.h"

DEFINE_DEVICE_TYPE(NOKIA_MAD2, nokia_mad2_device, "nokia_mad2", "Nokia MAD2 CTSI controller")

namespace {
constexpr u16 LINE_EXTENDED = 0x100;
constexpr u8 FIQ_ENABLE = 0x01;
constexpr u8 IRQ_ENABLE = 0x04;
constexpr u8 EXT_IRQ_MASK = 0x40;
constexpr u8 FIQ8_MASK = 0x04;
constexpr u16 TIMER0_FIQ = u16(1) << 4;
}

nokia_mad2_device::nokia_mad2_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_MAD2, tag, owner, clock),
	m_fiq_cb(*this),
	m_irq_cb(*this),
	m_irq_ack_cb(*this)
{
}

void nokia_mad2_device::device_start()
{
	m_timer0 = timer_alloc(FUNC(nokia_mad2_device::timer0_tick), this);
	m_timer1 = timer_alloc(FUNC(nokia_mad2_device::timer1_tick), this);
	m_fiq8 = timer_alloc(FUNC(nokia_mad2_device::fiq8_tick), this);
	save_item(NAME(m_regs));
	save_item(NAME(m_fiq_status));
	save_item(NAME(m_irq_status));
	save_item(NAME(m_timer0_counter));
	save_item(NAME(m_timer1_counter));
	save_item(NAME(m_timer0_divider));
	save_item(NAME(m_timer0_compare_latched));
	save_item(NAME(m_fiq_line_state));
	save_item(NAME(m_irq_line_state));
	machine().save().register_postload(save_prepost_delegate(FUNC(nokia_mad2_device::update_fiq_line), this));
	machine().save().register_postload(save_prepost_delegate(FUNC(nokia_mad2_device::update_irq_line), this));
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
	m_timer0_divider = 0xff;
	m_timer0_compare_latched = false;
	m_fiq_line_state = false;
	m_irq_line_state = false;
	m_timer_trace_count = 0;
	m_interrupt_trace_count = 0;
	m_fiq_cb(0);
	m_irq_cb(0);
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
	// Timer 1 runs independently of register access. Offsets 0x06/0x07 form
	// the stored destination latch; no observed firmware path programs it, so
	// do not synthesize elapsed time or compare behavior here.
	case 0x04: return m_timer1_counter >> 8;
	case 0x05: return m_timer1_counter;
	case 0x06: return m_regs[offset];
	case 0x07: return m_regs[offset];
	case 0x08: return m_fiq_status;
	case 0x09: return m_irq_status;
	case 0x0c: return (m_regs[offset] & ~0x20) | ((m_irq_status >> 3) & 0x20);
	case 0x10: return m_timer0_counter >> 8;
	case 0x11: return m_timer0_counter;
	case 0x16: return (m_regs[offset] & ~0x02) | ((m_fiq_status >> 7) & 0x02);
	default: return m_regs[offset];
	}
}

void nokia_mad2_device::write(offs_t offset, u8 data)
{
	offset &= 0x1f;
	m_regs[offset] = data;
	switch (offset)
	{
	case 0x08: ack_fiq(data); break;
	case 0x09: ack_irq(data); break;
	case 0x0a: update_fiq_line(); break;
	case 0x0b: update_irq_line(); break;
	case 0x0c:
		ack_irq((data << 3) & LINE_EXTENDED);
		update_fiq_line();
		update_irq_line();
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
		logerror("mad2_interrupt: event=assert domain=FIQ line=%u pending_before=%03x pending_after=%03x t=%.9f\n",
				line, before, m_fiq_status, machine().time().as_double());
	update_fiq_line();
}

void nokia_mad2_device::assert_irq(unsigned line)
{
	const u16 before = m_irq_status;
	m_irq_status |= line < 8 ? u16(1) << line : LINE_EXTENDED;
	if (m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		logerror("mad2_interrupt: event=assert domain=IRQ line=%u pending_before=%03x pending_after=%03x t=%.9f\n",
				line, before, m_irq_status, machine().time().as_double());
	update_irq_line();
}

void nokia_mad2_device::set_irq_line(unsigned line, bool state)
{
	const u16 mask = line < 8 ? u16(1) << line : LINE_EXTENDED;
	if (state)
		m_irq_status |= mask;
	else
		m_irq_status &= ~mask;
	update_irq_line();
}

void nokia_mad2_device::ack_fiq(u16 mask)
{
	const u16 before = m_fiq_status;
	m_fiq_status &= ~mask;
	update_fiq_line();
	if (m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		logerror("mad2_interrupt: event=ack domain=FIQ mask=%03x pending_before=%03x pending_after=%03x t=%.9f\n",
				mask, before, m_fiq_status, machine().time().as_double());
	if (m_timer_trace && m_timer_trace_count++ < 4096)
		logerror("mad2_timer: event=ack mask=%03x pending_before=%03x pending_after=%03x t=%.9f\n",
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
		logerror("mad2_interrupt: event=ack domain=IRQ mask=%03x pending_before=%03x pending_after=%03x t=%.9f\n",
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
		logerror("mad2_interrupt: event=route domain=FIQ active=%u pending=%03x mask=%02x ctrl=%02x extctrl=%02x t=%.9f\n",
				active, m_fiq_status, m_regs[0x0a], m_regs[0x0c], m_regs[0x16], machine().time().as_double());
	m_fiq_line_state = active;
	m_fiq_cb(active);
}

void nokia_mad2_device::update_irq_line()
{
	bool active = false;
	if (m_regs[0x0c] & IRQ_ENABLE)
	{
		active = (m_irq_status & ~m_regs[0x0b] & 0xff) != 0;
		if ((m_irq_status & LINE_EXTENDED) && !(m_regs[0x0c] & EXT_IRQ_MASK))
			active = true;
	}
	if (active != m_irq_line_state && m_interrupt_trace && m_interrupt_trace_count++ < 4096)
		logerror("mad2_interrupt: event=route domain=IRQ active=%u pending=%03x mask=%02x ctrl=%02x t=%.9f\n",
				active, m_irq_status, m_regs[0x0b], m_regs[0x0c], machine().time().as_double());
	m_irq_line_state = active;
	m_irq_cb(active);
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
			logerror("mad2_timer: event=assert counter=%04x compare=%04x divider=%02x pending=%03x mask=%02x ctrl=%02x t=%.9f\n",
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
	// Timer 1 is free-running; its overflow is the FIQ5 source. Firmware does
	// not need to touch the counter window for the overflow interrupt to run.
	m_timer1_counter++;
	if (m_timer1_counter == 0)
		assert_fiq(5);
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
