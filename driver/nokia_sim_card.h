#pragma once

#include "emu.h"

class nokia_sim_card_device : public device_t
{
public:
	nokia_sim_card_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto irq_cb() { return m_irq_cb.bind(); }

	void set_enabled(bool enabled) { m_enabled = enabled; }
	void set_cphs_aoc(bool enabled) { m_cphs_aoc = enabled; }
	bool enabled() const { return m_enabled; }
	void set_atr(const u8 *data, unsigned length);

	u8 control_r() const;
	void control_w(u8 data);
	void txd_w(u8 data);
	u8 rxd_r();
	u8 iir_r() const;
	void iir_w(u8 data);
	u8 rx_count_r() const;
	void rx_fifo_control_w(u8 data);
	void tx_fifo_control_w(u8 data);
	u8 tx_count_r() const;

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(rx_ready);
	void queue_rx(const u8 *data, unsigned length, bool tx_complete = true,
			attotime delay = attotime::from_usec(10));
	void finish_header();
	void finish_body();
	void consume_txd(u8 data);
	void queue_status(u8 sw1, u8 sw2);
	void queue_fcp(u16 fid, unsigned requested);
	void queue_read(u16 fid, unsigned requested);
	static bool is_directory(u16 fid);
	bool is_known_file(u16 fid) const;
	unsigned ef_size(u16 fid) const;
	u8 ef_byte(u16 fid, unsigned offset) const;

	devcb_write_line m_irq_cb;
	emu_timer *m_rx_timer = nullptr;
	bool m_enabled = false;
	bool m_cphs_aoc = false;
	u8 m_control = 0;
	u8 m_atr[40] = { 0x3b, 0x10, 0x05 };
	u8 m_atr_len = 3;
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
	u8 m_tx[260] = { 0 };
	u16 m_tx_len = 0;
	u16 m_tx_expected = 0;
	u8 m_ins = 0;
	u16 m_selected_file = 0x3f00;
	u16 m_selected_df = 0x3f00;
	bool m_receiving_body = false;
};

DECLARE_DEVICE_TYPE(NOKIA_SIM_CARD, nokia_sim_card_device)
