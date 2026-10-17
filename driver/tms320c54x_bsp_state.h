// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_BSP_STATE_H
#define MAME_NOKIA_TMS320C54X_BSP_STATE_H

#include <cstdint>

// Standard-mode completed-word state and external burst shifters. The
// attachment owns clocks and interrupt routing; autobuffering is not modeled.
struct tms320c54x_bsp_state
{
	std::uint16_t control = 0, receive = 0, transmit = 0;
	bool receive_ready = false, transmit_ready = true;
	bool transmit_pending = false, receive_overrun = false;
	bool transmit_loaded = false;
	bool transmit_no_underflow = false;
	std::uint16_t receive_shift = 0;
	std::uint8_t receive_bits_left = 0, receive_width = 0;
	bool receive_clock_level = false, receive_sync_active = false;
	bool receive_continuous = false;
	bool receive_sync_released = false;
	std::uint16_t transmit_shift = 0;
	std::uint8_t transmit_bits_left = 0;
	bool transmit_clock_level = false, transmit_sync_active = false;
	bool transmit_suppressed = false;
	bool transmit_continuous = false;
	bool transmit_sync_released = false;

	void reset() { *this = {}; }
	void reset_receive_shift()
	{
		receive_shift = 0;
		receive_bits_left = receive_width = 0;
		receive_continuous = false;
		receive_sync_released = false;
	}
	void control_w(std::uint16_t value)
	{
		control = value & 0xc0ff; // Clock pins and status bits are read-only.
		if (!(control & 0x0080))
		{
			receive_ready = false;
			receive_overrun = false;
			reset_receive_shift();
		}
		if (!(control & 0x0040))
		{
			transmit_ready = true;
			transmit_pending = false;
			transmit_loaded = false;
			transmit_no_underflow = false;
			reset_transmit_shift();
		}
	}
	std::uint16_t control_r() const
	{
		return control | (receive_ready ? 0x0400 : 0) |
				(transmit_ready ? 0x0800 : 0) | (receive_overrun ? 0x2000 : 0) |
				(transmit_no_underflow ? 0x1000 : 0) |
				(receive_clock_level ? 0x0100 : 0) | (transmit_clock_level ? 0x0200 : 0);
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
	void reset_transmit_shift()
	{
		transmit_shift = 0;
		transmit_bits_left = 0;
		transmit_suppressed = false;
		transmit_continuous = false;
		transmit_sync_released = false;
	}
	struct transmit_clock_result
	{
		bool supported, driven, data, transmit_ready_edge;
	};
	transmit_clock_result transmit_clock(bool clock, bool sync,
			std::uint16_t extension = 0)
	{
		// External CLKX/FSX only; FIG affects continuous mode, not bursts.
		const bool supported = !(control & 0x0032) && !(extension & 0xfc00);
		const bool edge = clock != transmit_clock_level && clock != bool(extension & 0x0040);
		transmit_clock_level = clock;
		const bool active = sync != bool(extension & 0x0020);
		const bool previous_sync = transmit_sync_active;
		// FIG ignores whole later pulses, not just their active edge. The
		// initial pulse must still release before serial shifting begins.
		const bool ignore_sync = transmit_continuous && transmit_sync_released && (extension & 0x0100);
		if (edge) transmit_sync_active = active;
		bool ready_edge = false;
		const auto load_word = [&]() {
			std::uint16_t word;
			if (!start_transmit(word, &ready_edge, extension))
			{
				reset_transmit_shift();
				return;
			}
			transmit_bits_left = word_bits(extension);
			transmit_shift = word << (16 - transmit_bits_left);
			transmit_suppressed = (extension & 0x0200) && (transmit & 0x8000);
			transmit_continuous = !(control & 0x0008);
		};
		if (!supported || !(control & 0x0040)) reset_transmit_shift();
		else if (edge)
		{
			if (active && !previous_sync && !ignore_sync)
			{
				transmit_sync_released = false;
				load_word();
			}
			else if ((!active || ignore_sync) && transmit_bits_left)
			{
				if (!active) transmit_sync_released = true;
				// MSB is driven during FSX; long FSX holds it. The remaining
				// bits advance after FSX deasserts (SPRU131G 9.2.4).
				if (--transmit_bits_left) transmit_shift <<= 1;
				else
				{
					const bool continue_stream = transmit_continuous && transmit_pending;
					finish_transmit();
					reset_transmit_shift();
					// SPRU131G 9.2.5: queued DXR transfers without another FSX;
					// underrun halts instead of inventing repeated word clocks.
					if (continue_stream)
					{
						load_word();
						transmit_sync_released = true;
					}
				}
			}
		}
		return {supported, bool(transmit_bits_left) && !transmit_suppressed,
			bool(transmit_shift & 0x8000), ready_edge};
	}
	bool frame_transmit(std::uint16_t &value, bool *ready_edge = nullptr,
			std::uint16_t extension = 0)
	{
		const bool sent = start_transmit(value, ready_edge, extension);
		if (sent) finish_transmit();
		return sent;
	}
	bool start_transmit(std::uint16_t &value, bool *ready_edge,
			std::uint16_t extension)
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
		// BSP XSREMPTY deasserts only after a fresh BDXR write AND FSX;
		// retransmitting a retained word does not clear a prior underrun.
		if (transmit_pending) transmit_no_underflow = true;
		transmit_pending = false;
		transmit_ready = true;
		return true;
	}
	void finish_transmit()
	{
		// SPRU131G 9.2.3: underrun asserts after the final bit only when
		// BDXR has not been reloaded since the last BDXR-to-XSR transfer.
		if (!transmit_pending) transmit_no_underflow = false;
	}
	bool frame_receive(std::uint16_t value, std::uint16_t extension = 0)
	{
		return receive_word(value, word_bits(extension));
	}
	bool receive_word(std::uint16_t value, unsigned bits)
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
		const unsigned mask = (1U << bits) - 1;
		const unsigned payload = value & mask;
		receive = std::uint16_t(payload & (1U << (bits - 1))
				? payload | ~mask : payload);
		receive_ready = true;
		return true;
	}
	struct clock_result
	{
		bool supported, receive_ready_edge;
	};
	clock_result receive_clock(bool clock, bool sync, bool bit,
			std::uint16_t extension = 0)
	{
		// External standard receive; DLB and autobuffering remain separate.
		const bool supported = !(control & 0x0002) && !(extension & 0xfc00);
		const bool edge = clock != receive_clock_level && clock == bool(extension & 0x0040);
		receive_clock_level = clock;
		if (!edge) return {supported, false};
		const bool active = sync != bool(extension & 0x0020);
		const bool previous_sync = receive_sync_active;
		const bool ignore_sync = receive_continuous && receive_sync_released && (extension & 0x0100);
		receive_sync_active = active;
		if (!supported || !(control & 0x0080) || receive_overrun)
		{
			reset_receive_shift();
			return {supported, false};
		}
		// SPRU131G 9.2.4/9.3: qualify the sync pulse at the selected clock
		// edge; its inactive edge precedes the MSB. A long pulse holds shifting.
		if (active && !ignore_sync)
		{
			if (!previous_sync)
			{
				receive_shift = 0;
				receive_bits_left = receive_width = word_bits(extension);
				receive_continuous = !(control & 0x0008);
				receive_sync_released = false;
			}
			return {true, false};
		}
		if (!receive_bits_left) return {true, false};
		if (!active) receive_sync_released = true;
		receive_shift = (receive_shift << 1) | unsigned(bit);
		if (--receive_bits_left) return {true, false};
		const unsigned width = receive_width;
		const bool continuous = receive_continuous;
		const bool ready = receive_word(receive_shift, width);
		reset_receive_shift();
		// SPRU131G 9.2.5: BSP overrun requires DRR read AND a fresh FSR.
		// An ordinary acknowledged word continues on the very next sample edge.
		if (continuous && !receive_overrun)
		{
			receive_continuous = true;
			receive_sync_released = true;
			receive_bits_left = receive_width = width;
		}
		return {true, ready};
	}
	std::uint16_t receive_r()
	{
		receive_ready = false;
		receive_overrun = false;
		return receive;
	}
	struct duplex_result
	{
		bool transmitted, transmit_ready_edge, receive_ready_edge;
	};
	// The caller supplies a completed frame with both clock domains advanced.
	// DLB alone must not create clocks (SPRU131G 9.2.3, MCM=0).
	duplex_result duplex_frame(std::uint16_t input, std::uint16_t &output,
			std::uint16_t extension = 0)
	{
		bool tx_edge;
		const bool sent = frame_transmit(output, &tx_edge, extension);
		const bool loopback = control & 0x0002;
		const bool received = (!loopback || sent) &&
				frame_receive(loopback ? output : input, extension);
		return {sent, tx_edge, received};
	}
};

#endif // MAME_NOKIA_TMS320C54X_BSP_STATE_H
