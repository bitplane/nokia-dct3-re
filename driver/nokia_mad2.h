// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_MAD2_H
#define MAME_NOKIA_NOKIA_MAD2_H

class nokia_mad2_device : public device_t
{
public:
	nokia_mad2_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	struct dsp_reset_wiring_contract
	{
		u8 running_status = 0;
		u8 release_mask = 0;

		constexpr bool enabled() const
		{
			return running_status != 0 && release_mask != 0;
		}

		constexpr bool valid() const
		{
			return (running_status == 0) == (release_mask == 0);
		}
	};

	auto fiq_cb() { return m_fiq_cb.bind(); }
	auto irq_cb() { return m_irq_cb.bind(); }
	auto irq_ack_cb() { return m_irq_ack_cb.bind(); }
	auto reset_cb() { return m_reset_cb.bind(); }
	auto sleep_cb() { return m_sleep_cb.bind(); }
	auto simi_clock_cb() { return m_simi_clock_cb.bind(); }

	void set_timer0_hz(u32 value) { m_timer0_hz = value; }
	void set_timer1_hz(u32 value) { m_timer1_hz = value; }
	void set_fiq8_hz(u32 value) { m_fiq8_hz = value; }
	void set_timer0_catchup(bool value) { m_timer0_catchup = value; }
	void set_dsp_reset_wiring_contract(dsp_reset_wiring_contract contract);

	u8 read(offs_t offset);
	void write(offs_t offset, u8 data);
	void assert_fiq(unsigned line);
	void assert_irq(unsigned line);
	void set_irq_line(unsigned line, bool state);
	void ack_fiq(u16 mask);
	void set_fiq_mask_bits(u8 mask);
	bool watchdog_tick();
	void set_reset_cause(u8 mask) { m_regs[0x01] |= mask; }

	u16 fiq_status() const { return m_fiq_status; }
	u16 irq_status() const { return m_irq_status; }
	u16 timer0_counter() const { return m_timer0_counter; }
	u16 timer1_counter() const { return m_timer1_counter; }
	u16 timer1_destination() const { return m_timer1_destination; }
	bool sleeping() const { return m_sleeping; }
	u8 reg(offs_t offset) const { return m_regs[offset & 0x1f]; }

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(timer0_tick);
	TIMER_CALLBACK_MEMBER(timer1_tick);
	TIMER_CALLBACK_MEMBER(fiq8_tick);
	void ack_irq(u16 mask);
	void update_fiq_line();
	void update_irq_line();
	void update_timer0_compare();
	bool timer0_compare_due() const;
	void enter_sleep();
	void leave_sleep(const char *domain);
	void restore_outputs();

	devcb_write_line m_fiq_cb;
	devcb_write_line m_irq_cb;
	devcb_write16 m_irq_ack_cb;
	devcb_write_line m_reset_cb;
	devcb_write_line m_sleep_cb;
	devcb_write_line m_simi_clock_cb;
	emu_timer *m_timer0 = nullptr;
	emu_timer *m_timer1 = nullptr;
	emu_timer *m_fiq8 = nullptr;
	u8 m_regs[0x20] = { 0 };
	u16 m_fiq_status = 0;
	u16 m_irq_status = 0;
	u16 m_timer0_counter = 0;
	u16 m_timer1_counter = 0;
	u16 m_timer1_destination = 0x7fff;
	u8 m_timer0_divider = 0xff;
	bool m_timer0_compare_latched = false;
	bool m_fiq_line_state = false;
	bool m_irq_line_state = false;
	bool m_sleeping = false;
	u32 m_timer0_hz = 33'055;
	// Calibrated against Timer 0's divided output. The ROM proves an 8:1
	// relationship; the exact CTSI divider tree remains to be recovered.
	u32 m_timer1_hz = 1'057;
	u32 m_fiq8_hz = 1000;
	bool m_timer0_catchup = false;
	dsp_reset_wiring_contract m_dsp_reset_wiring;
	bool m_timer_trace = false;
	bool m_interrupt_trace = false;
	bool m_clock_trace = false;
	u32 m_timer_trace_count = 0;
	u32 m_interrupt_trace_count = 0;
	u32 m_clock_trace_count = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_MAD2, nokia_mad2_device)

#endif // MAME_NOKIA_NOKIA_MAD2_H
