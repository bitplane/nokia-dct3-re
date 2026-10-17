// license:BSD-3-Clause
// copyright-holders:Gaz
#include "tms320c54x_bsp_state.h"
#include <cassert>
#include <initializer_list>

static auto receive_edge(tms320c54x_bsp_state &port, std::uint16_t extension, bool sync, bool bit)
{
	const bool sample_level = extension & 0x40;
	const auto opposite = port.receive_clock(!sample_level, sync, bit, extension);
	assert(!opposite.receive_ready_edge);
	return port.receive_clock(sample_level, sync, bit, extension);
}

static void check_clock_inputs()
{
	for (unsigned value = 0; value <= 0xffff; ++value)
		for (unsigned pins = 0; pins < 4; ++pins)
		{
			tms320c54x_bsp_state port;
			port.control_w(value);
			port.receive_clock(pins & 1, false, false);
			port.transmit_clock(pins & 2, false);
			assert((port.control_r() & 0x0300) == (pins << 8));
			const auto saved = port;
			// Read-only pin bits cannot be rewritten through BSPC, and engine
			// reset cannot drive externally supplied clocks low.
			port.control_w(0x0300);
			assert((port.control_r() & 0x0300) == (pins << 8));
			port = saved;
			assert((port.control_r() & 0x0300) == (pins << 8));
			port.reset();
			assert(!(port.control_r() & 0x0300));
		}
}

static void receive_sync(tms320c54x_bsp_state &port, std::uint16_t extension, unsigned clocks = 1)
{
	const bool active = !(extension & 0x20);
	const bool ready = port.receive_ready;
	for (unsigned i = 0; i < clocks; ++i)
	{
		const auto result = receive_edge(port, extension, active, i & 1);
		assert(result.supported && !result.receive_ready_edge && port.receive_ready == ready);
	}
}

static void receive_bits(tms320c54x_bsp_state &port, std::uint16_t extension,
		std::uint16_t word, unsigned width, unsigned begin, unsigned end, bool accepted)
{
	const bool inactive = extension & 0x20;
	const bool was_ready = port.receive_ready;
	for (unsigned i = begin; i < end; ++i)
	{
		const auto result = receive_edge(port, extension, inactive, (word >> (width - 1 - i)) & 1);
		assert(result.supported && result.receive_ready_edge == (accepted && i + 1 == width));
		assert(port.receive_ready == (was_ready || (accepted && i + 1 == width)));
	}
}

static void check_receive_pins()
{
	const unsigned widths[] = {16, 8, 10, 12};
	for (unsigned format = 0; format < 4; ++format)
		for (unsigned polarity = 0; polarity < 4; ++polarity)
			for (unsigned word = 0; word <= 0xffff; ++word)
			{
				tms320c54x_bsp_state port;
				const std::uint16_t extension = (format & 2 ? 0x80 : 0) | (polarity & 1 ? 0x40 : 0) | (polarity & 2 ? 0x20 : 0);
				const unsigned width = widths[format], mask = (1U << width) - 1, payload = word & mask;
				port.control_w(0x88 | (format & 1 ? 4 : 0));
				receive_sync(port, extension, 1 + word % 4);
				receive_bits(port, extension, word, width, 0, width, true);
				assert(port.receive_ready && !port.receive_overrun);
				// Holding a clock pin is not another sampled bit or ready edge.
				assert(!port.receive_clock(bool(extension & 0x40), bool(extension & 0x20), false, extension).receive_ready_edge);
				const std::uint16_t expected = payload & (1U << (width - 1)) ? payload | ~mask : payload;
				assert(port.receive_r() == expected && !port.receive_ready);
			}
	for (unsigned format = 0; format < 4; ++format)
		for (unsigned polarity = 0; polarity < 4; ++polarity)
		{
			const unsigned width = widths[format];
			const std::uint16_t extension = (format & 2 ? 0x80 : 0) | (polarity & 1 ? 0x40 : 0) | (polarity & 2 ? 0x20 : 0);
			for (unsigned checkpoint = 0; checkpoint < width; ++checkpoint)
			{
				tms320c54x_bsp_state port;
				port.control_w(0x88 | (format & 1 ? 4 : 0));
				receive_sync(port, extension, 3);
				receive_bits(port, extension, 0xa597, width, 0, checkpoint, false);
				auto restored = port;
				receive_bits(port, extension, 0xa597, width, checkpoint, width, true);
				receive_bits(restored, extension, 0xa597, width, checkpoint, width, true);
				assert(port.control_r() == restored.control_r() && port.receive_r() == restored.receive_r());
			}
		}
	tms320c54x_bsp_state port;
	port.control_w(0x88);
	// No FSR, or RRST during a partial word, cannot publish a sample.
	receive_bits(port, 0, 0x1234, 16, 0, 16, false);
	assert(!port.receive_ready);
	receive_sync(port, 0);
	receive_bits(port, 0, 0x1234, 16, 0, 8, false);
	port.control_w(8); port.control_w(0x88);
	receive_bits(port, 0, 0x1234, 16, 8, 16, false);
	assert(!port.receive_ready);
	// A new FSR aborts the incomplete word and starts the replacement.
	receive_sync(port, 0);
	receive_bits(port, 0, 0xffff, 16, 0, 8, false);
	receive_sync(port, 0);
	receive_bits(port, 0, 0x1234, 16, 0, 16, true);
	assert(port.receive == 0x1234);
	receive_sync(port, 0); // Unread DRR is preserved when the next word overruns.
	receive_bits(port, 0, 0x5678, 16, 0, 16, false);
	assert(port.receive_overrun && port.receive == 0x1234);
	receive_sync(port, 0); // A sync while overrun is active cannot restart RX.
	assert(port.receive_r() == 0x1234);
	receive_bits(port, 0, 0x5678, 16, 0, 16, false);
	assert(!port.receive_ready);
	receive_sync(port, 0);
	receive_bits(port, 0, 0x5678, 16, 0, 16, true);
	assert(port.receive_r() == 0x5678);
	for (unsigned mode = 1; mode < 3; ++mode)
	{
		port.reset();
		port.control_w(mode == 0 ? 0x80 : mode == 1 ? 0x8a : 0x88);
		const std::uint16_t extension = mode == 0 ? 0x100 : mode == 2 ? 0x400 : 0;
		assert(!receive_edge(port, extension, true, false).supported);
		assert(!receive_edge(port, extension, false, true).supported);
		assert(!port.receive_ready);
	}
}

static void check_continuous_receive()
{
	const unsigned widths[] = {16, 8, 10, 12};
	for (unsigned format = 0; format < 4; ++format)
		for (unsigned polarity = 0; polarity < 4; ++polarity)
		{
			const unsigned width = widths[format], mask = (1U << width) - 1;
			const std::uint16_t extension = (format & 2 ? 0x80 : 0) | (polarity & 1 ? 0x40 : 0) | (polarity & 2 ? 0x20 : 0);
			tms320c54x_bsp_state port;
			port.control_w(0x80 | (format & 1 ? 4 : 0));
			receive_bits(port, extension, 0, width, 0, width, false);
			receive_sync(port, extension, 3);
			// One FSR supplies the entire stream. Save both mid-word and at
			// every word boundary; copied state must retain continuous arming.
			for (unsigned word = 0; word <= 0xffff; ++word)
			{
				const unsigned checkpoint = word % width;
				receive_bits(port, extension, word, width, 0, checkpoint, false);
				auto restored = port;
				receive_bits(port, extension, word, width, checkpoint, width, true);
				receive_bits(restored, extension, word, width, checkpoint, width, true);
				const unsigned payload = word & mask;
				const std::uint16_t expected = payload & (1U << (width - 1)) ? payload | ~mask : payload;
				assert(port.receive == expected && restored.receive_r() == expected);
				assert(port.receive_r() == expected && port.receive_continuous && port.receive_bits_left == width);
			}
			receive_bits(port, extension, 0x1234, width, 0, width, true);
			receive_bits(port, extension, 0x5678, width, 0, width, false);
			assert(port.receive_overrun && !port.receive_continuous && !port.receive_bits_left);
			const auto unread = port.receive;
			assert(port.receive_r() == unread && !port.receive_overrun);
			receive_bits(port, extension, 0x5678, width, 0, width, false);
			assert(!port.receive_ready); // Read alone cannot restart a BSP stream.
			receive_sync(port, extension);
			receive_bits(port, extension, 0x5678, width, 0, width, true);
			port.receive_r();
			receive_bits(port, extension, 0xffff, width, 0, width / 2, false);
			receive_sync(port, extension); // Restart discards the incomplete word.
			receive_bits(port, extension, 0x1234, width, 0, width, true);
			port.receive_r();
			port.control_w(0); port.control_w(0x80 | (format & 1 ? 4 : 0));
			receive_bits(port, extension, 0x1234, width, 0, width, false);
			assert(!port.receive_ready && !port.receive_continuous);
		}
}

static auto transmit_edge(tms320c54x_bsp_state &port, std::uint16_t extension, bool sync)
{
	const bool shift_level = !(extension & 0x40);
	const auto opposite = port.transmit_clock(!shift_level, sync, extension);
	assert(!opposite.transmit_ready_edge);
	return port.transmit_clock(shift_level, sync, extension);
}

static void transmit_word(tms320c54x_bsp_state &port, std::uint16_t extension,
		unsigned width, std::uint16_t word, bool ready, unsigned hold = 1)
{
	const bool active = !(extension & 0x20), suppressed = (extension & 0x200) && (word & 0x8000);
	const bool status = port.transmit_no_underflow || ready;
	auto pin = transmit_edge(port, extension, active);
	assert(pin.supported && pin.transmit_ready_edge == ready && pin.driven == !suppressed);
	assert(port.transmit_ready);
	assert(bool(port.control_r() & 0x1000) == status);
	if (pin.driven) assert(pin.data == bool((word >> (width - 1)) & 1));
	for (unsigned i = 1; i < hold; ++i)
	{
		const auto held = transmit_edge(port, extension, active);
		assert(held.supported && !held.transmit_ready_edge && held.driven == pin.driven && held.data == pin.data);
		assert(bool(port.control_r() & 0x1000) == status);
	}
	for (unsigned i = 1; i < width; ++i)
	{
		pin = transmit_edge(port, extension, !active);
		assert(pin.supported && !pin.transmit_ready_edge && pin.driven == !suppressed);
		assert(bool(port.control_r() & 0x1000) == status);
		if (pin.driven) assert(pin.data == bool((word >> (width - 1 - i)) & 1));
	}
	pin = transmit_edge(port, extension, !active);
	assert(pin.supported && !pin.driven && !pin.transmit_ready_edge);
	assert(!(port.control_r() & 0x1000));
}

static void check_transmit_pins()
{
	const unsigned widths[] = {16, 8, 10, 12};
	for (unsigned format = 0; format < 4; ++format)
		for (unsigned polarity = 0; polarity < 4; ++polarity)
			for (unsigned word = 0; word <= 0xffff; ++word)
			{
				tms320c54x_bsp_state port;
				const std::uint16_t extension = (format & 2 ? 0x80 : 0) | (polarity & 1 ? 0x40 : 0) | (polarity & 2 ? 0x20 : 0);
				port.control_w(0x48 | (format & 1 ? 4 : 0));
				port.transmit_w(word);
				assert(!port.transmit_ready);
				assert(!(port.control_r() & 0x1000)); // A write alone is not FSX.
				transmit_word(port, extension, widths[format], word, true, 1 + word % 4);
			}
	for (unsigned format = 0; format < 4; ++format)
		for (unsigned polarity = 0; polarity < 4; ++polarity)
		{
			const unsigned width = widths[format];
			const std::uint16_t extension = (format & 2 ? 0x80 : 0) | (polarity & 1 ? 0x40 : 0) | (polarity & 2 ? 0x20 : 0);
			for (unsigned checkpoint = 1; checkpoint < width; ++checkpoint)
			{
				tms320c54x_bsp_state port;
				port.control_w(0x48 | (format & 1 ? 4 : 0)); port.transmit_w(0xa597);
				assert(transmit_edge(port, extension, !(extension & 0x20)).transmit_ready_edge);
				for (unsigned i = 1; i < checkpoint; ++i) transmit_edge(port, extension, extension & 0x20);
				auto replay = port;
				for (unsigned i = checkpoint; i <= width; ++i)
				{
					const auto a = transmit_edge(port, extension, extension & 0x20), b = transmit_edge(replay, extension, extension & 0x20);
					assert(a.supported == b.supported && a.driven == b.driven && a.data == b.data && a.transmit_ready_edge == b.transmit_ready_edge);
					assert(port.control_r() == replay.control_r());
				}
			}
			for (auto word : {0x7fff, 0x8000, 0x0080, 0xffff})
			{
				tms320c54x_bsp_state port;
				port.control_w(0x48 | (format & 1 ? 4 : 0)); port.transmit_w(word);
				transmit_word(port, extension | 0x200, width, word, true);
			}
		}
	tms320c54x_bsp_state port;
	port.control_w(0x48);
	assert(!transmit_edge(port, 0, true).driven); // Nothing loaded since reset.
	transmit_edge(port, 0, false);
	port.transmit_w(0x1234); port.transmit_w(0x5678);
	transmit_word(port, 0, 16, 0x5678, true); // Delayed FSX uses the last write.
	transmit_word(port, 0, 16, 0x5678, false); // Underrun repeats without XINT.
	port.transmit_w(0x1234);
	assert(transmit_edge(port, 0, true).transmit_ready_edge);
	for (unsigned i = 1; i < 8; ++i)
		assert(transmit_edge(port, 0, false).data == bool((0x1234 >> (15 - i)) & 1));
	port.transmit_w(0x5678); // Queued DXR must not rewrite the current XSR.
	for (unsigned i = 8; i < 16; ++i)
	{
		const auto pin = transmit_edge(port, 0, false);
		assert(pin.driven && pin.data == bool((0x1234 >> (15 - i)) & 1) && !pin.transmit_ready_edge);
	}
	assert(!transmit_edge(port, 0, false).driven);
	assert(port.control_r() & 0x1000); // Queued BDXR prevents end-of-word underrun.
	transmit_word(port, 0, 16, 0x5678, true);
	port.transmit_w(0xabcd);
	assert(transmit_edge(port, 0, true).transmit_ready_edge);
	for (unsigned i = 1; i < 4; ++i) transmit_edge(port, 0, false);
	port.transmit_w(0x5678);
	transmit_word(port, 0, 16, 0x5678, true); // New sync aborts the old word.
	port.transmit_w(0xabcd);
	assert(transmit_edge(port, 0, true).driven);
	port.control_w(8);
	assert(!(port.control_r() & 0x1000));
	assert(!transmit_edge(port, 0, true).driven && !port.transmit_loaded);
	port.control_w(0x48); port.transmit_w(0x5678);
	assert(!transmit_edge(port, 0, true).driven); // Held sync is not a new FSX.
	transmit_edge(port, 0, false);
	transmit_word(port, 0, 16, 0x5678, true);
	for (unsigned mode = 1; mode < 5; ++mode)
	{
		port.reset(); port.control_w(mode == 0 ? 0x40 : mode == 1 ? 0x58 : mode == 2 ? 0x68 : mode == 3 ? 0x4a : 0x48);
		port.transmit_w(0x1234);
		const auto pin = transmit_edge(port, mode == 0 ? 0x100 : mode == 4 ? 0x400 : 0, true);
		assert(!pin.supported && !pin.driven && !pin.transmit_ready_edge && !port.transmit_ready);
	}
}

static void check_joined_pins()
{
	const unsigned widths[] = {16, 8, 10, 12};
	for (unsigned format = 0; format < 4; ++format)
		for (unsigned polarity = 0; polarity < 4; ++polarity)
			for (unsigned input = 0; input < 0x20000; ++input)
			{
				const unsigned word = input & 0xffff;
				const bool long_sync = input & 0x10000;
				tms320c54x_bsp_state tx, rx;
				const std::uint16_t extension = (format & 2 ? 0x80 : 0) | (polarity & 1 ? 0x40 : 0) | (polarity & 2 ? 0x20 : 0);
				const unsigned width = widths[format], mask = (1U << width) - 1, payload = word & mask;
				const std::uint16_t expected = payload & (1U << (width - 1)) ? payload | ~mask : payload;
				const bool shift_level = !(extension & 0x40), active = !(extension & 0x20);
				tx.control_w(0x48 | (format & 1 ? 4 : 0));
				rx.control_w(0x88 | (format & 1 ? 4 : 0));
				tx.transmit_w(word);
				const auto edge = [extension](auto &source, auto &sink, bool clock, bool sync) {
					const auto pin = source.transmit_clock(clock, sync, extension);
					const auto received = sink.receive_clock(clock, sync, pin.driven && pin.data, extension);
					assert(pin.supported && received.supported);
					return pin;
				};
				edge(tx, rx, !shift_level, !active);
				if (!long_sync)
				{
					// The lab CLOCK source presents active sync at the RX edge
					// before the TX load edge, then releases it for the next RX.
					edge(tx, rx, shift_level, !active);
					edge(tx, rx, !shift_level, active);
				}
				assert(edge(tx, rx, shift_level, active).transmit_ready_edge);
				// SPRU131G 9.2.4: a long sync holds TX's MSB. Release it
				// between a TX edge and the opposite RX edge, never at either
				// sampling edge. This is an explicit lab phase, not Nokia wiring.
				for (unsigned hold = 0; long_sync && hold <= word % 4; ++hold)
				{
					edge(tx, rx, !shift_level, active);
					const auto pin = edge(tx, rx, shift_level, active);
					assert(pin.driven && pin.data == bool(payload & (1U << (width - 1))) && !pin.transmit_ready_edge);
					assert(!rx.receive_ready);
				}
				edge(tx, rx, shift_level, !active); // Sync-only transition: no bit clock.
				auto replay_tx = tx, replay_rx = rx;
				for (unsigned bit = 0; bit < width; ++bit)
				{
					if (bit)
					{
						edge(tx, rx, shift_level, !active);
						edge(replay_tx, replay_rx, shift_level, !active);
					}
					const auto pin = edge(tx, rx, !shift_level, !active);
					const auto replay = edge(replay_tx, replay_rx, !shift_level, !active);
					assert(pin.driven && pin.data == bool((payload >> (width - 1 - bit)) & 1));
					assert(!pin.transmit_ready_edge && pin.data == replay.data && pin.driven == replay.driven);
					assert(rx.receive_ready == (bit + 1 == width));
					assert(rx.control_r() == replay_rx.control_r() && tx.control_r() == replay_tx.control_r());
					// Also replay from a partially transmitted/received word.
					if (bit == width / 2) { replay_tx = tx; replay_rx = rx; }
				}
				assert(rx.receive_r() == expected && replay_rx.receive_r() == expected);
				assert(!rx.receive_overrun && !replay_rx.receive_overrun);
				assert(!edge(tx, rx, shift_level, !active).driven);
			}
}

static void check_continuous_transmit()
{
	const unsigned widths[] = {16, 8, 10, 12};
	for (unsigned format = 0; format < 4; ++format)
		for (unsigned polarity = 0; polarity < 4; ++polarity)
		{
			const unsigned width = widths[format];
			const std::uint16_t extension = (format & 2 ? 0x80 : 0) | (polarity & 1 ? 0x40 : 0) | (polarity & 2 ? 0x20 : 0);
			const bool active = !(extension & 0x20);
			tms320c54x_bsp_state port;
			port.control_w(0x40 | (format & 1 ? 4 : 0));
			port.transmit_w(0);
			for (unsigned word = 0; word <= 0xffff; ++word)
				for (unsigned bit = 0; bit < width; ++bit)
				{
					const auto pin = transmit_edge(port, extension, !word && !bit ? active : !active);
					assert(pin.supported && pin.driven && pin.data == bool((word >> (width - 1 - bit)) & 1));
					assert(pin.transmit_ready_edge == !bit && (port.control_r() & 0x1000));
					if (bit == width / 2 && word != 0xffff) port.transmit_w(word + 1);
				}
			assert(!transmit_edge(port, extension, !active).driven && !(port.control_r() & 0x1000));
			// Underrun halts: a later DXR write alone supplies no replacement FSX.
			port.transmit_w(0x1357);
			for (unsigned i = 0; i < width * 2; ++i)
			{
				const auto pin = transmit_edge(port, extension, !active);
				assert(!pin.driven && !pin.transmit_ready_edge && !port.transmit_ready);
			}
			transmit_word(port, extension, width, 0x1357, true, 3);
			transmit_word(port, extension, width, 0x1357, false); // Fresh FSX can repeat retained data.
			for (unsigned checkpoint = 1; checkpoint < width; ++checkpoint)
			{
				port.reset(); port.control_w(0x40 | (format & 1 ? 4 : 0));
				port.transmit_w(0x5678);
				assert(transmit_edge(port, extension, active).transmit_ready_edge);
				port.transmit_w(0x9abc);
				for (unsigned i = 1; i < checkpoint; ++i) transmit_edge(port, extension, !active);
				auto replay = port;
				for (unsigned i = checkpoint; i < 2 * width; ++i)
				{
					const auto a = transmit_edge(port, extension, !active), b = transmit_edge(replay, extension, !active);
					const unsigned expected = i < width ? 0x5678 : 0x9abc;
					assert(a.driven && b.driven && a.data == bool((expected >> (width - 1 - i % width)) & 1) && a.data == b.data);
					assert(a.transmit_ready_edge == (i == width) && a.transmit_ready_edge == b.transmit_ready_edge);
					assert(port.control_r() == replay.control_r());
				}
				assert(!transmit_edge(port, extension, !active).driven && !transmit_edge(replay, extension, !active).driven);
			}
			// Extra FSX aborts the current word and starts the queued replacement.
			port.reset(); port.control_w(0x40 | (format & 1 ? 4 : 0)); port.transmit_w(0xffff);
			assert(transmit_edge(port, extension, active).transmit_ready_edge);
			transmit_edge(port, extension, !active); port.transmit_w(0x1234);
			transmit_word(port, extension, width, 0x1234, true);
			port.transmit_w(0xffff); transmit_edge(port, extension, active);
			port.control_w(0); port.control_w(0x40 | (format & 1 ? 4 : 0));
			for (unsigned i = 0; i < width; ++i) assert(!transmit_edge(port, extension, !active).driven);
			// PCM suppression applies per word, not to the following queued word.
			port.transmit_w(0x8000);
			assert(!transmit_edge(port, extension | 0x200, active).driven);
			port.transmit_w(0x1234);
			for (unsigned i = 1; i < width; ++i) assert(!transmit_edge(port, extension | 0x200, !active).driven);
			const auto next = transmit_edge(port, extension | 0x200, !active);
			assert(next.driven && next.transmit_ready_edge && next.data == bool((0x1234 >> (width - 1)) & 1));
		}
}

static void check_frame_ignore()
{
	const unsigned widths[] = {16, 8, 10, 12};
	for (unsigned format = 0; format < 4; ++format)
		for (unsigned polarity = 0; polarity < 4; ++polarity)
			for (unsigned pattern = 0; pattern < 256; ++pattern)
			{
				const unsigned width = widths[format], mask = (1U << width) - 1;
				const std::uint16_t extension = 0x100 | (format & 2 ? 0x80 : 0) | (polarity & 1 ? 0x40 : 0) | (polarity & 2 ? 0x20 : 0);
				const bool active = !(extension & 0x20);
				const unsigned first = pattern * 257, second = first ^ 0xffff;
				tms320c54x_bsp_state rx, tx;
				rx.control_w(0x80 | (format & 1 ? 4 : 0));
				tx.control_w(0x40 | (format & 1 ? 4 : 0));
				// FIG does not supply the initial frame, or shorten its long pulse.
				assert(!receive_edge(rx, extension, !active, true).receive_ready_edge && !rx.receive_bits_left);
				receive_sync(rx, extension, 3);
				tx.transmit_w(first);
				assert(transmit_edge(tx, extension, active).transmit_ready_edge);
				assert(transmit_edge(tx, extension, active).data == bool((first >> (width - 1)) & 1));
				tx.transmit_w(second);
				for (unsigned i = 0; i < width * 2; ++i)
				{
					const unsigned word = i < width ? first : second;
					const bool bit = (word >> (width - 1 - i % width)) & 1;
					// Later pulses span multiple bits, including word boundaries.
					const bool sync = i && (i % 4 >= 2) ? active : !active;
					auto rx_copy = rx, tx_copy = tx;
					const auto received = receive_edge(rx, extension, sync, bit);
					const auto replay = receive_edge(rx_copy, extension, sync, bit);
					assert(received.supported && received.receive_ready_edge == (i % width == width - 1));
					assert(received.receive_ready_edge == replay.receive_ready_edge && rx.receive == rx_copy.receive);
					if (received.receive_ready_edge)
					{
						const unsigned value = word & mask;
						const std::uint16_t expected = value & (1U << (width - 1)) ? value | ~mask : value;
						assert(rx.receive_r() == expected);
					}
					if (i)
					{
						const auto sent = transmit_edge(tx, extension, sync);
						const auto replay_tx = transmit_edge(tx_copy, extension, sync);
						assert(sent.supported && sent.driven && sent.data == bit);
						assert(sent.transmit_ready_edge == (i == width));
						assert(sent.data == replay_tx.data && sent.transmit_ready_edge == replay_tx.transmit_ready_edge);
					}
				}
				assert(!transmit_edge(tx, extension, !active).driven);
				// FIG cannot keep a reset stream armed.
				rx.control_w(0); tx.control_w(0);
				assert(!rx.receive_sync_released && !tx.transmit_sync_released);
			}
}

int main()
{
	check_clock_inputs();
	check_receive_pins();
	check_continuous_receive();
	check_transmit_pins();
	check_joined_pins();
	check_continuous_transmit();
	check_frame_ignore();
	tms320c54x_bsp_state port;
	std::uint16_t duplex_output = 0xaaaa;
	port.control_w(0x00c2);
	// Released DLB without loaded TX/frame cannot manufacture RX readiness.
	auto frame = port.duplex_frame(0x5555, duplex_output);
	assert(!frame.transmitted && !frame.transmit_ready_edge && !frame.receive_ready_edge);
	assert(!port.receive_ready);
	port.transmit_w(0x1234);
	assert(!port.receive_ready); // Writing DXR does not advance either clock.
	frame = port.duplex_frame(0x5555, duplex_output);
	assert(frame.transmitted && frame.transmit_ready_edge && frame.receive_ready_edge);
	assert(port.receive == 0x1234 && duplex_output == 0x1234);
	frame = port.duplex_frame(0x5555, duplex_output);
	assert(frame.transmitted && !frame.transmit_ready_edge && !frame.receive_ready_edge);
	assert(port.receive_overrun && port.receive_r() == 0x1234);
	port.control_w(0x00c0);
	frame = port.duplex_frame(0x5555, duplex_output);
	assert(frame.receive_ready_edge && port.receive_r() == 0x5555);
	port.control_w(0x0042); // DLB with receiver held in reset.
	port.transmit_w(0xf234);
	frame = port.duplex_frame(0, duplex_output);
	assert(frame.transmitted && frame.transmit_ready_edge && !frame.receive_ready_edge);
	assert(!port.receive_ready);
	port.control_w(0x0082); // Transmitter reset cannot drive a looped frame.
	frame = port.duplex_frame(0x5555, duplex_output);
	assert(!frame.transmitted && !frame.transmit_ready_edge && !frame.receive_ready_edge);
	port.control_w(0x00c2);
	port.transmit_w(0x6789);
	auto duplex_saved = port;
	const auto original_frame = port.duplex_frame(0x5555, duplex_output);
	std::uint16_t restored_output = 0;
	const auto restored_frame = duplex_saved.duplex_frame(0x5555, restored_output);
	assert(original_frame.transmitted == restored_frame.transmitted);
	assert(original_frame.transmit_ready_edge == restored_frame.transmit_ready_edge);
	assert(original_frame.receive_ready_edge == restored_frame.receive_ready_edge);
	assert(duplex_output == restored_output && port.control_r() == duplex_saved.control_r());
	assert(port.receive_r() == duplex_saved.receive_r());
	port.reset();
	assert(port.control_r() == 0x0800);
	for (unsigned value = 0; value <= 0xffff; ++value)
	{
		port.reset();
		port.control_w(value);
		assert(port.control_r() == ((value & 0xc0ff) | 0x0800));
	}
	port.reset();
	assert(!port.frame_receive(1));
	port.control_w(0x00c0);
	assert(port.frame_receive(0x1234));
	assert(port.control_r() == 0x0cc0);
	assert(!port.frame_receive(0x5678));
	assert(port.control_r() == 0x2cc0);
	assert(!port.frame_receive(0x9999));
	assert(port.receive_r() == 0x1234);
	assert(port.control_r() == 0x08c0);
	assert(port.receive_r() == 0x1234);
	assert(port.frame_receive(0x5678));
	port.control_w(0x0040);
	assert(!port.receive_ready && !port.receive_overrun);
	assert(port.receive_r() == 0x5678);
	port.transmit_w(0xabcd);
	assert(port.transmit_pending && !port.transmit_ready);
	std::uint16_t output = 0;
	bool ready_edge = false;
	auto saved = port;
	assert(port.frame_transmit(output, &ready_edge) && output == 0xabcd);
	assert(ready_edge);
	assert(port.transmit_ready && !port.transmit_pending);
	assert(port.frame_transmit(output, &ready_edge) && output == 0xabcd);
	assert(!ready_edge);
	assert(port.transmit_ready && !port.transmit_pending);
	assert(saved.frame_transmit(output) && output == 0xabcd);
	assert(saved.control_r() == port.control_r());
	port.transmit_w(2);
	port.transmit_w(3);
	assert(port.frame_transmit(output) && output == 3);
	assert(port.frame_transmit(output) && output == 3);
	assert(port.frame_transmit(output) && output == 3);
	port.transmit_w(4);
	port.control_w(0);
	assert(port.transmit_ready && !port.transmit_pending);
	ready_edge = true;
	assert(!port.frame_transmit(output, &ready_edge));
	assert(!ready_edge);
	port.control_w(0x0040);
	assert(!port.frame_transmit(output));
	port.reset();
	assert(port.control_r() == 0x0800 && port.receive == 0 && port.transmit == 0);
	// Explicit completed frames, not a claimed handset clock. Delayed reads
	// exercise unread-word retention across a sustained stream and state copy.
	for (unsigned mode = 0; mode < 8; ++mode)
	{
		port.reset();
		const unsigned widths[] = {16, 8, 10, 12};
		const unsigned format = mode % 4;
		const unsigned width = widths[format];
		const unsigned mask = (1U << width) - 1;
		const std::uint16_t extension = format & 2 ? 0x0080 : 0;
		const bool loopback = mode >= 4;
		port.control_w(0x00c0 | (format & 1 ? 4 : 0) | (loopback ? 2 : 0));
		std::uint16_t held_tx = 0, held_rx = 0;
		bool unread = false;
		for (unsigned frame = 0; frame < 4096; ++frame)
		{
			const bool load = frame % 4 == 0;
			if (load)
			{
				held_tx = std::uint16_t(frame * 257U + 0x80a5U);
				port.transmit_w(held_tx);
			}
			const std::uint16_t input = std::uint16_t(frame * 131U + 0x5a80U);
			auto replay = port;
			std::uint16_t replay_output = 0;
			const auto result = port.duplex_frame(input, output, extension);
			const auto repeated = replay.duplex_frame(input, replay_output, extension);
			assert(result.transmitted && result.transmit_ready_edge == load);
			assert(result.receive_ready_edge == !unread);
			assert(output == (held_tx & mask));
			assert(repeated.transmitted == result.transmitted &&
				repeated.transmit_ready_edge == result.transmit_ready_edge &&
				repeated.receive_ready_edge == result.receive_ready_edge && replay_output == output);
			if (!unread)
			{
				const unsigned payload = (loopback ? held_tx : input) & mask;
				held_rx = std::uint16_t(payload & (1U << (width - 1)) ? payload | ~mask : payload);
			}
			unread = true;
			assert(port.receive == held_rx && replay.receive == held_rx);
			assert(port.control_r() == replay.control_r());
			if (frame % 3 == 2)
			{
				assert(port.receive_r() == held_rx && replay.receive_r() == held_rx);
				assert(port.control_r() == replay.control_r());
				unread = false;
			}
			port = replay;
		}
	}
	for (unsigned value = 0; value <= 0xffff; ++value)
	{
		for (unsigned format = 0; format != 4; ++format)
		{
			const unsigned bits[] = {16, 8, 10, 12};
			const unsigned width = bits[format];
			const std::uint16_t extension = format & 2 ? 0x0080 : 0;
			const unsigned mask = (1U << width) - 1;
			port.reset();
			port.control_w(0x00c0 | (format & 1 ? 0x0004 : 0));
			assert(port.word_bits(extension) == width);
			port.transmit_w(value);
			assert(port.frame_transmit(output, &ready_edge, extension));
			assert(output == (value & mask) && ready_edge);
			assert(port.frame_transmit(output, &ready_edge, extension) && !ready_edge);
			assert(port.frame_receive(value, extension));
			const unsigned payload = value & mask;
			const auto expected = std::uint16_t(payload & (1U << (width - 1))
					? payload | ~mask : payload);
			assert(port.receive_r() == expected);
			port.control_w(port.control | 2);
			port.transmit_w(value);
			const auto looped = port.duplex_frame(~value, output, extension);
			assert(looped.transmitted && looped.transmit_ready_edge && looped.receive_ready_edge);
			assert(output == (value & mask) && port.receive_r() == expected);
		}
		port.reset();
		port.control_w(0x00c4); // 8-bit BSP format.
		port.transmit_w(value);
		assert(port.transmit == value);
		assert(port.frame_transmit(output) && output == (value & 0xff));
		assert(port.frame_receive(value));
		const unsigned expected = value & 0x80 ? (value | 0xff00) : (value & 0xff);
		assert(port.receive_r() == expected);
		port.control_w(0x00c0);
		port.transmit_w(value);
		assert(port.frame_transmit(output) && output == value);
		assert(port.frame_receive(value));
		assert(port.receive_r() == value);
	}
}
