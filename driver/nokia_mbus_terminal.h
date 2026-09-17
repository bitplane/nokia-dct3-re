// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_NOKIA_MBUS_TERMINAL_H
#define MAME_NOKIA_NOKIA_MBUS_TERMINAL_H

#include <array>

class nokia_mbus_device;

class nokia_mbus_terminal_device : public device_t
{
public:
	nokia_mbus_terminal_device(
			const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	void set_enabled(bool enabled) { m_enabled = enabled; }
	void receive_phone_byte(u8 data);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	static constexpr unsigned MAX_FRAME = 128;
	static constexpr unsigned QUEUE_SIZE = 4;

	TIMER_CALLBACK_MEMBER(transmit_byte);
	void accept_phone_frame();
	bool queue_frame(const u8 *data, unsigned length, attotime idle);
	void start_next_frame();
	static u8 checksum(const u8 *data, unsigned length);

	required_device<nokia_mbus_device> m_mbus;
	emu_timer *m_tx_timer = nullptr;
	bool m_enabled = false;
	bool m_startup_sent = false;
	bool m_ack_queued = false;
	bool m_trace = false;
	std::array<u8, MAX_FRAME> m_rx = { 0 };
	u8 m_rx_length = 0;
	std::array<std::array<u8, MAX_FRAME>, QUEUE_SIZE> m_queue = { };
	std::array<u8, QUEUE_SIZE> m_queue_length = { 0 };
	std::array<attotime, QUEUE_SIZE> m_queue_idle = { };
	u8 m_queue_head = 0;
	u8 m_queue_tail = 0;
	u8 m_tx_offset = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_MBUS_TERMINAL, nokia_mbus_terminal_device)

#endif // MAME_NOKIA_NOKIA_MBUS_TERMINAL_H
