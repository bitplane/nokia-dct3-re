#pragma once

#include "emu.h"

class nokia_sim_card_device;

class nokia_simi_device : public device_t
{
public:
	nokia_simi_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto irq_cb() { return m_irq_cb.bind(); }

	void set_enabled(bool enabled) { m_enabled = enabled; }
	bool enabled() const { return m_enabled; }
	void set_clock_enabled(bool enabled);
	bool clock_enabled() const { return m_clock_enabled; }

	u8 control_r() const;
	void control_w(u8 data);
	void txd_w(u8 data);
	u8 rxd_r();
	u8 iir_r() const { return m_iir; }
	void iir_w(u8 data) { m_iir &= ~data; }
	u8 rx_count_r() const;
	void rx_fifo_control_w(u8 data);
	void tx_fifo_control_w(u8 data);
	u8 tx_count_r() const { return m_uart_tx_count; }

	void card_rx_w(u8 data);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(rx_ready);
	void schedule_card_bytes(bool tx_complete, bool response_added);

	required_device<nokia_sim_card_device> m_card;
	devcb_write_line m_irq_cb;
	emu_timer *m_rx_timer = nullptr;
	bool m_enabled = false;
	bool m_clock_enabled = false;
	u8 m_control = 0;
	// Card-response serialization queue, not firmware-visible FIFO capacity.
	u8 m_rx_fifo[320] = { 0 };
	u16 m_rx_head = 0;
	u16 m_rx_tail = 0;
	u16 m_rx_count = 0;
	bool m_rx_ready = false;
	u8 m_iir = 0;
	bool m_tx_ready_pending = false;
	u8 m_uart_tx_fifo[16] = { 0 };
	u8 m_uart_tx_count = 0;
	u8 m_rx_fifo_control = 0;
	u8 m_tx_fifo_control = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_SIMI, nokia_simi_device)
