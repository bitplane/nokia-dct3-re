// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_DSP_CONTROL_STATE_H
#define MAME_NOKIA_NOKIA_DSP_CONTROL_STATE_H

#include <cstdint>

// Partial DSP-side port-36 request/completion handshake. The attachment supplies
// completion clocks; this state supplies no RF/sample/calibration result.
struct nokia_dsp_control_state
{
	std::uint16_t control = 0;
	bool pending = false, busy = false, done = false, strobe = false;
	void reset() { *this = {}; }
	std::uint16_t read() const { return control | (done ? 0x4000 : 0); }
	// Return true when a new/aborted request cancels an older completion clock.
	bool write(std::uint16_t value)
	{
		bool const was_requested = control & 0x8000;
		std::uint16_t const next = value & ~0x4000; // DONE is hardware-owned.
		bool const changed = next != control;
		control = next;
		if (!(control & 0x8000) || !was_requested || changed) {
			busy = done = false;
			// The recovered synchronous control selector is 0x0e in bits
			// 1..4. Other commands cannot acquire an invented completion.
			pending = (control & 0x801e) == 0x801c;
			return true;
		}
		return false;
	}
	bool strobe_w(bool level)
	{
		bool const start = level && !strobe && pending;
		strobe = level;
		if (start) { pending = false; busy = true; }
		return start;
	}
	void complete()
	{
		if (busy) { busy = false; done = true; }
	}
};

#endif // MAME_NOKIA_NOKIA_DSP_CONTROL_STATE_H
