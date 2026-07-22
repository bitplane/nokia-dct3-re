// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_MBUS_H
#define MAME_NOKIA_NOKIA_MBUS_H

class nokia_mbus_device : public device_t
{
public:
	nokia_mbus_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto tx_cb() { return m_tx_cb.bind(); }
	auto fiq2_cb() { return m_fiq2_cb.bind(); }
	auto fiq3_cb() { return m_fiq3_cb.bind(); }


	u8 read(offs_t offset);
	void write(offs_t offset, u8 data);
	bool receive_byte(u8 data);

	u8 control() const { return m_control; }
	u8 status() const;
	u8 data() const { return m_data; }
	bool rx_ready() const { return m_rx_ready; }
	bool tx_pending() const { return m_tx_pending; }

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(byte_complete);
	TIMER_CALLBACK_MEMBER(fiq3_event);
	void schedule_byte();
	void trace_event(const char *event, u8 data = 0);

	devcb_write8 m_tx_cb;
	devcb_write_line m_fiq2_cb;
	devcb_write_line m_fiq3_cb;
	emu_timer *m_byte_timer = nullptr;
	emu_timer *m_fiq3_timer = nullptr;
	attotime m_byte_delay = attotime::from_hz(960); // 10 bits at 9,600 baud
	u8 m_control = 0;
	u8 m_status_latch = 0;
	u8 m_data = 0;
	u8 m_tx_data = 0;
	bool m_rx_ready = false;
	bool m_tx_ready = false;
	bool m_tx_pending = false;
	bool m_trace = false;
	u32 m_trace_count = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_MBUS, nokia_mbus_device)

#endif // MAME_NOKIA_NOKIA_MBUS_H
