// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_TMS320C54X_MCBSP_H
#define MAME_NOKIA_TMS320C54X_MCBSP_H

// Partial indexed McBSP: single-phase 8/12/16-bit TX and external framed RX.
class tms320c54x_mcbsp_device : public device_t
{
public:
	tms320c54x_mcbsp_device(machine_config const &config, char const *tag, device_t *owner, u32 clock);
	u16 control_r(offs_t offset);
	void control_w(offs_t offset, u16 value);
	u16 data_r(offs_t offset);
	void data_w(offs_t offset, u16 value);
	// Raw programmed contents without changing the index latch; control_r also
	// synthesizes dynamic readiness/empty bits, which are not included here.
	u16 inspect_register(unsigned index) const { return index < 32 ? m_regs[index] : 0; }
	void tx_clock_w(int state);
	void internal_clock_stop_w(int state);
	void tx_frame_w(int state);
	void rx_clock_w(int state);
	void rx_frame_w(int state) { m_rx_frame_input = bool(state); }
	void rx_data_w(int state) { m_rx_data_input = bool(state); }
	auto tx_word_cb() { return m_tx_word_cb.bind(); }
	auto tx_bit_cb() { return m_tx_bit_cb.bind(); }
	auto tx_irq_cb() { return m_tx_irq_cb.bind(); }
	auto tx_event_cb() { return m_tx_event_cb.bind(); }
	// Diagnostic state-transition observation, not an external hardware pin.
	enum : unsigned { TX_RESET, TX_IGNORE, TX_ACCEPT, TX_OVERWRITE,
		TX_LOAD_FRESH, TX_LOAD_REPEAT, TX_COMPLETE, TX_RESTART, TX_WAIT_FRAME };
	auto tx_observer_cb() { return m_tx_observer_cb.bind(); }
	auto rx_irq_cb() { return m_rx_irq_cb.bind(); }
	auto rx_event_cb() { return m_rx_event_cb.bind(); }
	// Observation only: publication and side-effecting DRR read/reset boundaries.
	enum : unsigned { RX_PUBLISH, RX_READ_FRESH, RX_READ_STALE, RX_RESET };
	auto rx_observer_cb() { return m_rx_observer_cb.bind(); }
protected:
	void device_start() override ATTR_COLD;
	void device_reset() override ATTR_COLD;
	void device_clock_changed() override;
private:
	TIMER_CALLBACK_MEMBER(bit_tick);
	bool internal_clock() const;
	attotime bit_period() const;
	void arm_clock();
	void set_ready(bool ready);
	void load_shift(bool external, bool first);
	void shift_bit();
	void reset_receiver();
	void set_rx_ready(bool ready);
	void update_receive_error_irq();
	void update_transmit_error_irq();
	void begin_receive();
	void publish_receive();
	u16 m_regs[32] = {};
	u8 m_index = 0;
	u16 m_buffer = 0, m_shift = 0;
	u8 m_bits = 0, m_delay = 0;
	bool m_buffer_full = false, m_shift_active = false, m_ready = false, m_xempty = false;
	bool m_clock_input = false, m_frame_input = false, m_ready_pending = false;
	bool m_async_bit = false;
	bool m_internal_clock_stopped = false;
	u32 m_system_clock = 0;
	attotime m_paused_bit_time = attotime::never;
	u16 m_frame_words = 0;
	bool m_tx_frame_pending = false;
	u8 m_tx_next_delay = 0;
	attotime m_async_time;
	u16 m_rx_shift = 0, m_rx_completed = 0, m_rx_buffer = 0, m_rx_word = 0, m_rx_high = 0;
	u16 m_rx_frame_words = 0;
	u8 m_rx_bits = 0, m_rx_delay = 0, m_rx_buffer_width = 0;
	bool m_rx_clock_input = false, m_rx_frame_input = false, m_rx_data_input = false;
	bool m_rx_frame_seen = false, m_rx_shift_full = false, m_rx_buffer_full = false, m_rx_ready = false, m_rx_overrun = false;
	bool m_rx_sync_irq = false;
	bool m_rx_frame_pending = false;
	u8 m_rx_next_delay = 0;
	bool m_tx_sync_irq = false;
	emu_timer *m_bit_timer = nullptr;
	devcb_write16 m_tx_word_cb;
	devcb_write16 m_tx_observer_cb;
	devcb_write_line m_tx_bit_cb;
	devcb_write_line m_tx_irq_cb;
	devcb_write_line m_tx_event_cb;
	devcb_write_line m_rx_irq_cb;
	devcb_write_line m_rx_event_cb;
	devcb_write16 m_rx_observer_cb;
};

DECLARE_DEVICE_TYPE(TMS320C54X_MCBSP, tms320c54x_mcbsp_device)
#endif // MAME_NOKIA_TMS320C54X_MCBSP_H
