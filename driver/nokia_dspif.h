// license:BSD-3-Clause
#pragma once

#include "emu.h"

#include <array>

class nokia_dspif_device : public device_t
{
public:
	struct packet
	{
		u8 type = 0;
		u8 length = 0;
		unsigned words = 0;
		std::array<u8, 255> payload = { 0 };
	};

	nokia_dspif_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto tx_commit_cb() { return m_tx_commit_cb.bind(); }
	auto service_pending_cb() { return m_service_pending_cb.bind(); }
	auto doorbell_cb() { return m_doorbell_cb.bind(); }
	auto bootstrap_fe_cb() { return m_bootstrap_fe_cb.bind(); }
	auto bootstrap_100_cb() { return m_bootstrap_100_cb.bind(); }
	auto fiq0_cb() { return m_fiq0_cb.bind(); }
	auto service_irq_cb() { return m_service_irq_cb.bind(); }

	void set_trace_enabled(bool enabled) { m_trace_enabled = enabled; }
	void set_bootstrap_ping_pong(bool enabled) { m_bootstrap_ping_pong = enabled; }
	void set_code_block_request(bool enabled) { m_code_block_request = enabled; }
	void set_parked_boot_status(bool enabled, u16 response)
	{
		m_parked_boot_status = enabled;
		m_boot_status_response = response;
	}
	bool bootstrap_ping_pong() const { return m_bootstrap_ping_pong; }

	u16 shared_r(offs_t offset);
	void shared_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	void peer_shared_w(offs_t offset, u16 data);
	u8 dspif_r(offs_t offset) const;
	void dspif_w(offs_t offset, u8 data);

	bool peek_tx_packet(packet &result) const;
	void consume_tx_packet(const packet &value);
	bool enqueue_rx_packet(u8 type, const u8 *payload, unsigned payload_length);
	void notify_rx();
	u16 service_pending() const;
	void complete_service();
	u8 run_conformance_checks();

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	static constexpr unsigned SVC_PENDING = 0x0e4 / 2;
	static constexpr unsigned TX_PRODUCER = 0x0a4 / 2;
	static constexpr unsigned TX_CONSUMER = 0x0a6 / 2;
	static constexpr unsigned TX_WORDS = 0x52;
	static constexpr unsigned RX_START = 0x100 / 2;
	static constexpr unsigned RX_END = 0x1c8 / 2;
	static constexpr unsigned RX_PRODUCER = 0x1c8 / 2;
	static constexpr unsigned RX_CONSUMER = 0x1ca / 2;

	u8 tx_payload_byte(unsigned cursor, unsigned index) const;

	devcb_write_line m_tx_commit_cb;
	devcb_write_line m_service_pending_cb;
	devcb_write_line m_doorbell_cb;
	devcb_write_line m_bootstrap_fe_cb;
	devcb_write_line m_bootstrap_100_cb;
	devcb_write_line m_fiq0_cb;
	devcb_write_line m_service_irq_cb;
	u16 m_ram[0x800] = { 0 };
	u8 m_dspif[4] = { 0 };
	bool m_trace_enabled = false;
	bool m_bootstrap_ping_pong = false;
	bool m_code_block_request = false;
	bool m_parked_boot_status = false;
	u16 m_boot_status_response = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_DSPIF, nokia_dspif_device)
