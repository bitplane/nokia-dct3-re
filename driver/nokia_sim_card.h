#pragma once

#include "emu.h"

class nokia_sim_card_device : public device_t
{
public:
	nokia_sim_card_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto response_cb() { return m_response_cb.bind(); }

	void set_cphs_aoc(bool enabled) { m_cphs_aoc = enabled; }
	void set_atr(const u8 *data, unsigned length);
	void activate();
	void rx_w(u8 data);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	void emit_response(const u8 *data, unsigned length);
	void finish_header();
	void finish_body();
	void queue_status(u8 sw1, u8 sw2);
	void queue_fcp(u16 fid, unsigned requested);
	void queue_read(u16 fid, unsigned requested);
	static bool is_directory(u16 fid);
	bool is_known_file(u16 fid) const;
	unsigned ef_size(u16 fid) const;
	u8 ef_byte(u16 fid, unsigned offset) const;

	devcb_write8 m_response_cb;
	bool m_cphs_aoc = false;
	u8 m_atr[40] = { 0x3b, 0x10, 0x05 };
	u8 m_atr_len = 3;
	u8 m_tx[260] = { 0 };
	u16 m_tx_len = 0;
	u16 m_tx_expected = 0;
	u8 m_ins = 0;
	u16 m_selected_file = 0x3f00;
	u16 m_selected_df = 0x3f00;
	bool m_receiving_body = false;
};

DECLARE_DEVICE_TYPE(NOKIA_SIM_CARD, nokia_sim_card_device)
