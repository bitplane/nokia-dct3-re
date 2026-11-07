// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_MFI_RX_H
#define MAME_NOKIA_NOKIA_MFI_RX_H
#include "nokia_mfi_rx_state.h"

// Explicit lab ADC clock/transport candidate. No HAGAR tuning or GSM messages.
class nokia_mfi_rx_device : public device_t
{
public:
	nokia_mfi_rx_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);
	void set_word_divider(unsigned divider) { m_divider = divider; }
	auto adc_cb() { return m_adc_cb.bind(); }
	auto data_available_cb() { return m_data_available_cb.bind(); }
	// Diagnostic observer, not a claimed fitted error-interrupt pin.
	auto fault_observer_cb() { return m_fault_observer_cb.bind(); }
	void enable_w(int state);
	u16 read();
	u64 produced() const { return m_state.produced; }
	u64 consumed() const { return m_state.consumed; }
	u64 overruns() const { return m_state.overruns; }
	u64 underruns() const { return m_state.underruns; }
	u64 front_source_tick() const { return m_state.count ? m_state.source_ticks[m_state.head] : 0; }

protected:
	void device_start() override;
	void device_reset() override;
	void device_stop() override;
private:
	TIMER_CALLBACK_MEMBER(word_tick);
	void arm();
	nokia_mfi_rx_state m_state;
	devcb_read16 m_adc_cb;
	devcb_write_line m_data_available_cb;
	devcb_write_line m_fault_observer_cb;
	emu_timer *m_timer = nullptr;
	unsigned m_divider = 6;
	u64 m_next_tick = 0;
};
DECLARE_DEVICE_TYPE(NOKIA_MFI_RX, nokia_mfi_rx_device)
#endif // MAME_NOKIA_NOKIA_MFI_RX_H
