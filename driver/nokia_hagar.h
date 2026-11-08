// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_HAGAR_H
#define MAME_NOKIA_NOKIA_HAGAR_H

#include "nokia_hagar_command_state.h"

class nokia_hagar_device : public device_t
{
public:
	nokia_hagar_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);
	// Raw compare-edge projection: control:P31:P32:target. This is a declared
	// delivery policy consistent with the observed stable-payload contract,
	// not an independently measured physical SCU latch/completion edge.
	void compare_output_w(u64 tuple);
	void set_trace(bool enabled) { m_trace = enabled; }
	bool rx_candidate(u16 &key, u64 &frequency) const { return m_state.rx_candidate(key, frequency); }
	u64 transactions() const { return m_transactions; }
	u32 last_word() const { return m_state.last_word; }

protected:
	void device_start() override;
	void device_reset() override;

private:
	nokia_hagar_command_state m_state;
	u64 m_transactions = 0;
	bool m_trace = false;
};

DECLARE_DEVICE_TYPE(NOKIA_HAGAR, nokia_hagar_device)
#endif // MAME_NOKIA_NOKIA_HAGAR_H
