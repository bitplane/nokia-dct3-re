// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_BSP_STATE_H
#define MAME_NOKIA_TMS320C54X_BSP_STATE_H

#include <cstdint>

// Standard-mode completed-word state. The attachment owns frame/bit clocks
// and routes ready transitions to interrupts; autobuffering is not modeled.
struct tms320c54x_bsp_state
{
	std::uint16_t control = 0, receive = 0, transmit = 0;
	bool receive_ready = false, transmit_ready = true;
	bool transmit_pending = false, receive_overrun = false;
	bool transmit_loaded = false;

	void reset() { *this = {}; }
	void control_w(std::uint16_t value)
	{
		control = value & 0xc0ff; // Clock pins and status bits are read-only.
		if (!(control & 0x0080))
		{
			receive_ready = false;
			receive_overrun = false;
		}
		if (!(control & 0x0040))
		{
			transmit_ready = true;
			transmit_pending = false;
			transmit_loaded = false;
		}
	}
	std::uint16_t control_r() const
	{
		return control | (receive_ready ? 0x0400 : 0) |
				(transmit_ready ? 0x0800 : 0) | (receive_overrun ? 0x2000 : 0);
	}
	void transmit_w(std::uint16_t value)
	{
		transmit = value;
		if (control & 0x0040)
		{
			transmit_pending = true;
			transmit_loaded = true;
			transmit_ready = false;
		}
	}
	unsigned word_bits(std::uint16_t extension = 0) const
	{
		return extension & 0x0080 ? (control & 0x0004 ? 12 : 10)
				: (control & 0x0004 ? 8 : 16);
	}
	bool frame_transmit(std::uint16_t &value, bool *ready_edge = nullptr,
			std::uint16_t extension = 0)
	{
		if (ready_edge)
			*ready_edge = false;
		if (!(control & 0x0040) || !transmit_loaded)
			return false;
		// External FSX retransmits the retained DXR on underrun (SPRU131G
		// 9.2.4). Only a new write drops XRDY before the next frame.
		value = transmit & ((1U << word_bits(extension)) - 1);
		if (ready_edge)
			*ready_edge = !transmit_ready;
		transmit_pending = false;
		transmit_ready = true;
		return true;
	}
	bool frame_receive(std::uint16_t value, std::uint16_t extension = 0)
	{
		if (!(control & 0x0080) || receive_overrun)
			return false;
		if (receive_ready)
		{
			receive_overrun = true;
			return false;
		}
		// BSP sign-extends 8/10/12-bit receptions (SPRU131G table 9-9).
		// DXR retains all bits even when only its low bits cross the wire.
		const unsigned bits = word_bits(extension);
		const unsigned mask = (1U << bits) - 1;
		const unsigned payload = value & mask;
		receive = std::uint16_t(payload & (1U << (bits - 1))
				? payload | ~mask : payload);
		receive_ready = true;
		return true;
	}
	std::uint16_t receive_r()
	{
		receive_ready = false;
		receive_overrun = false;
		return receive;
	}
};

#endif // MAME_NOKIA_TMS320C54X_BSP_STATE_H
